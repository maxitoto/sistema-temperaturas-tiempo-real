import * as fs from "node:fs";
import { SerialPort } from "serialport";
import { MedioFisico } from "../../layers/CapaFisica";

export interface InfoPuertoSerial {
  path: string;
  manufacturer?: string;
  vendorId?: string;
  productId?: string;
  esCandidatoESP: boolean;
}

export class PuertoUSBNode implements MedioFisico {
  private puerto: SerialPort | null = null;
  private oyentesDatos: Array<(datos: Uint8Array) => void> = [];
  private conectado = false;

  private static readonly VIDS_ESP = new Set(["10c4", "1a86", "0403", "303a"]);

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
        "No se detectó ningún dispositivo ESP por USB. Ingrese el puerto manualmente."
      );
    }

    return new Promise((resolve, reject) => {
      this.puerto = new SerialPort({
        path: pathFinal,
        baudRate,
        autoOpen: false,
        hupcl: false, // 1. Evita que el sistema operativo baje DTR al cerrar o abrir
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
          return;
        }

        // 2. Liberar de inmediato las líneas DTR y RTS para que el ESP32 no quede trabado en Bootloader
        this.puerto?.set({ dtr: false, rts: false }, () => {
          // 3. Dar margen al ESP32 para que finalice setup() si sufrió un reinicio eléctrico por el driver
          setTimeout(() => {
            this.puerto?.flush(() => {
              this.conectado = true;
              resolve(pathFinal);
            });
          }, 1800);
        });
      });
    });
  }

  public async desconectar(): Promise<void> {
    return new Promise((resolve) => {
      if (!this.puerto || !this.puerto.isOpen) {
        this.conectado = false;
        return resolve();
      }

      // Asegurar que DTR y RTS permanezcan inactivos antes de cerrar el descriptor
      this.puerto.set({ dtr: false, rts: false }, () => {
        this.puerto?.close(() => {
          this.conectado = false;
          this.puerto = null;
          resolve();
        });
      });
    });
  }

  public escribir(datos: Uint8Array): void {
    if (!this.puerto || !this.conectado) return;
    this.puerto.write(Buffer.from(datos));
  }

  public alRecibir(callback: (datos: Uint8Array) => void): void {
    this.oyentesDatos.push(callback);
  }

  public estaActivo(): boolean {
    return Boolean(this.puerto && this.puerto.isOpen && this.conectado);
  }
}