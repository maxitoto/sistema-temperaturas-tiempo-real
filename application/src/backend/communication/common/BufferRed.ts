// Archivo: ./src/communication/BufferRed.ts

/**
 * Límites y offsets del stack OSI.
 *
 * Layout del buffer (de izquierda a derecha en `raw`):
 *
 *   [0..1]   Cabecera de Red         (2B)
 *   [2..11]  Cabecera de Transporte  (10B)
 *   [12..19] Sobre de Sesión         (8B)
 *   [20..511] Payload de Aplicación  (492B)
 *
 * Los offsets son fijos para evitar copias: cada capa "empuja" su cabecera
 * hacia atrás (TX) o la "consume" hacia adelante (RX), moviendo solo el
 * puntero `inicio`, nunca los datos.
 */
export const LimitesPila = {
  MAX_NODOS_RED: 4,
  TAMANO_MAX_CHUNK: 64,
  MAX_TRAMA_ENLACE: 512,

  TAMANO_CABECERA_RED: 2,
  TAMANO_CABECERA_TRANS: 10,
  TAMANO_RECORD_MARKER_TCP: 4, // RFC 5531 Record Marking (Wi-Fi TCP)
  TAMANO_SOBRE_SESION: 8,

  OFFSET_TRANSPORTE: 2,
  OFFSET_SESION: 12,
  OFFSET_PAYLOAD_APP: 20,
  MAX_PAYLOAD_APP: 492,
} as const;

/**
 * Paquete de red con vista zero-copy sobre un buffer fijo de 512B.
 *
 * En lugar de hacer `memcpy` por cada capa, `BufferPaquete` expone un puntero
 * `inicio` que las capas mueven:
 *
 *   · TX: cada capa llama `empujarCabecera(n)`, que retrocede `inicio` `n`
 *     bytes y devuelve esa región para que la capa escriba su cabecera. El
 *     payload ya está al final; solo se anteponen cabeceras.
 *
 *   · RX: cada capa llama `consumirCabecera(n)`, que avanza `inicio` `n`
 *     bytes y devuelve esa región como vista de solo lectura. La capa
 *     superior ve el buffer como si empezara después de las cabeceras ya
 *     procesadas.
 *
 * Ventaja: el mismo buffer de 512B sirve para TX y RX sin reasignaciones ni
 * copias intermedias. Crítico en ESP8266/ESP32 donde el heap es escaso.
 */
export class BufferPaquete {
  /** ID del nodo destino (TX) u origen (RX). -1 si no aplica. */
  public idNodo = -1;

  public esDatagramaNativo = false;

  /** Buffer subyacente de tamaño fijo. Nunca se reasigna. */
  public readonly raw = new Uint8Array(LimitesPila.MAX_TRAMA_ENLACE);

  /** Offset actual del primer byte "visible" para la capa en curso. */
  public inicio: number = LimitesPila.OFFSET_PAYLOAD_APP;

  /** Bytes visibles desde `inicio` (payload + cabeceras aún no consumidas). */
  public longitud = 0;
  

  /**
   * Prepara el buffer para TX: posiciona `inicio` al final (offset 20) para
   * que las capas empujen cabeceras hacia atrás sin tocar el payload.
   *
   * @param destino ID del nodo destino.
   */
  public prepararParaTx(destino = -1): void {
    this.idNodo = destino;
    this.esDatagramaNativo = false;
    this.inicio = LimitesPila.OFFSET_PAYLOAD_APP;
    this.longitud = 0;
  }

  /**
   * Prepara el buffer para RX: posiciona `inicio` en 0 para que las capas
   * consuman cabeceras hacia adelante.
   *
   * @param origen ID del nodo origen.
   */
  public prepararParaRx(origen = -1): void {
    this.idNodo = origen;
    this.esDatagramaNativo = false;
    this.inicio = 0;
    this.longitud = 0;
  }

  /**
   * Vista del segmento actualmente visible (desde `inicio` hasta
   * `inicio + longitud`). Es una `subarray`: comparte memoria con `raw`,
   * no copia. Mutar la vista muta el buffer original.
   */
  public vistaActual(): Uint8Array {
    return this.raw.subarray(this.inicio, this.inicio + this.longitud);
  }

  /**
   * Escribe un payload completo en la posición de payload de aplicación
   * (offset 20) y ajusta `longitud`. Rechaza payloads que no caben.
   *
   * @returns `true` si el payload cabía; `false` si excede `MAX_PAYLOAD_APP`.
   */
  public escribirPayloadTx(payload: Uint8Array): boolean {
    if (payload.length > LimitesPila.MAX_PAYLOAD_APP) return false;
    this.inicio = LimitesPila.OFFSET_PAYLOAD_APP;
    this.raw.set(payload, this.inicio);
    this.longitud = payload.length;
    return true;
  }

  /**
   * Retrocede `inicio` `n` bytes y devuelve esa región para que la capa
   * escriba su cabecera (TX). Incrementa `longitud` en `n`.
   *
   * Devuelve `null` si no hay espacio físico antes del payload actual
   * (es decir, si `inicio < n`): evita corromper memoria al empujar
   * cabeceras de más.
   *
   * @param bytesCabecera Tamaño de la cabecera a anteponer.
   */
  public empujarCabecera(bytesCabecera: number): Uint8Array | null {
    if (this.inicio < bytesCabecera) return null;
    this.inicio -= bytesCabecera;
    this.longitud += bytesCabecera;
    return this.raw.subarray(this.inicio, this.inicio + bytesCabecera);
  }

  /**
   * Avanza `inicio` `n` bytes y devuelve esa región como vista de la
   * cabecera consumida (RX). Reduce `longitud` en `n`.
   *
   * Devuelve `null` si `longitud < bytesCabecera`: no hay suficiente
   * información para extraer la cabecera.
   *
   * @param bytesCabecera Tamaño de la cabecera a consumir.
   */
  public consumirCabecera(bytesCabecera: number): Uint8Array | null {
    if (this.longitud < bytesCabecera) return null;
    const cabecera = this.raw.subarray(this.inicio, this.inicio + bytesCabecera);
    this.inicio += bytesCabecera;
    this.longitud -= bytesCabecera;
    return cabecera;
  }
}

/**
 * CRC-16-CCITT (polinomio 0x1021, valor inicial 0xFFFF).
 *
 * Se usa en dos puntos del stack:
 *   · Capa 2 (Enlace): CRC por chunk y CRC del mensaje completo.
 *   · Capa 4 (Transporte): checksum de la cabecera + payload.
 *
 * La firma acepta `crcInicial` para permitir el cálculo incremental
 * (encadenar CRC de cabecera + CRC del payload sin concatenar buffers).
 *
 * @param datos      Bytes a procesar.
 * @param longitud   Cantidad de bytes a leer (default: todo `datos`).
 * @param crcInicial CRC acumulado de llamadas anteriores (default: 0xFFFF).
 * @returns CRC de 16 bits, enmascarado a 0xFFFF.
 */
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