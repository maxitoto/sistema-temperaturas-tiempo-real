// Archivo: ./src/backend/plugins/conectividad/modem/ClienteUDPNode.ts
import * as dgram from "node:dgram";
import { MedioFisico } from "../../../communication/layers/CapaFisica";

export class ClienteUDPNode implements MedioFisico {
  private socket: dgram.Socket | null = null;
  private oyentesDatos: Array<(datos: Uint8Array) => void> = [];
  private conectado = false;

  constructor(
    private host: string = "127.0.0.1",
    private puerto: number = 9080
  ) {}

  public cambiarDestino(nuevoHost: string, nuevoPuerto = 80): void {
    this.host = nuevoHost;
    this.puerto = nuevoPuerto;
  }

  public async conectar(timeoutMs = 4000): Promise<void> {
    this.desconectar();

    return new Promise((resolve, reject) => {
      const sock = dgram.createSocket("udp4");
      this.socket = sock;

      const timer = setTimeout(() => {
        this.desconectar();
        reject(new Error(`[UDP-WIFI-PLUGIN] Timeout abriendo socket UDP hacia ${this.host}:${this.puerto}`));
      }, timeoutMs);

      sock.on("message", (msg: Buffer) => {
        const bytes = new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength);
        for (const oyente of this.oyentesDatos) {
          oyente(bytes);
        }
      });

      sock.on("close", () => {
        this.conectado = false;
      });

      sock.on("error", (err) => {
        clearTimeout(timer);
        console.error("[UDP-WIFI-PLUGIN] Error de socket UDP:", err.message);
        if (!this.conectado) {
          this.desconectar();
          reject(new Error(`[UDP-WIFI-PLUGIN] Error en ${this.host}:${this.puerto}: ${err.message}`));
        }
      });

      // Vinculamos a un puerto local efímero para enviar y recibir datagramas desde el ESP
      sock.bind(0, () => {
        clearTimeout(timer);
        this.conectado = true;
        resolve();
      });
    });
  }

  public desconectar(): void {
    if (this.socket) {
      this.socket.removeAllListeners();
      try {
        this.socket.close();
      } catch {
        // Ignorar si ya estaba cerrado
      }
      this.socket = null;
    }
    this.conectado = false;
  }

  // ==========================================================
  // CONTRATO DE MEDIO FÍSICO (Para CapaFisica.ts)
  // ==========================================================
  public escribir(datos: Uint8Array): void {
    if (!this.socket || !this.conectado || datos.length === 0) return;
    this.socket.send(Buffer.from(datos), this.puerto, this.host);
  }

  public alRecibir(callback: (datos: Uint8Array) => void): void {
    this.oyentesDatos.push(callback);
  }

  public estaActivo(): boolean {
    return Boolean(this.socket && this.conectado);
  }
}