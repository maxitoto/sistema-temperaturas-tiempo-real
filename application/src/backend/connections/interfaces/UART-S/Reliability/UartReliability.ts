import { UartPhy } from "../UART/UartPhy";

export enum FrameType {
  SYN = 0x01,
  SYN_ACK = 0x02,
  DATA_RELIABLE = 0x03,
  DATA_STREAM = 0x04,
  ACK = 0x05,
  NACK = 0x06,
  HEARTBEAT = 0x07,
  DISC = 0x08,
}

export interface ReliabilityConfig {
  ackTimeoutMs: number;
  maxRetries: number;
  maxChunkSize: number;
  reassemblyTimeoutMs: number;
}

interface FrameHeader {
  type: FrameType;
  seq: number;
  msgId: number;
  chunkIdx: number;
  totalChunks: number;
  payloadLen: number;
}

interface PendingFrame {
  header: FrameHeader;
  payload: Uint8Array;
  requiresAck: boolean;
}

export type MessageReadyCb = (type: FrameType, message: Uint8Array) => void;

export class UartReliability {
  private onMessageReady_: MessageReadyCb | null = null;
  private txQueue_: PendingFrame[] = [];
  private inFlight_: PendingFrame | null = null;
  private waitingAck_ = false;
  private lastTxTime_ = 0;
  private currentRetries_ = 0;

  private cfg_: ReliabilityConfig = {
    ackTimeoutMs: 250,
    maxRetries: 6,
    maxChunkSize: 240,
    reassemblyTimeoutMs: 1500,
  };

  private nextSeq_ = Math.floor(Math.random() * 200) + 1;
  private nextMsgId_ = 0;
  private lastRxSeq_ = 0xff;

  private reassemblyBuffers_ = new Map<
    number,
    { data: number[]; count: number; startedMs: number }
  >();

  constructor(private phy_: UartPhy) {}

  public loadConfig(): void {
    // cargar de base de datos
    this.cfg_ = {
      ackTimeoutMs: 250,
      maxRetries: 6,
      maxChunkSize: 240,
      reassemblyTimeoutMs: 1500,
    };
  }

  public applyConfig(inCfg: Partial<ReliabilityConfig>, linkTimeoutMs = 2800): boolean {
    const merged: ReliabilityConfig = { ...this.cfg_, ...inCfg };
    if (
      merged.ackTimeoutMs < 50 ||
      merged.maxRetries < 1 ||
      merged.ackTimeoutMs * merged.maxRetries > linkTimeoutMs ||
      merged.maxChunkSize < 32 ||
      merged.maxChunkSize > 240
    ) {
      return false;
    }
    // zona base de datos
    this.cfg_ = merged;
    return true;
  }

  public getConfig(): Readonly<ReliabilityConfig> {
    return this.cfg_;
  }

  public resetSession(): void {
    this.waitingAck_ = false;
    this.inFlight_ = null;
    this.currentRetries_ = 0;
    this.lastRxSeq_ = 0xff;
    this.txQueue_ = [];
    this.reassemblyBuffers_.clear();
  }

  public setOnMessageReady(cb: MessageReadyCb): void {
    this.onMessageReady_ = cb;
  }

  public enqueueMessage(type: FrameType, data: Uint8Array): boolean {
    const len = data.byteLength;
    const totalChunks = len === 0 ? 1 : Math.ceil(len / this.cfg_.maxChunkSize);
    if (totalChunks > 255) return false;

    this.nextMsgId_ = (this.nextMsgId_ + 1) & 0xff;
    const msgId = this.nextMsgId_;

    for (let i = 0; i < totalChunks; i++) {
      const offset = i * this.cfg_.maxChunkSize;
      const chunkLen =
        len > offset ? Math.min(this.cfg_.maxChunkSize, len - offset) : 0;
      this.nextSeq_ = (this.nextSeq_ + 1) & 0xff;

      this.txQueue_.push({
        header: {
          type,
          seq: this.nextSeq_,
          msgId,
          chunkIdx: i & 0xff,
          totalChunks: totalChunks & 0xff,
          payloadLen: chunkLen & 0xff,
        },
        payload: chunkLen > 0 ? data.slice(offset, offset + chunkLen) : new Uint8Array(0),
        requiresAck:
          type === FrameType.DATA_RELIABLE ||
          type === FrameType.SYN ||
          type === FrameType.SYN_ACK ||
          type === FrameType.HEARTBEAT,
      });
    }
    return true;
  }

