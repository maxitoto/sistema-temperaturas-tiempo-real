export interface FieldCodec<T> {
    readonly size: number;
    readonly align: number;
    encode(value: T, buf: Buffer, offset: number): void;
    decode(buf: Buffer, offset: number): T;
}

export interface StructCodec<T extends Record<string, unknown>> extends FieldCodec<T> {
    serialize(value: T): Buffer;
    deserialize(buf: Buffer): T;
}

function alignUp(offset: number, align: number): number {
    return (offset + align - 1) & ~(align - 1);
}

function escribirCString(destino: Buffer, offset: number, maxLen: number, texto: string): void {
    destino.fill(0, offset, offset + maxLen);
    if (!texto) return;
    destino.write(String(texto).slice(0, maxLen - 1), offset, maxLen - 1, "utf8");
}

function leerCString(origen: Buffer, offset: number, maxLen: number): string {
    const sub = origen.subarray(offset, offset + maxLen);
    const fin = sub.indexOf(0);
    return sub.subarray(0, fin === -1 ? sub.length : fin).toString("utf8").trim();
}

function macHexABuffer8(macHex: string): Buffer {
    const limpio = String(macHex ?? "").replace(/[^0-9A-Fa-f]/g, "").padEnd(16, "0").slice(0, 16);
    return Buffer.from(limpio, "hex");
}

/**
 * Diccionario de tipos 1:1 con PgDtoTypes.hpp (C++) y DjangoFields.hpp (DDL)
 */
export const T = {
    uint8: (): FieldCodec<number> => ({
        size: 1,
        align: 1,
        encode: (v, b, o) => b.writeUInt8((v ?? 0) & 0xFF, o),
        decode: (b, o) => b.readUInt8(o)
    }),

    int8: (): FieldCodec<number> => ({
        size: 1,
        align: 1,
        encode: (v, b, o) => b.writeInt8(v ?? 0, o),
        decode: (b, o) => b.readInt8(o)
    }),

    bool: (): FieldCodec<boolean> => ({
        size: 1,
        align: 1,
        encode: (v, b, o) => b.writeUInt8(v ? 1 : 0, o),
        decode: (b, o) => b.readUInt8(o) !== 0
    }),

    enum: (): FieldCodec<number> => T.uint8(),

    smallInt: (): FieldCodec<number> => ({
        size: 2,
        align: 2,
        encode: (v, b, o) => b.writeInt16LE(v ?? 0, o),
        decode: (b, o) => b.readInt16LE(o)
    }),

    posSmallInt: (): FieldCodec<number> => ({
        size: 2,
        align: 2,
        encode: (v, b, o) => b.writeUInt16LE((v ?? 0) & 0xFFFF, o),
        decode: (b, o) => b.readUInt16LE(o)
    }),

    int: (): FieldCodec<number> => ({
        size: 4,
        align: 4,
        encode: (v, b, o) => b.writeInt32LE(v ?? 0, o),
        decode: (b, o) => b.readInt32LE(o)
    }),

    auto: (): FieldCodec<number> => T.int(),

    posInt: (): FieldCodec<number> => ({
        size: 4,
        align: 4,
        encode: (v, b, o) => b.writeUInt32LE((v ?? 0) >>> 0, o),
        decode: (b, o) => b.readUInt32LE(o)
    }),

    epoch: (): FieldCodec<number> => T.posInt(),

    float: (): FieldCodec<number> => ({
        size: 4,
        align: 4,
        encode: (v, b, o) => b.writeFloatLE(v ?? 0, o),
        decode: (b, o) => b.readFloatLE(o)
    }),

    double: (): FieldCodec<number> => ({
        size: 8,
        align: 4, // En Xtensa ESP32 32-bit el alineamiento natural en structs es 4/8 bytes
        encode: (v, b, o) => b.writeDoubleLE(v ?? 0, o),
        decode: (b, o) => b.readDoubleLE(o)
    }),

    char: (maxLength: number): FieldCodec<string> => ({
        size: maxLength,
        align: 1,
        encode: (v, b, o) => escribirCString(b, o, maxLength, v),
        decode: (b, o) => leerCString(b, o, maxLength)
    }),

    text: (maxLength = 128): FieldCodec<string> => T.char(maxLength),

    dateTime: (): FieldCodec<string> => T.char(20),
    date: (): FieldCodec<string> => T.char(12),
    time: (): FieldCodec<string> => T.char(12),

    mac: (): FieldCodec<string> => ({
        size: 8,
        align: 1,
        encode: (v, b, o) => macHexABuffer8(v).copy(b, o),
        decode: (b, o) => b.subarray(o, o + 8).toString("hex").toUpperCase()
    }),

    array: <Item>(itemCodec: FieldCodec<Item>, count: number): FieldCodec<Item[]> => ({
        size: itemCodec.size * count,
        align: itemCodec.align,
        encode: (arr, b, o) => {
            for (let i = 0; i < count; i++) {
                if (i < (arr?.length ?? 0)) {
                    itemCodec.encode(arr[i], b, o + i * itemCodec.size);
                }
            }
        },
        decode: (b, o) => {
            const out: Item[] = [];
            for (let i = 0; i < count; i++) {
                out.push(itemCodec.decode(b, o + i * itemCodec.size));
            }
            return out;
        }
    })
};

