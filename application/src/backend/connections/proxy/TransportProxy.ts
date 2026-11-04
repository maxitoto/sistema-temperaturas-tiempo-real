import {
  DisconnectReason,
  EndpointId,
  ITransport,
  MediumType,
  SessionHandle,
  TransportPacket,
  TransportState,
} from "./ITransport";

export interface LinkConfig {
  probeIntervalMs: number;
  linkTimeoutMs: number;
  idleTimeoutMs: number;
  gracefulWaitMs: number;
  maxPacketsPerTick: number;
}

export interface ClientSession {
  clientId: number;
  medium: MediumType;
  peerId: number;
  sessionId: number;
  peerBootId: number;
  peerResetReason: number;
  rebootDetected: boolean;
  lastRxMs: number;
  lastProbeTxMs: number;
  lastRpcActivityMs: number;
  negotiatedIdleMs: number;
  closingGracefully: boolean;
  pendingIdleClose: boolean;
  gracefulCloseStartMs: number;
  wasAuthenticated: boolean;
  lastDisconnectReason: DisconnectReason;
}

export type RpcLayerHandler = (
  clientId: number,
  sessionId: number,
  data: Uint8Array,
  isRealTime: boolean
) => void;

export type DisconnectListener = (
  clientId: number,
  sessionId: number,
  medium: MediumType,
  reason: DisconnectReason
) => void;

export type BusyCheckCallback = (clientId: number) => boolean;

export class TransportProxy {
  public static readonly PROXY_CTRL_MAGIC = 0xf0;
  public static readonly PROXY_PING = 0x01;
  public static readonly PROXY_PONG = 0x02;
  public static readonly PROXY_IDLE_CLOSE = 0x03;
  public static readonly PROXY_IDLE_CLOSE_ACK = 0x04;
  public static readonly PROXY_IDLE_CLOSE_NACK = 0x05;

  public static readonly DEFAULT_IDLE_TIMEOUT_MS = 10 * 60 * 1000; // 10 minutos

  private cfg_: LinkConfig = {
    probeIntervalMs: 1000,
    linkTimeoutMs: 2800,
    idleTimeoutMs: TransportProxy.DEFAULT_IDLE_TIMEOUT_MS,
    gracefulWaitMs: 1000,
    maxPacketsPerTick: 16,
  };

  private transports_ = new Map<MediumType, ITransport>();
  private sessions_ = new Map<MediumType, ClientSession>();
  private clientRoutes_ = new Map<number, EndpointId>();
  private reverseRoutes_ = new Map<string, number>();
  private rxQueue_: Array<{ clientId: number; sessionId: number; packet: TransportPacket }> = [];
  private txQueue_: TransportPacket[] = [];
  private rpcHandler_: RpcLayerHandler | null = null;
  private disconnectListeners_: DisconnectListener[] = [];
  private busyCheck_: BusyCheckCallback | null = null;

  private nextClientId_ = 1;
  private preferredClientId_ = 1;

  public loadConfig(): void {
    // cargar de base de datos
    this.cfg_ = {
      probeIntervalMs: 1000,
      linkTimeoutMs: 2800,
      idleTimeoutMs: TransportProxy.DEFAULT_IDLE_TIMEOUT_MS,
      gracefulWaitMs: 1000,
      maxPacketsPerTick: 16,
    };
  }

  public applyConfig(inCfg: Partial<LinkConfig>): boolean {
    const merged: LinkConfig = { ...this.cfg_, ...inCfg };
    if (
      merged.probeIntervalMs < 250 ||
      merged.linkTimeoutMs < merged.probeIntervalMs * 2 ||
      merged.idleTimeoutMs < 5000 ||
      merged.gracefulWaitMs < 200 ||
      merged.maxPacketsPerTick < 1 ||
      merged.maxPacketsPerTick > 64
    ) {
      return false;
    }
    // zona base de datos
    this.cfg_ = merged;
    for (const s of this.sessions_.values()) {
      s.negotiatedIdleMs = merged.idleTimeoutMs;
    }
    return true;
  }

  public getConfig(): Readonly<LinkConfig> {
    return this.cfg_;
  }