  public poll(): boolean {
    let raw = this.phy_.pollFrame();
    while (raw !== null) {
      this.processIncomingFrame(raw);
      raw = this.phy_.pollFrame();
    }

    const now = Date.now();
    for (const [msgId, entry] of this.reassemblyBuffers_.entries()) {
      if (now >= entry.startedMs && now - entry.startedMs > this.cfg_.reassemblyTimeoutMs) {
        this.reassemblyBuffers_.delete(msgId);
      }
    }

    if (this.waitingAck_ && this.inFlight_) {
      if (now - this.lastTxTime_ >= this.cfg_.ackTimeoutMs) {
        if (this.currentRetries_ >= this.cfg_.maxRetries) {
          this.waitingAck_ = false;
          this.inFlight_ = null;
          return false;
        }
        this.currentRetries_++;
        this.transmitFrame(this.inFlight_);
      }
    } else if (this.txQueue_.length > 0) {
      this.inFlight_ = this.txQueue_.shift()!;
      this.currentRetries_ = 0;
      this.transmitFrame(this.inFlight_);
      this.waitingAck_ = this.inFlight_.requiresAck;
    }
    return true;
  }

  private transmitFrame(pf: PendingFrame): void {
    const buf = new Uint8Array(6 + pf.payload.byteLength + 2);
    buf[0] = pf.header.type;
    buf[1] = pf.header.seq;
    buf[2] = pf.header.msgId;
    buf[3] = pf.header.chunkIdx;
    buf[4] = pf.header.totalChunks;
    buf[5] = pf.header.payloadLen;

    if (pf.payload.byteLength > 0) {
      buf.set(pf.payload, 6);
    }

    const crc = UartReliability.crc16CcittFalse(
      buf.subarray(0, 6 + pf.payload.byteLength)
    );
    buf[buf.length - 2] = (crc >> 8) & 0xff;
    buf[buf.length - 1] = crc & 0xff;

    this.phy_.sendFrame(buf);
    this.lastTxTime_ = Date.now();
  }

  private sendControlAck(ackType: FrameType, seq: number): void {
    this.transmitFrame({
      header: {
        type: ackType,
        seq,
        msgId: 0,
        chunkIdx: 0,
        totalChunks: 1,
        payloadLen: 0,
      },
      payload: new Uint8Array(0),
      requiresAck: false,
    });
  }

  private processIncomingFrame(raw: Uint8Array): void {
    if (raw.byteLength < 8) return;

    const dataLen = raw.byteLength - 2;
    const rxCrc = (raw[dataLen] << 8) | raw[dataLen + 1];
    if (UartReliability.crc16CcittFalse(raw.subarray(0, dataLen)) !== rxCrc) {
      return;
    }

    const hdr: FrameHeader = {
      type: raw[0] as FrameType,
      seq: raw[1],
      msgId: raw[2],
      chunkIdx: raw[3],
      totalChunks: raw[4],
      payloadLen: raw[5],
    };

    if (hdr.payloadLen !== dataLen - 6) return;

    if (hdr.type === FrameType.ACK) {
      if (this.waitingAck_ && this.inFlight_ && hdr.seq === this.inFlight_.header.seq) {
        this.waitingAck_ = false;
        this.inFlight_ = null;
      }
      return;
    }

    if (hdr.type === FrameType.SYN) {
      this.sendControlAck(FrameType.ACK, hdr.seq);
      if (hdr.seq === this.lastRxSeq_) return;
      this.resetSession();
      this.lastRxSeq_ = hdr.seq;
    } else if (
      hdr.type === FrameType.SYN_ACK ||
      hdr.type === FrameType.DATA_RELIABLE ||
      hdr.type === FrameType.HEARTBEAT
    ) {
      this.sendControlAck(FrameType.ACK, hdr.seq);
      if (hdr.seq === this.lastRxSeq_) return;
      this.lastRxSeq_ = hdr.seq;
    }

    const now = Date.now();
    if (hdr.chunkIdx === 0) {
      this.reassemblyBuffers_.set(hdr.msgId, { data: [], count: 0, startedMs: now });
    }
    const entry = this.reassemblyBuffers_.get(hdr.msgId);
    if (!entry) return;

    const chunk = raw.subarray(6, 6 + hdr.payloadLen);
    for (let i = 0; i < chunk.byteLength; i++) {
      entry.data.push(chunk[i]);
    }
    entry.count++;

    if (entry.count >= hdr.totalChunks) {
      const fullMsg = new Uint8Array(entry.data);
      this.reassemblyBuffers_.delete(hdr.msgId);
      if (this.onMessageReady_) {
        this.onMessageReady_(hdr.type, fullMsg);
      }
    }
  }

  public static crc16CcittFalse(data: Uint8Array): number {
    let crc = 0xffff;
    for (let i = 0; i < data.byteLength; i++) {
      crc ^= data[i] << 8;
      for (let b = 0; b < 8; b++) {
        crc = (crc & 0x8000) !== 0 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
      }
    }
    return crc & 0xffff;
  }
}