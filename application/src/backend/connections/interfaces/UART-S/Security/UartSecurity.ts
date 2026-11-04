import * as crypto from "node:crypto";

export class UartSecurity {
  private ecdh_: crypto.ECDH | null = null;
  private rawSharedSecret_: Buffer = Buffer.alloc(32, 0);
  private txKey_: Buffer = Buffer.alloc(32, 0);
  private rxKey_: Buffer = Buffer.alloc(32, 0);
  private txDirByte_ = 0x01;
  private rxDirByte_ = 0x02;
  private channelByte_ = 0x01;
  private txCounter_ = 0n;
  private rxCounter_ = 0n;

  public loadConfig(): void {
    // cargar de base de datos
  }

  public generateHandshakePublic(): Uint8Array {
    this.ecdh_ = crypto.createECDH("prime256v1");
    this.ecdh_.generateKeys();
    return new Uint8Array(this.ecdh_.getPublicKey());
  }

  public computeSharedKey(
    peerPub: Uint8Array,
    isInitiator = true,
    channelLabel: "uart" | "tcp" | "udp" = "uart"
  ): boolean {
    try {
      if (!this.ecdh_ || peerPub.byteLength < 65) return false;
      this.rawSharedSecret_ = this.ecdh_.computeSecret(Buffer.from(peerPub.subarray(0, 65)));
      return this.setupDirectionalKeys(this.rawSharedSecret_, isInitiator, channelLabel);
    } catch {
      return false;
    }
  }

  public deriveChannelKeysFrom(
    base: UartSecurity,
    isInitiator = true,
    channelLabel: "uart" | "tcp" | "udp" = "udp"
  ): boolean {
    this.rawSharedSecret_ = Buffer.from(base.rawSharedSecret_);
    return this.setupDirectionalKeys(this.rawSharedSecret_, isInitiator, channelLabel);
  }

  public encrypt(plain: Uint8Array): Uint8Array | null {
    try {
      this.txCounter_ += 1n;
      const counterBuf = Buffer.alloc(8);
      counterBuf.writeBigUInt64LE(this.txCounter_, 0);

      const iv = Buffer.alloc(12, 0);
      iv[0] = this.txDirByte_;
      iv[1] = this.channelByte_;
      counterBuf.copy(iv, 4);

      const cipher = crypto.createCipheriv("aes-256-gcm", this.txKey_, iv);
      cipher.setAAD(counterBuf);
      const encrypted = Buffer.concat([cipher.update(plain), cipher.final()]);
      const tag = cipher.getAuthTag();

      return new Uint8Array(Buffer.concat([counterBuf, encrypted, tag]));
    } catch {
      return null;
    }
  }

  public decrypt(inData: Uint8Array): Uint8Array | null {
    try {
      if (inData.byteLength < 24) return null;
      const buf = Buffer.from(inData.buffer, inData.byteOffset, inData.byteLength);

      const counterBuf = buf.subarray(0, 8);
      const packetCounter = counterBuf.readBigUInt64LE(0);
      if (packetCounter <= this.rxCounter_) return null;

      const iv = Buffer.alloc(12, 0);
      iv[0] = this.rxDirByte_;
      iv[1] = this.channelByte_;
      counterBuf.copy(iv, 4);

      const cipherLen = buf.byteLength - 24;
      const cipherText = buf.subarray(8, 8 + cipherLen);
      const tag = buf.subarray(8 + cipherLen, 8 + cipherLen + 16);

      const decipher = crypto.createDecipheriv("aes-256-gcm", this.rxKey_, iv);
      decipher.setAAD(counterBuf);
      decipher.setAuthTag(tag);

      const plain = Buffer.concat([decipher.update(cipherText), decipher.final()]);
      this.rxCounter_ = packetCounter;
      return new Uint8Array(plain);
    } catch {
      return null;
    }
  }

  private setupDirectionalKeys(
    secret: Buffer,
    isInitiator: boolean,
    channelLabel: "uart" | "tcp" | "udp"
  ): boolean {
    const c2s = crypto
      .createHash("sha256")
      .update(Buffer.concat([secret, Buffer.from(`c2s_${channelLabel}`, "utf-8")]))
      .digest();
    const s2c = crypto
      .createHash("sha256")
      .update(Buffer.concat([secret, Buffer.from(`s2c_${channelLabel}`, "utf-8")]))
      .digest();

    if (isInitiator) {
      this.txKey_ = c2s;
      this.rxKey_ = s2c;
      this.txDirByte_ = 0x01;
      this.rxDirByte_ = 0x02;
    } else {
      this.txKey_ = s2c;
      this.rxKey_ = c2s;
      this.txDirByte_ = 0x02;
      this.rxDirByte_ = 0x01;
    }

    this.channelByte_ = channelLabel === "udp" ? 0x03 : channelLabel === "tcp" ? 0x02 : 0x01;
    this.txCounter_ = 0n;
    this.rxCounter_ = 0n;
    return true;
  }
}