  public registerMedium(transport: ITransport): void {
    const m = transport.getMediumType();
    this.transports_.set(m, transport);

    const cid = this.bindDefaultEndpoint(m, 1);
    this.sessions_.set(m, {
      clientId: cid,
      medium: m,
      peerId: 1,
      sessionId: 0,
      peerBootId: 0,
      peerResetReason: 0,
      rebootDetected: false,
      lastRxMs: 0,
      lastProbeTxMs: 0,
      lastRpcActivityMs: 0,
      negotiatedIdleMs: this.cfg_.idleTimeoutMs,
      closingGracefully: false,
      pendingIdleClose: false,
      gracefulCloseStartMs: 0,
      wasAuthenticated: false,
      lastDisconnectReason: DisconnectReason.NONE,
    });

    transport.setRxCallback((pkt: TransportPacket) => {
      const now = Date.now();
      const s = this.sessions_.get(pkt.endpoint.medium);
      if (!s) return;
      s.lastRxMs = now;

      if (
        pkt.payload.byteLength === 2 &&
        pkt.payload[0] === TransportProxy.PROXY_CTRL_MAGIC
      ) {
        const cmd = pkt.payload[1];
        if (cmd === TransportProxy.PROXY_PING) {
          this.sendControlFrame(
            pkt.endpoint.medium,
            pkt.endpoint.peerId,
            TransportProxy.PROXY_PONG
          );
        } else if (cmd === TransportProxy.PROXY_IDLE_CLOSE) {
          const isBusy =
            (this.busyCheck_ && this.busyCheck_(s.clientId)) ||
            (now >= s.lastRpcActivityMs &&
              now - s.lastRpcActivityMs < s.negotiatedIdleMs / 2);
          if (isBusy) {
            this.sendControlFrame(
              pkt.endpoint.medium,
              pkt.endpoint.peerId,
              TransportProxy.PROXY_IDLE_CLOSE_NACK
            );
          } else {
            this.sendControlFrame(
              pkt.endpoint.medium,
              pkt.endpoint.peerId,
              TransportProxy.PROXY_IDLE_CLOSE_ACK
            );
            s.pendingIdleClose = true;
            s.lastDisconnectReason = DisconnectReason.IDLE_CLOSE;
          }
        } else if (cmd === TransportProxy.PROXY_IDLE_CLOSE_ACK) {
          s.pendingIdleClose = true;
          s.lastDisconnectReason = DisconnectReason.IDLE_CLOSE;
        } else if (cmd === TransportProxy.PROXY_IDLE_CLOSE_NACK) {
          s.closingGracefully = false;
          s.lastRpcActivityMs = now;
        }
        return;
      }

      s.lastRpcActivityMs = now;
      s.closingGracefully = false;
      this.rxQueue_.push({ clientId: s.clientId, sessionId: s.sessionId, packet: pkt });
    });
  }

  public setRpcHandler(handler: RpcLayerHandler): void {
    this.rpcHandler_ = handler;
  }

  public onMediumDisconnected(listener: DisconnectListener): void {
    this.disconnectListeners_.push(listener);
  }

  public setBusyCheck(cb: BusyCheckCallback): void {
    this.busyCheck_ = cb;
  }

  public setPreferredClient(clientId: number): void {
    this.preferredClientId_ = clientId;
  }

  public getActiveClientId(): number {
    const prefEp = this.clientRoutes_.get(this.preferredClientId_);
    if (prefEp && this.isMediumConnected(prefEp.medium)) {
      return this.preferredClientId_;
    }
    for (const [cid, ep] of this.clientRoutes_.entries()) {
      if (this.isMediumConnected(ep.medium)) {
        this.preferredClientId_ = cid;
        return cid;
      }
    }
    return this.preferredClientId_;
  }

  public getSessionHandle(medium: MediumType): SessionHandle | null {
    const s = this.sessions_.get(medium);
    if (!s || !this.isMediumConnected(medium)) return null;
    return {
      clientId: s.clientId,
      sessionId: s.sessionId,
      medium: s.medium,
      peerBootId: s.peerBootId,
      peerResetReason: s.peerResetReason,
    };
  }

