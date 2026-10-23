// src/communication/conectividad/modem/ClienteTCPNode.ts
import * as net from "net";
import { MedioFisico } from "../../../communication/layers/CapaFisica";

export class ClienteTCPNode implements MedioFisico {
  private socket: net.Socket | null = null;
  private oyentesDatos: Array<(datos: Uint8Array) => void> = [];
  private conectado = false;

  constructor(
    private host: string = "192.168.4.1", // IP del ESP (AP o STA)
    private puerto: number = 80           // Coincide con servidorTCP(80, 2) en Contenedor.h
  ) {}

  public cambiarDestino(nuevoHost: string, nuevoPuerto = 80): void {
    this.host = nuevoHost;
    this.puerto = nuevoPuerto;
  }

  // El Inicializador o Gestor de Red de Electron llama a este método (NUNCA la Capa Física)
  public async conectar(timeoutMs = 4000): Promise<void> {
    this.desconectar();

    return new Promise((resolve, reject) => {
      const sock = new net.Socket();
      this.socket = sock;

      const timer = setTimeout(() => {
        sock.destroy();
        this.conectado = false;
        reject(new Error(`[TCP-PLUGIN] Timeout conectando a ${this.host}:${this.puerto}`));
      }, timeoutMs);

      sock.setNoDelay(true); // Envío inmediato de tramas de 64B sin agrupar en el OS
      sock.setKeepAlive(true, 5000);

      sock.connect(this.puerto, this.host, () => {
        clearTimeout(timer);
        this.conectado = true;
        resolve();
      });

      sock.on("data", (chunk: Buffer) => {
        const bytes = new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
        for (const oyente of this.oyentesDatos) {
          oyente(bytes);
        }
      });

      sock.on("close", () => {
        this.conectado = false;
      });

      sock.on("error", (err) => {
        clearTimeout(timer);
        this.conectado = false;
        console.error("[TCP-PLUGIN] Error de socket:", err.message);
        if (!this.conectado) {
          reject(new Error(`[TCP-PLUGIN] Error al conectar a ${this.host}:${this.puerto}: ${err.message}`));
        }
      });
    });
  }

  public desconectar(): void {
    if (this.socket) {
      this.socket.removeAllListeners();
      this.socket.destroy();
      this.socket = null;
    }
    this.conectado = false;
  }

  // ==========================================================
  // CONTRATO DE MEDIO FÍSICO (Para CapaFisica.ts)
  // ==========================================================
  public escribir(datos: Uint8Array): void {
    if (!this.socket || !this.conectado || this.socket.destroyed) return;
    this.socket.write(Buffer.from(datos));
  }

  public alRecibir(callback: (datos: Uint8Array) => void): void {
    this.oyentesDatos.push(callback);
  }

  public estaActivo(): boolean {
    return Boolean(this.socket && !this.socket.destroyed && this.conectado);
  }
}