/**
 * Calcula automáticamente los offsets y el padding del compilador C++ (Xtensa GCC)
 */
export function struct<Schema extends Record<string, FieldCodec<any>>>(
    schema: Schema
): StructCodec<{ [K in keyof Schema]: Schema[K] extends FieldCodec<infer U> ? U : never }> {
    type OutputType = { [K in keyof Schema]: Schema[K] extends FieldCodec<infer U> ? U : never };

    const layout: Array<{ key: keyof Schema; codec: FieldCodec<any>; offset: number }> = [];
    let currentOffset = 0;
    let maxAlign = 1;

    for (const [key, codec] of Object.entries(schema)) {
        if (codec.align > maxAlign) {
            maxAlign = codec.align;
        }
        currentOffset = alignUp(currentOffset, codec.align);
        layout.push({ key: key as keyof Schema, codec, offset: currentOffset });
        currentOffset += codec.size;
    }

    const totalSize = alignUp(currentOffset, maxAlign);

    return {
        size: totalSize,
        align: maxAlign,
        encode(value: OutputType, buf: Buffer, baseOffset: number): void {
            for (const field of layout) {
                field.codec.encode(value[field.key], buf, baseOffset + field.offset);
            }
        },
        decode(buf: Buffer, baseOffset: number): OutputType {
            if (buf.length < baseOffset + totalSize) {
                throw new Error(
                    `ProtocolError: payload binario incompleto (${buf.length - baseOffset}/${totalSize} bytes)`
                );
            }
            const result = {} as OutputType;
            for (const field of layout) {
                result[field.key] = field.codec.decode(buf, baseOffset + field.offset);
            }
            return result;
        },
        serialize(value: OutputType): Buffer {
            const buf = Buffer.alloc(totalSize);
            this.encode(value, buf, 0);
            return buf;
        },
        deserialize(buf: Buffer): OutputType {
            return this.decode(buf, 0);
        }
    };
}

/**
 * Espejo exacto de PgList<TItem, MaxN> en C++:
 * Lee/escribe uint8 cantidad en offset 0, alinea a itemCodec.align y decodifica solo los elementos válidos.
 */
export function listOf<Item>(
    itemCodec: FieldCodec<Item>,
    maxItems = 32
): StructCodec<{ items: Item[] }> & {
    serializeArray(items: Item[]): Buffer;
    deserializeArray(buf: Buffer): Item[];
} {
    const itemsOffset = alignUp(1, itemCodec.align);
    const totalSize = alignUp(itemsOffset + itemCodec.size * maxItems, itemCodec.align);

    return {
        size: totalSize,
        align: itemCodec.align,
        encode(value: { items: Item[] }, buf: Buffer, baseOffset: number): void {
            const arr = value.items ?? [];
            const count = Math.min(arr.length, maxItems);
            buf.writeUInt8(count & 0xFF, baseOffset);
            for (let i = 0; i < count; i++) {
                itemCodec.encode(arr[i], buf, baseOffset + itemsOffset + i * itemCodec.size);
            }
        },
        decode(buf: Buffer, baseOffset: number): { items: Item[] } {
            if (buf.length < baseOffset + itemsOffset) {
                return { items: [] };
            }
            const count = Math.min(buf.readUInt8(baseOffset), maxItems);
            const items: Item[] = [];
            for (let i = 0; i < count; i++) {
                const pos = baseOffset + itemsOffset + i * itemCodec.size;
                if (pos + itemCodec.size > buf.length) break;
                items.push(itemCodec.decode(buf, pos));
            }
            return { items };
        },
        serialize(value: { items: Item[] }): Buffer {
            const buf = Buffer.alloc(totalSize);
            this.encode(value, buf, 0);
            return buf;
        },
        deserialize(buf: Buffer): { items: Item[] } {
            return this.decode(buf, 0);
        },
        serializeArray(items: Item[]): Buffer {
            return this.serialize({ items });
        },
        deserializeArray(buf: Buffer): Item[] {
            return this.deserialize(buf).items;
        }
    };
}

export type InferDTO<C> = C extends FieldCodec<infer U> ? U : never;