import { UARTSClient } from "../interfaces/UART-S/UARTSClient";
import { WiFiClient } from "../interfaces/WiFi/WiFiClient";
import { StructCodec, T, struct, InferDTO } from "./SchemaBuilder";

export enum ProcedimientoSistemaRPC {
    L1_HANDSHAKE_UARTS    = 0x0101,
    L1_KEEPALIVE_UARTS    = 0x0102,
    L1_CERRAR_UARTS       = 0x0103,
    L1_KEEPALIVE_WIFI     = 0x0104,
    L1_SUSCRIBIR_UDP_WIFI = 0x0105,
    L1_CERRAR_WIFI        = 0x0106,

    L2_LOGIN_USUARIO      = 0x0201,
    L2_LOGOUT_USUARIO     = 0x0202
}

export const BANDERA_ERROR_RPC = 0x8000;
export type MedioActivoCliente = "NINGUNO" | "UART-S" | "WIFI";

export enum PrioridadRPC {
    CRITICA = 0, // L1 / L2 (handshake, keepalive, login, logout, cerrar)
    ALTA    = 1, // Mutaciones y acciones explícitas del operador
    NORMAL  = 2  // Lecturas, listados y polling periódico
}

export interface OpcionesInvocacionRPC {
    deduplicable?: boolean;
    prioridad?: PrioridadRPC;
    cacheTtlMs?: number;
    invalidaCache?: boolean;
}

// DTOs internos de L1
export const HandshakeUartsDTO = struct({
    versionProtocolo: T.uint8()
});

export const PerfilEnlaceUartsDTO = struct({
    versionServidor: T.uint8(),
    mtuBytes:        T.posSmallInt()
});
export type PerfilEnlaceUartsCliente = InferDTO<typeof PerfilEnlaceUartsDTO>;

const SuscripcionUdpDTO = struct({
    puertoEscuchaCliente: T.posSmallInt()
});

const CanalStreamingUdpDTO = struct({
    puertoEmisorServidor: T.posSmallInt()
});

export function hashNombreProcedimiento(nombre: string): number {
    let h = 2166136261 >>> 0;
    for (let i = 0; i < nombre.length; i++) {
        h ^= nombre.charCodeAt(i) & 0xFF;
        h = Math.imul(h, 16777619) >>> 0;
    }
    const id15 = ((h ^ (h >>> 15)) & 0x7FFF) >>> 0;
    return id15 < 0x0300 ? (id15 + 0x0300) & 0x7FFF : id15;
}

interface PeticionPendiente {
    resolve: (payload: Buffer) => void;
    reject: (err: Error) => void;
    timer: ReturnType<typeof setTimeout>;
}

interface TareaEncoladaRPC {
    idProc: number;
    payloadDto: Buffer;
    prioridad: PrioridadRPC;
    reintentosBusy: number;
    resolve: (payload: Buffer) => void;
    reject: (err: Error) => void;
}

interface EntradaCacheRPC {
    payload: Buffer;
    expiraEnMs: number;
}

export class FirmwareRPCClient {
    private static readonly INTERVALO_REVISION_KEEPALIVE_MS = 10000;
    private static readonly UMBRAL_SILENCIO_KEEPALIVE_MS    = 40000;
    private static readonly TIMEOUT_RPC_MS                  = 14000;

    // Protección del ESP32 desde el backend
    private static readonly CAPACIDAD_MAX_COLA_TAREAS       = 24;
    private static readonly PACING_MIN_WIFI_MS              = 45;
    private static readonly MAX_REINTENTOS_BUSY             = 3;
    private static readonly BACKOFF_BUSY_MS                 = [250, 500, 900];

    private readonly uarts: UARTSClient;
    private readonly wifi: WiFiClient;

    private medioActivo: MedioActivoCliente ="NINGUNO";
    private proximoTicket = 1;
    private pendientes = new Map<number, PeticionPendiente>();
    private enVueloPorClave = new Map<string, Promise<Buffer>>();
    private cacheRespuestas = new Map<string, EntradaCacheRPC>();

    private colaTareas: TareaEncoladaRPC[] = [];
    private despachadorActivo = false;
    private ultimoEnvioMs = 0;
    private ultimoContactoMs = 0;

