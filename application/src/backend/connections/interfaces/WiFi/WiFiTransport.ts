import * as net from "node:net";
import * as dgram from "node:dgram";
import * as crypto from "node:crypto";
import {
  DisconnectReason,
  ITransport,
  MediumType,
  TransportState,
} from "../../proxy/ITransport";
import { UartSecurity } from "../UART-S/Security/UartSecurity";

export class WiFiTransport extends ITransport {
  public static readonly TCP_PORT = 8443;
  public static readonly UDP_PORT = 8444;
  public static readonly DEFAULT_AP_IP = "192.168.4.1";

  private hostIp_ = WiFiTransport.DEFAULT_AP_IP;
  private localUdpPort_ = 0;
  private localIdleMs_ = 10 * 60 * 1000;
  private localLinkMs_ = 2800;

  private tcpClient_: net.Socket | null = null;
  private udp_: dgram.Socket | null = null;
  private tcpCrypto_ = new UartSecurity();
  private udpCrypto_ = new UartSecurity();
  private rxTcpBuffer_: Buffer = Buffer.alloc(0);
  private handshakeCompleted_ = false;
  private authResolver_: ((ok: boolean) => void) | null = null;

  public setLinkParameters(idleTimeoutMs: number, linkTimeoutMs: number): void {
    this.localIdleMs_ = idleTimeoutMs;
    this.localLinkMs_ = linkTimeoutMs;
  }

  public loadConfig(): void {
    // cargar de base de datos
    this.tcpCrypto_.loadConfig();
    this.udpCrypto_.loadConfig();
  }

  public configureTarget(hostIp?: string): void {
    if (hostIp && hostIp.trim().length > 0) {
      // zona base de datos
      this.hostIp_ = hostIp.trim();
    }
  }

  public getTargetIp(): string {
    return this.hostIp_;
  }

