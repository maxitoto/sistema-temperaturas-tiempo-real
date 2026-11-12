export enum ProcedimientoRPC {
    L1_HANDSHAKE_UARTS        = 0x0101,
    L1_KEEPALIVE_UARTS        = 0x0102,
    L1_CERRAR_UARTS           = 0x0103,
    L1_KEEPALIVE_WIFI         = 0x0104,
    L1_SUSCRIBIR_UDP_WIFI     = 0x0105,
    L1_CERRAR_WIFI            = 0x0106,

    L2_LOGIN_USUARIO          = 0x0201,
    L2_LOGOUT_USUARIO         = 0x0202,

    L3_SUSCRIBIR_SENSORES     = 0x0301,
    L3_OBTENER_CENSOS_MAC     = 0x0302,
    L3_OBTENER_CENSOS_FECHA   = 0x0303,
    L3_REINICIAR_HARDWARE     = 0x0310,
    L3_ACTUALIZAR_CONFIG_WIFI = 0x0311,
    L3_DIAGNOSTICO_SISTEMA    = 0x0312
}

export const BANDERA_ERROR_RPC = 0x8000;

export enum CodigoErrorRPC {
    OK                          = 0,
    NO_AUTENTICADO              = 1,
    SESION_BLOQUEADA            = 2,
    ROL_INSUFICIENTE            = 3,
    ARGUMENTOS_INVALIDOS        = 4,
    PROCEDIMIENTO_NO_ENCONTRADO = 5,
    SERVIDOR_OCUPADO            = 6
}

export interface LecturaSensorCliente {
    timestampMs: number;
    idSensor: number;
    macHex: string;
    temperaturaCorregida: number;
}

export interface ResultadoLoginCliente {
    autenticado: boolean;
    rolAsignado: number;
    tiempoExpiracionSegundos: number;
}

export interface ConfirmacionSistemaCliente {
    exito: boolean;
    codigoEstado: number;
}

export class CodecRPC {
    // ¿Cómo lo hace?: Antepone la cabecera [longitudPayload:4 BE][idTicket:2 BE][idProc:2 BE] al buffer del DTO de entrada.
    static empaquetarPeticion(idTicket: number, idProc: ProcedimientoRPC, payloadDto: Buffer = Buffer.alloc(0)): Buffer {
        const longitudPayload = 4 + payloadDto.length;
        const frame = Buffer.alloc(8 + payloadDto.length);

        frame.writeUInt32BE(longitudPayload, 0);
        frame.writeUInt16BE(idTicket & 0xFFFF, 4);
        frame.writeUInt16BE(idProc & 0xFFFF, 6);

        if (payloadDto.length > 0) {
            payloadDto.copy(frame, 8);
        }
        return frame;
    }

    // ¿Cómo lo hace?: Extrae idTicket, idProc y el sub-buffer del DTO de salida a partir de un frame recibido con cabecera de 8 bytes.
    static desempaquetarRespuesta(frameCompleto: Buffer): {
        idTicket: number;
        idProc: number;
        esError: boolean;
        payload: Buffer;
    } | null {
        if (frameCompleto.length < 8) {
            return null;
        }
        const longitudUtil = frameCompleto.readUInt32BE(0);
        if (frameCompleto.length < 4 + longitudUtil) {
            return null;
        }
        const idTicket = frameCompleto.readUInt16BE(4);
        const idProcCrudo = frameCompleto.readUInt16BE(6);
        const esError = (idProcCrudo & BANDERA_ERROR_RPC) !== 0;
        const idProc = idProcCrudo & ~BANDERA_ERROR_RPC;
        const payload = frameCompleto.subarray(8, 4 + longitudUtil);
        return { idTicket, idProc, esError, payload };
    }

    // ¿Cómo lo hace?: Escribe un string UTF-8 truncado y terminado en '\0' dentro de un segmento de longitud fija.
    private static escribirCString(destino: Buffer, offset: number, maxLen: number, texto: string): void {
        destino.fill(0, offset, offset + maxLen);
        destino.write(texto.slice(0, maxLen - 1), offset, maxLen - 1, "utf8");
    }

    // ¿Cómo lo hace?: Convierte una dirección MAC hexadecimal ("28FF12...") en un arreglo binario de 8 bytes.
    static macHexABuffer8(macHex: string): Buffer {
        const limpio = macHex.replace(/[^0-9A-Fa-f]/g, "").padEnd(16, "0").slice(0, 16);
        return Buffer.from(limpio, "hex");
    }

    // ¿Cómo lo hace?: Serializa HandshakeUartsDTO (1 byte: versionProtocolo = 1).
    static codificarHandshakeUarts(version = 1): Buffer {
        const b = Buffer.alloc(1);
        b.writeUInt8(version & 0xFF, 0);
        return b;
    }

    // ¿Cómo lo hace?: Serializa SuscripcionUdpDTO (2 bytes LE: uint16_t puertoEscuchaCliente).
    static codificarSuscripcionUdp(puertoLocal: number): Buffer {
        const b = Buffer.alloc(2);
        b.writeUInt16LE(puertoLocal & 0xFFFF, 0);
        return b;
    }

    // ¿Cómo lo hace?: Serializa CredencialesLoginDTO (64 bytes: usuario[32] + clavePlana[32]).
    static codificarLogin(usuario: string, clavePlana: string): Buffer {
        const b = Buffer.alloc(64);
        this.escribirCString(b, 0, 32, usuario);
        this.escribirCString(b, 32, 32, clavePlana);
        return b;
    }