    private listenersStreamPorProc = new Map<number, (payload: Buffer) => void>();
    private udpWiFiVinculado = false;
    private promesaSuscripcionUdp: Promise<void> | null = null;
    private timerKeepalive?: ReturnType<typeof setInterval>;

    private logsVisor: string[] = [];
    private visorConsolaActivo = true;

    constructor() {
        this.uarts = new UARTSClient({
            onMessage: (frame: Buffer) => this.procesarFrameRecibido(frame),
            onConnectionChange: estado => {
                this.registrarLogVisor(`[ESTADO UART-S] -> ${estado}`);
                if (estado === "DESCONECTADO" || estado === "ERROR") {
                    this.limpiarEstadoAlDesconectar();
                }
            },
            onDebugLog: linea => this.registrarLogVisor(linea)
        });

        this.wifi = new WiFiClient({
            onMessage: (frame: Buffer) => this.procesarFrameRecibido(frame),
            onDisconnect: () => {
                this.registrarLogVisor("[ESTADO WIFI] -> DESCONECTADO");
                this.limpiarEstadoAlDesconectar();
            },
            onDebugLog: linea => this.registrarLogVisor(linea)
        });
    }

    static empaquetarPeticion(idTicket: number, idProc: number, payloadDto: Buffer = Buffer.alloc(0)): Buffer {
        const longitudPayload = 4 + payloadDto.length;
        const frame = Buffer.alloc(8 + payloadDto.length);
        frame.writeUInt32BE(longitudPayload, 0);
        frame.writeUInt16BE(idTicket & 0xFFFF, 4);
        frame.writeUInt16BE(idProc & 0xFFFF, 6);
        if (payloadDto.length > 0) {
            payloadDto.copy(frame, 8);
        }
        return frame;
    }

    static desempaquetarRespuesta(frameCompleto: Buffer): {
        idTicket: number;
        idProc: number;
        esError: boolean;
        payload: Buffer;
    } | null {
        if (frameCompleto.length < 8) return null;
        const longitudUtil = frameCompleto.readUInt32BE(0);
        if (frameCompleto.length < 4 + longitudUtil) return null;
        const idTicket = frameCompleto.readUInt16BE(4);
        const idProcCrudo = frameCompleto.readUInt16BE(6);
        const esError = (idProcCrudo & BANDERA_ERROR_RPC) !== 0;
        const idProc = idProcCrudo & ~BANDERA_ERROR_RPC;
        const payload = frameCompleto.subarray(8, 4 + longitudUtil);
        return { idTicket, idProc, esError, payload };
    }

    static decodificarExcepcionCruda(idProc: number, payload: Buffer): Error {
        const finNulo = payload.indexOf(0);
        const textoCrudo = payload
            .subarray(0, finNulo === -1 ? payload.length : finNulo)
            .toString("utf8")
            .trim();
        return new Error(
            textoCrudo.length > 0
                ? textoCrudo
                : `RuntimeError: excepción remota en procedimiento 0x${idProc.toString(16)}`
        );
    }

    private registrarLogVisor(linea: string): void {
        const entrada = `${new Date().toISOString()} ${linea}`;
        this.logsVisor.push(entrada);
        if (this.logsVisor.length > 200) {
            this.logsVisor.shift();
        }
        if (this.visorConsolaActivo) {
            console.log(entrada);
        }
    }

    obtenerLogsVisor(): string[] {
        return [...this.logsVisor];
    }

    limpiarLogsVisor(): void {
        this.logsVisor = [];
    }

    invalidarCache(): void {
        this.cacheRespuestas.clear();
    }

