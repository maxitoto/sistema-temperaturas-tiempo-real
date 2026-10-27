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

  // VIDs típicos de placas ESP8266 / ESP32:
  // 10C4 = Silicon Labs (CP2102/CP2104)
  // 1A86 = QinHeng Electronics (CH340/CH341/CH9102)
  // 0403 = FTDI
  // 303A = Espressif Systems (USB nativo ESP32-S2/S3/C3)
  private static readonly VIDS_ESP = new Set(["10c4", "1a86", "0403", "303a"]);

  // Lista todos los puertos y marca cuáles son placas ESP
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

    // Soporte directo para puente virtual socat + Wokwi (/tmp/virtual_serial)
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

  // Busca automáticamente el primer puerto que coincida con un ESP8266/ESP32
  public static async detectarPuertoESP(): Promise<string | null> {
    const lista = await PuertoUSBNode.listarPuertos();
    const candidato = lista.find((p) => p.esCandidatoESP);
    return candidato ? candidato.path : null;
  }

  // Conecta al puerto indicado (o lo auto-detecta si no se envía path). Baudios opcionales (default 115200)
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

  // ==========================================================
  // CONTRATO DE MEDIO FÍSICO (Para CapaFisica.ts)
  // ==========================================================
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