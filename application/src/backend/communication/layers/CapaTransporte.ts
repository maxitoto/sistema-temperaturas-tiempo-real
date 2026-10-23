// Archivo: ./src/backend/communication/layers/CapaTransporte.ts
import { BufferPaquete, LimitesPila, calcularCRC16 } from "../common/BufferRed";
import { CapaRed } from "./CapaRed";

export interface SegmentoTransporte {
  idNodo: number;
  protocolo: number;
  puertoOrigen: number;
  puertoDestino: number;
  flags: number;
  numSecuencia: number;
  numAcuse: number;
}

export enum EstadoTCP {
  TCP_CERRADO = 0,
  TCP_SYN_ENVIADO = 1,
  TCP_SYN_RECIBIDO = 2,
  TCP_ESTABLECIDO = 3,
}

interface ElementoColaTx {
  puertoOrigen: number;
  puertoDestino: number;
  payloadSesion: Uint8Array;
}

interface ControlFlujoNodo {
  estado: EstadoTCP;
  seqLocal: number;
  seqRemotaEsperada: number;
  esperandoAck: boolean;
  seqPendienteAck: number;
  reintentos: number;
  timerRto: ReturnType<typeof setTimeout> | null;
  copiaSegmentoTx: Uint8Array | null;
  colaTx: ElementoColaTx[];
  resolverHandshake: (() => void) | null;
  rechazarHandshake: ((err: Error) => void) | null;
}

export class CapaTransporte {
  public static readonly PROTO_TCP = 0x06;
  public static readonly PROTO_UDP = 0x11;

  public static readonly FLAG_SYN = 0x01;
  public static readonly FLAG_ACK = 0x02;
  public static readonly FLAG_PSH = 0x04;
  public static readonly FLAG_FIN = 0x08;
  public static readonly FLAG_RST = 0x10;

  public static readonly PUERTO_RPC = 80;
  public static readonly PUERTO_EVENTOS = 81;

  public static readonly RTO_MS = 1000;
  public static readonly MAX_REINTENTOS = 3;

  private readonly nodos: ControlFlujoNodo[] = [];
  private oyentesSubida: Array<(pkt: BufferPaquete, meta: SegmentoTransporte) => void> = [];

  constructor(private red: CapaRed) {
    for (let i = 0; i < LimitesPila.MAX_NODOS_RED; i++) {
      this.nodos.push({
        estado: EstadoTCP.TCP_CERRADO,
        seqLocal: 1,
        seqRemotaEsperada: 0,
        esperandoAck: false,
        seqPendienteAck: 0,
        reintentos: 0,
        timerRto: null,
        copiaSegmentoTx: null,
        colaTx: [],
        resolverHandshake: null,
        rechazarHandshake: null,
      });
    }

    this.red.alSubirPaquete((pkt) => this.procesarPaquete(pkt));
  }

  public obtenerEstadoTCP(idNodo: number): EstadoTCP {
    if (idNodo < 0 || idNodo >= LimitesPila.MAX_NODOS_RED) return EstadoTCP.TCP_CERRADO;
    return this.nodos[idNodo].estado;
  }

  private limpiarTimerArq(ctrl: ControlFlujoNodo, vaciarCola = false): void {
    if (ctrl.timerRto) {
      clearTimeout(ctrl.timerRto);
      ctrl.timerRto = null;
    }
    ctrl.esperandoAck = false;
    ctrl.reintentos = 0;
    ctrl.copiaSegmentoTx = null;
    if (vaciarCola) {
      ctrl.colaTx = [];
    }
  }

  private escribirCabeceraYCRC(
    pkt: BufferPaquete,
    proto: number,
    ptoOrig: number,
    ptoDest: number,
    flags: number,
    seq: number,
    ack: number
  ): void {
    const payload = pkt.vistaActual();
    const lenPayload = payload.length;

    const hdr = pkt.empujarCabecera(LimitesPila.TAMANO_CABECERA_TRANS);
    if (!hdr) return;

    hdr[0] = proto;
    hdr[1] = ptoOrig;
    hdr[2] = ptoDest;
    hdr[3] = flags;
    hdr[4] = (seq >> 8) & 0xff;
    hdr[5] = seq & 0xff;
    hdr[6] = (ack >> 8) & 0xff;
    hdr[7] = ack & 0xff;

    let crc = calcularCRC16(hdr.subarray(0, 8), 8);
    if (lenPayload > 0) {
      crc = calcularCRC16(payload, lenPayload, crc);
    }
    hdr[8] = (crc >> 8) & 0xff;
    hdr[9] = crc & 0xff;
  }