    async conectarPorSerie(rutaPuerto: string): Promise<PerfilEnlaceUartsCliente> {
        await this.desconectar();
        await this.uarts.conectar(rutaPuerto);
        this.medioActivo = "UART-S";
        this.ultimoContactoMs = Date.now();

        try {
            const resp = await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_HANDSHAKE_UARTS,
                HandshakeUartsDTO.serialize({ versionProtocolo: 1 }),
                { prioridad: PrioridadRPC.CRITICA }
            );
            const perfil = PerfilEnlaceUartsDTO.deserialize(resp);
            this.iniciarPulsoKeepaliveAutomatico();
            return perfil;
        } catch (err) {
            await this.desconectar();
            throw err;
        }
    }

    async conectarPorWiFi(ipServidor: string): Promise<void> {
        await this.desconectar();
        await this.wifi.conectar(ipServidor);
        this.medioActivo = "WIFI";
        this.udpWiFiVinculado = false;
        this.promesaSuscripcionUdp = null;
        this.ultimoContactoMs = Date.now();

        try {
            await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_KEEPALIVE_WIFI,
                Buffer.alloc(0),
                { prioridad: PrioridadRPC.CRITICA }
            );
            this.iniciarPulsoKeepaliveAutomatico();
        } catch (err) {
            await this.desconectar();
            throw err;
        }
    }

    async desconectar(): Promise<void> {
        this.detenerPulsoKeepaliveAutomatico();
        try {
            if (this.medioActivo === "UART-S" && this.uarts.estaConectado()) {
                await this.invocarPorId(
                    ProcedimientoSistemaRPC.L1_CERRAR_UARTS,
                    Buffer.alloc(0),
                    { prioridad: PrioridadRPC.CRITICA }
                );
            } else if (this.medioActivo === "WIFI" && this.wifi.estaConectado()) {
                await this.invocarPorId(
                    ProcedimientoSistemaRPC.L1_CERRAR_WIFI,
                    Buffer.alloc(0),
                    { prioridad: PrioridadRPC.CRITICA }
                );
            }
        } catch {
            // Ignora errores si el medio físico ya se cerró
        }
        await this.uarts.cerrar();
        await this.wifi.cerrar();
        this.limpiarEstadoAlDesconectar();
    }

    async cerrarTransporteLocal(): Promise<void> {
        await this.uarts.cerrar();
        await this.wifi.cerrar();
        this.limpiarEstadoAlDesconectar();
    }

    estaConectado(): boolean {
        return this.medioActivo !== "NINGUNO";
    }

    obtenerMedioActivo(): MedioActivoCliente {
        return this.medioActivo;
    }

    async asegurarCanalUdpStream(): Promise<void> {
        if (this.medioActivo !== "WIFI" || this.udpWiFiVinculado) {
            return;
        }
        if (this.promesaSuscripcionUdp) {
            return this.promesaSuscripcionUdp;
        }

        const puertoUdp = this.wifi.obtenerPuertoUdpLocal();
        this.promesaSuscripcionUdp = this.invocarPorId(
            ProcedimientoSistemaRPC.L1_SUSCRIBIR_UDP_WIFI,
            SuscripcionUdpDTO.serialize({ puertoEscuchaCliente: puertoUdp }),
            { deduplicable: true, prioridad: PrioridadRPC.CRITICA }
        )
            .then(respUdp => {
                CanalStreamingUdpDTO.deserialize(respUdp);
                this.udpWiFiVinculado = true;
            })
            .finally(() => {
                this.promesaSuscripcionUdp = null;
            });

        return this.promesaSuscripcionUdp;
    }

    registrarListenerStream(idProc: number, cb: (payload: Buffer) => void): void {
        this.listenersStreamPorProc.set(idProc, cb);
    }

    invocarPorId(
        idProc: number,
        payloadDto: Buffer = Buffer.alloc(0),
        opcionesODeduplicable: boolean | OpcionesInvocacionRPC = false
    ): Promise<Buffer> {
        if (this.medioActivo === "NINGUNO") {
            return Promise.reject(new Error("ConnectionError: el cliente no está conectado al servidor ESP32"));
        }

        const opts: OpcionesInvocacionRPC =
            typeof opcionesODeduplicable === "boolean"
                ? { deduplicable: opcionesODeduplicable }
                : opcionesODeduplicable;

        const deduplicable = opts.deduplicable ?? false;
        const prioridad = opts.prioridad ?? (
            idProc <= 0x0202
                ? PrioridadRPC.CRITICA
                : deduplicable
                    ? PrioridadRPC.NORMAL
                    : PrioridadRPC.ALTA
        );
        const cacheTtlMs = opts.cacheTtlMs ?? 0;
        const invalidaCache = opts.invalidaCache ?? (!deduplicable && idProc >= 0x0300);

        const claveVuelo = (deduplicable || cacheTtlMs > 0)
            ? `${idProc}:${payloadDto.toString("hex")}`
            : "";

        // 1. Respuesta inmediata desde caché del backend si aún está vigente
        if (cacheTtlMs > 0) {
            const cached = this.cacheRespuestas.get(claveVuelo);
            if (cached && cached.expiraEnMs > Date.now()) {
                return Promise.resolve(Buffer.from(cached.payload));
            }
            if (cached) {
                this.cacheRespuestas.delete(claveVuelo);
            }
        }

        // 2. Deduplicación de peticiones idénticas ya en cola o en vuelo
        if (deduplicable) {
            const existente = this.enVueloPorClave.get(claveVuelo);
            if (existente) {
                return existente;
            }
        }

        // 3. Backpressure local si hay demasiadas consultas NORMAL acumuladas
        if (this.colaTareas.length >= FirmwareRPCClient.CAPACIDAD_MAX_COLA_TAREAS) {
            if (prioridad === PrioridadRPC.NORMAL) {
                return Promise.reject(new Error("BusyError: cola local de consultas saturada"));
            }
            // Si llega una tarea CRITICA/ALTA, descarta la consulta NORMAL más antigua para hacer lugar
            const idxNormal = this.colaTareas.findIndex(t => t.prioridad === PrioridadRPC.NORMAL);
            if (idxNormal >= 0) {
                const [descartada] = this.colaTareas.splice(idxNormal, 1);
                descartada.reject(new Error("BusyError: consulta desplazada por operación prioritaria"));
            }
        }

        const promesa = new Promise<Buffer>((resolve, reject) => {
            const tarea: TareaEncoladaRPC = {
                idProc,
                payloadDto,
                prioridad,
                reintentosBusy: 0,
                resolve,
                reject
            };
            this.insertarOrdenadoPorPrioridad(tarea);
            void this.ejecutarDespachadorCola();
        });

        const promesaFinal = promesa.then(res => {
            if (invalidaCache) {
                this.cacheRespuestas.clear();
            } else if (cacheTtlMs > 0 && claveVuelo) {
                this.cacheRespuestas.set(claveVuelo, {
                    payload: Buffer.from(res),
                    expiraEnMs: Date.now() + cacheTtlMs
                });
            }
            return res;
        });

        if (deduplicable) {
            this.enVueloPorClave.set(claveVuelo, promesaFinal);
            promesaFinal.finally(() => this.enVueloPorClave.delete(claveVuelo)).catch(() => {});
        }

        return promesaFinal;
    }

    private insertarOrdenadoPorPrioridad(tarea: TareaEncoladaRPC): void {
        let pos = this.colaTareas.length;
        for (let i = 0; i < this.colaTareas.length; i++) {
            if (tarea.prioridad < this.colaTareas[i].prioridad) {
                pos = i;
                break;
            }
        }
        this.colaTareas.splice(pos, 0, tarea);
    }

    private async ejecutarDespachadorCola(): Promise<void> {
        if (this.despachadorActivo) {
            return;
        }
        this.despachadorActivo = true;

        try {
            while (this.colaTareas.length > 0 && this.estaConectado()) {
                const tarea = this.colaTareas.shift()!;

                // Pacing suave en WiFi para no disparar el RateLimiter del ESP32
                if (this.medioActivo === "WIFI") {
                    const transcurrido = Date.now() - this.ultimoEnvioMs;
                    if (transcurrido < FirmwareRPCClient.PACING_MIN_WIFI_MS) {
                        await new Promise<void>(r =>
                            setTimeout(r, FirmwareRPCClient.PACING_MIN_WIFI_MS - transcurrido)
                        );
                    }
                }

                if (!this.estaConectado()) {
                    tarea.reject(new Error("ConnectionError: conexión cerrada"));
                    break;
                }

                try {
                    this.ultimoEnvioMs = Date.now();
                    const payload = await this.enviarYEsperarTicket(tarea.idProc, tarea.payloadDto);
                    tarea.resolve(payload);
                } catch (err) {
                    const msg = err instanceof Error ? err.message : String(err);
                    const esBusy = msg.startsWith("BusyError:");

                    if (
                        esBusy &&
                        this.estaConectado() &&
                        tarea.reintentosBusy < FirmwareRPCClient.MAX_REINTENTOS_BUSY
                    ) {
                        const esperaMs = FirmwareRPCClient.BACKOFF_BUSY_MS[tarea.reintentosBusy] ?? 900;
                        tarea.reintentosBusy++;
                        this.registrarLogVisor(
                            `[COLA RPC] BusyError en 0x${tarea.idProc.toString(16)}; reintento ${tarea.reintentosBusy}/${FirmwareRPCClient.MAX_REINTENTOS_BUSY} en ${esperaMs} ms`
                        );
                        await new Promise<void>(r => setTimeout(r, esperaMs));
                        if (this.estaConectado()) {
                            this.insertarOrdenadoPorPrioridad(tarea);
                        } else {
                            tarea.reject(new Error("ConnectionError: conexión cerrada"));
                        }
                    } else {
                        tarea.reject(err instanceof Error ? err : new Error(msg));
                    }
                }
            }
        } finally {
            this.despachadorActivo = false;
            if (this.colaTareas.length > 0 && this.estaConectado()) {
                void this.ejecutarDespachadorCola();
            }
        }
    }

    private enviarYEsperarTicket(idProc: number, payloadDto: Buffer): Promise<Buffer> {
        if (this.medioActivo === "NINGUNO") {
            return Promise.reject(new Error("ConnectionError: el cliente no está conectado al servidor ESP32"));
        }

        const idTicket = this.proximoTicket;
        this.proximoTicket = (this.proximoTicket % 65530) + 1;
        const frame = FirmwareRPCClient.empaquetarPeticion(idTicket, idProc, payloadDto);

        return new Promise<Buffer>((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendientes.delete(idTicket);
                reject(new Error(`TimeoutError: sin respuesta del procedimiento 0x${idProc.toString(16)}`));
            }, FirmwareRPCClient.TIMEOUT_RPC_MS);

            this.pendientes.set(idTicket, { resolve, reject, timer });
            this.ultimoContactoMs = Date.now();

            const enviado = this.medioActivo === "UART-S"
                ? this.uarts.enviarMensaje(frame)
                : this.wifi.enviarMensaje(frame);

            if (!enviado) {
                clearTimeout(timer);
                this.pendientes.delete(idTicket);
                reject(new Error("IOError: no se pudo transmitir el paquete en el medio físico"));
            }
        });
    }

    private procesarFrameRecibido(frameCompleto: Buffer): void {
        const desglose = FirmwareRPCClient.desempaquetarRespuesta(frameCompleto);
        if (!desglose) return;

        this.ultimoContactoMs = Date.now();

        const pendiente = this.pendientes.get(desglose.idTicket);
        if (pendiente) {
            clearTimeout(pendiente.timer);
            this.pendientes.delete(desglose.idTicket);
            if (desglose.esError) {
                pendiente.reject(FirmwareRPCClient.decodificarExcepcionCruda(desglose.idProc, desglose.payload));
            } else {
                pendiente.resolve(desglose.payload);
            }
            return;
        }

        if (!desglose.esError) {
            const listenerStream = this.listenersStreamPorProc.get(desglose.idProc);
            if (listenerStream) {
                listenerStream(desglose.payload);
            }
        }
    }

    private iniciarPulsoKeepaliveAutomatico(): void {
        this.detenerPulsoKeepaliveAutomatico();
        this.timerKeepalive = setInterval(() => {
            if (this.pendientes.size > 0 || this.colaTareas.length > 0) {
                return;
            }
            if (Date.now() - this.ultimoContactoMs < FirmwareRPCClient.UMBRAL_SILENCIO_KEEPALIVE_MS) {
                return;
            }

            const proc = this.medioActivo === "UART-S"
                ? ProcedimientoSistemaRPC.L1_KEEPALIVE_UARTS
                : this.medioActivo === "WIFI"
                    ? ProcedimientoSistemaRPC.L1_KEEPALIVE_WIFI
                    : null;

            if (proc !== null) {
                this.invocarPorId(proc, Buffer.alloc(0), {
                    deduplicable: true,
                    prioridad: PrioridadRPC.CRITICA
                }).catch(err => {
                    this.registrarLogVisor(`[KEEPALIVE FALLÓ] ${err.message}. Cerrando conexión.`);
                    void this.cerrarTransporteLocal();
                });
            }
        }, FirmwareRPCClient.INTERVALO_REVISION_KEEPALIVE_MS);
    }

    private detenerPulsoKeepaliveAutomatico(): void {
        if (this.timerKeepalive) {
            clearInterval(this.timerKeepalive);
            this.timerKeepalive = undefined;
        }
    }

    private limpiarEstadoAlDesconectar(): void {
        this.detenerPulsoKeepaliveAutomatico();
        this.medioActivo = "NINGUNO";
        this.udpWiFiVinculado = false;
        this.promesaSuscripcionUdp = null;
        this.enVueloPorClave.clear();
        this.cacheRespuestas.clear();
        this.listenersStreamPorProc.clear();

        while (this.colaTareas.length > 0) {
            const tarea = this.colaTareas.shift()!;
            tarea.reject(new Error("ConnectionError: conexión cerrada"));
        }

        for (const [, p] of this.pendientes) {
            clearTimeout(p.timer);
            p.reject(new Error("ConnectionError: conexión cerrada"));
        }
        this.pendientes.clear();
    }
}

