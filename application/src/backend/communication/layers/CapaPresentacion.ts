// Archivo: ./src/backend/communication/layers/CapaPresentacion.ts
import { encode, decode } from "@msgpack/msgpack";
import { BufferPaquete, LimitesPila } from "../common/BufferRed";
import { CapaSesion, MensajeSesion } from "./CapaSesion";

export interface MensajePresentacion {
  idNodo: number;
  tipoSesion: number;
  reqId: number;
  cabecera: string;
  argumentos: unknown[];
  lector: LectorMsgPack;
}

export type TransformadorPayloadCapa6 = (
  idNodo: number,
  reqId: number,
  vistaPayload: Uint8Array
) => Uint8Array | null;

export class LectorMsgPack {
  private pos = 0;
  private readonly view: DataView;

  constructor(private readonly buf: Uint8Array) {
    this.view = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  }

  public hayDatos(): boolean {
    return this.pos < this.buf.length;
  }

  public iniciarArray(): number | null {
    if (this.pos >= this.buf.length) return null;
    const b = this.buf[this.pos];
    if (b >= 0x90 && b <= 0x9f) {
      this.pos++;
      return b & 0x0f;
    }
    if (b === 0xdc && this.pos + 3 <= this.buf.length) {
      this.pos++;
      const count = this.view.getUint16(this.pos, false);
      this.pos += 2;
      return count;
    }
    return null;
  }

  public leerInt32(): number | null {
    if (this.pos >= this.buf.length) return null;
    const b = this.buf[this.pos++];
    if (b <= 0x7f) return b;
    if (b >= 0xe0) return b - 256;
    switch (b) {
      case 0xcc:
        if (this.pos + 1 > this.buf.length) return null;
        return this.buf[this.pos++];
      case 0xd0:
        if (this.pos + 1 > this.buf.length) return null;
        return this.view.getInt8(this.pos++);
      case 0xcd: {
        if (this.pos + 2 > this.buf.length) return null;
        const v = this.view.getUint16(this.pos, false);
        this.pos += 2;
        return v;
      }
      case 0xd1: {
        if (this.pos + 2 > this.buf.length) return null;
        const v = this.view.getInt16(this.pos, false);
        this.pos += 2;
        return v;
      }
      case 0xce:
      case 0xd2: {
        if (this.pos + 4 > this.buf.length) return null;
        const v = this.view.getInt32(this.pos, false);
        this.pos += 4;
        return v;
      }
      default:
        this.pos--;
        return null;
    }
  }

  public leerFloat(): number | null {
    if (this.pos >= this.buf.length) return null;
    const b = this.buf[this.pos];
    if (b === 0xca && this.pos + 5 <= this.buf.length) {
      this.pos++;
      const v = this.view.getFloat32(this.pos, false);
      this.pos += 4;
      return v;
    }
    if (b === 0xcb && this.pos + 9 <= this.buf.length) {
      this.pos++;
      const v = this.view.getFloat64(this.pos, false);
      this.pos += 8;
      return v;
    }
    return this.leerInt32();
  }

  public leerTexto(): string | null {
    if (this.pos >= this.buf.length) return null;
    const b = this.buf[this.pos++];
    let slen = 0;
    if (b >= 0xa0 && b <= 0xbf) {
      slen = b & 0x1f;
    } else if (b === 0xd9) {
      if (this.pos >= this.buf.length) return null;
      slen = this.buf[this.pos++];
    } else if (b === 0xda) {
      if (this.pos + 2 > this.buf.length) return null;
      slen = this.view.getUint16(this.pos, false);
      this.pos += 2;
    } else {
      this.pos--;
      return null;
    }
    if (this.pos + slen > this.buf.length) return null;
    const texto = Buffer.from(
      this.buf.buffer,
      this.buf.byteOffset + this.pos,
      slen
    ).toString("utf-8");
    this.pos += slen;
    return texto;
  }
}

export class EscritorMsgPack {
  private pos: number;
  private readonly view: DataView;

