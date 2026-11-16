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

export class FirmwareRPCClient {
    private static readonly INTERVALO_REVISION_KEEPALIVE_MS = 10000;
    private static readonly UMBRAL_SILENCIO_KEEPALIVE_MS    = 40000;
    private static readonly TIMEOUT_RPC_MS                  = 8000;

    private readonly uarts: UARTSClient;
    private readonly wifi: WiFiClient;

    private medioActivo: MedioActivoCliente = "NINGUNO";
    private proximoTicket = 1;
    private pendientes = new Map<number, PeticionPendiente>();
    private enVueloPorClave = new Map<string, Promise<Buffer>>();
    private colaSeriePromesa: Promise<unknown> = Promise.resolve();
    private ultimoContactoMs = 0;

    private listenersStreamPorProc = new Map<number, (payload: Buffer) => void>();
    private udpWiFiVinculado = false;
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

    async conectarPorSerie(rutaPuerto: string): Promise<PerfilEnlaceUartsCliente> {
        await this.desconectar();
        await this.uarts.conectar(rutaPuerto);
        this.medioActivo = "UART-S";
        this.ultimoContactoMs = Date.now();

        try {
            const resp = await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_HANDSHAKE_UARTS,
                HandshakeUartsDTO.serialize({ versionProtocolo: 1 })
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
        this.ultimoContactoMs = Date.now();

        try {
            await this.invocarPorId(ProcedimientoSistemaRPC.L1_KEEPALIVE_WIFI);
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
                await this.invocarPorId(ProcedimientoSistemaRPC.L1_CERRAR_UARTS);
            } else if (this.medioActivo === "WIFI" && this.wifi.estaConectado()) {
                await this.invocarPorId(ProcedimientoSistemaRPC.L1_CERRAR_WIFI);
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
        if (this.medioActivo === "WIFI" && !this.udpWiFiVinculado) {
            const puertoUdp = this.wifi.obtenerPuertoUdpLocal();
            const respUdp = await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_SUSCRIBIR_UDP_WIFI,
                SuscripcionUdpDTO.serialize({ puertoEscuchaCliente: puertoUdp })
            );
            CanalStreamingUdpDTO.deserialize(respUdp);
            this.udpWiFiVinculado = true;
        }
    }

    registrarListenerStream(idProc: number, cb: (payload: Buffer) => void): void {
        this.listenersStreamPorProc.set(idProc, cb);
    }

    invocarPorId(idProc: number, payloadDto: Buffer = Buffer.alloc(0), deduplicable = false): Promise<Buffer> {
        if (this.medioActivo === "NINGUNO") {
            return Promise.reject(new Error("ConnectionError: el cliente no está conectado al servidor ESP32"));
        }

        const claveVuelo = deduplicable ? `${idProc}:${payloadDto.toString("hex")}` : "";
        if (deduplicable) {
            const existente = this.enVueloPorClave.get(claveVuelo);
            if (existente) {
                return existente;
            }
        }

        const ejecutarTransaccion = () => this.enviarYEsperarTicket(idProc, payloadDto);

        // En UART-S (enlace serie punto a punto) encolamos secuencialmente para no saturar el FIFO del ESP32
        const promesa = (this.medioActivo === "UART-S")
            ? (this.colaSeriePromesa = this.colaSeriePromesa.then(ejecutarTransaccion, ejecutarTransaccion)) as Promise<Buffer>
            : ejecutarTransaccion();

        if (deduplicable) {
            this.enVueloPorClave.set(claveVuelo, promesa);
            promesa.finally(() => this.enVueloPorClave.delete(claveVuelo)).catch(() => {});
        }

        return promesa;
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
            // No enviar Keepalive si ya hay llamadas L3 en curso o si hubo actividad reciente en el canal
            if (this.pendientes.size > 0) {
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
                this.invocarPorId(proc, Buffer.alloc(0), true).catch(err => {
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
        this.enVueloPorClave.clear();
        this.colaSeriePromesa = Promise.resolve();
        this.listenersStreamPorProc.clear();
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
        salidaCodec?: { deserialize(b: Buffer): Out } | null
    ): () => Promise<Out>;
    alMenosUnaVez<In extends Record<string, any>, Out = void>(
        nombre: string,
        entradaCodec: StructCodec<In>,
        salidaCodec?: { deserialize(b: Buffer): Out } | null
    ): (entrada: In) => Promise<Out>;
    alMenosUnaVez(
        nombre: string,
        entradaCodec: StructCodec<any> | null = null,
        salidaCodec: { deserialize(b: Buffer): any } | null = null
    ) {
        const idProc = hashNombreProcedimiento(nombre);
        return async (entrada?: any): Promise<any> => {
            const payload = entradaCodec && entrada !== undefined
                ? entradaCodec.serialize(entrada)
                : Buffer.alloc(0);
            const raw = await this.transporte.invocarPorId(idProc, payload, true);  
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
            const raw = await this.transporte.invocarPorId(idProc, payload);
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
            const primerRaw = await this.transporte.invocarPorId(idProc, Buffer.alloc(0));
            const primerDato = salidaCodec.deserialize(primerRaw);
            onStreamUpdate?.(primerDato);
            return primerDato;
        };
    }
}

export const llamadoCliente = new ConstructorLlamadoCliente(clienteFirmware);