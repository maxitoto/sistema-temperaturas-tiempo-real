import net from "node:net";
import dgram from "node:dgram";

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

    constructor(private readonly eventos: WiFiClientEvents = {}) {}

    private registrarLogFrame(direccion: "TX >" | "RX <", protocolo: "TCP" | "UDP", frame: Buffer): void {
        if (frame.length >= 8) {
            const len = frame.readUInt32BE(0);
            const ticket = frame.readUInt16BE(4);
            const procCrudo = frame.readUInt16BE(6);
            const esErr = (procCrudo & 0x8000) !== 0;
            const proc = `0x${(procCrudo & 0x7FFF).toString(16).padStart(4, "0")}`;
            this.eventos.onDebugLog?.(
                `[WIFI-${protocolo} ${direccion}] PROC:${proc}${esErr ? " (ERR)" : ""} TICKET:${ticket} LEN:${len} | ${frame.toString("hex").toUpperCase()}`
            );
        } else {
            this.eventos.onDebugLog?.(
                `[WIFI-${protocolo} ${direccion} RAW] (${frame.length}B) ${frame.toString("hex").toUpperCase()}`
            );
        }
    }

    async conectar(ipServidor: string, timeoutMs = 5000): Promise<void> {
        await this.cerrar();

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
                this.eventos.onDebugLog?.(`[ESTADO WIFI] -> CONECTADO (${ipServidor}:${WiFiClient.PUERTO_TCP_SERVIDOR})`);
                resolve();
            });

            socket.on("data", (chunk: Buffer) => this.procesarFragmentoTcp(chunk));

            // Si el ESP32 cierra el socket por ocio o keepalive (FIN), cerramos inmediatamente el lado local
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
                    this.eventos.onDisconnect?.();
                }
            });
        });

        await this.abrirReceptorUdpLocal();
    }

    private async abrirReceptorUdpLocal(): Promise<void> {
        return new Promise<void>((resolve, _reject) => {
            const udp = dgram.createSocket("udp4");

            udp.on("message", (msg: Buffer) => {
                if (msg.length >= 8) {
                    this.registrarLogFrame("RX <", "UDP", msg);
                    this.eventos.onMessage?.(msg);
                }
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

        while (this.bufferAcumuladoTcp.length >= 4) {
            const longitudPayload = this.bufferAcumuladoTcp.readUInt32BE(0);
            const longitudTotalFrame = 4 + longitudPayload;

            if (longitudPayload < 4 || longitudTotalFrame > 2048) {
                this.bufferAcumuladoTcp = Buffer.alloc(0);
                break;
            }

            if (this.bufferAcumuladoTcp.length < longitudTotalFrame) {
                break;
            }

            const frameCompleto = Buffer.from(this.bufferAcumuladoTcp.subarray(0, longitudTotalFrame));
            this.bufferAcumuladoTcp = this.bufferAcumuladoTcp.subarray(longitudTotalFrame);

            this.registrarLogFrame("RX <", "TCP", frameCompleto);
            this.eventos.onMessage?.(frameCompleto);
        }
    }

    enviarMensaje(frameConPrefijoLen: Buffer): boolean {
        if (!this.conectado || !this.socketTcp) {
            return false;
        }
        this.registrarLogFrame("TX >", "TCP", frameConPrefijoLen);
        return this.socketTcp.write(frameConPrefijoLen);
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