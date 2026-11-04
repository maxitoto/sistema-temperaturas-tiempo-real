import * as crypto from "node:crypto";
import {
  DisconnectReason,
  ITransport,
  MediumType,
  TransportState,
} from "../../proxy/ITransport";
import { UartPhy } from "./UART/UartPhy";
import { UartSecurity } from "./Security/UartSecurity";
import { FrameType, UartReliability } from "./Reliability/UartReliability";

export class UartSTransport extends ITransport {
  private phy_ = new UartPhy();
  private security_ = new UartSecurity();
  private reliability_ = new UartReliability(this.phy_);
  private authResolver_: ((ok: boolean) => void) | null = null;
  private localIdleMs_ = 10 * 60 * 1000;
  private localLinkMs_ = 2800;

  public reliability(): UartReliability {
    return this.reliability_;
  }

  public setLinkParameters(idleTimeoutMs: number, linkTimeoutMs: number): void {
    this.localIdleMs_ = idleTimeoutMs;
    this.localLinkMs_ = linkTimeoutMs;
  }

  public loadConfig(): void {
    this.phy_.loadConfig();
    this.security_.loadConfig();
    this.reliability_.loadConfig();
  }

  public configureSerial(pathManual?: string): void {
    this.phy_.configure(pathManual);
  }

  public getConfiguredPath(): string {
    return this.phy_.getConfiguredPath();
  }

  public async init(): Promise<boolean> {
    this.reliability_.setOnMessageReady((type, msg) => {
      this.handleFrame(type, msg);
    });
    const ok = await this.phy_.begin();
    if (!ok) {
      this.setState(TransportState.DISCONNECTED, DisconnectReason.PORT_CLOSED);
      return false;
    }
    this.setState(TransportState.PHY_LINKED, DisconnectReason.NONE);
    return true;
  }

  public async connect(timeoutMs = 3500): Promise<boolean> {
    // Atajo H-02: si ya está autenticado y el puerto sigue abierto, no reabrir ni reiniciar el ESP32
    if (this.state_ === TransportState.AUTHENTICATED && this.phy_.isOpen()) {
      return true;
    }

    const inited = await this.init();
    if (!inited) return false;

    this.setState(TransportState.HANDSHAKING, DisconnectReason.NONE);
    this.reliability_.resetSession();
    this.phy_.clearBuffers();

    this.sessionId_ = crypto.randomInt(1, 0x7fffffff);
    const pubKey = this.security_.generateHandshakePublic();

    // SYN: [pubKey(65B) | sessionId(4B BE) | idleTimeoutMs(4B BE) | linkTimeoutMs(4B BE)] = 77B
    const synBuf = Buffer.alloc(77);
    Buffer.from(pubKey).copy(synBuf, 0);
    synBuf.writeUInt32BE(this.sessionId_ >>> 0, 65);
    synBuf.writeUInt32BE(this.localIdleMs_ >>> 0, 69);
    synBuf.writeUInt32BE(this.localLinkMs_ >>> 0, 73);

    this.reliability_.enqueueMessage(FrameType.SYN, new Uint8Array(synBuf));

    return new Promise<boolean>((resolve) => {
      const timer = setTimeout(() => {
        if (this.authResolver_) {
          this.authResolver_ = null;
          this.reliability_.resetSession();
          this.setState(TransportState.PHY_LINKED, DisconnectReason.HANDSHAKE_TIMEOUT);
          resolve(false);
        }
      }, timeoutMs);

      this.authResolver_ = (ok: boolean) => {
        clearTimeout(timer);
        this.authResolver_ = null;
        if (!ok) {
          this.reliability_.resetSession();
          this.setState(TransportState.PHY_LINKED, DisconnectReason.HANDSHAKE_TIMEOUT);
        }
        resolve(ok);
      };
    });
  }

  // En cierre por inactividad se cierra la sesión criptográfica pero se deja abierto el puerto físico para no pulsar DTR
  public override async closeIdle(): Promise<void> {
    this.reliability_.resetSession();
    this.setState(TransportState.CLOSED_IDLE, DisconnectReason.IDLE_CLOSE);
  }

