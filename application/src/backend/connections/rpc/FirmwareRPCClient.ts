import { UARTSClient } from "../interfaces/UART-S/UARTSClient";
import { WiFiClient } from "../interfaces/WiFi/WiFiClient";
import {
    CodecRPC,
    ConfirmacionSistemaCliente,
    LecturaSensorCliente,
    ProcedimientoRPC,
    ResultadoLoginCliente
} from "./CodecRPC";

export type MedioActivoCliente = "NINGUNO" | "UART-S" | "WIFI";

interface PeticionPendiente {
    resolve: (payload: Buffer) => void;
    reject: (err: Error) => void;
    timer: ReturnType<typeof setTimeout>;
}

export class FirmwareRPCClient {
    private static readonly INTERVALO_KEEPALIVE_MS = 45000; // Envía pulso cada 45s (el servidor expira a los 120s)
    private static readonly TIMEOUT_RPC_MS = 6000;

    private readonly uarts: UARTSClient;
    private readonly wifi: WiFiClient;

    private medioActivo: MedioActivoCliente = "NINGUNO";
    private proximoTicket = 1;
    private pendientes = new Map<number, PeticionPendiente>();
    private timerKeepalive?: ReturnType<typeof setInterval>;
    private listenerStream?: (lecturas: LecturaSensorCliente[]) => void;

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

    // ¿Cómo lo hace?: Abre el puerto serie, sincroniza tramas UART-S, invoca el RPC de handshake (0x0101) y arranca el pulso automático de keepalive.
    async conectarPorSerie(rutaPuerto: string): Promise<boolean> {
        await this.desconectar();
        await this.uarts.conectar(rutaPuerto);
        this.medioActivo = "UART-S";

        const resp = await this.invocarRPC(
            ProcedimientoRPC.L1_HANDSHAKE_UARTS,
            CodecRPC.codificarHandshakeUarts(1)
        );

        const aceptado = resp.length >= 1 && resp.readUInt8(0) !== 0;
        if (aceptado) {
            this.iniciarPulsoKeepaliveAutomatico();
        } else {
            await this.desconectar();
        }
        return aceptado;
    }

    // ¿Cómo lo hace?: Abre el socket TCP hacia el ESP32 (puerto 8443), envía el primer keepalive WiFi (0x0104) y arranca el pulso automático.
    async conectarPorWiFi(ipServidor: string): Promise<boolean> {
        await this.desconectar();
        await this.wifi.conectar(ipServidor);
        this.medioActivo = "WIFI";

        await this.invocarRPC(ProcedimientoRPC.L1_KEEPALIVE_WIFI);
        this.iniciarPulsoKeepaliveAutomatico();
        return true;
    }

    // ¿Cómo lo hace?: Envía el RPC formal de cierre (0x0103 o 0x0106) si estaba conectado y cierra el puerto o los sockets locales.
    async desconectar(): Promise<void> {
        this.detenerPulsoKeepaliveAutomatico();

        try {
            if (this.medioActivo === "UART-S" && this.uarts.estaConectado()) {
                await this.invocarRPC(ProcedimientoRPC.L1_CERRAR_UARTS);
            } else if (this.medioActivo === "WIFI" && this.wifi.estaConectado()) {
                await this.invocarRPC(ProcedimientoRPC.L1_CERRAR_WIFI);
            }
        } catch {
            // Si el cable ya se desconectó físicamente, continuamos con el cierre local
        }

        await this.uarts.cerrar();
        await this.wifi.cerrar();
        this.limpiarEstadoAlDesconectar();
    }

    // ¿Cómo lo hace?: Envía usuario y clave al procedimiento 0x0201 y devuelve si fue autenticado y qué rol le asignó el servidor.
    async iniciarSesion(usuario: string, clavePlana: string): Promise<ResultadoLoginCliente> {
        const payload = await this.invocarRPC(
            ProcedimientoRPC.L2_LOGIN_USUARIO,
            CodecRPC.codificarLogin(usuario, clavePlana)
        );
        return CodecRPC.decodificarResultadoLogin(payload);
    }