export const clienteFirmware = new FirmwareRPCClient();

/**
 * Constructor Declarativo de Procedimientos Remotos Cliente (Espejo de llamadoSemantico en C++)
 */
class ConstructorLlamadoCliente {
    constructor(private readonly transporte: FirmwareRPCClient) {}

    alMenosUnaVez<Out = void>(
        nombre: string,
        entradaCodec?: null,
        salidaCodec?: { deserialize(b: Buffer): Out } | null,
        cacheTtlMs?: number
    ): () => Promise<Out>;
    alMenosUnaVez<In extends Record<string, any>, Out = void>(
        nombre: string,
        entradaCodec: StructCodec<In>,
        salidaCodec?: { deserialize(b: Buffer): Out } | null,
        cacheTtlMs?: number
    ): (entrada: In) => Promise<Out>;
    alMenosUnaVez(
        nombre: string,
        entradaCodec: StructCodec<any> | null = null,
        salidaCodec: { deserialize(b: Buffer): any } | null = null,
        cacheTtlMs = 0
    ) {
        const idProc = hashNombreProcedimiento(nombre);
        return async (entrada?: any): Promise<any> => {
            const payload = entradaCodec && entrada !== undefined
                ? entradaCodec.serialize(entrada)
                : Buffer.alloc(0);
            const raw = await this.transporte.invocarPorId(idProc, payload, {
                deduplicable: true,
                prioridad: PrioridadRPC.NORMAL,
                cacheTtlMs
            });
            return salidaCodec ? salidaCodec.deserialize(raw) : undefined;
        };
    }

