// Archivo: ./src/backend/plugins/conectividad/usb/PuertoUSBNode.ts
import * as fs from "node:fs";
import * as dgram from "node:dgram";
import { SerialPort } from "serialport";
import { MedioFisico } from "../../../communication/layers/CapaFisica";

export interface InfoPuertoSerial {
  path: string;
  manufacturer?: string;
  vendorId?: string;
  productId?: string;
  esCandidatoESP: boolean;
}

export class PuertoUSBNode implements MedioFisico {
  private puerto: SerialPort | null = null;
  private socketUdp: dgram.Socket | null = null;
  private hostUdp = "127.0.0.1";
  private portUdp = 0;
  private rinfoCliente: { address: string; port: number } | null = null;

  private oyentesDatos: Array<(datos: Uint8Array) => void> = [];
  private conectado = false;

  private static readonly VIDS_ESP = new Set(["10c4", "1a86", "0403", "303a"]);

  private static parsearEndpointUdp(entrada: string): { host: string; port: number } | null {
    const limpio = entrada.trim().replace(/^udp:\/\//i, "");
    const match = /^([^:/\s]+):(\d+)$/.exec(limpio);
    if (!match) return null;
    const port = Number(match[2]);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) return null;
    return { host: match[1], port };
  }

  public static async listarPuertos(): Promise<InfoPuertoSerial[]> {
    const puertos = await SerialPort.list();
    const resultado: InfoPuertoSerial[] = puertos.map((p) => {
      const vid = (p.vendorId ?? "").toLowerCase();
      const mfr = (p.manufacturer ?? "").toLowerCase();
      const pathLower = p.path.toLowerCase();

      const esCandidatoESP =
        PuertoUSBNode.VIDS_ESP.has(vid) ||
        mfr.includes("silicon labs") ||
        mfr.includes("wch.cn") ||
        mfr.includes("espressif") ||
        pathLower.includes("usbserial") ||
        pathLower.includes("wchusbserial") ||
        pathLower.includes("ttyusb") ||
        pathLower.includes("ttyacm");

      return {
        path: p.path,
        manufacturer: p.manufacturer,
        vendorId: p.vendorId,
        productId: p.productId,
        esCandidatoESP,
      };
    });

    const rutaSocat = "/tmp/virtual_serial";
    if (fs.existsSync(rutaSocat) && !resultado.some((r) => r.path === rutaSocat)) {
      resultado.unshift({
        path: rutaSocat,
        manufacturer: "Wokwi Virtual Serial (socat)",
        esCandidatoESP: true,
      });
    }

    return resultado;
  }

  public static async detectarPuertoESP(): Promise<string | null> {
    const lista = await PuertoUSBNode.listarPuertos();
    const candidato = lista.find((p) => p.esCandidatoESP);
    return candidato ? candidato.path : null;
  }

  public async conectar(pathManual?: string, baudRate: number = 115200): Promise<string> {
    await this.desconectar();

    const pathFinal = pathManual?.trim() || (await PuertoUSBNode.detectarPuertoESP());
    if (!pathFinal) {
      throw new Error(
        "No se detectó ningún dispositivo ESP por USB. Ingrese el puerto o endpoint UDP (ej. 127.0.0.1:4000)."
      );
    }

    // MODO A: Si pathFinal es un endpoint UDP ("127.0.0.1:PORT" o "udp://127.0.0.1:PORT") para extension.js
    const udpCfg = PuertoUSBNode.parsearEndpointUdp(pathFinal);
    if (udpCfg) {
      this.hostUdp = udpCfg.host;
      this.portUdp = udpCfg.port;
      this.rinfoCliente = null;

      return new Promise((resolve, reject) => {
        const sock = dgram.createSocket("udp4");
        this.socketUdp = sock;

        sock.on("message", (msg: Buffer, rinfo) => {
          this.rinfoCliente = { address: rinfo.address, port: rinfo.port };
          const bytes = new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength);
          for (const oyente of this.oyentesDatos) {
            oyente(bytes);
          }
        });

        sock.on("close", () => {
          this.conectado = false;
        });

        sock.on("error", (err) => {
          this.conectado = false;
          console.error("[USB-UDP-PLUGIN] Error UDP:", err.message);
          reject(new Error(`Error en puente USB-UDP (${pathFinal}): ${err.message}`));
        });

        sock.bind(0, () => {
          this.conectado = true;
          resolve(`udp://${this.hostUdp}:${this.portUdp}`);
        });
      });
    }

    // MODO B: Dispositivo Serial o PTY (/tmp/virtual_serial)
    return new Promise((resolve, reject) => {
      this.puerto = new SerialPort({
        path: pathFinal,
        baudRate,
        autoOpen: false,
      });

      this.puerto.on("open", () => {
        this.puerto?.flush(() => {
          this.conectado = true;
          resolve(pathFinal);
        });
      });

      this.puerto.on("data", (chunk: Buffer) => {
        const bytes = new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
        for (const oyente of this.oyentesDatos) {
          oyente(bytes);
        }
      });

      this.puerto.on("close", () => {
        this.conectado = false;
      });

      this.puerto.on("error", (err) => {
        this.conectado = false;
        console.error("[USB-PLUGIN] Error serial:", err.message);
      });

      this.puerto.open((err) => {
        if (err) {
          this.conectado = false;
          reject(new Error(`No se pudo abrir ${pathFinal} a ${baudRate} bps: ${err.message}`));
        }
      });
    });
  }

  public async desconectar(): Promise<void> {
    if (this.socketUdp) {
      this.socketUdp.removeAllListeners();
      try {
        this.socketUdp.close();
      } catch {
        // Ignorar
      }
      this.socketUdp = null;
      this.rinfoCliente = null;
    }

    return new Promise((resolve) => {
      if (!this.puerto || !this.puerto.isOpen) {
        this.conectado = false;
        return resolve();
      }
      this.puerto.close(() => {
        this.conectado = false;
        this.puerto = null;
        resolve();
      });
    });
  }

  public escribir(datos: Uint8Array): void {
    if (!this.conectado || datos.length === 0) return;

    if (this.socketUdp) {
      const destHost = this.rinfoCliente?.address ?? this.hostUdp;
      const destPort = this.rinfoCliente?.port ?? this.portUdp;
      this.socketUdp.send(Buffer.from(datos), destPort, destHost);
      return;
    }

    if (this.puerto && this.puerto.isOpen) {
      this.puerto.write(Buffer.from(datos));
    }
  }

  public alRecibir(callback: (datos: Uint8Array) => void): void {
    this.oyentesDatos.push(callback);
  }

  public estaActivo(): boolean {
    if (this.socketUdp) return this.conectado;
    return Boolean(this.puerto && this.puerto.isOpen && this.conectado);
  }
}