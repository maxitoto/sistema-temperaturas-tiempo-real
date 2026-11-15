import { UARTSClient } from "../interfaces/UART-S/UARTSClient";
import { WiFiClient } from "../interfaces/WiFi/WiFiClient";
import {
    CodecRPC,
    ConfiguracionWiFiCliente,
    DiagnosticoSistemaCliente,
    EstadisticasUsuariosCliente,
    hashNombreProcedimiento,
    LecturaSensorCliente,
    PerfilEnlaceUartsCliente,
    PerfilUsuarioCliente,
    ProcedimientoSistemaRPC,
    ResumenUsuarioCliente,
    SesionIniciadaCliente
} from "./CodecRPC";

export type MedioActivoCliente = "NINGUNO" | "UART-S" | "WIFI";

interface PeticionPendiente {
    resolve: (payload: Buffer) => void;
    reject: (err: Error) => void;
    timer: ReturnType<typeof setTimeout>;
}

export class FirmwareRPCClient {
    private static readonly INTERVALO_KEEPALIVE_MS = 45000;
    private static readonly TIMEOUT_RPC_MS = 6000;

    private readonly uarts: UARTSClient;
    private readonly wifi: WiFiClient;

    private medioActivo: MedioActivoCliente = "NINGUNO";
    private proximoTicket = 1;
    private pendientes = new Map<number, PeticionPendiente>();
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

    // --- L1: Conexión Física ---

    async conectarPorSerie(rutaPuerto: string): Promise<PerfilEnlaceUartsCliente> {
        await this.desconectar();
        await this.uarts.conectar(rutaPuerto);
        this.medioActivo = "UART-S";

        try {
            const resp = await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_HANDSHAKE_UARTS,
                CodecRPC.codificarHandshakeUarts(1)
            );
            const perfil = CodecRPC.decodificarPerfilEnlaceUarts(resp);
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
            // Si el enlace físico ya cayó, continuamos con el cierre local
        }