    // ¿Cómo lo hace?: Llama al procedimiento 0x0202 (que también cierra la conexión en el servidor) y limpia el cliente local.
    async cerrarSesion(): Promise<void> {
        try {
            await this.invocarRPC(ProcedimientoRPC.L2_LOGOUT_USUARIO);
        } finally {
            await this.uarts.cerrar();
            await this.wifi.cerrar();
            this.limpiarEstadoAlDesconectar();
        }
    }

    // ¿Cómo lo hace?: Si está en WiFi registra primero su puerto UDP local (0x0105) y luego activa la suscripción de tiempo real en L3 (0x0301).
    async suscribirSensoresTiempoReal(onNuevasLecturas?: (lecturas: LecturaSensorCliente[]) => void): Promise<boolean> {
        if (onNuevasLecturas) {
            this.listenerStream = onNuevasLecturas;
        }

        if (this.medioActivo === "WIFI") {
            const puertoUdp = this.wifi.obtenerPuertoUdpLocal();
            await this.invocarRPC(
                ProcedimientoRPC.L1_SUSCRIBIR_UDP_WIFI,
                CodecRPC.codificarSuscripcionUdp(puertoUdp)
            );
        }

        const byteDummy = Buffer.alloc(1, 1);
        const resp = await this.invocarRPC(ProcedimientoRPC.L3_SUSCRIBIR_SENSORES, byteDummy);
        return resp.length >= 1 && resp.readUInt8(0) !== 0;
    }

    // ¿Cómo lo hace?: Solicita al procedimiento 0x0302 las últimas N lecturas de una MAC y las devuelve como arreglo de objetos JS.
    async obtenerCensosPorMac(macHex: string, limiteRegistros = 32): Promise<LecturaSensorCliente[]> {
        const resp = await this.invocarRPC(
            ProcedimientoRPC.L3_OBTENER_CENSOS_MAC,
            CodecRPC.codificarFiltroPorMac(macHex, limiteRegistros)
        );
        return CodecRPC.decodificarHistorialSensores(resp);
    }

    // ¿Cómo lo hace?: Solicita al procedimiento 0x0303 las lecturas de una MAC entre dos fechas Unix y las decodifica.
    async obtenerCensosPorMacYFecha(
        macHex: string,
        timestampDesde: number,
        timestampHasta: number,
        limiteRegistros = 32
    ): Promise<LecturaSensorCliente[]> {
        const resp = await this.invocarRPC(
            ProcedimientoRPC.L3_OBTENER_CENSOS_FECHA,
            CodecRPC.codificarFiltroPorMacYFecha(macHex, timestampDesde, timestampHasta, limiteRegistros)
        );
        return CodecRPC.decodificarHistorialSensores(resp);
    }

    // ¿Cómo lo hace?: Envía las nuevas credenciales WiFi al procedimiento 0x0311 (requiere rol ADMIN en el servidor).
    async actualizarConfiguracionWiFi(
        ssidSta: string,
        claveSta: string,
        ssidAp: string,
        claveAp: string
    ): Promise<ConfirmacionSistemaCliente> {
        const resp = await this.invocarRPC(
            ProcedimientoRPC.L3_ACTUALIZAR_CONFIG_WIFI,
            CodecRPC.codificarConfiguracionWiFi(ssidSta, claveSta, ssidAp, claveAp)
        );
        return CodecRPC.decodificarConfirmacionSistema(resp);
    }

    // ¿Cómo lo hace?: Invoca el procedimiento 0x0312 y devuelve el estado y los bytes de memoria heap libre del ESP32.
    async ejecutarDiagnosticoSistema(): Promise<ConfirmacionSistemaCliente> {
        const resp = await this.invocarRPC(ProcedimientoRPC.L3_DIAGNOSTICO_SISTEMA, Buffer.alloc(1, 1));
        return CodecRPC.decodificarConfirmacionSistema(resp);
    }

    // ¿Cómo lo hace?: Ordena el reinicio remoto del ESP32 mediante el procedimiento 0x0310.
    async reiniciarHardware(): Promise<ConfirmacionSistemaCliente> {
        const resp = await this.invocarRPC(ProcedimientoRPC.L3_REINICIAR_HARDWARE, Buffer.alloc(1, 1));
        return CodecRPC.decodificarConfirmacionSistema(resp);
    }

