// Archivo: ./src/backend/plugins/conectividad/modem/ClienteWiFiNode.ts
import * as net from "node:net";
import * as dgram from "node:dgram";
import { MedioFisico } from "../../../communication/layers/CapaFisica";

export class ClienteWiFiNode implements MedioFisico {
  public readonly esMedioNativo = true;

  private socketTcp: net.Socket | null = null;
  private socketUdp: dgram.Socket | null = null;
  private oyentesDatos: Array<(datos: Uint8Array, esDatagrama?: boolean) => void> = [];
  private conectado = false;

  constructor(
    private host: string = "192.168.4.1",
    private puerto: number = 80
  ) {}

  public cambiarDestino(nuevoHost: string, nuevoPuerto = 80): void {
    this.host = nuevoHost;
    this.puerto = nuevoPuerto;
  }

  public async conectar(timeoutMs = 4000): Promise<void> {
    this.desconectar();

    return new Promise((resolve, reject) => {
      const sock = new net.Socket();
      this.socketTcp = sock;

      const timer = setTimeout(() => {
        this.desconectar();
        reject(new Error(`[WIFI-PLUGIN] Timeout conectando a ${this.host}:${this.puerto}`));
      }, timeoutMs);

      sock.setNoDelay(true);
      sock.setKeepAlive(true, 5000);

      sock.connect(this.puerto, this.host, () => {
        clearTimeout(timer);

        const udp = dgram.createSocket("udp4");
        this.socketUdp = udp;

        udp.on("message", (msg: Buffer) => {
          const bytes = new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength);
          for (const oyente of this.oyentesDatos) {
            oyente(bytes, true); // esDatagrama = true (Canal UDP)
          }
        });

        udp.bind(0, () => {
          this.conectado = true;
          resolve();
        });
      });

      sock.on("data", (chunk: Buffer) => {
        const bytes = new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
        for (const oyente of this.oyentesDatos) {
          oyente(bytes, false); // esDatagrama = false (Canal TCP)
        }
      });

      sock.on("close", () => {
        this.conectado = false;
      });

      sock.on("error", (err) => {
        clearTimeout(timer);
        if (!this.conectado) {
          this.desconectar();
          reject(new Error(`[WIFI-PLUGIN] Error al conectar a ${this.host}:${this.puerto}: ${err.message}`));
        }
      });
    });
  }

  public desconectar(): void {
    if (this.socketTcp) {
      this.socketTcp.removeAllListeners();
      this.socketTcp.destroy();
      this.socketTcp = null;
    }
    if (this.socketUdp) {
      this.socketUdp.removeAllListeners();
      try {
        this.socketUdp.close();
      } catch {}
      this.socketUdp = null;
    }
    this.conectado = false;
  }

  public escribir(datos: Uint8Array, comoDatagrama = false): void {
    if (!this.conectado || datos.length === 0) return;

    if (comoDatagrama) {
      if (!this.socketUdp) return;
      this.socketUdp.send(Buffer.from(datos), this.puerto + 1, this.host);
      return;
    }

    if (!this.socketTcp || this.socketTcp.destroyed) return;
    this.socketTcp.write(Buffer.from(datos));
  }

  public alRecibir(callback: (datos: Uint8Array, esDatagrama?: boolean) => void): void {
    this.oyentesDatos.push(callback);
  }

  public estaActivo(): boolean {
    return Boolean(this.socketTcp && !this.socketTcp.destroyed && this.conectado);
  }
}