  public async init(): Promise<boolean> {
    await this.closeSockets();
    this.tcpCrypto_.loadConfig();
    this.udpCrypto_.loadConfig();

    return new Promise((resolve) => {
      this.udp_ = dgram.createSocket("udp4");
      this.udp_.on("message", (msg: Buffer) => {
        if (this.state_ !== TransportState.AUTHENTICATED) return;
        const plain = this.udpCrypto_.decrypt(
          new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength)
        );
        if (plain && this.onRx_) {
          this.onRx_({
            endpoint: { medium: MediumType.WIFI, peerId: 1 },
            isRealTime: true,
            payload: plain,
          });
        }
      });

      this.udp_.bind(0, () => {
        const addr = this.udp_?.address();
        this.localUdpPort_ =
          typeof addr === "object" && addr ? addr.port : WiFiTransport.UDP_PORT;
        this.setState(TransportState.PHY_LINKED, DisconnectReason.NONE);
        resolve(true);
      });
    });
  }

  public async connect(timeoutMs = 3500): Promise<boolean> {
    if (this.state_ === TransportState.AUTHENTICATED && this.tcpClient_ && !this.tcpClient_.destroyed) {
      return true;
    }

    const ok = await this.init();
    if (!ok) return false;

    this.setState(TransportState.HANDSHAKING, DisconnectReason.NONE);
    this.handshakeCompleted_ = false;
    this.rxTcpBuffer_ = Buffer.alloc(0);
    this.sessionId_ = crypto.randomInt(1, 0x7fffffff);

    return new Promise<boolean>((resolve) => {
      const timer = setTimeout(() => {
        if (this.authResolver_) {
          this.authResolver_ = null;
          void this.disconnect(DisconnectReason.HANDSHAKE_TIMEOUT);
          resolve(false);
        }
      }, timeoutMs);

      this.authResolver_ = (authenticated: boolean) => {
        clearTimeout(timer);
        this.authResolver_ = null;
        if (!authenticated) {
          void this.disconnect(DisconnectReason.HANDSHAKE_TIMEOUT);
        }
        resolve(authenticated);
      };

      this.tcpClient_ = net.createConnection(
        { host: this.hostIp_, port: WiFiTransport.TCP_PORT },
        () => {
          if (!this.tcpClient_) return;
          this.tcpClient_.setNoDelay(true);
          this.tcpClient_.setKeepAlive(true, 1000);

          const myPub = this.tcpCrypto_.generateHandshakePublic();
          // Handshake: [0xFE, 79, pubKey(65B) | udpPort(2B) | sessionId(4B) | idleMs(4B) | linkMs(4B)]
          const payload = Buffer.alloc(79);
          Buffer.from(myPub).copy(payload, 0);
          payload.writeUInt16BE(this.localUdpPort_ & 0xffff, 65);
          payload.writeUInt32BE(this.sessionId_ >>> 0, 67);
          payload.writeUInt32BE(this.localIdleMs_ >>> 0, 71);
          payload.writeUInt32BE(this.localLinkMs_ >>> 0, 75);

          const hdr = Buffer.from([0xfe, payload.byteLength]);
          this.tcpClient_.write(Buffer.concat([hdr, payload]));
        }
      );

      this.tcpClient_.on("data", (chunk: Buffer) => {
        this.onTcpData(chunk);
      });

      this.tcpClient_.on("error", () => {
        if (this.authResolver_) this.authResolver_(false);
        else void this.disconnect(DisconnectReason.PORT_CLOSED);
      });

      this.tcpClient_.on("close", () => {
        if (this.authResolver_) this.authResolver_(false);
        else if (this.state_ !== TransportState.CLOSED_IDLE) {
          void this.disconnect(DisconnectReason.PORT_CLOSED);
        }
      });
    });
  }

  public override async closeIdle(): Promise<void> {
    await this.closeSockets();
    this.setState(TransportState.CLOSED_IDLE, DisconnectReason.IDLE_CLOSE);
  }

  public async disconnect(
    reason: DisconnectReason = DisconnectReason.USER_CLOSE
  ): Promise<void> {
    await this.closeSockets();
    this.setState(TransportState.DISCONNECTED, reason);
  }

  public poll(): void {}

  public sendReliable(_peerId: number, data: Uint8Array): boolean {
    if (this.state_ !== TransportState.AUTHENTICATED || !this.tcpClient_) return false;
    const cipher = this.tcpCrypto_.encrypt(data);
    if (!cipher) return false;

    const hdr = Buffer.from([(cipher.byteLength >> 8) & 0xff, cipher.byteLength & 0xff]);
    return this.tcpClient_.write(Buffer.concat([hdr, Buffer.from(cipher)]));
  }

  public sendStream(_peerId: number, data: Uint8Array): boolean {
    if (this.state_ !== TransportState.AUTHENTICATED || !this.udp_) return false;
    const cipher = this.udpCrypto_.encrypt(data);
    if (!cipher) return false;
    this.udp_.send(Buffer.from(cipher), WiFiTransport.UDP_PORT, this.hostIp_);
    return true;
  }

  public getMediumType(): MediumType {
    return MediumType.WIFI;
  }

  private async closeSockets(): Promise<void> {
    if (this.tcpClient_) {
      this.tcpClient_.removeAllListeners();
      this.tcpClient_.destroy();
      this.tcpClient_ = null;
    }
    if (this.udp_) {
      this.udp_.removeAllListeners();
      try {
        this.udp_.close();
      } catch {
        // Ignorar
      }
      this.udp_ = null;
    }
    this.handshakeCompleted_ = false;
  }

  private onTcpData(chunk: Buffer): void {
    this.rxTcpBuffer_ = Buffer.concat([this.rxTcpBuffer_, chunk]);

    if (!this.handshakeCompleted_) {
      if (this.rxTcpBuffer_.length < 2 || this.rxTcpBuffer_[0] !== 0xfe) return;
      const pubLen = this.rxTcpBuffer_[1];
      if (this.rxTcpBuffer_.length < 2 + pubLen) return;

      const hsSlice = this.rxTcpBuffer_.subarray(2, 2 + pubLen);
      this.rxTcpBuffer_ = this.rxTcpBuffer_.subarray(2 + pubLen);

      if (
        hsSlice.byteLength >= 65 &&
        this.tcpCrypto_.computeSharedKey(
          new Uint8Array(hsSlice.buffer, hsSlice.byteOffset, 65),
          true,
          "tcp"
        ) &&
        this.udpCrypto_.deriveChannelKeysFrom(this.tcpCrypto_, true, "udp")
      ) {
        if (hsSlice.byteLength >= 78) {
          this.sessionId_ = hsSlice.readUInt32BE(65);
          this.peerBootId_ = hsSlice.readUInt32BE(69);
          this.peerResetReason_ = hsSlice[73];
          this.negotiatedIdleMs_ = hsSlice.readUInt32BE(74);
        }
        this.handshakeCompleted_ = true;
        this.setState(TransportState.AUTHENTICATED, DisconnectReason.NONE);
        if (this.authResolver_) this.authResolver_(true);
      } else {
        if (this.authResolver_) this.authResolver_(false);
        return;
      }
      // H-13 / T18: no hacer return aquí; procesar inmediatamente cualquier trama recibida en el mismo segmento TCP
    }

    while (this.rxTcpBuffer_.length >= 2) {
      const pktLen = (this.rxTcpBuffer_[0] << 8) | this.rxTcpBuffer_[1];
      if (this.rxTcpBuffer_.length < 2 + pktLen) break;

      const cipherSlice = new Uint8Array(this.rxTcpBuffer_.subarray(2, 2 + pktLen));
      this.rxTcpBuffer_ = this.rxTcpBuffer_.subarray(2 + pktLen);

      const plain = this.tcpCrypto_.decrypt(cipherSlice);
      if (plain && this.onRx_) {
        this.onRx_({
          endpoint: { medium: MediumType.WIFI, peerId: 1 },
          isRealTime: false,
          payload: plain,
        });
      }
    }
  }
}