    estaConectado(): boolean {
        return this.medioActivo !== "NINGUNO";
    }

    obtenerMedioActivo(): MedioActivoCliente {
        return this.medioActivo;
    }

    // ¿Cómo lo hace?: Asigna un idTicket correlativo, envía el paquete por el medio activo y devuelve una Promesa que se resuelve cuando llega ese mismo idTicket.
    private invocarRPC(idProc: ProcedimientoRPC, payloadDto: Buffer = Buffer.alloc(0)): Promise<Buffer> {
        if (this.medioActivo === "NINGUNO") {
            return Promise.reject(new Error("El cliente no está conectado al servidor ESP32."));
        }

        const idTicket = this.proximoTicket;
        this.proximoTicket = (this.proximoTicket % 65530) + 1;

        const frame = CodecRPC.empaquetarPeticion(idTicket, idProc, payloadDto);

        return new Promise<Buffer>((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendientes.delete(idTicket);
                reject(new Error(`Timeout esperando respuesta del procedimiento 0x${idProc.toString(16)}`));
            }, FirmwareRPCClient.TIMEOUT_RPC_MS);

            this.pendientes.set(idTicket, { resolve, reject, timer });

            const enviado = this.medioActivo === "UART-S"
                ? this.uarts.enviarMensaje(frame)
                : this.wifi.enviarMensaje(frame);

            if (!enviado) {
                clearTimeout(timer);
                this.pendientes.delete(idTicket);
                reject(new Error("No se pudo escribir el mensaje en el medio de transporte."));
            }
        });
    }

    // ¿Cómo lo hace?: Desempaqueta cualquier mensaje entrante; si coincide con un ticket pendiente resuelve su Promesa, y si es un lote de streaming (0x0301) notifica al listener.
    private procesarFrameRecibido(frameCompleto: Buffer): void {
        const desglose = CodecRPC.desempaquetarRespuesta(frameCompleto);
        if (!desglose) {
            return;
        }

        if (desglose.idTicket !== 0) {
            const pendiente = this.pendientes.get(desglose.idTicket);
            if (pendiente) {
                clearTimeout(pendiente.timer);
                this.pendientes.delete(desglose.idTicket);
                if (desglose.esError) {
                    pendiente.reject(CodecRPC.decodificarErrorRPC(desglose.idProc, desglose.payload));
                } else {
                    pendiente.resolve(desglose.payload);
                }
                return;
            }
        }

        if (!desglose.esError && desglose.idProc === ProcedimientoRPC.L3_SUSCRIBIR_SENSORES && this.listenerStream) {
            const lecturas = CodecRPC.decodificarHistorialSensores(desglose.payload);
            this.listenerStream(lecturas);
        }
    }

    // ¿Cómo lo hace?: Dispara cada 45 segundos el procedimiento notificarEstoyVivo correspondiente al medio conectado sin intervención del usuario.
    private iniciarPulsoKeepaliveAutomatico(): void {
        this.detenerPulsoKeepaliveAutomatico();
        this.timerKeepalive = setInterval(() => {
            const proc = this.medioActivo === "UART-S"
                ? ProcedimientoRPC.L1_KEEPALIVE_UARTS
                : this.medioActivo === "WIFI"
                    ? ProcedimientoRPC.L1_KEEPALIVE_WIFI
                    : null;

            if (proc !== null) {
                this.invocarRPC(proc).catch(err => {
                    this.registrarLogVisor(`[KEEPALIVE FALLÓ] ${err.message}. Cerrando conexión.`);
                    void this.uarts.cerrar();
                    void this.wifi.cerrar();
                    this.limpiarEstadoAlDesconectar();
                });
            }
        }, FirmwareRPCClient.INTERVALO_KEEPALIVE_MS);
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
        for (const [, p] of this.pendientes) {
            clearTimeout(p.timer);
            p.reject(new Error("Conexión cerrada."));
        }
        this.pendientes.clear();
    }
}