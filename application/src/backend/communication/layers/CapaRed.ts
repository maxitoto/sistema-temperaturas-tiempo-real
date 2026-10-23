// Archivo: ./src/communication/layers/CapaRed.ts
import { BufferPaquete, LimitesPila } from "../common/BufferRed";
import { CapaEnlace } from "./CapaEnlace";
import { CapaFisica } from "./CapaFisica";

export class CapaRed {
  public static readonly NODO_BROADCAST = 0x0f;
  public static readonly DEFAULT_TTL = 8;

  private readonly enlaces: CapaEnlace[] = [];
  private oyentesSubida: Array<(pkt: BufferPaquete) => void> = [];

  constructor(private fisica: CapaFisica, cantidadNodos = LimitesPila.MAX_NODOS_RED) {
    const total = Math.min(cantidadNodos, LimitesPila.MAX_NODOS_RED);
    for (let i = 0; i < total; i++) {
      const enlace = new CapaEnlace(this.fisica, i);
      enlace.alSubirTrama((pkt) => this.procesarTramaSubida(i, pkt));
      this.enlaces.push(enlace);
    }

    this.fisica.alSubirBytes((idMedio, bytes) => {
      if (idMedio >= 0 && idMedio < this.enlaces.length) {
        this.enlaces[idMedio].alimentarBytesRx(bytes);
      }
    });
  }

  private procesarTramaSubida(idMedio: number, pkt: BufferPaquete): void {
    const hdr = pkt.consumirCabecera(LimitesPila.TAMANO_CABECERA_RED);
    if (!hdr) return;

    const ttl = hdr[1];
    if (ttl === 0) return;

    pkt.idNodo = idMedio;
    for (const oyente of this.oyentesSubida) {
      oyente(pkt);
    }
  }

  public alSubirPaquete(callback: (pkt: BufferPaquete) => void): void {
    this.oyentesSubida.push(callback);
  }

  public bajarPaqueteUnicast(idDestino: number, pkt: BufferPaquete): boolean {
    if (idDestino < 0 || idDestino >= this.enlaces.length) return false;

    const hdr = pkt.empujarCabecera(LimitesPila.TAMANO_CABECERA_RED);
    if (!hdr) return false;

    const nodoBits = idDestino & 0x0f;
    hdr[0] = (nodoBits << 4) | nodoBits;
    hdr[1] = CapaRed.DEFAULT_TTL;

    return this.enlaces[idDestino].bajarTrama(pkt);
  }

  public bajarPaqueteBroadcast(pkt: BufferPaquete): void {
    const hdr = pkt.empujarCabecera(LimitesPila.TAMANO_CABECERA_RED);
    if (!hdr) return;

    hdr[0] = (0x00 << 4) | CapaRed.NODO_BROADCAST;
    hdr[1] = CapaRed.DEFAULT_TTL;

    for (let i = 0; i < this.enlaces.length; i++) {
      this.enlaces[i].bajarTrama(pkt);
    }
  }
}