  public getSessionByClientId(clientId: number): Readonly<ClientSession> | null {
    const ep = this.clientRoutes_.get(clientId);
    if (!ep) return null;
    return this.sessions_.get(ep.medium) ?? null;
  }

  public explainClientDisconnect(clientId: number): string {
    const s = this.getSessionByClientId(clientId);
    if (!s) return "Sesión inexistente";
    const t = this.transports_.get(s.medium);
    const state = t ? TransportState[t.getState()] : "DESCONOCIDO";
    const reason = s.lastDisconnectReason !== DisconnectReason.NONE
      ? s.lastDisconnectReason
      : (t?.getLastDisconnectReason() ?? DisconnectReason.NONE);
    return `Medio ${MediumType[s.medium]} no autenticado (estado=${state}, motivo=${reason})`;
  }

  public isConnected(): boolean {
    for (const t of this.transports_.values()) {
      if (t.getState() === TransportState.AUTHENTICATED) return true;
    }
    return false;
  }

  public isMediumConnected(medium: MediumType): boolean {
    const t = this.transports_.get(medium);
    return Boolean(t && t.getState() === TransportState.AUTHENTICATED);
  }

  public schedule(): void {
    for (const [m, transport] of this.transports_.entries()) {
      const s = this.sessions_.get(m)!;
      transport.poll();
      const now = Date.now();

      if (s.pendingIdleClose) {
        transport.poll();
        void transport.closeIdle();
        s.pendingIdleClose = false;
        s.wasAuthenticated = false;
        s.closingGracefully = false;
        s.lastDisconnectReason = DisconnectReason.IDLE_CLOSE;
        this.notifyDisconnected(s, DisconnectReason.IDLE_CLOSE);
        continue;
      }

      const isAuth = transport.getState() === TransportState.AUTHENTICATED;
      if (isAuth && !s.wasAuthenticated) {
        const newBootId = transport.getPeerBootId();
        s.rebootDetected = s.peerBootId !== 0 && newBootId !== 0 && newBootId !== s.peerBootId;
        if (s.rebootDetected) {
          console.warn(
            `[PROXY:${MediumType[m]}] Reinicio del ESP32 detectado: bootId ${s.peerBootId} -> ${newBootId} (resetReason=${transport.getPeerResetReason()})`
          );
        }
        s.peerBootId = newBootId;
        s.peerResetReason = transport.getPeerResetReason();
        s.sessionId = transport.getSessionId();
        s.negotiatedIdleMs =
          transport.getNegotiatedIdleMs() > 0
            ? Math.min(this.cfg_.idleTimeoutMs, transport.getNegotiatedIdleMs())
            : this.cfg_.idleTimeoutMs;
        s.lastRxMs = now;
        s.lastProbeTxMs = now;
        s.lastRpcActivityMs = now;
        s.closingGracefully = false;
        s.lastDisconnectReason = DisconnectReason.NONE;
      }

      if (!isAuth && s.wasAuthenticated) {
        s.closingGracefully = false;
        s.wasAuthenticated = false;
        const r =
          transport.getLastDisconnectReason() !== DisconnectReason.NONE
            ? transport.getLastDisconnectReason()
            : DisconnectReason.DISC_RX;
        s.lastDisconnectReason = r;
        this.notifyDisconnected(s, r);
      }
      s.wasAuthenticated = isAuth;

      if (isAuth) {
        if (now >= s.lastRxMs && now - s.lastRxMs > this.cfg_.linkTimeoutMs) {
          s.lastDisconnectReason = DisconnectReason.LINK_TIMEOUT;
          void transport.disconnect(DisconnectReason.LINK_TIMEOUT);
          s.wasAuthenticated = false;
          s.closingGracefully = false;
          this.notifyDisconnected(s, DisconnectReason.LINK_TIMEOUT);
          continue;
        }

        if (s.closingGracefully) {
          if (
            now >= s.gracefulCloseStartMs &&
            now - s.gracefulCloseStartMs >= this.cfg_.gracefulWaitMs
          ) {
            s.lastDisconnectReason = DisconnectReason.IDLE_NO_AGREEMENT;
            void transport.closeIdle();
            s.wasAuthenticated = false;
            s.closingGracefully = false;
            this.notifyDisconnected(s, DisconnectReason.IDLE_NO_AGREEMENT);
            continue;
          }
        } else if (
          now >= s.lastRpcActivityMs &&
          now - s.lastRpcActivityMs >= s.negotiatedIdleMs
        ) {
          if (this.busyCheck_ && this.busyCheck_(s.clientId)) {
            s.lastRpcActivityMs = now;
          } else {
            s.closingGracefully = true;
            s.gracefulCloseStartMs = now;
            this.sendControlFrame(m, s.peerId, TransportProxy.PROXY_IDLE_CLOSE);
            continue;
          }
        }

        if (
          !s.closingGracefully &&
          now >= s.lastProbeTxMs &&
          now - s.lastProbeTxMs >= this.cfg_.probeIntervalMs &&
          now - s.lastRxMs >= this.cfg_.probeIntervalMs
        ) {
          this.sendControlFrame(m, s.peerId, TransportProxy.PROXY_PING);
          s.lastProbeTxMs = now;
        }
      }
    }

    let processed = 0;
    while (this.rxQueue_.length > 0 && processed < this.cfg_.maxPacketsPerTick) {
      const item = this.rxQueue_.shift()!;
      if (this.rpcHandler_) {
        this.rpcHandler_(item.clientId, item.sessionId, item.packet.payload, item.packet.isRealTime);
      }
      processed++;
    }

    processed = 0;
    while (this.txQueue_.length > 0 && processed < this.cfg_.maxPacketsPerTick) {
      const pkt = this.txQueue_.shift()!;
      this.dispatchToMedium(pkt);
      processed++;
    }
  }

