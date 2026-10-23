// Archivo: ./src/backend/communication/layers/CapaEnlace.ts
import { BufferPaquete, LimitesPila, calcularCRC16 } from "../common/BufferRed";
import { CapaFisica } from "./CapaFisica";

enum EstadoRX {
  ESPERANDO_SOF,
  LEYENDO_CABECERA,
  LEYENDO_DATOS,
  LEYENDO_FCS,
}

export class CapaEnlace {
  public static readonly SOF_BYTE = 0x7e;
  public static readonly ESC_BYTE = 0x7d;
  public static readonly IAC_BYTE = 0xff; // Byte reservado por Telnet/RFC2217 en Wokwi
  public static readonly TIMEOUT_TRAMA_MS = 500;

  private estadoRx: EstadoRX = EstadoRX.ESPERANDO_SOF;
  private escapandoRx = false;
  private ultimoByteMs = 0;

  private readonly cabeceraRx = new Uint8Array(3);
  private posCabecera = 0;

  private readonly bufferChunkRx = new Uint8Array(LimitesPila.TAMANO_MAX_CHUNK);
  private posDatosChunk = 0;

  private readonly fcsRx = new Uint8Array(4);
  private posFcs = 0;

  private readonly bufferEnsamblaje = new Uint8Array(LimitesPila.MAX_TRAMA_ENLACE);
  private longitudEnsamblada = 0;
  private totalChunksEsperados = 0;
  private chunkSiguiente = 0;

  private oyenteSubida: ((pkt: BufferPaquete) => void) | null = null;

  constructor(private fisica: CapaFisica, private idMedio: number) {}

  private reiniciarEstadoTrama(): void {
    this.estadoRx = EstadoRX.ESPERANDO_SOF;
    this.escapandoRx = false;
    this.posCabecera = 0;
    this.posDatosChunk = 0;
    this.posFcs = 0;
  }

  private reiniciarEnsamblaje(): void {
    this.longitudEnsamblada = 0;
    this.totalChunksEsperados = 0;
    this.chunkSiguiente = 0;
  }

  private static requiereEscape(b: number): boolean {
    return b === CapaEnlace.SOF_BYTE || b === CapaEnlace.ESC_BYTE || b === CapaEnlace.IAC_BYTE;
  }

  public alSubirTrama(callback: (pkt: BufferPaquete) => void): void {
    this.oyenteSubida = callback;
  }

  // CAMINO DE BAJADA (TX): Fragmenta y aplica Byte-Stuffing HDLC (0x7E, 0x7D, 0xFF)
  public bajarTrama(pkt: BufferPaquete): boolean {
    if (pkt.longitud === 0 || pkt.longitud > LimitesPila.MAX_TRAMA_ENLACE) {
      return false;
    }

    const payload = pkt.vistaActual();
    const longitud = payload.length;
    const crcMensajeUnificado = calcularCRC16(payload, longitud);
    const totalChunks = Math.ceil(longitud / LimitesPila.TAMANO_MAX_CHUNK);

    for (let i = 0; i < totalChunks; i++) {
      const inicio = i * LimitesPila.TAMANO_MAX_CHUNK;
      const segmento = payload.subarray(inicio, inicio + LimitesPila.TAMANO_MAX_CHUNK);
      const lenSegmento = segmento.length;

      const crudo = new Uint8Array(7 + lenSegmento);
      crudo[0] = i;
      crudo[1] = totalChunks;
      crudo[2] = lenSegmento;
      crudo.set(segmento, 3);

      const crcTrama = calcularCRC16(crudo.subarray(0, 3 + lenSegmento), 3 + lenSegmento);
      crudo[3 + lenSegmento] = (crcTrama >> 8) & 0xff;
      crudo[4 + lenSegmento] = crcTrama & 0xff;

      const crcFinal = i === totalChunks - 1 ? crcMensajeUnificado : 0x0000;
      crudo[5 + lenSegmento] = (crcFinal >> 8) & 0xff;
      crudo[6 + lenSegmento] = crcFinal & 0xff;

      const maxEscapado = new Uint8Array(1 + crudo.length * 2);
      let posTx = 0;
      maxEscapado[posTx++] = CapaEnlace.SOF_BYTE;

      for (let k = 0; k < crudo.length; k++) {
        const b = crudo[k];
        if (CapaEnlace.requiereEscape(b)) {
          maxEscapado[posTx++] = CapaEnlace.ESC_BYTE;
          maxEscapado[posTx++] = b ^ 0x20;
        } else {
          maxEscapado[posTx++] = b;
        }
      }

      const tramaFisica = maxEscapado.subarray(0, posTx);
      if (this.fisica.enviarBytes(this.idMedio, tramaFisica) !== tramaFisica.length) {
        return false;
      }
    }
    return true;
  }

