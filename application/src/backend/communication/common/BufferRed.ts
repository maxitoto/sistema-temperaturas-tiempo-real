// Archivo: ./src/communication/BufferRed.ts

export const LimitesPila = {
  MAX_NODOS_RED: 4,
  TAMANO_MAX_CHUNK: 64,
  MAX_TRAMA_ENLACE: 512,

  TAMANO_CABECERA_RED: 2,
  TAMANO_CABECERA_TRANS: 10,
  TAMANO_SOBRE_SESION: 8,

  OFFSET_TRANSPORTE: 2,
  OFFSET_SESION: 12,
  OFFSET_PAYLOAD_APP: 20,
  MAX_PAYLOAD_APP: 492,
} as const;

export class BufferPaquete {
  public idNodo = -1;
  public readonly raw = new Uint8Array(LimitesPila.MAX_TRAMA_ENLACE);
  public inicio: number = LimitesPila.OFFSET_PAYLOAD_APP;
  public longitud = 0;

  public prepararParaTx(destino = -1): void {
    this.idNodo = destino;
    this.inicio = LimitesPila.OFFSET_PAYLOAD_APP;
    this.longitud = 0;
  }

  public prepararParaRx(origen = -1): void {
    this.idNodo = origen;
    this.inicio = 0;
    this.longitud = 0;
  }

  public vistaActual(): Uint8Array {
    return this.raw.subarray(this.inicio, this.inicio + this.longitud);
  }

  public escribirPayloadTx(payload: Uint8Array): boolean {
    if (payload.length > LimitesPila.MAX_PAYLOAD_APP) return false;
    this.inicio = LimitesPila.OFFSET_PAYLOAD_APP;
    this.raw.set(payload, this.inicio);
    this.longitud = payload.length;
    return true;
  }

  public empujarCabecera(bytesCabecera: number): Uint8Array | null {
    if (this.inicio < bytesCabecera) return null;
    this.inicio -= bytesCabecera;
    this.longitud += bytesCabecera;
    return this.raw.subarray(this.inicio, this.inicio + bytesCabecera);
  }

  public consumirCabecera(bytesCabecera: number): Uint8Array | null {
    if (this.longitud < bytesCabecera) return null;
    const cabecera = this.raw.subarray(this.inicio, this.inicio + bytesCabecera);
    this.inicio += bytesCabecera;
    this.longitud -= bytesCabecera;
    return cabecera;
  }
}

export function calcularCRC16(datos: Uint8Array, longitud = datos.length, crcInicial = 0xffff): number {
  let crc = crcInicial;
  for (let i = 0; i < longitud; i++) {
    crc ^= datos[i] << 8;
    for (let j = 0; j < 8; j++) {
      if ((crc & 0x8000) !== 0) {
        crc = ((crc << 1) ^ 0x1021) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }
  return crc & 0xffff;
}