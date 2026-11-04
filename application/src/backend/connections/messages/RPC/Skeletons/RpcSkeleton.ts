import { Flags, MsgPack, PacketType, RpcEnvelope, RpcFlow, RpcSemantics } from "../DTOs/RpcDto";
import { TransportProxy } from "../../../proxy/TransportProxy";

export interface RpcSkeletonConfig {
  maxHistoryEntries: number;
  historyTtlMs: number;
}

interface HistoryEntry {
  clientId: number;
  inProgress: boolean;
  rawResponse?: Uint8Array;
  createdAtMs: number;
}

export class StreamContext<TIn, TOut> {
  constructor(
    private clientId_: number,
    private env_: RpcEnvelope,
    private proxy_: TransportProxy
  ) {}

  public clientId(): number {
    return this.clientId_;
  }

  public streamId(): number {
    return this.env_.streamId;
  }

  public isEndOfStream(): boolean {
    return (this.env_.flags & Flags.EOS) !== 0;
  }

  public request(): TIn {
    const unpacked = MsgPack.deserialize<unknown>(this.env_.payload);
    if (Array.isArray(unpacked) && unpacked.length === 1) {
      return unpacked[0] as TIn;
    }
    return unpacked as TIn;
  }

  public emit(item: TOut, realTimeOverride = false): boolean {
    const useRt =
      realTimeOverride ||
      this.env_.semantics === RpcSemantics.Maybe ||
      (this.env_.flags & Flags.REAL_TIME) !== 0;

    const outEnv: RpcEnvelope = {
      type: PacketType.StreamChunk,
      flow: this.env_.flow,
      semantics: this.env_.semantics,
      flags: useRt ? Flags.REAL_TIME : Flags.NONE,
      callId: this.env_.callId,
      streamId: this.env_.streamId,
      methodId: this.env_.methodId,
      payload: MsgPack.serialize(item),
    };

    const raw = MsgPack.packEnvelope(outEnv);
    return this.proxy_.routeToClient(this.clientId_, raw, useRt);
  }

  public close(finalResponse?: TOut): boolean {
    const outEnv: RpcEnvelope = {
      type: PacketType.StreamEnd,
      flow: this.env_.flow,
      semantics: this.env_.semantics,
      flags: Flags.EOS,
      callId: this.env_.callId,
      streamId: this.env_.streamId,
      methodId: this.env_.methodId,
      payload: finalResponse !== undefined ? MsgPack.serialize(finalResponse) : new Uint8Array(0),
    };

    const raw = MsgPack.packEnvelope(outEnv);
    return this.proxy_.routeToClient(this.clientId_, raw, false);
  }
}

export type RawHandler = (clientId: number, env: RpcEnvelope) => Promise<Uint8Array> | Uint8Array;

export class RpcSkeleton {
  private routes_ = new Map<
    number,
    { flow: RpcFlow; semantics: RpcSemantics; handler: RawHandler }
  >();
  private responseHistory_ = new Map<string, HistoryEntry>();
  private cfg_: RpcSkeletonConfig = {
    maxHistoryEntries: 64,
    historyTtlMs: 60_000,
  };

  constructor(private proxy_: TransportProxy) {
    // H-04 / T10: limpiar historial AtMostOnce cuando la sesión de ese medio cae o se cierra
    this.proxy_.onMediumDisconnected((disconnectedClientId) => {
      this.clearClientSession(disconnectedClientId);
    });
  }

  public loadConfig(): void {
    // cargar de base de datos
    this.cfg_ = {
      maxHistoryEntries: 64,
      historyTtlMs: 60_000,
    };
  }

  public applyConfig(inCfg: Partial<RpcSkeletonConfig>): boolean {
    const merged: RpcSkeletonConfig = { ...this.cfg_, ...inCfg };
    if (
      merged.maxHistoryEntries < 8 ||
      merged.maxHistoryEntries > 512 ||
      merged.historyTtlMs < 1000
    ) {
      return false;
    }
    // zona base de datos
    this.cfg_ = merged;
    return true;
  }

