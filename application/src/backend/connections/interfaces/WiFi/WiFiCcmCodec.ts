import crypto from "node:crypto";

// Cambiar a false solo si compilás el firmware con -DWIFI_CIFRADO_CCM_ACTIVO=0
export const WIFI_CIFRADO_CCM_ACTIVO = true;

export const PSK_DEFECTO_128 = Buffer.from("UNTDF-ESP32-PSK1", "ascii"); // 16 bytes = PSK_DEFECTO_128 en C++
export const VERSION_CCM = 0x01;
export const OVERHEAD_CCM = 26; // 10 B cabecera [VER:1][FLAGS:1][SALT:4][SEQ:4] + 16 B tag GCM

export enum CanalDireccionCcm {
    DIR_C2S_TCP = 0x01,
    DIR_S2C_TCP = 0x02,
    DIR_S2C_UDP = 0x03
}

function construirNonce(ver: number, flags: number, salt: number, seq: number): Buffer {
    const nonce = Buffer.alloc(12);
    nonce[0] = ver;
    nonce[1] = flags;
    nonce[2] = 0x00;
    nonce[3] = 0x00;
    nonce.writeUInt32BE(salt >>> 0, 4);
    nonce.writeUInt32BE(seq >>> 0, 8);
    return nonce;
}

export function generarSaltNoCero(): number {
    const val = crypto.randomBytes(4).readUInt32BE(0);
    return val === 0 ? 0x5A5AA5A5 : val;
}

export class EstadoSesionCcmCliente {
    private txSalt = generarSaltNoCero();
    private txSeq = 0;

    private rxTcpSalt = 0;
    private rxTcpUltimoSeq = 0;
    private rxTcpIniciado = false;

    private rxUdpSalt = 0;
    private rxUdpUltimoSeq = 0;
    private rxUdpIniciado = false;

    constructor(private readonly psk: Buffer = PSK_DEFECTO_128) {}

    reiniciar(): void {
        this.txSalt = generarSaltNoCero();
        this.txSeq = 0;

        this.rxTcpSalt = 0;
        this.rxTcpUltimoSeq = 0;
        this.rxTcpIniciado = false;

        this.rxUdpSalt = 0;
        this.rxUdpUltimoSeq = 0;
        this.rxUdpIniciado = false;
    }

    cifrarFrameTcp(framePlanoConLen: Buffer): { frameCifrado: Buffer; seq: number } | null {
        if (framePlanoConLen.length <= 4) return null;
        if (this.txSeq >= 0xFFFFFFFE) return null;

        const plainSinLen = framePlanoConLen.subarray(4);
        const seq = ++this.txSeq;

        const hdr = Buffer.alloc(10);
        hdr[0] = VERSION_CCM;
        hdr[1] = CanalDireccionCcm.DIR_C2S_TCP;
        hdr.writeUInt32BE(this.txSalt >>> 0, 2);
        hdr.writeUInt32BE(seq >>> 0, 6);

        const nonce = construirNonce(VERSION_CCM, CanalDireccionCcm.DIR_C2S_TCP, this.txSalt, seq);
        const cipher = crypto.createCipheriv("aes-128-gcm", this.psk, nonce);
        cipher.setAAD(hdr);

        const ct = Buffer.concat([cipher.update(plainSinLen), cipher.final()]);
        const tag = cipher.getAuthTag(); // 16 bytes

        const lenPref = Buffer.alloc(4);
        lenPref.writeUInt32BE(hdr.length + ct.length + tag.length, 0);

        return {
            frameCifrado: Buffer.concat([lenPref, hdr, ct, tag]),
            seq
        };
    }

    descifrarFrameEntrante(
        canalEsperado: CanalDireccionCcm.DIR_S2C_TCP | CanalDireccionCcm.DIR_S2C_UDP,
        frameInternoSinLen: Buffer
    ): { framePlanoConLen: Buffer; seq: number; salt: number } | null {
        if (frameInternoSinLen.length <= OVERHEAD_CCM) {
            return null;
        }

        const ver = frameInternoSinLen[0];
        const flags = frameInternoSinLen[1];
        const salt = frameInternoSinLen.readUInt32BE(2);
        const seq = frameInternoSinLen.readUInt32BE(6);

        if (ver !== VERSION_CCM || flags !== canalEsperado || salt === 0 || seq === 0) {
            return null;
        }

        if (canalEsperado === CanalDireccionCcm.DIR_S2C_TCP && this.rxTcpIniciado) {
            if (salt !== this.rxTcpSalt || seq <= this.rxTcpUltimoSeq) {
                return null;
            }
        }
        if (canalEsperado === CanalDireccionCcm.DIR_S2C_UDP && this.rxUdpIniciado) {
            if (salt === this.rxUdpSalt && seq <= this.rxUdpUltimoSeq) {
                return null;
            }
        }

        const hdr = frameInternoSinLen.subarray(0, 10);
        const lenCipher = frameInternoSinLen.length - OVERHEAD_CCM;
        const ct = frameInternoSinLen.subarray(10, 10 + lenCipher);
        const tag = frameInternoSinLen.subarray(10 + lenCipher);

        try {
            const nonce = construirNonce(ver, flags, salt, seq);
            const decipher = crypto.createDecipheriv("aes-128-gcm", this.psk, nonce);
            decipher.setAAD(hdr);
            decipher.setAuthTag(tag);
            const plain = Buffer.concat([decipher.update(ct), decipher.final()]);

            if (canalEsperado === CanalDireccionCcm.DIR_S2C_TCP) {
                if (!this.rxTcpIniciado) {
                    this.rxTcpSalt = salt;
                    this.rxTcpIniciado = true;
                }
                this.rxTcpUltimoSeq = seq;
            } else {
                this.rxUdpSalt = salt;
                this.rxUdpUltimoSeq = seq;
                this.rxUdpIniciado = true;
            }

            const framePlanoConLen = Buffer.alloc(4 + plain.length);
            framePlanoConLen.writeUInt32BE(plain.length, 0);
            plain.copy(framePlanoConLen, 4);

            return { framePlanoConLen, seq, salt };
        } catch {
            return null;
        }
    }
}