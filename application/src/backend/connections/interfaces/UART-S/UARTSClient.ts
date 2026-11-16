import { SerialPort } from "serialport";
import {
    BanderasControlTrama,
    EstadoConexionUARTS,
    TAMANO_MAXIMO_PAYLOAD_ESTATICO,
    TramaSegmentoUARTS
} from "./protocol/DefinicionTramaUARTS";
import { DeserializadorTramasUARTS } from "./protocol/DeserializadorTramasUARTS";
import { SerializadorTramasUARTS } from "./protocol/SerializadorTramasUARTS";
import { SegmentadorMensajesUARTS } from "./out/SegmentadorMensajesUARTS";
import { GestorRetransmisionUARTS } from "./out/GestorRetransmisionUARTS";
import { ReensambladorMensajesUARTS } from "./in/ReensambladorMensajesUARTS";

export interface UARTSClientEvents {
    onConnectionChange?: (estado: EstadoConexionUARTS) => void;
    onMessage?: (mensaje: Buffer) => void;
    onError?: (error: Error) => void;
    onDebugLog?: (linea: string) => void;
}

export class UARTSClient {
    private static readonly BAUDIOS_SERVIDOR = 115200;
    private static readonly INTERVALO_PROTOCOLO_MS = 10;
    private static readonly TIMEOUT_SINCRONIZACION_MS = 12000;

    // Puerto físico persistente a nivel proceso para evitar que el SO pulse DTR/RTS y reinicie el ESP32
    private static puertoFisicoCompartido: SerialPort | null = null;
    private static rutaFisicaAbierta = "";

    private puerto: SerialPort | null = null;
    private estado = EstadoConexionUARTS.DESCONECTADO;

    private readonly deserializador = new DeserializadorTramasUARTS();
    private readonly reensamblador = new ReensambladorMensajesUARTS();
    private readonly gestorSalida: GestorRetransmisionUARTS;

    private contadorSecuenciaSalida = 0;
    private ultimoSecuenciaConfirmada = 0;

    private temporizadorProtocolo?: ReturnType<typeof setInterval>;
    private temporizadorConexion?: ReturnType<typeof setTimeout>;
    private temporizadorReintentoSync?: ReturnType<typeof setInterval>;
    private resolverSyncPendiente?: () => void;

    private readonly onDataHandler = (datos: Buffer) => {
        const tramas = this.deserializador.procesarBytes(datos);
        for (const trama of tramas) {
            this.registrarLogTrama("RX <", SerializadorTramasUARTS.serializar(trama));
            this.procesarTrama(trama);
        }
    };

    private readonly onErrorHandler = (error: Error) => {
        this.eventos.onError?.(error);
        this.actualizarEstado(EstadoConexionUARTS.ERROR);
    };

    private readonly onCloseHandler = () => {
        UARTSClient.puertoFisicoCompartido = null;
        UARTSClient.rutaFisicaAbierta = "";
        this.puerto = null;
        this.limpiarTemporizadores();
        this.reiniciarEstadoInterno();
        this.actualizarEstado(EstadoConexionUARTS.DESCONECTADO);
    };

    constructor(private readonly eventos: UARTSClientEvents = {}) {
        this.gestorSalida = new GestorRetransmisionUARTS({
            escribir: (datos: Buffer): boolean => {
                if (!this.puerto || !this.puerto.isOpen) {
                    return false;
                }
                this.registrarLogTrama("TX >", datos);
                this.puerto.write(datos, error => {
                    if (error) {
                        this.eventos.onError?.(error);
                    }
                });
                return true;
            },
            onFalloEnlace: () => {
                this.eventos.onDebugLog?.("[UART-S] Enlace perdido (máximo de reintentos sin ACK). Cerrando sesión.");
                void this.cerrar();
            }
        });
    }

    private formatearBanderas(b: number): string {
        const nombres: string[] = [];
        if (b & BanderasControlTrama.SINCRONIZAR) nombres.push("SYN");
        if (b & BanderasControlTrama.ACK) nombres.push("ACK");
        if (b & BanderasControlTrama.NACK) nombres.push("NACK");
        if (b & BanderasControlTrama.DATOS) nombres.push("DATOS");
        if (b & BanderasControlTrama.MAS_FRAGMENTOS) nombres.push("FRAG");
        if (b & BanderasControlTrama.CEDER_TURNO_HD) nombres.push("HD");
        if (b & BanderasControlTrama.CERRAR) nombres.push("CERRAR");
        return nombres.join("|") || "0x00";
    }

    private registrarLogTrama(direccion: "TX >" | "RX <", raw: Buffer): void {
        if (raw.length >= 9 && raw[0] === 0xAA && raw[raw.length - 1] === 0x55) {
            const flags = this.formatearBanderas(raw[1]);
            const seq = raw.readUInt16BE(2);
            const ack = raw.readUInt16BE(4);
            const win = raw[6];
            const len = raw[7];
            const hex = raw.toString("hex").toUpperCase();
            this.eventos.onDebugLog?.(
                `[UART-S ${direccion}] [${flags}] SEQ:${seq} ACK:${ack} WIN:${win} LEN:${len} | ${hex}`
            );
        }
    }

