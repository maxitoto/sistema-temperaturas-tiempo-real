export const PROTOCOL_VERSION = 1;

export enum RpcFlow {
  Unary = 1,
  ServerStream = 2,
  ClientStream = 3,
  BidiStream = 4,
}

export enum RpcSemantics {
  Maybe = 0,
  AtLeastOnce = 1,
  AtMostOnce = 2,
}

export enum PacketType {
  Request = 1,
  Response = 2,
  StreamChunk = 3,
  StreamEnd = 4,
  Error = 5,
}

export const Flags = {
  NONE: 0x00,
  EOS: 0x01,
  REAL_TIME: 0x02,
} as const;

export interface RpcEnvelope {
  type: PacketType;
  flow: RpcFlow;
  semantics: RpcSemantics;
  flags: number;
  callId: number;
  streamId: number;
  methodId: number;
  payload: Uint8Array;
}

export function hashEndpoint(str: string): number {
  let hash = 2166136261 >>> 0;
  for (let i = 0; i < str.length; i++) {
    hash ^= str.charCodeAt(i) & 0xff;
    hash = Math.imul(hash, 16777619) >>> 0;
  }
  return hash >>> 0;
}

export class MsgPack {
  public static packNil(buf: number[]): void {
    buf.push(0xc0);
  }

  public static packValue(buf: number[], v: unknown): void {
    if (v === null || v === undefined) {
      buf.push(0xc0);
      return;
    }
    if (typeof v === "boolean") {
      buf.push(v ? 0xc3 : 0xc2);
      return;
    }
    if (typeof v === "number") {
      if (Number.isInteger(v)) {
        const big = BigInt(v);
        const b = Buffer.alloc(8);
        b.writeBigUInt64BE(BigInt.asUintN(64, big), 0);
        buf.push(0xcf, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
      } else {
        const b = Buffer.alloc(4);
        b.writeFloatBE(v, 0);
        buf.push(0xca, b[0], b[1], b[2], b[3]);
      }
      return;
    }
    if (typeof v === "string") {
      const utf8 = Buffer.from(v, "utf-8");
      const len = utf8.byteLength;
      buf.push(0xda, (len >> 8) & 0xff, len & 0xff);
      for (let i = 0; i < len; i++) buf.push(utf8[i]);
      return;
    }
    if (v instanceof Uint8Array || Buffer.isBuffer(v)) {
      const len = v.byteLength;
      buf.push(0xc5, (len >> 8) & 0xff, len & 0xff);
      for (let i = 0; i < len; i++) buf.push(v[i]);
      return;
    }
    if (Array.isArray(v)) {
      if (v.length < 16) {
        buf.push(0x90 | (v.length & 0x0f));
      } else {
        buf.push(0xdc, (v.length >> 8) & 0xff, v.length & 0xff);
      }
      for (const item of v) {
        MsgPack.packValue(buf, item);
      }
      return;
    }
    if (typeof v === "object") {
      const vals = Object.values(v as Record<string, unknown>);
      if (vals.length < 16) {
        buf.push(0x90 | (vals.length & 0x0f));
      } else {
        buf.push(0xdc, (vals.length >> 8) & 0xff, vals.length & 0xff);
      }
      for (const item of vals) {
        MsgPack.packValue(buf, item);
      }
    }
  }

  public static unpackAny(data: Uint8Array, state: { offset: number }): unknown {
    if (state.offset >= data.byteLength) return undefined;
    const tag = data[state.offset++];

    if (tag === 0xc0) return null;
    if (tag === 0xc2) return false;
    if (tag === 0xc3) return true;

    if (tag === 0xcf) {
      if (state.offset + 8 > data.byteLength) return undefined;
      const b = Buffer.from(data.buffer, data.byteOffset + state.offset, 8);
      state.offset += 8;
      return Number(b.readBigInt64BE(0));
    }

    if (tag === 0xca) {
      if (state.offset + 4 > data.byteLength) return undefined;
      const b = Buffer.from(data.buffer, data.byteOffset + state.offset, 4);
      state.offset += 4;
      return b.readFloatBE(0);
    }

    if (tag === 0xcb) {
      if (state.offset + 8 > data.byteLength) return undefined;
      const b = Buffer.from(data.buffer, data.byteOffset + state.offset, 8);
      state.offset += 8;
      return b.readDoubleBE(0);
    }

    if (tag === 0xda) {
      if (state.offset + 2 > data.byteLength) return undefined;
      const sLen = (data[state.offset] << 8) | data[state.offset + 1];
      state.offset += 2;
      if (state.offset + sLen > data.byteLength) return undefined;
      const str = Buffer.from(data.buffer, data.byteOffset + state.offset, sLen).toString("utf-8");
      state.offset += sLen;
      return str;
    }

    if (tag === 0xc5) {
      if (state.offset + 2 > data.byteLength) return undefined;
      const bLen = (data[state.offset] << 8) | data[state.offset + 1];
      state.offset += 2;
      if (state.offset + bLen > data.byteLength) return undefined;
      const bin = data.slice(state.offset, state.offset + bLen);
      state.offset += bLen;
      return bin;
    }

    if ((tag & 0xf0) === 0x90) {
      const count = tag & 0x0f;
      const arr: unknown[] = [];
      for (let i = 0; i < count; i++) {
        arr.push(MsgPack.unpackAny(data, state));
      }
      return arr;
    }

    if (tag === 0xdc) {
      if (state.offset + 2 > data.byteLength) return undefined;
      const count = (data[state.offset] << 8) | data[state.offset + 1];
      state.offset += 2;
      const arr: unknown[] = [];
      for (let i = 0; i < count; i++) {
        arr.push(MsgPack.unpackAny(data, state));
      }
      return arr;
    }

    return undefined;
  }

  public static serialize<T>(obj: T): Uint8Array {
    const buf: number[] = [];
    MsgPack.packValue(buf, obj);
    return new Uint8Array(buf);
  }

  public static deserialize<T>(buf: Uint8Array): T | undefined {
    const state = { offset: 0 };
    return MsgPack.unpackAny(buf, state) as T | undefined;
  }

  public static packEnvelope(env: RpcEnvelope): Uint8Array {
    return MsgPack.serialize([
      env.type,
      env.flow,
      env.semantics,
      env.flags,
      env.callId,
      env.streamId,
      env.methodId,
      env.payload,
    ]);
  }

  public static unpackEnvelope(raw: Uint8Array): RpcEnvelope | null {
    const tup = MsgPack.deserialize<unknown[]>(raw);
    if (!Array.isArray(tup) || tup.length !== 8) return null;
    return {
      type: Number(tup[0]) as PacketType,
      flow: Number(tup[1]) as RpcFlow,
      semantics: Number(tup[2]) as RpcSemantics,
      flags: Number(tup[3]),
      callId: Number(tup[4]),
      streamId: Number(tup[5]),
      methodId: Number(tup[6]) >>> 0,
      payload: tup[7] instanceof Uint8Array ? tup[7] : new Uint8Array(0),
    };
  }
}