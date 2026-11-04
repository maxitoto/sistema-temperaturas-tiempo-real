import * as fs from "node:fs";
import * as dgram from "node:dgram";
import { SerialPort } from "serialport";

export interface SerialPortInfo {
  path: string;
  manufacturer?: string;
  vendorId?: string;
  productId?: string;
  esCandidatoESP: boolean;
}

export class UartPhy {
  public static readonly BAUD_RATE = 115200;

  private port_: SerialPort | null = null;
  private udpBridge_: dgram.Socket | null = null;
  private udpHost_ = "127.0.0.1";
  private udpPort_ = 0;
  private udpClientRinfo_: { address: string; port: number } | null = null;

  private path_ = "";
  private activePath_ = "";
  private rxAccum_: number[] = [];
  private decodedFramesQueue_: Uint8Array[] = [];
  private isOpen_ = false;

  private static readonly VIDS_ESP = new Set(["10c4", "1a86", "0403", "303a"]);

  public loadConfig(): void {
    // cargar de base de datos
  }

  public configure(pathManual?: string): void {
    if (pathManual && pathManual.trim().length > 0) {
      // zona base de datos
      this.path_ = pathManual.trim();
    }
  }

  public getConfiguredPath(): string {
    return this.path_ || this.activePath_;
  }