    static decodificarErrorRPC(idProc: number, payload: Buffer): Error {
        if (payload.length > 4) {
            const finTexto = payload.indexOf(0, 4);
            const detalleServidor = payload
                .subarray(4, finTexto === -1 ? payload.length : finTexto)
                .toString("utf8")
                .trim();

            if (detalleServidor.length > 0) {
                return new Error(detalleServidor);
            }
        }

        const codigo = payload.length >= 1 ? (payload.readUInt8(0) as CodigoErrorRPC) : CodigoErrorRPC.OK;
        switch (codigo) {
            case CodigoErrorRPC.NO_AUTENTICADO:
                return new Error("PROXY_UNAUTHENTICATED: Sesion no autenticada. Inicie sesion primero.");
            case CodigoErrorRPC.SESION_BLOQUEADA:
                return new Error("PROXY_SESSION_LOCKED: Sesion bloqueada temporalmente por intentos fallidos.");
            case CodigoErrorRPC.ROL_INSUFICIENTE:
                return new Error("RPC_FORBIDDEN_ADMIN: Operacion exclusiva para rol ADMIN");
            case CodigoErrorRPC.ARGUMENTOS_INVALIDOS:
                return new Error("RPC_INVALID_ARGS: Argumentos invalidos para el procedimiento.");
            case CodigoErrorRPC.PROCEDIMIENTO_NO_ENCONTRADO:
                return new Error("RPC_NOT_FOUND: Endpoint no registrado");
            default:
                return new Error(`RPC_ERROR: Fallo en procedimiento 0x${idProc.toString(16)}`);
        }
    }

    // ¿Cómo lo hace?: Deserializa ResultadoLoginDTO (8 bytes en ESP32: bool autenticado, uint8_t rol, 2B pad, uint32_t LE expiracion).
    static decodificarResultadoLogin(payload: Buffer): ResultadoLoginCliente {
        if (payload.length < 8) {
            return { autenticado: false, rolAsignado: 0, tiempoExpiracionSegundos: 0 };
        }
        return {
            autenticado: payload.readUInt8(0) !== 0,
            rolAsignado: payload.readUInt8(1),
            tiempoExpiracionSegundos: payload.readUInt32LE(4)
        };
    }

    // ¿Cómo lo hace?: Serializa FiltroPorMacDTO (10 bytes: uint8_t mac[8] + uint16_t LE limiteRegistros).
    static codificarFiltroPorMac(macHex: string, limiteRegistros: number): Buffer {
        const b = Buffer.alloc(10);
        this.macHexABuffer8(macHex).copy(b, 0);
        b.writeUInt16LE(limiteRegistros & 0xFFFF, 8);
        return b;
    }

    // ¿Cómo lo hace?: Serializa FiltroPorMacYFechaDTO (20 bytes con alineación 32-bit: mac[8], desde:4 LE, hasta:4 LE, limite:2 LE, 2B pad).
    static codificarFiltroPorMacYFecha(macHex: string, desdeSeg: number, hastaSeg: number, limiteRegistros: number): Buffer {
        const b = Buffer.alloc(20);
        this.macHexABuffer8(macHex).copy(b, 0);
        b.writeUInt32LE(desdeSeg >>> 0, 8);
        b.writeUInt32LE(hastaSeg >>> 0, 12);
        b.writeUInt16LE(limiteRegistros & 0xFFFF, 16);
        return b;
    }

    // ¿Cómo lo hace?: Deserializa HistorialSensoresDTO (cantidadLecturas en byte 0, 3B pad, y hasta 32 structs LecturaSensorDTO de 20 bytes cada uno).
    static decodificarHistorialSensores(payload: Buffer): LecturaSensorCliente[] {
        if (payload.length < 4) {
            return [];
        }
        const cantidad = Math.min(payload.readUInt8(0), 32);
        const lecturas: LecturaSensorCliente[] = [];

        for (let i = 0; i < cantidad; i++) {
            const base = 4 + i * 20;
            if (base + 20 > payload.length) {
                break;
            }
            const timestampMs = payload.readUInt32LE(base);
            const idSensor = payload.readUInt16LE(base + 4);
            const macHex = payload.subarray(base + 6, base + 14).toString("hex").toUpperCase();
            const temperaturaCorregida = payload.readFloatLE(base + 16);

            lecturas.push({ timestampMs, idSensor, macHex, temperaturaCorregida });
        }
        return lecturas;
    }

    // ¿Cómo lo hace?: Serializa ConfiguracionWiFiDTO (192 bytes: ssidSta[32], claveSta[64], ssidAp[32], claveAp[64]).
    static codificarConfiguracionWiFi(ssidSta: string, claveSta: string, ssidAp: string, claveAp: string): Buffer {
        const b = Buffer.alloc(192);
        this.escribirCString(b, 0, 32, ssidSta);
        this.escribirCString(b, 32, 64, claveSta);
        this.escribirCString(b, 96, 32, ssidAp);
        this.escribirCString(b, 128, 64, claveAp);
        return b;
    }

    // ¿Cómo lo hace?: Deserializa ConfirmacionSistemaDTO (8 bytes: bool exito en byte 0, 3B pad, uint32_t LE codigoEstado en byte 4).
    static decodificarConfirmacionSistema(payload: Buffer): ConfirmacionSistemaCliente {
        if (payload.length < 8) {
            return { exito: false, codigoEstado: 0 };
        }
        return {
            exito: payload.readUInt8(0) !== 0,
            codigoEstado: payload.readUInt32LE(4)
        };
    }
}