    exactamenteUnaVez<Out = void>(
        nombre: string,
        entradaCodec?: null,
        salidaCodec?: { deserialize(b: Buffer): Out } | null
    ): () => Promise<Out>;
    exactamenteUnaVez<In extends Record<string, any>, Out = void>(
        nombre: string,
        entradaCodec: StructCodec<In>,
        salidaCodec?: { deserialize(b: Buffer): Out } | null
    ): (entrada: In) => Promise<Out>;
    exactamenteUnaVez(
        nombre: string,
        entradaCodec: StructCodec<any> | null = null,
        salidaCodec: { deserialize(b: Buffer): any } | null = null
    ) {
        const idProc = hashNombreProcedimiento(nombre);
        return async (entrada?: any): Promise<any> => {
            const payload = entradaCodec && entrada !== undefined
                ? entradaCodec.serialize(entrada)
                : Buffer.alloc(0);
            const raw = await this.transporte.invocarPorId(idProc, payload, {
                deduplicable: false,
                prioridad: PrioridadRPC.ALTA,
                invalidaCache: true
            });
            return salidaCodec ? salidaCodec.deserialize(raw) : undefined;
        };
    }

    talVez<Out>(
        nombre: string,
        salidaCodec: { deserialize(b: Buffer): Out }
    ) {
        const idProc = hashNombreProcedimiento(nombre);
        return async (onStreamUpdate?: (dato: Out) => void): Promise<Out> => {
            await this.transporte.asegurarCanalUdpStream();
            if (onStreamUpdate) {
                this.transporte.registrarListenerStream(idProc, raw => {
                    onStreamUpdate(salidaCodec.deserialize(raw));
                });
            }
            const primerRaw = await this.transporte.invocarPorId(idProc, Buffer.alloc(0), {
                deduplicable: true,
                prioridad: PrioridadRPC.ALTA
            });
            const primerDato = salidaCodec.deserialize(primerRaw);
            onStreamUpdate?.(primerDato);
            return primerDato;
        };
    }
}

export const llamadoCliente = new ConstructorLlamadoCliente(clienteFirmware);