    private static async obtenerOAbrirPuertoFisico(rutaPuerto: string): Promise<SerialPort> {
        if (
            this.puertoFisicoCompartido &&
            this.puertoFisicoCompartido.isOpen &&
            this.rutaFisicaAbierta === rutaPuerto
        ) {
            return this.puertoFisicoCompartido;
        }

        if (this.puertoFisicoCompartido && this.puertoFisicoCompartido.isOpen) {
            await new Promise<void>(resolve => this.puertoFisicoCompartido!.close(() => resolve()));
        }

        const nuevoPuerto = new SerialPort({
            path: rutaPuerto,
            baudRate: UARTSClient.BAUDIOS_SERVIDOR,
            dataBits: 8,
            parity: "even",
            stopBits: 1,
            hupcl: false,
            autoOpen: false,
            highWaterMark: 4096
        });

        await new Promise<void>((resolve, reject) => {
            nuevoPuerto.open(error => (error ? reject(error) : resolve()));
        });

        await new Promise<void>(resolve => {
            nuevoPuerto.set({ dtr: false, rts: false }, () => resolve());
        });

        this.puertoFisicoCompartido = nuevoPuerto;
        this.rutaFisicaAbierta = rutaPuerto;
        return nuevoPuerto;
    }

    static async listarPuertos(): Promise<string[]> {
        const puertos = await SerialPort.list();
        const rutas = puertos.map(p => p.path);

        // Pre-abre en segundo plano el puerto USB del ESP32 para que al pulsar "Conectar" nunca se reinicie
        const candidatoEsp32 = rutas.find(r => /usbserial|SLAB_USBtoUART|wchusbserial|ttyUSB|COM/i.test(r));
        if (candidatoEsp32 && (!this.puertoFisicoCompartido || !this.puertoFisicoCompartido.isOpen)) {
            this.obtenerOAbrirPuertoFisico(candidatoEsp32).catch(() => {});
        }
        return rutas;
    }

    async conectar(rutaPuerto: string): Promise<void> {
        this.limpiarTemporizadores();
        this.desvincularListenersPuerto();
        this.actualizarEstado(EstadoConexionUARTS.CONECTANDO);

        try {
            this.puerto = await UARTSClient.obtenerOAbrirPuertoFisico(rutaPuerto);
            this.vincularListenersPuerto(this.puerto);
            this.iniciarTemporizadorProtocolo();

            await new Promise<void>((resolve, reject) => {
                this.iniciarSincronizacion(resolve, reject);
            });
        } catch (error) {
            await this.cerrar();
            throw error;
        }
    }

    private iniciarSincronizacion(resolve: () => void, reject: (err: Error) => void): void {
        this.reiniciarEstadoInterno();
        this.resolverSyncPendiente = resolve;

        const enviarPulsoSync = () => {
            if (!this.puerto?.isOpen) return;
            const trama = SerializadorTramasUARTS.crearTramaControl(
                BanderasControlTrama.SINCRONIZAR,
                0,
                this.reensamblador.calcularVentanaDisponible(TAMANO_MAXIMO_PAYLOAD_ESTATICO)
            );
            const raw = SerializadorTramasUARTS.serializar(trama);
            this.registrarLogTrama("TX >", raw);
            this.puerto.write(raw);
        };

        enviarPulsoSync();
        this.limpiarTemporizadorConexion();
        this.temporizadorReintentoSync = setInterval(enviarPulsoSync, 250);

        this.temporizadorConexion = setTimeout(() => {
            this.limpiarTemporizadorConexion();
            this.resolverSyncPendiente = undefined;
            this.actualizarEstado(EstadoConexionUARTS.ERROR);
            reject(new Error("El servidor ESP32 no respondió a la sincronización UART-S."));
        }, UARTSClient.TIMEOUT_SINCRONIZACION_MS);
    }

    private limpiarTemporizadorConexion(): void {
        if (this.temporizadorReintentoSync) {
            clearInterval(this.temporizadorReintentoSync);
            this.temporizadorReintentoSync = undefined;
        }
        if (this.temporizadorConexion) {
            clearTimeout(this.temporizadorConexion);
            this.temporizadorConexion = undefined;
        }
    }

    private vincularListenersPuerto(puerto: SerialPort): void {
        puerto.on("data", this.onDataHandler);
        puerto.on("error", this.onErrorHandler);
        puerto.on("close", this.onCloseHandler);
    }

    private desvincularListenersPuerto(): void {
        if (this.puerto) {
            this.puerto.off("data", this.onDataHandler);
            this.puerto.off("error", this.onErrorHandler);
            this.puerto.off("close", this.onCloseHandler);
        }
    }

