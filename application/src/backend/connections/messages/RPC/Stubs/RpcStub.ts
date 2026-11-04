import {
  Flags,
  hashEndpoint,
  MsgPack,
  PacketType,
  RpcEnvelope,
  RpcFlow,
  RpcSemantics,
} from "../DTOs/RpcDto";
import { MediumType } from "../../../proxy/ITransport";
import { TransportProxy } from "../../../proxy/TransportProxy";

export type RawCallback = (payload: Uint8Array, isEos: boolean) => void;
export type ErrorCallback = (msg: string) => void;

export interface RpcStubConfig {
  retryTimeoutMs: number;
  maxRpcRetries: number;
}

interface PendingTicket {
  clientId: number;
  ticketId: number;
  streamId: number;
  semantics: RpcSemantics;
  rawPacket: Uint8Array;
  lastSentMs: number;
  timeoutMs: number;
  retriesLeft: number;
  onData?: RawCallback;
  onError?: ErrorCallback;
}

export class RpcStub {
  private pendingTickets_ = new Map<number, PendingTicket>();
  private streamListeners_ = new Map<
    number,
    { clientId: number; onData: RawCallback; onError?: ErrorCallback }
  >();
  private nextTicketId_ = 0;
  private nextStreamId_ = 0;
  private cfg_: RpcStubConfig = {
    retryTimeoutMs: 2500,
    maxRpcRetries: 2,
  };

  constructor(private proxy_: TransportProxy) {
    // T04 / T16: abortar al instante los tickets del medio desconectado informando el motivo exacto
    this.proxy_.onMediumDisconnected((disconnectedClientId, _sessionId, medium, reason) => {
      const msg = `Medio ${MediumType[medium]} desconectado durante llamada RPC (motivo=${reason})`;
      for (const [ticketId, ticket] of this.pendingTickets_.entries()) {
        if (ticket.clientId === disconnectedClientId) {
          if (ticket.onError) ticket.onError(msg);
          this.pendingTickets_.delete(ticketId);
        }
      }
      for (const [sid, st] of this.streamListeners_.entries()) {
        if (st.clientId === disconnectedClientId) {
          if (st.onError) st.onError(msg);
          this.streamListeners_.delete(sid);
        }
      }
    });
  }

  public loadConfig(): void {
    // cargar de base de datos
    this.cfg_ = {
      retryTimeoutMs: 2500,
      maxRpcRetries: 2,
    };
  }

  public applyConfig(inCfg: Partial<RpcStubConfig>): boolean {
    const merged: RpcStubConfig = { ...this.cfg_, ...inCfg };
    if (
      merged.retryTimeoutMs < 200 ||
      merged.retryTimeoutMs > 30_000 ||
      merged.maxRpcRetries < 0 ||
      merged.maxRpcRetries > 10
    ) {
      return false;
    }
    // zona base de datos
    this.cfg_ = merged;
    return true;
  }

  public getConfig(): Readonly<RpcStubConfig> {
    return this.cfg_;
  }

  public hasActiveOperations(clientId: number): boolean {
    for (const ticket of this.pendingTickets_.values()) {
      if (ticket.clientId === clientId) return true;
    }
    for (const st of this.streamListeners_.values()) {
      if (st.clientId === clientId) return true;
    }
    return false;
  }