  private enviarControlTCP(
    idNodo: number,
    ptoOrig: number,
    ptoDest: number,
    flags: number,
    seq: number,
    ack: number
  ): void {
    const pktCtrl = new BufferPaquete();
    pktCtrl.prepararParaTx(idNodo);
    this.escribirCabeceraYCRC(pktCtrl, CapaTransporte.PROTO_TCP, ptoOrig, ptoDest, flags, seq, ack);
    this.red.bajarPaqueteUnicast(idNodo, pktCtrl);
  }

  // Inicia el Three-Way Handshake TCP con reintento periódico de SYN cada 600ms
  public async conectarTCP(
    idNodo: number,
    puerto = CapaTransporte.PUERTO_RPC,
    timeoutMs = 3500
  ): Promise<void> {
    if (idNodo < 0 || idNodo >= LimitesPila.MAX_NODOS_RED) {
      throw new Error(`idNodo fuera de rango: ${idNodo}`);
    }
    const ctrl = this.nodos[idNodo];
    this.limpiarTimerArq(ctrl, true);

    return new Promise((resolve, reject) => {
      const seqInicial = ctrl.seqLocal & 0xffff;
      ctrl.seqLocal = (ctrl.seqLocal + 1) & 0xffff;
      ctrl.estado = EstadoTCP.TCP_SYN_ENVIADO;

      const retrySyn = setInterval(() => {
        if (ctrl.estado === EstadoTCP.TCP_SYN_ENVIADO) {
          this.enviarControlTCP(idNodo, puerto, puerto, CapaTransporte.FLAG_SYN, seqInicial, 0);
        }
      }, 1200);

      const timer = setTimeout(() => {
        clearInterval(retrySyn);
        ctrl.resolverHandshake = null;
        ctrl.rechazarHandshake = null;
        ctrl.estado = EstadoTCP.TCP_CERRADO;
        reject(new Error(`Timeout en Three-Way Handshake TCP (nodo ${idNodo})`));
      }, timeoutMs);

      ctrl.resolverHandshake = () => {
        clearInterval(retrySyn);
        clearTimeout(timer);
        ctrl.resolverHandshake = null;
        ctrl.rechazarHandshake = null;
        resolve();
      };

      ctrl.rechazarHandshake = (err: Error) => {
        clearInterval(retrySyn);
        clearTimeout(timer);
        ctrl.resolverHandshake = null;
        ctrl.rechazarHandshake = null;
        reject(err);
      };

      this.enviarControlTCP(idNodo, puerto, puerto, CapaTransporte.FLAG_SYN, seqInicial, 0);
    });
  }

  public alSubirSegmento(callback: (pkt: BufferPaquete, meta: SegmentoTransporte) => void): void {
    this.oyentesSubida.push(callback);
  }

  private transmitirSegmentoInmediato(
    idDestino: number,
    puertoOrigen: number,
    puertoDestino: number,
    pkt: BufferPaquete
  ): boolean {
    const ctrl = this.nodos[idDestino];
    const seqActual = ctrl.seqLocal & 0xffff;
    ctrl.seqLocal = (ctrl.seqLocal + 1) & 0xffff;
    const flags = CapaTransporte.FLAG_PSH | CapaTransporte.FLAG_ACK;

    this.escribirCabeceraYCRC(
      pkt,
      CapaTransporte.PROTO_TCP,
      puertoOrigen,
      puertoDestino,
      flags,
      seqActual,
      ctrl.seqRemotaEsperada
    );

    this.limpiarTimerArq(ctrl, false);
    ctrl.esperandoAck = true;
    ctrl.seqPendienteAck = (seqActual + 1) & 0xffff;
    ctrl.copiaSegmentoTx = pkt.vistaActual().slice();
    this.programarRetransmision(idDestino);

    return this.red.bajarPaqueteUnicast(idDestino, pkt);
  }