  // CAMINO DE SUBIDA (RX): Des-escapa 0x7D y resincroniza inmediatamente en 0x7E
  public alimentarBytesRx(bytes: Uint8Array): void {
    const ahora = Date.now();
    if (
      (this.estadoRx !== EstadoRX.ESPERANDO_SOF || this.totalChunksEsperados > 0) &&
      ahora - this.ultimoByteMs > CapaEnlace.TIMEOUT_TRAMA_MS
    ) {
      this.reiniciarEstadoTrama();
      this.reiniciarEnsamblaje();
    }
    this.ultimoByteMs = ahora;

    for (let b = 0; b < bytes.length; b++) {
      const byteCrudo = bytes[b];

      if (byteCrudo === CapaEnlace.SOF_BYTE) {
        this.posCabecera = 0;
        this.posDatosChunk = 0;
        this.posFcs = 0;
        this.escapandoRx = false;
        this.estadoRx = EstadoRX.LEYENDO_CABECERA;
        continue;
      }

      if (this.estadoRx === EstadoRX.ESPERANDO_SOF) {
        continue;
      }

      if (byteCrudo === CapaEnlace.ESC_BYTE) {
        this.escapandoRx = true;
        continue;
      }

      const byteLeido = this.escapandoRx ? (byteCrudo ^ 0x20) : byteCrudo;
      this.escapandoRx = false;

      switch (this.estadoRx) {
        
        case EstadoRX.LEYENDO_CABECERA:
          this.cabeceraRx[this.posCabecera++] = byteLeido;
          if (this.posCabecera === 3) {
            const indice = this.cabeceraRx[0];
            const total = this.cabeceraRx[1];
            const len = this.cabeceraRx[2];
            if (len === 0 || len > LimitesPila.TAMANO_MAX_CHUNK || total === 0 || indice >= total) {
              this.reiniciarEstadoTrama();
              this.reiniciarEnsamblaje();
            } else {
              this.posDatosChunk = 0;
              this.estadoRx = EstadoRX.LEYENDO_DATOS;
            }
          }
          break;

        case EstadoRX.LEYENDO_DATOS:
          this.bufferChunkRx[this.posDatosChunk++] = byteLeido;
          if (this.posDatosChunk === this.cabeceraRx[2]) {
            this.posFcs = 0;
            this.estadoRx = EstadoRX.LEYENDO_FCS;
          }
          break;

        case EstadoRX.LEYENDO_FCS:
          this.fcsRx[this.posFcs++] = byteLeido;
          if (this.posFcs === 4) {
            const indice = this.cabeceraRx[0];
            const total = this.cabeceraRx[1];
            const len = this.cabeceraRx[2];
            const crcTramaRecib = (this.fcsRx[0] << 8) | this.fcsRx[1];
            const crcMsgRecib = (this.fcsRx[2] << 8) | this.fcsRx[3];

            this.reiniciarEstadoTrama();

            let crcCalc = calcularCRC16(this.cabeceraRx, 3);
            crcCalc = calcularCRC16(this.bufferChunkRx.subarray(0, len), len, crcCalc);
            if (crcCalc !== crcTramaRecib) {
              this.reiniciarEnsamblaje();
              continue;
            }

            if (indice === 0) {
              this.reiniciarEnsamblaje();
              this.totalChunksEsperados = total;
              this.chunkSiguiente = 1;
              this.bufferEnsamblaje.set(this.bufferChunkRx.subarray(0, len), 0);
              this.longitudEnsamblada = len;
            } else if (indice === this.chunkSiguiente && total === this.totalChunksEsperados) {
              if (this.longitudEnsamblada + len > LimitesPila.MAX_TRAMA_ENLACE) {
                this.reiniciarEnsamblaje();
                continue;
              }
              this.bufferEnsamblaje.set(this.bufferChunkRx.subarray(0, len), this.longitudEnsamblada);
              this.longitudEnsamblada += len;
              this.chunkSiguiente++;
            } else {
              this.reiniciarEnsamblaje();
              continue;
            }

            if (this.chunkSiguiente === this.totalChunksEsperados) {
              const crcTotalCalc = calcularCRC16(
                this.bufferEnsamblaje.subarray(0, this.longitudEnsamblada),
                this.longitudEnsamblada
              );
              if (crcTotalCalc !== crcMsgRecib) {
                this.reiniciarEnsamblaje();
                continue;
              }

              const pktSalida = new BufferPaquete();
              pktSalida.prepararParaRx(this.idMedio);
              pktSalida.raw.set(this.bufferEnsamblaje.subarray(0, this.longitudEnsamblada), 0);
              pktSalida.longitud = this.longitudEnsamblada;
              this.reiniciarEnsamblaje();

              if (this.oyenteSubida) {
                this.oyenteSubida(pktSalida);
              }
            }
          }
          break;
      }
    }
  }
}