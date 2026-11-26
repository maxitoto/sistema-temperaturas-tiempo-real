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

export interface DestinoWiFiParseado {
    host: string;
    puerto: number;
    esRemoto: boolean;
    etiqueta: string;
}

function esHostLocalOLan(host: string): boolean {
    const h = host.trim().toLowerCase();
    if (!h) return true;
    if (h === "localhost" || h === "::1" || h.endsWith(".local")) return true;

    const m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(h);
    if (!m) {
        // Dominio público (ej. ngrok, pinggy, duckdns, dominio propio)
        return false;
    }

    const a = Number(m[1]);
    const b = Number(m[2]);
    if (a === 10 || a === 127) return true;
    if (a === 192 && b === 168) return true;
    if (a === 172 && b >= 16 && b <= 31) return true;
    if (a === 169 && b === 254) return true;
    return false;
}

export function parsearDestinoWiFi(
    destinoCrudo: string,
    puertoDefecto = WiFiClient.PUERTO_TCP_SERVIDOR
): DestinoWiFiParseado {
    let limpio = String(destinoCrudo ?? "").trim();
    // Elimina esquemas si el usuario copia desde ngrok/pinggy/navegador (ej. tcp://host:12345)
    limpio = limpio.replace(/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//, "");
    limpio = limpio.split("/")[0]?.trim() ?? "";

    if (!limpio) {
        throw new Error("ValueError: ingrese una dirección IP o dominio válido");
    }

    let host = limpio;
    let puerto = puertoDefecto;

    // Formato IPv6 con puerto: [::1]:8443
    if (limpio.startsWith("[")) {
        const cierre = limpio.indexOf("]");
        if (cierre < 0) {
            throw new Error("ValueError: dirección IPv6 mal formada");
        }
        host = limpio.slice(1, cierre).trim();
        const resto = limpio.slice(cierre + 1).trim();
        if (resto.startsWith(":")) {
            puerto = Number(resto.slice(1));
        }
    } else {
        const primerDosPuntos = limpio.indexOf(":");
        const ultimoDosPuntos = limpio.lastIndexOf(":");
        if (primerDosPuntos > 0 && primerDosPuntos === ultimoDosPuntos) {
            host = limpio.slice(0, primerDosPuntos).trim();
            puerto = Number(limpio.slice(primerDosPuntos + 1).trim());
        }
    }

    if (!host || /\s/.test(host)) {
        throw new Error(`ValueError: host inválido "${destinoCrudo}"`);
    }
    if (!Number.isInteger(puerto) || puerto < 1 || puerto > 65535) {
        throw new Error(`ValueError: puerto TCP inválido (${puerto}). Use un valor entre 1 y 65535`);
    }

    const esRemoto = !esHostLocalOLan(host);
    const etiqueta = puerto === puertoDefecto ? host : `${host}:${puerto}`;
    return { host, puerto, esRemoto, etiqueta };
}

export class WiFiClient {
    public static readonly PUERTO_TCP_SERVIDOR = 8443;
    public static readonly PUERTO_UDP_SERVIDOR = 8266;

    private socketTcp: net.Socket | null = null;
    private socketUdp: dgram.Socket | null = null;
    private bufferAcumuladoTcp: Buffer = Buffer.alloc(0);
    private conectado = false;
    private puertoUdpLocal = 0;

    private destinoActual: DestinoWiFiParseado | null = null;
    private ultimoPaqueteUdpMs = 0;

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

    async conectar(destinoServidor: string, timeoutMs?: number): Promise<void> {
        const destino = parsearDestinoWiFi(destinoServidor);
        const timeoutEfectivo = timeoutMs ?? (destino.esRemoto ? 10000 : 5000);

        await this.cerrar();
        this.ccm.reiniciar();
        this.destinoActual = destino;
        this.ultimoPaqueteUdpMs = 0;

        await new Promise<void>((resolve, reject) => {
            const socket = new net.Socket();
            socket.setNoDelay(true);
            socket.setKeepAlive(true, 10000);

            const timer = setTimeout(() => {
                socket.removeAllListeners();
                socket.destroy();
                reject(
                    new Error(
                        `Tiempo agotado conectando a ${destino.host}:${destino.puerto}`
                    )
                );
            }, timeoutEfectivo);

            socket.connect({ host: destino.host, port: destino.puerto }, () => {
                clearTimeout(timer);
                this.socketTcp = socket;
                this.conectado = true;
                const ipResuelta = socket.remoteAddress ? ` [${socket.remoteAddress}]` : "";
                this.eventos.onDebugLog?.(
                    `[ESTADO WIFI] -> CONECTADO (${destino.host}:${destino.puerto}${ipResuelta} · ${
                        destino.esRemoto ? "INTERNET" : "LAN"
                    }) · CCM:${WIFI_CIFRADO_CCM_ACTIVO ? "ON" : "OFF"}`
                );
                resolve();
            });

            socket.on("data", (chunk: Buffer) => this.procesarFragmentoTcp(chunk));

            socket.on("end", () => {
                this.eventos.onDebugLog?.("[WIFI-TCP] FIN recibido desde el servidor ESP32.");
                socket.destroy();
            });

            socket.on("error", (err: NodeJS.ErrnoException) => {
                clearTimeout(timer);
                if (this.socketTcp === socket) {
                    this.eventos.onError?.(err);
                } else {
                    if (err.code === "ENOTFOUND") {
                        reject(new Error(`DNSError: no se pudo resolver el dominio "${destino.host}"`));
                    } else if (err.code === "ECONNREFUSED") {
                        reject(
                            new Error(
                                `ConnectionRefused: ${destino.host}:${destino.puerto} rechazó la conexión (verifique ESP32 o túnel)`
                            )
                        );
                    } else if (err.code === "ETIMEDOUT" || err.code === "EHOSTUNREACH") {
                        reject(
                            new Error(
                                `NetworkError: sin ruta hacia ${destino.host}:${destino.puerto}`
                            )
                        );
                    } else {
                        reject(err);
                    }
                }
            });

            socket.on("close", () => {
                if (this.socketTcp === socket) {
                    this.socketTcp = null;
                    this.conectado = false;
                    this.bufferAcumuladoTcp = Buffer.alloc(0);
                    this.ultimoPaqueteUdpMs = 0;
                    this.ccm.reiniciar();
                    this.eventos.onDisconnect?.();
                }
            });
        });

        // En red local/SoftAP abrimos receptor UDP para telemetría en vivo; en Internet usa fallback TCP cifrado
        if (!destino.esRemoto) {
            await this.abrirReceptorUdpLocal();
        }
    }

    private async abrirReceptorUdpLocal(): Promise<void> {
        return new Promise<void>(resolve => {
            const udp = dgram.createSocket("udp4");

            udp.on("message", (msg: Buffer) => {
                if (msg.length < 8) return;

                if (!WIFI_CIFRADO_CCM_ACTIVO) {
                    this.ultimoPaqueteUdpMs = Date.now();
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

                this.ultimoPaqueteUdpMs = Date.now();
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

    obtenerUltimoPaqueteUdpMs(): number {
        return this.ultimoPaqueteUdpMs;
    }

    esRemoto(): boolean {
        return this.destinoActual?.esRemoto ?? false;
    }

    obtenerDestinoActual(): DestinoWiFiParseado | null {
        return this.destinoActual;
    }

    estaConectado(): boolean {
        return this.conectado;
    }

    async cerrar(): Promise<void> {
        this.conectado = false;
        this.bufferAcumuladoTcp = Buffer.alloc(0);
        this.ultimoPaqueteUdpMs = 0;
        this.destinoActual = null;
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