  public invoke(
    clientId: number,
    endpointName: string,
    flow: RpcFlow,
    semantics: RpcSemantics,
    payload: Uint8Array,
    isEos: boolean,
    onData?: RawCallback,
    onError?: ErrorCallback,
    existingStreamId = 0,
    customTimeoutMs?: number
  ): number {
    this.nextTicketId_ = (this.nextTicketId_ + 1) >>> 0;
    const ticketId = this.nextTicketId_;

    // H-03 / T06: exigir clientId explícito (prohibido clientId <= 0)
    if (clientId <= 0) {
      if (onError) {
        queueMicrotask(() =>
          onError("Se requiere una sesión/clientId explícito (prohibido clientId = 0)")
        );
      }
      return ticketId;
    }

    const streamId =
      existingStreamId !== 0
        ? existingStreamId
        : (this.nextStreamId_ = (this.nextStreamId_ + 1) >>> 0);

    const pktType =
      flow === RpcFlow.Unary || existingStreamId === 0
        ? PacketType.Request
        : PacketType.StreamChunk;
    const isRt = semantics === RpcSemantics.Maybe && pktType !== PacketType.Request;

    const env: RpcEnvelope = {
      type: pktType,
      flow,
      semantics,
      flags: (isEos ? Flags.EOS : Flags.NONE) | (isRt ? Flags.REAL_TIME : Flags.NONE),
      callId: ticketId,
      streamId,
      methodId: hashEndpoint(endpointName),
      payload,
    };

    const raw = MsgPack.packEnvelope(env);
    const routed = this.proxy_.routeToClient(clientId, raw, isRt);
    if (!routed) {
      const detail = this.proxy_.explainClientDisconnect(clientId);
      if (onError) {
        queueMicrotask(() => onError(detail));
      }
      return ticketId;
    }

    if (onData || onError) {
      this.pendingTickets_.set(ticketId, {
        clientId,
        ticketId,
        streamId,
        semantics,
        rawPacket: raw,
        lastSentMs: Date.now(),
        timeoutMs: customTimeoutMs ?? this.cfg_.retryTimeoutMs,
        retriesLeft: semantics === RpcSemantics.Maybe ? 0 : this.cfg_.maxRpcRetries,
        onData,
        onError,
      });
    }

    if (flow !== RpcFlow.Unary && onData) {
      this.streamListeners_.set(streamId, { clientId, onData, onError });
    }

    return ticketId;
  }

  public onIncomingResponse(sourceClientId: number, env: RpcEnvelope): void {
    if (env.type === PacketType.StreamChunk || env.type === PacketType.StreamEnd) {
      const st = this.streamListeners_.get(env.streamId);
      if (!st) return;
      // H-03 / T06: descartar y registrar si el stream llega por un medio distinto al que lo abrió
      if (st.clientId !== sourceClientId) {
        console.warn(
          `[RPC-STUB] Stream #${env.streamId} descartado: esperado clientId=${st.clientId}, recibido de clientId=${sourceClientId}`
        );
        return;
      }
      const eos = env.type === PacketType.StreamEnd || (env.flags & Flags.EOS) !== 0;
      st.onData(env.payload, eos);
      if (eos) this.streamListeners_.delete(env.streamId);
      this.pendingTickets_.delete(env.callId);
      return;
    }

    const ticket = this.pendingTickets_.get(env.callId);
    if (!ticket) return;

    // H-03 / T06: validar que la respuesta provenga exactamente del mismo medio que emitió el ticket
    if (ticket.clientId !== sourceClientId) {
      console.warn(
        `[RPC-STUB] Ticket #${env.callId} descartado por cruce de medio: esperado clientId=${ticket.clientId}, recibido de clientId=${sourceClientId}`
      );
      return;
    }

    this.pendingTickets_.delete(env.callId);
    if (env.type === PacketType.Error) {
      if (ticket.onError) {
        const errMsg = MsgPack.deserialize<string>(env.payload) ?? "Error RPC en ESP32";
        ticket.onError(errMsg);
      }
    } else if (ticket.onData) {
      ticket.onData(env.payload, true);
    }
  }

  public pollRetries(): void {
    const now = Date.now();
    for (const [ticketId, ticket] of this.pendingTickets_.entries()) {
      if (now - ticket.lastSentMs >= ticket.timeoutMs) {
        if (ticket.retriesLeft > 0) {
          ticket.retriesLeft--;
          ticket.lastSentMs = now;
          if (!this.proxy_.routeToClient(ticket.clientId, ticket.rawPacket, false)) {
            if (ticket.onError) {
              ticket.onError(this.proxy_.explainClientDisconnect(ticket.clientId));
            }
            this.pendingTickets_.delete(ticketId);
          }
        } else {
          if (ticket.onError) {
            ticket.onError(`RPC Timeout agotado (Ticket #${ticketId}, clientId=${ticket.clientId})`);
          }
          this.pendingTickets_.delete(ticketId);
        }
      }
    }
  }
}