  private despacharSiguienteEnCola(idNodo: number): void {
    const ctrl = this.nodos[idNodo];
    if (ctrl.estado !== EstadoTCP.TCP_ESTABLECIDO || ctrl.esperandoAck || ctrl.colaTx.length === 0) {
      return;
    }
    const siguiente = ctrl.colaTx.shift()!;
    const pkt = new BufferPaquete();
    pkt.idNodo = idNodo;
    pkt.inicio = LimitesPila.OFFSET_SESION;
    pkt.longitud = siguiente.payloadSesion.length;
    pkt.raw.set(siguiente.payloadSesion, pkt.inicio);

    this.transmitirSegmentoInmediato(idNodo, siguiente.puertoOrigen, siguiente.puertoDestino, pkt);
  }

  private procesarPaquete(pkt: BufferPaquete): void {
    const id = pkt.idNodo;
    if (id < 0 || id >= LimitesPila.MAX_NODOS_RED) return;

    const hdr = pkt.consumirCabecera(LimitesPila.TAMANO_CABECERA_TRANS);
    if (!hdr) return;

    const proto = hdr[0];
    const ptoOrigen = hdr[1];
    const ptoDestino = hdr[2];
    const flags = hdr[3];
    const seq = (hdr[4] << 8) | hdr[5];
    const ack = (hdr[6] << 8) | hdr[7];
    const crcRecib = (hdr[8] << 8) | hdr[9];

    const payload = pkt.vistaActual();
    let crcCalc = calcularCRC16(hdr.subarray(0, 8), 8);
    if (payload.length > 0) {
      crcCalc = calcularCRC16(payload, payload.length, crcCalc);
    }
    if (crcCalc !== crcRecib) return;

    if (proto === CapaTransporte.PROTO_UDP) {
      if (payload.length === 0) return;
      const meta: SegmentoTransporte = {
        idNodo: id,
        protocolo: proto,
        puertoOrigen: ptoOrigen,
        puertoDestino: ptoDestino,
        flags,
        numSecuencia: seq,
        numAcuse: ack,
      };
      for (const oyente of this.oyentesSubida) {
        oyente(pkt, meta);
      }
      return;
    }

    if (proto === CapaTransporte.PROTO_TCP) {
      const ctrl = this.nodos[id];

      if ((flags & CapaTransporte.FLAG_RST) !== 0) {
        this.limpiarTimerArq(ctrl, true);
        ctrl.estado = EstadoTCP.TCP_CERRADO;
        if (ctrl.rechazarHandshake) {
          ctrl.rechazarHandshake(new Error("Conexión rechazada con FLAG_RST"));
        }
        return;
      }

      if (flags === CapaTransporte.FLAG_SYN) {
        this.limpiarTimerArq(ctrl, true);
        ctrl.estado = EstadoTCP.TCP_SYN_RECIBIDO;
        ctrl.seqRemotaEsperada = (seq + 1) & 0xffff;
        const miSeq = ctrl.seqLocal & 0xffff;
        ctrl.seqLocal = (ctrl.seqLocal + 1) & 0xffff;
        this.enviarControlTCP(
          id,
          ptoDestino,
          ptoOrigen,
          CapaTransporte.FLAG_SYN | CapaTransporte.FLAG_ACK,
          miSeq,
          ctrl.seqRemotaEsperada
        );
        return;
      }

      if (
        ctrl.estado === EstadoTCP.TCP_SYN_ENVIADO &&
        flags === (CapaTransporte.FLAG_SYN | CapaTransporte.FLAG_ACK)
      ) {
        ctrl.seqRemotaEsperada = (seq + 1) & 0xffff;
        ctrl.estado = EstadoTCP.TCP_ESTABLECIDO;
        this.enviarControlTCP(
          id,
          ptoDestino,
          ptoOrigen,
          CapaTransporte.FLAG_ACK,
          ctrl.seqLocal,
          ctrl.seqRemotaEsperada
        );
        if (ctrl.resolverHandshake) {
          ctrl.resolverHandshake();
        }
        return;
      }

      if ((flags & CapaTransporte.FLAG_FIN) !== 0) {
        this.limpiarTimerArq(ctrl, true);
        ctrl.estado = EstadoTCP.TCP_CERRADO;
        ctrl.seqRemotaEsperada = (seq + 1) & 0xffff;
        this.enviarControlTCP(
          id,
          ptoDestino,
          ptoOrigen,
          CapaTransporte.FLAG_ACK,
          ctrl.seqLocal,
          ctrl.seqRemotaEsperada
        );
        return;
      }

      if ((flags & CapaTransporte.FLAG_ACK) !== 0) {
        if (ctrl.estado === EstadoTCP.TCP_SYN_RECIBIDO) {
          ctrl.estado = EstadoTCP.TCP_ESTABLECIDO;
        }
        if (ctrl.esperandoAck && ack === ctrl.seqPendienteAck) {
          this.limpiarTimerArq(ctrl, false);
          this.despacharSiguienteEnCola(id);
        }
      }

      if (ctrl.estado !== EstadoTCP.TCP_ESTABLECIDO) {
        this.enviarControlTCP(id, ptoDestino, ptoOrigen, CapaTransporte.FLAG_RST, 0, 0);
        return;
      }

      if ((flags & CapaTransporte.FLAG_PSH) !== 0 && payload.length > 0) {
        if (seq === ctrl.seqRemotaEsperada) {
          ctrl.seqRemotaEsperada = (seq + 1) & 0xffff;
          this.enviarControlTCP(
            id,
            ptoDestino,
            ptoOrigen,
            CapaTransporte.FLAG_ACK,
            ctrl.seqLocal,
            ctrl.seqRemotaEsperada
          );

          const meta: SegmentoTransporte = {
            idNodo: id,
            protocolo: proto,
            puertoOrigen: ptoOrigen,
            puertoDestino: ptoDestino,
            flags,
            numSecuencia: seq,
            numAcuse: ack,
          };
          for (const oyente of this.oyentesSubida) {
            oyente(pkt, meta);
          }
        } else {
          this.enviarControlTCP(
            id,
            ptoDestino,
            ptoOrigen,
            CapaTransporte.FLAG_ACK,
            ctrl.seqLocal,
            ctrl.seqRemotaEsperada
          );
        }
      }
    }
  }