  public static async listarPuertos(): Promise<SerialPortInfo[]> {
    const puertos = await SerialPort.list();
    const resultado: SerialPortInfo[] = puertos.map((p) => {
      const vid = (p.vendorId ?? "").toLowerCase();
      const mfr = (p.manufacturer ?? "").toLowerCase();
      const pathLower = p.path.toLowerCase();

      const esCandidatoESP =
        UartPhy.VIDS_ESP.has(vid) ||
        mfr.includes("silicon labs") ||
        mfr.includes("wch.cn") ||
        mfr.includes("espressif") ||
        pathLower.includes("usbserial") ||
        pathLower.includes("slab_usb") ||
        pathLower.includes("wchusbserial") ||
        pathLower.includes("usbmodem") ||
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

  public async begin(): Promise<boolean> {
    let targetPath = this.path_;
    if (!targetPath) {
      const candidatos = await UartPhy.listarPuertos();
      const esp = candidatos.find((c) => c.esCandidatoESP);
      if (!esp) return false;
      targetPath = esp.path;
      this.path_ = targetPath;
    }

    // Política keepOpen: si el puerto ya está abierto en el mismo path, no cerrarlo ni pulsar DTR/RTS
    if (this.isOpen_ && this.activePath_ === targetPath && ((this.port_ && this.port_.isOpen) || this.udpBridge_)) {
      this.rxAccum_ = [];
      this.decodedFramesQueue_ = [];
      return true;
    }

    await this.close();
    this.rxAccum_ = [];
    this.decodedFramesQueue_ = [];

    const udpMatch =
      /^udp:\/\/([^:/\s]+):(\d+)$/i.exec(targetPath) ?? /^([^:/\s]+):(\d+)$/.exec(targetPath);
    if (udpMatch) {
      this.udpHost_ = udpMatch[1];
      this.udpPort_ = Number(udpMatch[2]);
      return new Promise((resolve) => {
        const sock = dgram.createSocket("udp4");
        this.udpBridge_ = sock;
        sock.on("message", (msg: Buffer, rinfo) => {
          this.udpClientRinfo_ = { address: rinfo.address, port: rinfo.port };
          this.onRawBytes(new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength));
        });
        sock.on("close", () => {
          this.isOpen_ = false;
        });
        sock.bind(0, () => {
          this.activePath_ = targetPath;
          this.isOpen_ = true;
          resolve(true);
        });
      });
    }

    return new Promise((resolve) => {
      this.port_ = new SerialPort({
        path: targetPath,
        baudRate: UartPhy.BAUD_RATE,
        autoOpen: false,
        hupcl: false,
      });

      this.port_.on("data", (chunk: Buffer) => {
        this.onRawBytes(new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength));
      });

      this.port_.on("close", () => {
        this.isOpen_ = false;
      });

      this.port_.on("error", () => {
        this.isOpen_ = false;
      });

      this.port_.open((err) => {
        if (err) {
          this.isOpen_ = false;
          resolve(false);
          return;
        }
        this.port_?.set({ dtr: false, rts: false }, () => {
          // Esperar si el pulso inicial de apertura del SO disparó el bootloader del ESP32
          setTimeout(() => {
            this.port_?.flush(() => {
              this.rxAccum_ = [];
              this.decodedFramesQueue_ = [];
              this.activePath_ = targetPath;
              this.isOpen_ = true;
              resolve(true);
            });
          }, 450);
        });
      });
    });
  }

  public clearBuffers(): void {
    this.rxAccum_ = [];
    this.decodedFramesQueue_ = [];
  }

  public async close(): Promise<void> {
    if (this.udpBridge_) {
      this.udpBridge_.removeAllListeners();
      try {
        this.udpBridge_.close();
      } catch {
        // Ignorar
      }
      this.udpBridge_ = null;
    }
    if (this.port_ && this.port_.isOpen) {
      await new Promise<void>((resolve) => {
        this.port_!.close(() => resolve());
      });
    }
    this.port_ = null;
    this.activePath_ = "";
    this.isOpen_ = false;
  }

  public isOpen(): boolean {
    return this.isOpen_;
  }

  public sendFrame(data: Uint8Array): void {
    if (!this.isOpen_) return;
    const encoded = UartPhy.cobsEncode(data);
    const frameWithDelimiters = new Uint8Array(encoded.length + 2);
    frameWithDelimiters[0] = 0x00;
    frameWithDelimiters.set(encoded, 1);
    frameWithDelimiters[encoded.length + 1] = 0x00;

    if (this.udpBridge_) {
      const host = this.udpClientRinfo_?.address ?? this.udpHost_;
      const port = this.udpClientRinfo_?.port ?? this.udpPort_;
      this.udpBridge_.send(Buffer.from(frameWithDelimiters), port, host);
      return;
    }

    if (this.port_ && this.port_.isOpen) {
      this.port_.write(Buffer.from(frameWithDelimiters));
    }
  }

  public pollFrame(): Uint8Array | null {
    return this.decodedFramesQueue_.shift() ?? null;
  }

  private onRawBytes(bytes: Uint8Array): void {
    for (let i = 0; i < bytes.length; i++) {
      const b = bytes[i];
      if (b === 0x00) {
        if (this.rxAccum_.length > 0) {
          const rawFrame = new Uint8Array(this.rxAccum_);
          this.rxAccum_ = [];
          const decoded = UartPhy.cobsDecode(rawFrame);
          if (decoded) {
            this.decodedFramesQueue_.push(decoded);
          }
        }
      } else {
        if (this.rxAccum_.length < 512) {
          this.rxAccum_.push(b);
        } else {
          this.rxAccum_ = [];
        }
      }
    }
  }

  public static cobsEncode(input: Uint8Array): Uint8Array {
    const output: number[] = [0];
    let codeIndex = 0;
    let code = 1;

    for (let i = 0; i < input.length; i++) {
      if (input[i] === 0) {
        output[codeIndex] = code;
        code = 1;
        codeIndex = output.length;
        output.push(0);
      } else {
        output.push(input[i]);
        code++;
        if (code === 0xff) {
          output[codeIndex] = code;
          code = 1;
          codeIndex = output.length;
          output.push(0);
        }
      }
    }
    output[codeIndex] = code;
    return new Uint8Array(output);
  }

  public static cobsDecode(input: Uint8Array): Uint8Array | null {
    const output: number[] = [];
    let i = 0;
    while (i < input.length) {
      const code = input[i++];
      if (code === 0 || i + code - 1 > input.length) return null;
      for (let j = 1; j < code; j++) {
        output.push(input[i++]);
      }
      if (code < 0xff && i < input.length) {
        output.push(0);
      }
    }
    return new Uint8Array(output);
  }
}