    private procesarTrama(trama: TramaSegmentoUARTS): void {
        if (trama.banderas & BanderasControlTrama.CERRAR) {
            this.eventos.onDebugLog?.("[UART-S] Trama CERRAR recibida desde el servidor ESP32.");
            void this.cerrar();
            return;
        }

        if ((trama.banderas & BanderasControlTrama.SINCRONIZAR) && (trama.banderas & BanderasControlTrama.ACK)) {
            if (this.estado === EstadoConexionUARTS.CONECTANDO) {
                this.reiniciarEstadoInterno();
                this.gestorSalida.procesarConfirmacion(BanderasControlTrama.ACK, 0, trama.ventanaRecepcion);
                this.limpiarTemporizadorConexion();
                this.actualizarEstado(EstadoConexionUARTS.CONECTADO);
                this.resolverSyncPendiente?.();
                this.resolverSyncPendiente = undefined;
            }
            return;
        }

        if (this.estado !== EstadoConexionUARTS.CONECTADO) {
            return;
        }

        if (trama.banderas & (BanderasControlTrama.ACK | BanderasControlTrama.NACK)) {
            this.gestorSalida.procesarConfirmacion(
                trama.banderas,
                trama.numeroConfirmacion,
                trama.ventanaRecepcion
            );
        }

        if (trama.banderas & BanderasControlTrama.DATOS) {
            const aceptado = this.reensamblador.procesarSegmento(trama);

            if (aceptado) {
                const mensaje = this.reensamblador.hayMensajeCompleto()
                    ? this.reensamblador.extraerMensaje()
                    : null;
                const ventana = this.reensamblador.calcularVentanaDisponible(TAMANO_MAXIMO_PAYLOAD_ESTATICO);

                this.ultimoSecuenciaConfirmada = trama.numeroSecuencia;
                this.gestorSalida.enviarControl(BanderasControlTrama.ACK, trama.numeroSecuencia, ventana);

                if (mensaje) {
                    this.eventos.onMessage?.(mensaje);
                }
            } else {
                const ventana = this.reensamblador.calcularVentanaDisponible(TAMANO_MAXIMO_PAYLOAD_ESTATICO);
                this.gestorSalida.enviarControl(
                    BanderasControlTrama.NACK,
                    this.reensamblador.obtenerSecuenciaEsperada(),
                    ventana
                );
            }
        }
    }

    enviarMensaje(datos: Buffer): boolean {
        if (this.estado !== EstadoConexionUARTS.CONECTADO || datos.length === 0) {
            return false;
        }

        const segmentos = SegmentadorMensajesUARTS.dividirMensaje(
            datos,
            this.contadorSecuenciaSalida,
            this.ultimoSecuenciaConfirmada,
            this.reensamblador.calcularVentanaDisponible(TAMANO_MAXIMO_PAYLOAD_ESTATICO),
            TAMANO_MAXIMO_PAYLOAD_ESTATICO
        );

        if (segmentos.length === 0 || segmentos.length > 32) {
            return false;
        }

        for (const seg of segmentos) {
            if (!this.gestorSalida.encolar(seg)) {
                return false;
            }
        }

        this.contadorSecuenciaSalida = (this.contadorSecuenciaSalida + segmentos.length) & 0xFFFF;
        this.gestorSalida.ejecutarCiclo();
        return true;
    }

    // Cierra la sesión lógica UART-S SIN cerrar el descriptor físico del SO para no resetear el ESP32
    async cerrar(): Promise<void> {
        this.limpiarTemporizadores();
        this.desvincularListenersPuerto();
        this.puerto = null;
        this.reiniciarEstadoInterno();
        this.actualizarEstado(EstadoConexionUARTS.DESCONECTADO);
    }

    estaConectado(): boolean {
        return this.estado === EstadoConexionUARTS.CONECTADO;
    }

    obtenerEstado(): EstadoConexionUARTS {
        return this.estado;
    }

    obtenerRutaPuerto(): string {
        return this.puerto?.path ?? UARTSClient.rutaFisicaAbierta;
    }

    private iniciarTemporizadorProtocolo(): void {
        this.limpiarTemporizadorProtocolo();
        this.temporizadorProtocolo = setInterval(() => {
            if (this.estado === EstadoConexionUARTS.CONECTADO) {
                this.gestorSalida.ejecutarCiclo();
            }
        }, UARTSClient.INTERVALO_PROTOCOLO_MS);
    }

    private actualizarEstado(nuevo: EstadoConexionUARTS): void {
        if (this.estado !== nuevo) {
            this.estado = nuevo;
            this.eventos.onConnectionChange?.(nuevo);
        }
    }

    private limpiarTemporizadorProtocolo(): void {
        if (this.temporizadorProtocolo) {
            clearInterval(this.temporizadorProtocolo);
            this.temporizadorProtocolo = undefined;
        }
    }

    private limpiarTemporizadores(): void {
        this.limpiarTemporizadorConexion();
        this.limpiarTemporizadorProtocolo();
    }

    private reiniciarEstadoInterno(): void {
        this.contadorSecuenciaSalida = 0;
        this.ultimoSecuenciaConfirmada = 0;
        this.deserializador.reiniciar();
        this.reensamblador.reiniciar();
        this.gestorSalida.reiniciar();
    }
}