        await this.uarts.cerrar();
        await this.wifi.cerrar();
        this.limpiarEstadoAlDesconectar();
    }

    // --- L2: Autenticación (RPAuth lanza excepción si falla el login o si está bloqueado) ---

    async iniciarSesion(usuario: string, clavePlana: string): Promise<SesionIniciadaCliente> {
        const payload = await this.invocarPorId(
            ProcedimientoSistemaRPC.L2_LOGIN_USUARIO,
            CodecRPC.codificarLogin(usuario, clavePlana)
        );
        return CodecRPC.decodificarSesionIniciada(payload);
    }

    async cerrarSesion(): Promise<void> {
        try {
            await this.invocarPorId(ProcedimientoSistemaRPC.L2_LOGOUT_USUARIO);
        } finally {
            await this.uarts.cerrar();
            await this.wifi.cerrar();
            this.limpiarEstadoAlDesconectar();
        }
    }

    // --- L3: Invocador Semántico Genérico por Nombre (Igual que llamadoSemantico en C++) ---

    async llamarProcedimiento<T>(
        nombreProcedimiento: string,
        payloadEntrada: Buffer,
        decodificadorSalida: (raw: Buffer) => T
    ): Promise<T> {
        const idProc = hashNombreProcedimiento(nombreProcedimiento);
        const raw = await this.invocarPorId(idProc, payloadEntrada);
        return decodificadorSalida(raw);
    }

    async llamarProcedimientoVoid(
        nombreProcedimiento: string,
        payloadEntrada: Buffer = Buffer.alloc(0)
    ): Promise<void> {
        const idProc = hashNombreProcedimiento(nombreProcedimiento);
        await this.invocarPorId(idProc, payloadEntrada);
    }

    // Suscribe a cualquier procedimiento registrado con llamadoSemantico.talVez en el ESP32
    async suscribirStreamTalVez<T>(
        nombreProcedimiento: string,
        decodificadorSalida: (raw: Buffer) => T,
        onMensajeStream?: (dato: T) => void
    ): Promise<T> {
        if (this.medioActivo === "WIFI" && !this.udpWiFiVinculado) {
            const puertoUdp = this.wifi.obtenerPuertoUdpLocal();
            const respUdp = await this.invocarPorId(
                ProcedimientoSistemaRPC.L1_SUSCRIBIR_UDP_WIFI,
                CodecRPC.codificarSuscripcionUdp(puertoUdp)
            );
            CodecRPC.decodificarCanalStreamingUdp(respUdp);
            this.udpWiFiVinculado = true;
        }

        const idProc = hashNombreProcedimiento(nombreProcedimiento);
        if (onMensajeStream) {
            this.listenersStreamPorProc.set(idProc, (payload: Buffer) => {
                onMensajeStream(decodificadorSalida(payload));
            });
        }

        const primerPayload = await this.invocarPorId(idProc, Buffer.alloc(0));
        const primerDato = decodificadorSalida(primerPayload);
        onMensajeStream?.(primerDato);
        return primerDato;
    }

    // --- Procedimientos de Dominio (RPSensor y RPSistema) ---

    async suscribirSensoresTiempoReal(
        onNuevasLecturas?: (lecturas: LecturaSensorCliente[]) => void
    ): Promise<LecturaSensorCliente[]> {
        return this.suscribirStreamTalVez(
            "suscribirSensoresTiempoReal",
             payload => CodecRPC.decodificarHistorialSensores(payload),
            onNuevasLecturas
        );
    }

    async obtenerCensosPorMac(macHex: string, limiteRegistros = 32): Promise<LecturaSensorCliente[]> {
        return this.llamarProcedimiento(
            "obtenerCensosPorMac",
            CodecRPC.codificarFiltroPorMac(macHex, limiteRegistros),
            payload => CodecRPC.decodificarHistorialSensores(payload)
        );
    }

    async obtenerCensosPorMacYFecha(
        macHex: string,
        timestampDesde: number,
        timestampHasta: number,
        limiteRegistros = 32
    ): Promise<LecturaSensorCliente[]> {
        return this.llamarProcedimiento(
            "obtenerCensosPorMacYFecha",
            CodecRPC.codificarFiltroPorMacYFecha(macHex, timestampDesde, timestampHasta, limiteRegistros),
            payload => CodecRPC.decodificarHistorialSensores(payload)
        );
    }

    async actualizarConfiguracionWiFi(
        ssidSta: string,
        claveSta: string,
        ssidAp: string,
        claveAp: string
    ): Promise<ConfiguracionWiFiCliente> {
        return this.llamarProcedimiento(
            "actualizarConfiguracionWiFi",
            CodecRPC.codificarConfiguracionWiFi(ssidSta, claveSta, ssidAp, claveAp),
            payload => CodecRPC.decodificarConfiguracionWiFi(payload)
        );
    }

    async obtenerConfiguracionWiFi(): Promise<ConfiguracionWiFiCliente> {
        return this.llamarProcedimiento(
            "obtenerConfiguracionWiFi",
            Buffer.alloc(0),
            payload => CodecRPC.decodificarConfiguracionWiFi(payload)
        );
    }

    async ejecutarDiagnosticoSistema(): Promise<DiagnosticoSistemaCliente> {
        return this.llamarProcedimiento(
            "ejecutarDiagnosticoSistema",
            Buffer.alloc(0),
            payload => CodecRPC.decodificarDiagnosticoSistema(payload)
        );
    }

    async reiniciarHardware(): Promise<void> {
        await this.llamarProcedimientoVoid("reiniciarHardware");
    }

    // --- Procedimientos de Dominio Usuarios (RPUsuario) ---

    async obtenerUsuarioPorNombre(usuario: string): Promise<PerfilUsuarioCliente> {
        return this.llamarProcedimiento(
            "obtenerUsuarioPorNombre",
            CodecRPC.codificarConsultaUsuarioNombre(usuario),
            payload => CodecRPC.decodificarPerfilUsuario(payload)
        );
    }

    async listarUsuarios(
        limiteRegistros = 32,
        offsetRegistros = 0,
        soloActivos = false
    ): Promise<ResumenUsuarioCliente[]> {
        return this.llamarProcedimiento(
            "listarUsuarios",
            CodecRPC.codificarFiltroListadoUsuarios(limiteRegistros, offsetRegistros, soloActivos),
            payload => CodecRPC.decodificarListaUsuarios(payload)
        );
    }

    async crearUsuario(
        usuario: string,
        clavePlana: string,
        rol = 1
    ): Promise<PerfilUsuarioCliente> {
        return this.llamarProcedimiento(
            "crearUsuario",
            CodecRPC.codificarCrearUsuario(usuario, clavePlana, rol),
            payload => CodecRPC.decodificarPerfilUsuario(payload)
        );
    }

    async actualizarRolYEstadoUsuario(
        usuario: string,
        nuevoRol: number,
        activa: boolean
    ): Promise<PerfilUsuarioCliente> {
        return this.llamarProcedimiento(
            "actualizarRolYEstadoUsuario",
            CodecRPC.codificarActualizarUsuario(usuario, nuevoRol, activa),
            payload => CodecRPC.decodificarPerfilUsuario(payload)
        );
    }

    async cambiarContrasenaUsuario(
        usuario: string,
        claveActual: string,
        claveNueva: string
    ): Promise<PerfilUsuarioCliente> {
        return this.llamarProcedimiento(
            "cambiarContrasenaUsuario",
            CodecRPC.codificarCambiarContrasena(usuario, claveActual, claveNueva),
            payload => CodecRPC.decodificarPerfilUsuario(payload)
        );
    }

    async desbloquearUsuario(usuario: string): Promise<PerfilUsuarioCliente> {
        return this.llamarProcedimiento(
            "desbloquearUsuario",
            CodecRPC.codificarConsultaUsuarioNombre(usuario),
            payload => CodecRPC.decodificarPerfilUsuario(payload)
        );
    }

    async eliminarUsuario(usuario: string): Promise<void> {
        await this.llamarProcedimientoVoid(
            "eliminarUsuario",
            CodecRPC.codificarConsultaUsuarioNombre(usuario)
        );
    }

    async obtenerEstadisticasUsuarios(): Promise<EstadisticasUsuariosCliente> {
        return this.llamarProcedimiento(
            "obtenerEstadisticasUsuarios",
            Buffer.alloc(0),
            payload => CodecRPC.decodificarEstadisticasUsuarios(payload)
        );
    }

    estaConectado(): boolean {
        return this.medioActivo !== "NINGUNO";
    }

    obtenerMedioActivo(): MedioActivoCliente {
        return this.medioActivo;
    }

    private invocarPorId(idProc: number, payloadDto: Buffer = Buffer.alloc(0)): Promise<Buffer> {
        if (this.medioActivo === "NINGUNO") {
            return Promise.reject(new Error("ConnectionError: el cliente no está conectado al servidor ESP32"));
        }

        const idTicket = this.proximoTicket;
        this.proximoTicket = (this.proximoTicket % 65530) + 1;

        const frame = CodecRPC.empaquetarPeticion(idTicket, idProc, payloadDto);

        return new Promise<Buffer>((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendientes.delete(idTicket);
                reject(new Error(`TimeoutError: sin respuesta del procedimiento 0x${idProc.toString(16)}`));
            }, FirmwareRPCClient.TIMEOUT_RPC_MS);

            this.pendientes.set(idTicket, { resolve, reject, timer });

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
        const desglose = CodecRPC.desempaquetarRespuesta(frameCompleto);
        if (!desglose) {
            return;
        }

        // 1. Si coincide con un ticket de llamada RPC en espera, resuelve el dato o lanza la excepción cruda
        const pendiente = this.pendientes.get(desglose.idTicket);
        if (pendiente) {
            clearTimeout(pendiente.timer);
            this.pendientes.delete(desglose.idTicket);
            if (desglose.esError) {
                pendiente.reject(CodecRPC.decodificarExcepcionCruda(desglose.idProc, desglose.payload));
            } else {
                pendiente.resolve(desglose.payload);
            }
            return;
        }

        // 2. Si es una emisión periódica de cualquier procedimiento con semántica talVez, notifica a su listener
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
            const proc = this.medioActivo === "UART-S"
                ? ProcedimientoSistemaRPC.L1_KEEPALIVE_UARTS
                : this.medioActivo === "WIFI"
                    ? ProcedimientoSistemaRPC.L1_KEEPALIVE_WIFI
                    : null;

            if (proc !== null) {
                this.invocarPorId(proc).catch(err => {
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
        this.udpWiFiVinculado = false;
        this.listenersStreamPorProc.clear();
        for (const [, p] of this.pendientes) {
            clearTimeout(p.timer);
            p.reject(new Error("ConnectionError: conexión cerrada"));
        }
        this.pendientes.clear();
    }

}