  constructor(private readonly pkt: BufferPaquete) {
    this.pos = pkt.longitud;
    this.view = new DataView(
      pkt.raw.buffer,
      pkt.raw.byteOffset + LimitesPila.OFFSET_PAYLOAD_APP,
      LimitesPila.MAX_PAYLOAD_APP
    );
  }

  public iniciarArray(cantidadElementos: number): boolean {
    if (cantidadElementos <= 15) {
      if (this.pos + 1 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0x90 | cantidadElementos);
    } else {
      if (this.pos + 3 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xdc);
      this.view.setUint16(this.pos, cantidadElementos, false);
      this.pos += 2;
    }
    this.pkt.longitud = this.pos;
    return true;
  }

  public escribirInt(valor: number): boolean {
    const v = Math.trunc(valor);
    if (v >= 0 && v <= 127) {
      if (this.pos + 1 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, v);
    } else if (v >= -32 && v < 0) {
      if (this.pos + 1 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setInt8(this.pos++, v);
    } else if (v >= -32768 && v <= 32767) {
      if (this.pos + 3 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xd1);
      this.view.setInt16(this.pos, v, false);
      this.pos += 2;
    } else {
      if (this.pos + 5 > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xd2);
      this.view.setInt32(this.pos, v, false);
      this.pos += 4;
    }
    this.pkt.longitud = this.pos;
    return true;
  }

  public escribirFloat(valor: number): boolean {
    if (this.pos + 5 > LimitesPila.MAX_PAYLOAD_APP) return false;
    this.view.setUint8(this.pos++, 0xca);
    this.view.setFloat32(this.pos, valor, false);
    this.pos += 4;
    this.pkt.longitud = this.pos;
    return true;
  }

  public escribirTexto(str: string): boolean {
    const bytes = Buffer.from(str, "utf-8");
    const slen = bytes.length;
    if (slen <= 31) {
      if (this.pos + 1 + slen > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xa0 | slen);
    } else if (slen <= 255) {
      if (this.pos + 2 + slen > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xd9);
      this.view.setUint8(this.pos++, slen);
    } else {
      if (this.pos + 3 + slen > LimitesPila.MAX_PAYLOAD_APP) return false;
      this.view.setUint8(this.pos++, 0xda);
      this.view.setUint16(this.pos, slen, false);
      this.pos += 2;
    }
    this.pkt.raw.set(bytes, LimitesPila.OFFSET_PAYLOAD_APP + this.pos);
    this.pos += slen;
    this.pkt.longitud = this.pos;
    return true;
  }
}

export class CapaPresentacion {
  private oyentesMensaje: Array<(msj: MensajePresentacion) => void> = [];
  private transformadorTx: TransformadorPayloadCapa6 | null = null;
  private transformadorRx: TransformadorPayloadCapa6 | null = null;

  constructor(private readonly sesion: CapaSesion) {
    this.sesion.alSubirSesion((pkt, meta) => this.procesarSubida(pkt, meta));
  }

  public configurarTransformadores(
    fnTx: TransformadorPayloadCapa6 | null,
    fnRx: TransformadorPayloadCapa6 | null
  ): void {
    this.transformadorTx = fnTx;
    this.transformadorRx = fnRx;
  }

  private procesarSubida(pkt: BufferPaquete, meta: MensajeSesion): void {
    let vista = pkt.vistaActual();
    if (vista.length === 0 || vista.length > LimitesPila.MAX_PAYLOAD_APP) return;

    if (this.transformadorRx) {
      const desencriptado = this.transformadorRx(meta.idNodo, meta.reqId, vista);
      if (!desencriptado) return;
      vista = desencriptado;
    }

    const decodificado = decode(vista);
    if (!Array.isArray(decodificado) || decodificado.length === 0) return;

    const [cabeceraRaw, ...argumentos] = decodificado;
    const msj: MensajePresentacion = {
      idNodo: meta.idNodo,
      tipoSesion: meta.tipo,
      reqId: meta.reqId,
      cabecera: String(cabeceraRaw),
      argumentos,
      lector: new LectorMsgPack(vista),
    };

    for (const oyente of this.oyentesMensaje) {
      oyente(msj);
    }
  }

  public alSubirMensaje(callback: (msj: MensajePresentacion) => void): void {
    this.oyentesMensaje.push(callback);
  }