  public async disconnect(
    reason: DisconnectReason = DisconnectReason.USER_CLOSE
  ): Promise<void> {
    if (this.state_ === TransportState.AUTHENTICATED && this.phy_.isOpen()) {
      this.reliability_.enqueueMessage(FrameType.DISC, new Uint8Array(0));
      this.reliability_.poll();
    }
    this.reliability_.resetSession();
    if (reason === DisconnectReason.USER_CLOSE || !this.phy_.isOpen()) {
      await this.phy_.close();
      this.setState(TransportState.DISCONNECTED, reason);
    } else {
      this.setState(TransportState.PHY_LINKED, reason);
    }
  }

  public poll(): void {
    if (
      this.state_ === TransportState.DISCONNECTED ||
      this.state_ === TransportState.CLOSED_IDLE ||
      this.state_ === TransportState.PHY_LINKED
    ) {
      return;
    }

    if (!this.phy_.isOpen()) {
      const pendingAuth = this.authResolver_;
      this.authResolver_ = null;
      this.reliability_.resetSession();
      this.setState(TransportState.DISCONNECTED, DisconnectReason.PORT_CLOSED);
      if (pendingAuth) pendingAuth(false);
      return;
    }

    if (!this.reliability_.poll()) {
      const pendingAuth = this.authResolver_;
      this.authResolver_ = null;
      this.reliability_.resetSession();
      this.setState(TransportState.PHY_LINKED, DisconnectReason.ACK_FAIL);
      if (pendingAuth) pendingAuth(false);
    }
  }

  public sendReliable(_peerId: number, data: Uint8Array): boolean {
    if (this.state_ !== TransportState.AUTHENTICATED) return false;
    const cipher = this.security_.encrypt(data);
    if (!cipher) return false;
    return this.reliability_.enqueueMessage(FrameType.DATA_RELIABLE, cipher);
  }

  public sendStream(_peerId: number, data: Uint8Array): boolean {
    if (this.state_ !== TransportState.AUTHENTICATED) return false;
    const cipher = this.security_.encrypt(data);
    if (!cipher) return false;
    return this.reliability_.enqueueMessage(FrameType.DATA_STREAM, cipher);
  }

  public getMediumType(): MediumType {
    return MediumType.UART_S;
  }

  private handleFrame(type: FrameType, msg: Uint8Array): void {
    if (type === FrameType.SYN) {
      if (msg.byteLength < 65) return;
      const myPub = this.security_.generateHandshakePublic();
      if (this.security_.computeSharedKey(msg.subarray(0, 65), false, "uart")) {
        this.reliability_.enqueueMessage(FrameType.SYN_ACK, myPub);
        this.setState(TransportState.AUTHENTICATED, DisconnectReason.NONE);
        if (this.authResolver_) this.authResolver_(true);
      }
    } else if (type === FrameType.SYN_ACK) {
      if (
        msg.byteLength >= 65 &&
        this.security_.computeSharedKey(msg.subarray(0, 65), true, "uart")
      ) {
        if (msg.byteLength >= 78) {
          const b = Buffer.from(msg.buffer, msg.byteOffset, msg.byteLength);
          this.sessionId_ = b.readUInt32BE(65);
          this.peerBootId_ = b.readUInt32BE(69);
          this.peerResetReason_ = b[73];
          this.negotiatedIdleMs_ = b.readUInt32BE(74);
        }
        this.setState(TransportState.AUTHENTICATED, DisconnectReason.NONE);
        if (this.authResolver_) this.authResolver_(true);
      } else if (this.authResolver_) {
        this.authResolver_(false);
      }
    } else if (type === FrameType.DISC) {
      this.reliability_.resetSession();
      this.setState(TransportState.PHY_LINKED, DisconnectReason.DISC_RX);
    } else if (
      (type === FrameType.DATA_RELIABLE || type === FrameType.DATA_STREAM) &&
      this.state_ === TransportState.AUTHENTICATED
    ) {
      const plain = this.security_.decrypt(msg);
      if (plain && this.onRx_) {
        this.onRx_({
          endpoint: { medium: MediumType.UART_S, peerId: 1 },
          isRealTime: type === FrameType.DATA_STREAM,
          payload: plain,
        });
      }
    }
  }
}