  public bajarSegmentoUnicast(
    idDestino: number,
    protocolo: number,
    puertoOrigen: number,
    puertoDestino: number,
    pkt: BufferPaquete
  ): boolean {
    if (idDestino < 0 || idDestino >= LimitesPila.MAX_NODOS_RED) return false;

    if (protocolo === CapaTransporte.PROTO_UDP) {
      this.escribirCabeceraYCRC(pkt, CapaTransporte.PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0);
      return this.red.bajarPaqueteUnicast(idDestino, pkt);
    }

    const ctrl = this.nodos[idDestino];
    if (ctrl.estado !== EstadoTCP.TCP_ESTABLECIDO) return false;

    // Si ya hay un segmento esperando ACK, lo encolamos para no sobrescribir copiaSegmentoTx
    if (ctrl.esperandoAck) {
      ctrl.colaTx.push({
        puertoOrigen,
        puertoDestino,
        payloadSesion: pkt.vistaActual().slice(),
      });
      return true;
    }

    return this.transmitirSegmentoInmediato(idDestino, puertoOrigen, puertoDestino, pkt);
  }

  public bajarDatagramaBroadcast(puertoOrigen: number, puertoDestino: number, pkt: BufferPaquete): void {
    this.escribirCabeceraYCRC(pkt, CapaTransporte.PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0);
    this.red.bajarPaqueteBroadcast(pkt);
  }

  private programarRetransmision(idNodo: number): void {
    const ctrl = this.nodos[idNodo];
    ctrl.timerRto = setTimeout(() => {
      if (!ctrl.esperandoAck || ctrl.estado !== EstadoTCP.TCP_ESTABLECIDO || !ctrl.copiaSegmentoTx) {
        return;
      }
      if (ctrl.reintentos < CapaTransporte.MAX_REINTENTOS) {
        ctrl.reintentos++;
        const pktReenvio = new BufferPaquete();
        pktReenvio.idNodo = idNodo;
        pktReenvio.inicio = LimitesPila.OFFSET_TRANSPORTE;
        pktReenvio.longitud = ctrl.copiaSegmentoTx.length;
        pktReenvio.raw.set(ctrl.copiaSegmentoTx, pktReenvio.inicio);

        this.red.bajarPaqueteUnicast(idNodo, pktReenvio);
        this.programarRetransmision(idNodo);
      } else {
        this.limpiarTimerArq(ctrl, true);
        ctrl.estado = EstadoTCP.TCP_CERRADO;
      }
    }, CapaTransporte.RTO_MS);
  }
}