  public empaquetarEnBuffer(
    idNodo: number,
    cabecera: string,
    argumentos: unknown[] = [],
    reqId = 0
  ): BufferPaquete {
    const pkt = new BufferPaquete();
    pkt.prepararParaTx(idNodo);
    let bytes: Uint8Array = encode([cabecera, ...argumentos]);
    if (this.transformadorTx) {
      const transformado = this.transformadorTx(idNodo, reqId, bytes);
      if (!transformado) {
        throw new Error("Fallo al encriptar payload en Capa 6 (AES-256-GCM)");
      }
      bytes = transformado;
    }
    if (!pkt.escribirPayloadTx(bytes)) {
      throw new Error(
        `Payload Capa 6 (${bytes.length}B) excede MAX_PAYLOAD_APP (${LimitesPila.MAX_PAYLOAD_APP}B)`
      );
    }
    return pkt;
  }

  public async invocarRequest(
    idNodo: number,
    comando: string,
    parametros: unknown[] = [],
    timeoutMs = 5000
  ): Promise<unknown[]> {
    const pktTx = this.empaquetarEnBuffer(idNodo, comando, parametros, 0);
    const pktRx = await this.sesion.enviarPeticionRPC(idNodo, pktTx, timeoutMs);
    let vistaRx = pktRx.vistaActual();
    if (this.transformadorRx) {
      const desencriptado = this.transformadorRx(idNodo, 0, vistaRx);
      if (!desencriptado) {
        throw new Error("Tag de autenticidad GCM inválido en respuesta Request");
      }
      vistaRx = desencriptado;
    }
    const decodificado = decode(vistaRx);
    if (!Array.isArray(decodificado) || decodificado.length === 0) {
      throw new Error("Formato MessagePack inválido en respuesta Request");
    }
    return decodificado;
  }

  public bajarRespuestaFormateada(
    idNodo: number,
    reqId: number,
    estado: "OK" | "ERR",
    datos: unknown[] = []
  ): boolean {
    const pktTx = this.empaquetarEnBuffer(idNodo, estado, datos, reqId);
    return this.sesion.bajarRespuesta(idNodo, reqId, pktTx);
  }

  public bajarEventoFormateado(
    idNodo: number,
    evento: string,
    datos: unknown[] = []
  ): boolean {
    const pktTx = this.empaquetarEnBuffer(idNodo, evento, datos, 0);
    return this.sesion.bajarEvento(idNodo, pktTx);
  }

  // --- CAMINO 2: STREAMING TIEMPO REAL (UDP / DATAGRAMA) ---
  public async invocarStream(
    idNodo: number,
    canal: string,
    parametros: unknown[] = [],
    timeoutMs = 1500
  ): Promise<unknown[]> {
    const pktTx = this.empaquetarEnBuffer(idNodo, canal, parametros, 0);
    const pktRx = await this.sesion.consultarStream(idNodo, pktTx, timeoutMs);
    let vistaRx = pktRx.vistaActual();
    if (this.transformadorRx) {
      const desencriptado = this.transformadorRx(idNodo, 0, vistaRx);
      if (!desencriptado) {
        throw new Error("Tag de autenticidad GCM inválido en respuesta Stream");
      }
      vistaRx = desencriptado;
    }
    const decodificado = decode(vistaRx);
    if (!Array.isArray(decodificado) || decodificado.length === 0) {
      throw new Error("Formato MessagePack inválido en cuadro de Stream");
    }
    return decodificado;
  }

  public bajarRespuestaStreamFormateada(
    idNodo: number,
    reqId: number,
    estado: "OK" | "ERR",
    datos: unknown[] = []
  ): boolean {
    const pktTx = this.empaquetarEnBuffer(idNodo, estado, datos, reqId);
    return this.sesion.bajarRespuestaStream(idNodo, reqId, pktTx);
  }

  public bajarStreamFormateado(
    idNodo: number,
    canal: string,
    datos: unknown[] = []
  ): boolean {
    const pktTx = this.empaquetarEnBuffer(idNodo, canal, datos, 0);
    return this.sesion.bajarStream(idNodo, pktTx);
  }
}