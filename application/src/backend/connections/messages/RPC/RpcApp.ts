import { hashEndpoint, MsgPack, PacketType, RpcFlow, RpcSemantics } from "./DTOs/RpcDto";
import { RpcSkeleton, RpcSkeletonConfig, StreamContext } from "./Skeletons/RpcSkeleton";
import { RpcStub, RpcStubConfig } from "./Stubs/RpcStub";
import { SessionHandle } from "../../proxy/ITransport";
import { TransportProxy } from "../../proxy/TransportProxy";

export interface RpcConfig extends RpcSkeletonConfig, RpcStubConfig {}

export class RpcResult {
  constructor(
    private readonly ticketId_: number,
    private readonly raw_: Uint8Array
  ) {}

  public ticketId(): number {
    return this.ticketId_;
  }

  public as<T>(): T {
    return MsgPack.deserialize<T>(this.raw_) as T;
  }
}

export class server {
  private skeleton_: RpcSkeleton;
  private attachedStub_: RpcStub | null = null;

  constructor(private proxy_: TransportProxy) {
    this.skeleton_ = new RpcSkeleton(proxy_);
    this.skeleton_.loadConfig();

    // Firma de 4 argumentos alineada con TransportProxy.ts
    this.proxy_.setRpcHandler((clientId, sessionId, raw) => {
      const env = MsgPack.unpackEnvelope(raw);
      if (!env) return;

      if (
        env.type === PacketType.Request ||
        (env.type === PacketType.StreamChunk &&
          (env.flow === RpcFlow.ClientStream || env.flow === RpcFlow.BidiStream))
      ) {
        void this.skeleton_.onIncomingPacket(clientId, sessionId, env);
      } else if (this.attachedStub_) {
        this.attachedStub_.onIncomingResponse(clientId, env);
      }
    });
  }

  public skeleton(): RpcSkeleton {
    return this.skeleton_;
  }

  public attachClientStub(stub: RpcStub): void {
    this.attachedStub_ = stub;
    this.proxy_.setBusyCheck((clientId) => stub.hasActiveOperations(clientId));
  }

  public bind<TArgs extends unknown[] = unknown[], TOut = unknown>(
    name: string,
    fn: (...args: TArgs) => Promise<TOut> | TOut,
    semantics: RpcSemantics = RpcSemantics.AtMostOnce
  ): void {
    this.skeleton_.bindRaw(hashEndpoint(name), RpcFlow.Unary, semantics, async (_clientId, env) => {
      const unpacked = MsgPack.deserialize<unknown>(env.payload);
      const argsArray = Array.isArray(unpacked)
        ? unpacked
        : unpacked !== undefined
        ? [unpacked]
        : [];
      const res = await fn(...(argsArray as TArgs));
      return MsgPack.serialize(res);
    });
  }

  public bindStream<TIn = unknown, TOut = unknown>(
    name: string,
    fn: (ctx: StreamContext<TIn, TOut>) => void,
    flow: RpcFlow = RpcFlow.ServerStream,
    semantics: RpcSemantics = RpcSemantics.Maybe
  ): void {
    this.skeleton_.bindRaw(hashEndpoint(name), flow, semantics, (clientId, env) => {
      const ctx = new StreamContext<TIn, TOut>(clientId, env, this.proxy_);
      fn(ctx);
      return new Uint8Array(0);
    });
  }

  public run(): void {
    this.proxy_.schedule();
    if (this.attachedStub_) {
      this.attachedStub_.pollRetries();
    }
  }
}

export class client {
  private stub_: RpcStub;
  private ensureConnectedHook_: ((clientId: number) => Promise<boolean>) | null = null;

  constructor(private proxy_: TransportProxy, srv?: server) {
    this.stub_ = new RpcStub(proxy_);
    this.stub_.loadConfig();
    if (srv) {
      srv.attachClientStub(this.stub_);
    } else {
      this.proxy_.setRpcHandler((clientId, _sessionId, raw) => {
        const env = MsgPack.unpackEnvelope(raw);
        if (env) this.stub_.onIncomingResponse(clientId, env);
      });
    }
  }

  public stub(): RpcStub {
    return this.stub_;
  }

  public setEnsureConnectedHook(hook: (clientId: number) => Promise<boolean>): void {
    this.ensureConnectedHook_ = hook;
  }

  public async callTo(
    target: number | SessionHandle,
    name: string,
    ...args: unknown[]
  ): Promise<RpcResult> {
    const clientId = typeof target === "number" ? target : target.clientId;
    if (clientId <= 0) {
      throw new Error("Llamada RPC rechazada: debe especificar un SessionHandle o clientId válido");
    }

    if (this.ensureConnectedHook_) {
      await this.ensureConnectedHook_(clientId);
    }

    const payload = MsgPack.serialize(args);
    return new Promise<RpcResult>((resolve, reject) => {
      let ticketId = 0;
      ticketId = this.stub_.invoke(
        clientId,
        name,
        RpcFlow.Unary,
        RpcSemantics.AtMostOnce,
        payload,
        true,
        (raw) => resolve(new RpcResult(ticketId, raw)),
        (err) => reject(new Error(err))
      );
    });
  }

  public async stream<TChunk>(
    name: string,
    arg: unknown,
    onChunk: (chunk: TChunk) => void,
    target: number | SessionHandle,
    timeoutMs = 4000
  ): Promise<void> {
    const clientId = typeof target === "number" ? target : target.clientId;
    if (clientId <= 0) {
      throw new Error("Stream RPC rechazado: debe especificar un SessionHandle o clientId válido");
    }

    if (this.ensureConnectedHook_) {
      await this.ensureConnectedHook_(clientId);
    }

    const payload = MsgPack.serialize([arg]);
    return new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => {
        resolve();
      }, timeoutMs);

      this.stub_.invoke(
        clientId,
        name,
        RpcFlow.ServerStream,
        RpcSemantics.Maybe,
        payload,
        true,
        (raw, isEos) => {
          if (raw.byteLength > 0) {
            const decoded = MsgPack.deserialize<TChunk>(raw);
            if (decoded !== undefined) onChunk(decoded);
          }
          if (isEos) {
            clearTimeout(timer);
            resolve();
          }
        },
        (err) => {
          clearTimeout(timer);
          reject(new Error(err));
        }
      );
    });
  }
}

export const rpc = { server, client };