  public routeToClient(clientId: number, data: Uint8Array, isRealTime: boolean): boolean {
    const ep = this.clientRoutes_.get(clientId);
    if (!ep || !this.isMediumConnected(ep.medium)) return false;

    const s = this.sessions_.get(ep.medium);
    if (s) {
      s.lastRpcActivityMs = Date.now();
      s.closingGracefully = false;
    }

    this.txQueue_.push({
      endpoint: ep,
      isRealTime,
      payload: new Uint8Array(data),
    });
    return true;
  }

  public bindDefaultEndpoint(medium: MediumType, peerId = 1): number {
    return this.resolveOrCreateClientId({ medium, peerId });
  }

  private sendControlFrame(medium: MediumType, peerId: number, cmd: number): void {
    const t = this.transports_.get(medium);
    if (!t || t.getState() !== TransportState.AUTHENTICATED) return;
    t.sendReliable(peerId, new Uint8Array([TransportProxy.PROXY_CTRL_MAGIC, cmd]));
  }

  private notifyDisconnected(s: ClientSession, reason: DisconnectReason): void {
    this.txQueue_ = this.txQueue_.filter((p) => p.endpoint.medium !== s.medium);
    for (const fn of this.disconnectListeners_) {
      fn(s.clientId, s.sessionId, s.medium, reason);
    }
  }

  private endpointKey(ep: EndpointId): string {
    return `${ep.medium}:${ep.peerId}`;
  }

  private resolveOrCreateClientId(ep: EndpointId): number {
    const key = this.endpointKey(ep);
    const existing = this.reverseRoutes_.get(key);
    if (existing !== undefined) return existing;

    const newId = this.nextClientId_++;
    this.clientRoutes_.set(newId, ep);
    this.reverseRoutes_.set(key, newId);
    return newId;
  }

  private dispatchToMedium(pkt: TransportPacket): boolean {
    const medium = this.transports_.get(pkt.endpoint.medium);
    if (!medium || medium.getState() !== TransportState.AUTHENTICATED) {
      return false;
    }

    if (pkt.isRealTime) {
      return medium.sendStream(pkt.endpoint.peerId, pkt.payload);
    }
    return medium.sendReliable(pkt.endpoint.peerId, pkt.payload);
  }
}