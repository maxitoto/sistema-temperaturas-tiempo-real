import net from "node:net";
import dgram from "node:dgram";
import {
    CanalDireccionCcm,
    EstadoSesionCcmCliente,
    OVERHEAD_CCM,
    WIFI_CIFRADO_CCM_ACTIVO
} from "./WiFiCcmCodec";

export interface WiFiClientEvents {
    onMessage?: (mensajeCompletoConCabeceraLen: Buffer) => void;
    onDisconnect?: () => void;
    onError?: (error: Error) => void;
    onDebugLog?: (linea: string) => void;
}

export class WiFiClient {
    public static readonly PUERTO_TCP_SERVIDOR = 8443;
    public static readonly PUERTO_UDP_SERVIDOR = 8266;

    private socketTcp: net.Socket | null = null;
    private socketUdp: dgram.Socket | null = null;
    private bufferAcumuladoTcp: Buffer = Buffer.alloc(0);
    private conectado = false;
    private puertoUdpLocal = 0;

    private readonly ccm = new EstadoSesionCcmCliente();

    constructor(private readonly eventos: WiFiClientEvents = {}) {}

    private registrarLogFrame(
        direccion: "TX >" | "RX <",
        protocolo: "TCP" | "UDP",
        framePlano: Buffer,
        extra = ""
    ): void {
        if (framePlano.length >= 8) {
            const len = framePlano.readUInt32BE(0);
            const ticket = framePlano.readUInt16BE(4);
            const procCrudo = framePlano.readUInt16BE(6);
            const esErr = (procCrudo & 0x8000) !== 0;
            const proc = `0x${(procCrudo & 0x7FFF).toString(16).padStart(4, "0")}`;
            this.eventos.onDebugLog?.(
                `[WIFI-${protocolo} ${direccion}] PROC:${proc}${esErr ? " (ERR)" : ""} TICKET:${ticket} LEN:${len}${extra}`
            );
        } else {
            this.eventos.onDebugLog?.(
                `[WIFI-${protocolo} ${direccion} RAW] (${framePlano.length}B)${extra}`
            );
        }
    }

    async conectar(ipServidor: string, timeoutMs = 5000): Promise<void> {
        await this.cerrar();
        this.ccm.reiniciar();

        await new Promise<void>((resolve, reject) => {
            const socket = new net.Socket();
            socket.setNoDelay(true);
            socket.setKeepAlive(true, 10000);

            const timer = setTimeout(() => {
                socket.removeAllListeners();
                socket.destroy();
                reject(new Error(`Tiempo agotado conectando a ${ipServidor}:${WiFiClient.PUERTO_TCP_SERVIDOR}`));
            }, timeoutMs);

            socket.connect(WiFiClient.PUERTO_TCP_SERVIDOR, ipServidor, () => {
                clearTimeout(timer);
                this.socketTcp = socket;
                this.conectado = true;
                this.eventos.onDebugLog?.(
                    `[ESTADO WIFI] -> CONECTADO (${ipServidor}:${WiFiClient.PUERTO_TCP_SERVIDOR}) · CCM:${WIFI_CIFRADO_CCM_ACTIVO ? "ON" : "OFF"}`
                );
                resolve();
            });

            socket.on("data", (chunk: Buffer) => this.procesarFragmentoTcp(chunk));

            socket.on("end", () => {
                this.eventos.onDebugLog?.("[WIFI-TCP] FIN recibido desde el servidor ESP32.");
                socket.destroy();
            });

            socket.on("error", (err: Error) => {
                clearTimeout(timer);
                if (this.socketTcp === socket) {
                    this.eventos.onError?.(err);
                } else {
                    reject(err);
                }
            });

            socket.on("close", () => {
                if (this.socketTcp === socket) {
                    this.socketTcp = null;
                    this.conectado = false;
                    this.bufferAcumuladoTcp = Buffer.alloc(0);
                    this.ccm.reiniciar();
                    this.eventos.onDisconnect?.();
                }
            });
        });

        await this.abrirReceptorUdpLocal();
    }