  public getConfig(): Readonly<RpcSkeletonConfig> {
    return this.cfg_;
  }

  public clearClientSession(clientId: number): void {
    for (const [k, entry] of this.responseHistory_.entries()) {
      if (entry.clientId === clientId) {
        this.responseHistory_.delete(k);
      }
    }
  }

  public bindRaw(
    methodId: number,
    flow: RpcFlow,
    defaultSemantics: RpcSemantics,
    handler: RawHandler
  ): void {
    this.routes_.set(methodId >>> 0, { flow, semantics: defaultSemantics, handler });
  }

  public async onIncomingPacket(
    clientId: number,
    sessionId: number,
    env: RpcEnvelope
  ): Promise<void> {
    const route = this.routes_.get(env.methodId >>> 0);
    if (!route) {
      this.sendError(clientId, env, "Endpoint no registrado en Electron");
      return;
    }

    const now = Date.now();
    for (const [k, entry] of this.responseHistory_.entries()) {
      if (now >= entry.createdAtMs && now - entry.createdAtMs > this.cfg_.historyTtlMs) {
        this.responseHistory_.delete(k);
      }
    }

    // H-04 / T10: clave aislada por (clientId, sessionId, callId, methodId) + marcador inProgress
    const dedupKey = `${clientId}:${sessionId}:${env.callId}:${env.methodId >>> 0}`;
    if (env.semantics === RpcSemantics.AtMostOnce && env.type === PacketType.Request) {
      const existing = this.responseHistory_.get(dedupKey);
      if (existing) {
        if (!existing.inProgress && existing.rawResponse) {
          this.proxy_.routeToClient(clientId, existing.rawResponse, false);
        }
        return;
      }
      if (this.responseHistory_.size >= this.cfg_.maxHistoryEntries) {
        const oldestKey = this.responseHistory_.keys().next().value;
        if (oldestKey !== undefined) this.responseHistory_.delete(oldestKey);
      }
      this.responseHistory_.set(dedupKey, {
        clientId,
        inProgress: true,
        createdAtMs: now,
      });
    }

    try {
      const resultPayload = await route.handler(clientId, env);

      if (
        env.flow === RpcFlow.Unary ||
        (env.flow === RpcFlow.ClientStream && (env.flags & Flags.EOS) !== 0)
      ) {
        const resp: RpcEnvelope = {
          type: PacketType.Response,
          flow: env.flow,
          semantics: env.semantics,
          flags: Flags.EOS,
          callId: env.callId,
          streamId: env.streamId,
          methodId: env.methodId,
          payload: resultPayload,
        };

        const isRt =
          env.semantics === RpcSemantics.Maybe || (env.flags & Flags.REAL_TIME) !== 0;
        const rawResp = MsgPack.packEnvelope(resp);

        if (env.semantics === RpcSemantics.AtMostOnce) {
          this.responseHistory_.set(dedupKey, {
            clientId,
            inProgress: false,
            rawResponse: rawResp,
            createdAtMs: Date.now(),
          });
        }

        this.proxy_.routeToClient(clientId, rawResp, isRt);
      }
    } catch (err) {
      if (env.semantics === RpcSemantics.AtMostOnce) {
        this.responseHistory_.delete(dedupKey);
      }
      this.sendError(clientId, env, err instanceof Error ? err.message : String(err));
    }
  }

  private sendError(clientId: number, req: RpcEnvelope, msg: string): void {
    const errEnv: RpcEnvelope = {
      type: PacketType.Error,
      flow: req.flow,
      semantics: req.semantics,
      flags: Flags.EOS,
      callId: req.callId,
      streamId: req.streamId,
      methodId: req.methodId,
      payload: MsgPack.serialize(msg),
    };
    this.proxy_.routeToClient(clientId, MsgPack.packEnvelope(errEnv), false);
  }
}