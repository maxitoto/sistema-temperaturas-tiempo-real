export enum MediumType {
  NONE = 0,
  UART_S = 1,
  WIFI = 2,
}

export enum TransportState {
  DISCONNECTED = 0,
  PHY_LINKED = 1,
  HANDSHAKING = 2,
  AUTHENTICATED = 3,
  CLOSED_IDLE = 4,
  ERROR_RECOVERY = 5,
  CLOSING = 6,
}

export enum DisconnectReason {
  NONE = "NONE",
  USER_CLOSE = "USER_CLOSE",
  IDLE_CLOSE = "IDLE_CLOSE",
  IDLE_NO_AGREEMENT = "IDLE_NO_AGREEMENT",
  LINK_TIMEOUT = "LINK_TIMEOUT",
  ACK_FAIL = "ACK_FAIL",
  DISC_RX = "DISC_RX",
  PORT_CLOSED = "PORT_CLOSED",
  HANDSHAKE_TIMEOUT = "HANDSHAKE_TIMEOUT",
}

export interface EndpointId {
  medium: MediumType;
  peerId: number;
}

export interface TransportPacket {
  endpoint: EndpointId;
  isRealTime: boolean;
  payload: Uint8Array;
}

export interface SessionHandle {
  clientId: number;
  sessionId: number;
  medium: MediumType;
  peerBootId: number;
  peerResetReason: number;
}

export type RxCallback = (pkt: TransportPacket) => void;
export type StateChangeCallback = (
  medium: MediumType,
  state: TransportState,
  reason: DisconnectReason
) => void;

export abstract class ITransport {
  protected state_: TransportState = TransportState.DISCONNECTED;
  protected lastReason_: DisconnectReason = DisconnectReason.NONE;
  protected sessionId_ = 0;
  protected peerBootId_ = 0;
  protected peerResetReason_ = 0;
  protected negotiatedIdleMs_ = 10 * 60 * 1000;
  protected onRx_: RxCallback | null = null;
  protected onStateChange_: StateChangeCallback | null = null;

  public abstract loadConfig(): void;
  public abstract init(): Promise<boolean>;
  public abstract connect(timeoutMs?: number): Promise<boolean>;
  public abstract disconnect(reason?: DisconnectReason): Promise<void>;
  public async closeIdle(): Promise<void> {
    await this.disconnect(DisconnectReason.IDLE_CLOSE);
  }
  public abstract poll(): void;

  public abstract sendReliable(peerId: number, data: Uint8Array): boolean;
  public abstract sendStream(peerId: number, data: Uint8Array): boolean;
  public abstract getMediumType(): MediumType;

  public getState(): TransportState {
    return this.state_;
  }

  public getLastDisconnectReason(): DisconnectReason {
    return this.lastReason_;
  }

  public getSessionId(): number {
    return this.sessionId_;
  }

  public getPeerBootId(): number {
    return this.peerBootId_;
  }

  public getPeerResetReason(): number {
    return this.peerResetReason_;
  }

  public getNegotiatedIdleMs(): number {
    return this.negotiatedIdleMs_;
  }

  public setRxCallback(cb: RxCallback): void {
    this.onRx_ = cb;
  }

  public setStateChangeCallback(cb: StateChangeCallback): void {
    this.onStateChange_ = cb;
  }

  protected setState(
    nextState: TransportState,
    reason: DisconnectReason = DisconnectReason.NONE
  ): void {
    if (reason !== DisconnectReason.NONE) {
      this.lastReason_ = reason;
    }
    if (this.state_ !== nextState) {
      this.state_ = nextState;
      console.log(
        `[TRANSPORT:${MediumType[this.getMediumType()]}] -> ${TransportState[nextState]} (motivo=${this.lastReason_})`
      );
      if (this.onStateChange_) {
        this.onStateChange_(this.getMediumType(), this.state_, this.lastReason_);
      }
    }
  }
}