    private async abrirReceptorUdpLocal(): Promise<void> {
        return new Promise<void>(resolve => {
            const udp = dgram.createSocket("udp4");

            udp.on("message", (msg: Buffer) => {
                if (msg.length < 8) return;

                if (!WIFI_CIFRADO_CCM_ACTIVO) {
                    this.registrarLogFrame("RX <", "UDP", msg);
                    this.eventos.onMessage?.(msg);
                    return;
                }

                const longitudPayload = msg.readUInt32BE(0);
                if (longitudPayload < 4 + OVERHEAD_CCM || msg.length < 4 + longitudPayload) {
                    return;
                }

                const res = this.ccm.descifrarFrameEntrante(
                    CanalDireccionCcm.DIR_S2C_UDP,
                    msg.subarray(4, 4 + longitudPayload)
                );
                if (!res) {
                    this.eventos.onDebugLog?.("[WIFI-UDP RX <] Datagrama descartado por fallo de autenticación CCM / replay.");
                    return;
                }

                this.registrarLogFrame("RX <", "UDP", res.framePlanoConLen, ` [CCM SEQ:${res.seq}]`);
                this.eventos.onMessage?.(res.framePlanoConLen);
            });

            udp.on("error", err => this.eventos.onError?.(err));

            udp.bind(0, () => {
                this.puertoUdpLocal = udp.address().port;
                this.socketUdp = udp;
                resolve();
            });
        });
    }

    private procesarFragmentoTcp(chunk: Buffer): void {
        this.bufferAcumuladoTcp = Buffer.concat([this.bufferAcumuladoTcp, chunk]);

        const minPayload = WIFI_CIFRADO_CCM_ACTIVO ? 4 + OVERHEAD_CCM : 4;

        while (this.bufferAcumuladoTcp.length >= 4) {
            const longitudPayload = this.bufferAcumuladoTcp.readUInt32BE(0);
            const longitudTotalFrame = 4 + longitudPayload;

            if (longitudPayload < minPayload || longitudTotalFrame > 2048) {
                this.eventos.onDebugLog?.(`[WIFI-TCP RX <] Framing inválido (LEN:${longitudPayload}). Cerrando socket.`);
                this.bufferAcumuladoTcp = Buffer.alloc(0);
                this.socketTcp?.destroy();
                break;
            }

            if (this.bufferAcumuladoTcp.length < longitudTotalFrame) {
                break;
            }

            const frameCompleto = Buffer.from(this.bufferAcumuladoTcp.subarray(0, longitudTotalFrame));
            this.bufferAcumuladoTcp = this.bufferAcumuladoTcp.subarray(longitudTotalFrame);

            if (!WIFI_CIFRADO_CCM_ACTIVO) {
                this.registrarLogFrame("RX <", "TCP", frameCompleto);
                this.eventos.onMessage?.(frameCompleto);
                continue;
            }

            const res = this.ccm.descifrarFrameEntrante(
                CanalDireccionCcm.DIR_S2C_TCP,
                frameCompleto.subarray(4)
            );
            if (!res) {
                this.eventos.onDebugLog?.("[WIFI-TCP RX <] Error de autenticación AES-CCM o replay detectado. Cerrando socket.");
                this.socketTcp?.destroy();
                break;
            }

            this.registrarLogFrame("RX <", "TCP", res.framePlanoConLen, ` [CCM SEQ:${res.seq}]`);
            this.eventos.onMessage?.(res.framePlanoConLen);
        }
    }

    enviarMensaje(frameConPrefijoLen: Buffer): boolean {
        if (!this.conectado || !this.socketTcp) {
            return false;
        }

        if (!WIFI_CIFRADO_CCM_ACTIVO) {
            this.registrarLogFrame("TX >", "TCP", frameConPrefijoLen);
            return this.socketTcp.write(frameConPrefijoLen);
        }

        const cifrado = this.ccm.cifrarFrameTcp(frameConPrefijoLen);
        if (!cifrado) {
            this.eventos.onDebugLog?.("[WIFI-TCP TX >] No se pudo cifrar el paquete CCM (secuencia agotada).");
            return false;
        }

        this.registrarLogFrame("TX >", "TCP", frameConPrefijoLen, ` [CCM SEQ:${cifrado.seq}]`);
        return this.socketTcp.write(cifrado.frameCifrado);
    }

    obtenerPuertoUdpLocal(): number {
        return this.puertoUdpLocal;
    }

    estaConectado(): boolean {
        return this.conectado;
    }

    async cerrar(): Promise<void> {
        this.conectado = false;
        this.bufferAcumuladoTcp = Buffer.alloc(0);
        this.ccm.reiniciar();

        if (this.socketUdp) {
            const udp = this.socketUdp;
            this.socketUdp = null;
            this.puertoUdpLocal = 0;
            udp.removeAllListeners();
            await new Promise<void>(resolve => udp.close(() => resolve()));
        }

        if (this.socketTcp) {
            const tcp = this.socketTcp;
            this.socketTcp = null;
            tcp.removeAllListeners();
            tcp.destroy();
            await new Promise<void>(resolve => setTimeout(resolve, 60));
        }
    }
}