// Archivo: ./src/backend/communication/layers/CapaSesion.ts
import { createHash, randomInt } from "crypto";
import { BufferPaquete, LimitesPila } from "../common/BufferRed";
import { CapaTransporte, EstadoTCP, SegmentoTransporte } from "./CapaTransporte";

export interface MensajeSesion {
  idNodo: number;
  tipo: number;
  sessionId: number;
  reqId: number;
  payloadLen: number;
  fiable: boolean;
}

export enum EstadoAuth {
  NO_AUTENTICADO = 0,
  ESPERANDO_DESAFIO = 1,
  AUTENTICADO = 2,
}

interface EstadoNodoSesion {
  estado: EstadoAuth;
  sessionId: number;
  nonceActual: number;
  ultimoLatido: number;
}

interface PeticionPendiente {
  resolve: (pkt: BufferPaquete) => void;
  reject: (err: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class CapaSesion {
  public static readonly VERSION_PROTOCOLO = 0x01;

  public static readonly TIPO_HEARTBEAT = 0x01;
  public static readonly TIPO_HANDSHAKE_INIT = 0x02;
  public static readonly TIPO_HANDSHAKE_CHAL = 0x03;
  public static readonly TIPO_HANDSHAKE_AUTH = 0x04;
  public static readonly TIPO_HANDSHAKE_OK = 0x05;
  public static readonly TIPO_RPC_REQ = 0x10;
  public static readonly TIPO_RPC_RES = 0x11;
  public static readonly TIPO_EVENTO = 0x20;
  public static readonly TIPO_ERROR_SESION = 0xff;

  public static readonly ERR_NO_AUTORIZADO = 0x01;
  public static readonly ERR_AUTH_FALLIDA = 0x02;
  public static readonly ERR_SOBRE_INVALIDO = 0x03;

  private readonly nodos: EstadoNodoSesion[] = [];
  private contadorTickets = 1;

  private readonly pendientes: Map<number, PeticionPendiente> = new Map();
  private oyentesSubida: Array<(pkt: BufferPaquete, meta: MensajeSesion) => void> = [];

  constructor(
    private transporte: CapaTransporte,
    private secretoCompartido: string,
    private timeoutMs = 15000
  ) {
    for (let i = 0; i < LimitesPila.MAX_NODOS_RED; i++) {
      this.nodos.push({
        estado: EstadoAuth.NO_AUTENTICADO,
        sessionId: 0,
        nonceActual: 0,
        ultimoLatido: 0,
      });
    }

    this.transporte.alSubirSegmento((pkt, metaTrans) => this.procesarSegmento(pkt, metaTrans));
  }

  private claveTicket(idNodo: number, reqId: number): number {
    return ((idNodo & 0xff) << 16) | (reqId & 0xffff);
  }

  public siguienteTicket(): number {
    this.contadorTickets = (this.contadorTickets % 65534) + 1;
    return this.contadorTickets;
  }

  public estaAutenticada(idNodo = 0): boolean {
    if (idNodo < 0 || idNodo >= LimitesPila.MAX_NODOS_RED) return false;
    return (
      this.nodos[idNodo].estado === EstadoAuth.AUTENTICADO &&
      this.nodos[idNodo].sessionId !== 0 &&
      this.transporte.obtenerEstadoTCP(idNodo) === EstadoTCP.TCP_ESTABLECIDO
    );
  }

  public establecerSecreto(nuevoSecreto: string): void {
    if (nuevoSecreto && nuevoSecreto.length > 0) {
      this.secretoCompartido = nuevoSecreto;
    }
  }

  private calcularHashDesafio(nonce: number): Buffer {
    const bytesNonce = Buffer.allocUnsafe(4);
    bytesNonce.writeUInt32BE(nonce >>> 0, 0);
    return createHash("sha256")
      .update(bytesNonce)
      .update(Buffer.from(this.secretoCompartido, "utf-8"))
      .digest();
  }

  private empujarSobreYEnviar(
    idNodo: number,
    tipo: number,
    sesId: number,
    reqId: number,
    pkt: BufferPaquete,
    protocolo = CapaTransporte.PROTO_TCP,
    puerto = CapaTransporte.PUERTO_RPC
  ): boolean {
    if (pkt.longitud > LimitesPila.MAX_PAYLOAD_APP) return false;
    const lenPayload = pkt.longitud;

    const sobre = pkt.empujarCabecera(LimitesPila.TAMANO_SOBRE_SESION);
    if (!sobre) return false;

    sobre[0] = CapaSesion.VERSION_PROTOCOLO;
    sobre[1] = tipo;
    sobre[2] = (sesId >> 8) & 0xff;
    sobre[3] = sesId & 0xff;
    sobre[4] = (reqId >> 8) & 0xff;
    sobre[5] = reqId & 0xff;
    sobre[6] = (lenPayload >> 8) & 0xff;
    sobre[7] = lenPayload & 0xff;

    return this.transporte.bajarSegmentoUnicast(idNodo, protocolo, puerto, puerto, pkt);
  }

  private enviarControlSesion(
    idNodo: number,
    tipo: number,
    sesId: number,
    reqId: number,
    datos?: Uint8Array
  ): boolean {
    const pkt = new BufferPaquete();
    pkt.prepararParaTx(idNodo);
    if (datos && datos.length > 0) {
      pkt.escribirPayloadTx(datos);
    }
    return this.empujarSobreYEnviar(idNodo, tipo, sesId, reqId, pkt, CapaTransporte.PROTO_TCP);
  }

  public async iniciarSesion(idNodo: number): Promise<number> {
    if (idNodo < 0 || idNodo >= LimitesPila.MAX_NODOS_RED) {
      throw new Error(`idNodo inválido: ${idNodo}`);
    }
    await this.transporte.conectarTCP(idNodo);

    const pktInit = new BufferPaquete();
    pktInit.prepararParaTx(idNodo);
    const pktChal = await this.enviarConTicket(idNodo, CapaSesion.TIPO_HANDSHAKE_INIT, 0, pktInit);
    const nonceBytes = pktChal.vistaActual();
    if (nonceBytes.length !== 4) {
      throw new Error("Nonce inválido recibido del servidor");
    }

    const nonce =
      ((nonceBytes[0] << 24) | (nonceBytes[1] << 16) | (nonceBytes[2] << 8) | nonceBytes[3]) >>> 0;

    const hash = this.calcularHashDesafio(nonce);
    const pktAuth = new BufferPaquete();
    pktAuth.prepararParaTx(idNodo);
    pktAuth.escribirPayloadTx(new Uint8Array(hash));

    await this.enviarConTicket(idNodo, CapaSesion.TIPO_HANDSHAKE_AUTH, 0, pktAuth);
    return this.nodos[idNodo].sessionId;
  }

  // Permite elegir fiable = true (PROTO_TCP con ARQ) o fiable = false (PROTO_UDP en tiempo real)
  public async enviarPeticionRPC(
    idNodo: number,
    pkt: BufferPaquete,
    timeoutMs = 5000,
    fiable = true
  ): Promise<BufferPaquete> {
    if (!this.estaAutenticada(idNodo)) {
      throw new Error(`Nodo ${idNodo} no autenticado. Ejecute iniciarSesion(${idNodo}) primero.`);
    }
    return this.enviarConTicket(
      idNodo,
      CapaSesion.TIPO_RPC_REQ,
      this.nodos[idNodo].sessionId,
      pkt,
      timeoutMs,
      fiable
    );
  }

  public bajarRespuesta(idNodo: number, reqId: number, pkt: BufferPaquete, fiable = true): boolean {
    if (!this.estaAutenticada(idNodo)) return false;
    const proto = fiable ? CapaTransporte.PROTO_TCP : CapaTransporte.PROTO_UDP;
    return this.empujarSobreYEnviar(
      idNodo,
      CapaSesion.TIPO_RPC_RES,
      this.nodos[idNodo].sessionId,
      reqId,
      pkt,
      proto
    );
  }

  public bajarEvento(idNodo: number, pkt: BufferPaquete, fiable = true): boolean {
    if (!this.estaAutenticada(idNodo)) return false;
    const proto = fiable ? CapaTransporte.PROTO_TCP : CapaTransporte.PROTO_UDP;
    const puerto = fiable ? CapaTransporte.PUERTO_RPC : CapaTransporte.PUERTO_EVENTOS;
    return this.empujarSobreYEnviar(
      idNodo,
      CapaSesion.TIPO_EVENTO,
      this.nodos[idNodo].sessionId,
      0,
      pkt,
      proto,
      puerto
    );
  }

  public tienePeticionesEnVuelo(idNodo: number): boolean {
    for (const clave of this.pendientes.keys()) {
      if ((clave >>> 16) === idNodo) return true;
    }
    return false;
  }

  public async enviarLatido(idNodo: number): Promise<void> {
    if (!this.estaAutenticada(idNodo)) return;

    const inactivoMs = Date.now() - this.nodos[idNodo].ultimoLatido;
    if (this.tienePeticionesEnVuelo(idNodo) || inactivoMs < 3500) {
      return;
    }

    const pkt = new BufferPaquete();
    pkt.prepararParaTx(idNodo);
    await this.enviarConTicket(
      idNodo,
      CapaSesion.TIPO_HEARTBEAT,
      this.nodos[idNodo].sessionId,
      pkt,
      2500,
      true
    );
  }

  public alSubirSesion(callback: (pkt: BufferPaquete, meta: MensajeSesion) => void): void {
    this.oyentesSubida.push(callback);
  }

  private enviarConTicket(
    idNodo: number,
    tipo: number,
    sesId: number,
    pkt: BufferPaquete,
    timeoutMs = 5000,
    fiable = true
  ): Promise<BufferPaquete> {
    const reqId = this.siguienteTicket();
    const clave = this.claveTicket(idNodo, reqId);
    const proto = fiable ? CapaTransporte.PROTO_TCP : CapaTransporte.PROTO_UDP;

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendientes.delete(clave);
        reject(new Error(`Timeout esperando respuesta para nodo=${idNodo} req_id=${reqId}`));
      }, timeoutMs);

      this.pendientes.set(clave, { resolve, reject, timer });

      if (!this.empujarSobreYEnviar(idNodo, tipo, sesId, reqId, pkt, proto)) {
        clearTimeout(timer);
        this.pendientes.delete(clave);
        reject(new Error("Fallo al transmitir sobre en la Capa de Transporte"));
      }
    });
  }

  private procesarSegmento(pkt: BufferPaquete, metaTrans: SegmentoTransporte): void {
    const id = pkt.idNodo;
    if (id < 0 || id >= LimitesPila.MAX_NODOS_RED) return;

    const sobre = pkt.consumirCabecera(LimitesPila.TAMANO_SOBRE_SESION);
    if (!sobre) return;

    const ver = sobre[0];
    const tipo = sobre[1];
    const sesIdRecibido = (sobre[2] << 8) | sobre[3];
    const reqId = (sobre[4] << 8) | sobre[5];
    const payloadLen = (sobre[6] << 8) | sobre[7];
    const esFiable = metaTrans.protocolo === CapaTransporte.PROTO_TCP;

    if (ver !== CapaSesion.VERSION_PROTOCOLO || payloadLen > pkt.longitud) {
      return;
    }

    pkt.longitud = payloadLen;
    const cli = this.nodos[id];

    if (tipo === CapaSesion.TIPO_HANDSHAKE_INIT) {
      cli.nonceActual = randomInt(100000, 0x7fffffff);
      cli.estado = EstadoAuth.ESPERANDO_DESAFIO;
      cli.sessionId = 0;
      const nonceBuf = new Uint8Array([
        (cli.nonceActual >> 24) & 0xff,
        (cli.nonceActual >> 16) & 0xff,
        (cli.nonceActual >> 8) & 0xff,
        cli.nonceActual & 0xff,
      ]);
      this.enviarControlSesion(id, CapaSesion.TIPO_HANDSHAKE_CHAL, 0, reqId, nonceBuf);
      return;
    }

    if (tipo === CapaSesion.TIPO_HANDSHAKE_AUTH) {
      const payload = pkt.vistaActual();
      if (cli.estado !== EstadoAuth.ESPERANDO_DESAFIO || payloadLen !== 32) {
        this.enviarControlSesion(
          id,
          CapaSesion.TIPO_ERROR_SESION,
          0,
          reqId,
          new Uint8Array([CapaSesion.ERR_AUTH_FALLIDA])
        );
        return;
      }
      const esperado = this.calcularHashDesafio(cli.nonceActual);
      if (Buffer.from(payload).equals(esperado)) {
        cli.estado = EstadoAuth.AUTENTICADO;
        cli.sessionId = randomInt(1, 65534);
        cli.ultimoLatido = Date.now();
        this.enviarControlSesion(id, CapaSesion.TIPO_HANDSHAKE_OK, cli.sessionId, reqId);
      } else {
        cli.estado = EstadoAuth.NO_AUTENTICADO;
        this.enviarControlSesion(
          id,
          CapaSesion.TIPO_ERROR_SESION,
          0,
          reqId,
          new Uint8Array([CapaSesion.ERR_AUTH_FALLIDA])
        );
      }
      return;
    }

    if (tipo === CapaSesion.TIPO_RPC_REQ || tipo === CapaSesion.TIPO_EVENTO) {
      if (
        cli.estado !== EstadoAuth.AUTENTICADO ||
        sesIdRecibido === 0 ||
        sesIdRecibido !== cli.sessionId
      ) {
        this.enviarControlSesion(
          id,
          CapaSesion.TIPO_ERROR_SESION,
          0,
          reqId,
          new Uint8Array([CapaSesion.ERR_NO_AUTORIZADO])
        );
        return;
      }
      cli.ultimoLatido = Date.now();
      const meta: MensajeSesion = {
        idNodo: id,
        tipo,
        sessionId: sesIdRecibido,
        reqId,
        payloadLen,
        fiable: esFiable,
      };
      for (const oyente of this.oyentesSubida) {
        oyente(pkt, meta);
      }
      return;
    }

    const clave = this.claveTicket(id, reqId);
    const pendiente = this.pendientes.get(clave);
    if (!pendiente) return;

    clearTimeout(pendiente.timer);
    this.pendientes.delete(clave);

    if (tipo === CapaSesion.TIPO_ERROR_SESION) {
      const vista = pkt.vistaActual();
      const codigoError = vista.length > 0 ? vista[0] : 0xff;
      pendiente.reject(new Error(`Error de Sesión en Nodo ${id} (Código: 0x${codigoError.toString(16)})`));
      return;
    }

    if (tipo === CapaSesion.TIPO_HANDSHAKE_OK) {
      cli.estado = EstadoAuth.AUTENTICADO;
      cli.sessionId = sesIdRecibido;
      cli.ultimoLatido = Date.now();
    } else if (cli.estado === EstadoAuth.AUTENTICADO) {
      cli.ultimoLatido = Date.now();
    }

    const esRespuestaValida =
      tipo === CapaSesion.TIPO_HANDSHAKE_CHAL ||
      tipo === CapaSesion.TIPO_HANDSHAKE_OK ||
      tipo === CapaSesion.TIPO_RPC_RES ||
      tipo === CapaSesion.TIPO_HEARTBEAT;

    if (!esRespuestaValida) {
      pendiente.reject(new Error(`Tipo de sobre inesperado en Capa de Sesión: 0x${tipo.toString(16)}`));
      return;
    }

    pendiente.resolve(pkt);
  }

  public auditarSesiones(): void {
    const ahora = Date.now();
    for (let i = 0; i < LimitesPila.MAX_NODOS_RED; i++) {
      if (
        this.nodos[i].estado === EstadoAuth.AUTENTICADO &&
        ahora - this.nodos[i].ultimoLatido > this.timeoutMs
      ) {
        this.nodos[i].estado = EstadoAuth.NO_AUTENTICADO;
        this.nodos[i].sessionId = 0;
      }
    }
  }

  public cerrarSesionLocal(idNodo?: number): void {
    if (typeof idNodo === "number" && idNodo >= 0 && idNodo < LimitesPila.MAX_NODOS_RED) {
      this.nodos[idNodo].estado = EstadoAuth.NO_AUTENTICADO;
      this.nodos[idNodo].sessionId = 0;
    } else {
      for (const n of this.nodos) {
        n.estado = EstadoAuth.NO_AUTENTICADO;
        n.sessionId = 0;
      }
    }
    for (const [clave, pend] of this.pendientes) {
      if (typeof idNodo !== "number" || (clave >>> 16) === idNodo) {
        clearTimeout(pend.timer);
        pend.reject(new Error("Sesión cerrada localmente"));
        this.pendientes.delete(clave);
      }
    }
  }
}