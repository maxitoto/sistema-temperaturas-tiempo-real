export enum ProcedimientoSistemaRPC {
    L1_HANDSHAKE_UARTS    = 0x0101,
    L1_KEEPALIVE_UARTS    = 0x0102,
    L1_CERRAR_UARTS       = 0x0103,
    L1_KEEPALIVE_WIFI     = 0x0104,
    L1_SUSCRIBIR_UDP_WIFI = 0x0105,
    L1_CERRAR_WIFI        = 0x0106,

    L2_LOGIN_USUARIO      = 0x0201,
    L2_LOGOUT_USUARIO     = 0x0202
}

export const BANDERA_ERROR_RPC = 0x8000;

export interface PerfilEnlaceUartsCliente {
    versionServidor: number;
    mtuBytes: number;
}

export interface CanalStreamingUdpCliente {
    puertoEmisorServidor: number;
}

export interface SesionIniciadaCliente {
    rolAsignado: number;
    tiempoExpiracionSegundos: number;
}

export interface LecturaSensorCliente {
    timestampMs: number;
    idSensor: number;
    macHex: string;
    temperaturaCorregida: number;
}

export interface DiagnosticoSistemaCliente {
    heapLibreBytes: number;
    uptimeSegundos: number;
}

export interface ConfiguracionWiFiCliente {
    ssidSta: string;
    ssidAp: string;
    ipSta: string;
    ipAp: string;
    conectadoARouter: boolean;
    puntoAccesoActivo: boolean;
}

// Calcula el mismo hash FNV-1a de 15 bits (0x0300..0x7FFF) que C++ constexpr hashNombreProcedimiento()
export function hashNombreProcedimiento(nombre: string): number {
    let h = 2166136261 >>> 0;
    for (let i = 0; i < nombre.length; i++) {
        h ^= nombre.charCodeAt(i) & 0xFF;
        h = Math.imul(h, 16777619) >>> 0;
    }
    const id15 = ((h ^ (h >>> 15)) & 0x7FFF) >>> 0;
    return id15 < 0x0300 ? (id15 + 0x0300) & 0x7FFF : id15;
}

export class CodecRPC {
    static empaquetarPeticion(
        idTicket: number,
        idProc: number,
        payloadDto: Buffer = Buffer.alloc(0)
    ): Buffer {
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

    private static escribirCString(destino: Buffer, offset: number, maxLen: number, texto: string): void {
        destino.fill(0, offset, offset + maxLen);
        destino.write(texto.slice(0, maxLen - 1), offset, maxLen - 1, "utf8");
    }

    private static leerCString(origen: Buffer, offset: number, maxLen: number): string {
        const sub = origen.subarray(offset, offset + maxLen);
        const fin = sub.indexOf(0);
        return sub.subarray(0, fin === -1 ? sub.length : fin).toString("utf8").trim();
    }

    static macHexABuffer8(macHex: string): Buffer {
        const limpio = macHex.replace(/[^0-9A-Fa-f]/g, "").padEnd(16, "0").slice(0, 16);
        return Buffer.from(limpio, "hex");
    }

    // Convierte el mensaje de excepción crudo enviado por el servidor ESP32 en un Error nativo de JS
    static decodificarExcepcionCruda(idProc: number, payload: Buffer): Error {
        const finNulo = payload.indexOf(0);
        const textoCrudo = payload
            .subarray(0, finNulo === -1 ? payload.length : finNulo)
            .toString("utf8")
            .trim();

        if (textoCrudo.length > 0) {
            return new Error(textoCrudo);
        }
        return new Error(`RuntimeError: excepción remota en procedimiento 0x${idProc.toString(16)}`);
    }

    // --- L1: Transporte y Enlace ---

    static codificarHandshakeUarts(version = 1): Buffer {
        const b = Buffer.alloc(1);
        b.writeUInt8(version & 0xFF, 0);
        return b;
    }

    static decodificarPerfilEnlaceUarts(payload: Buffer): PerfilEnlaceUartsCliente {
        if (payload.length < 4) {
            throw new Error("ProtocolError: respuesta de handshake UART-S incompleta");
        }
        return {
            versionServidor: payload.readUInt8(0),
            mtuBytes: payload.readUInt16LE(2)
        };
    }

    static codificarSuscripcionUdp(puertoLocal: number): Buffer {
        const b = Buffer.alloc(2);
        b.writeUInt16LE(puertoLocal & 0xFFFF, 0);
        return b;
    }

    static decodificarCanalStreamingUdp(payload: Buffer): CanalStreamingUdpCliente {
        if (payload.length < 2) {
            throw new Error("ProtocolError: respuesta de suscripción UDP incompleta");
        }
        return {
            puertoEmisorServidor: payload.readUInt16LE(0)
        };
    }

    // --- L2: Autenticación (RPAuth) ---

    static codificarLogin(usuario: string, clavePlana: string): Buffer {
        const b = Buffer.alloc(64);
        this.escribirCString(b, 0, 32, usuario);
        this.escribirCString(b, 32, 32, clavePlana);
        return b;
    }

    // SesionIniciadaDTO en ESP32: uint8_t rolAsignado (offset 0), 3B pad, uint32_t LE tiempoExpiracionSegundos (offset 4)
    static decodificarSesionIniciada(payload: Buffer): SesionIniciadaCliente {
        if (payload.length < 8) {
            throw new Error("ProtocolError: SesionIniciadaDTO inválido");
        }
        return {
            rolAsignado: payload.readUInt8(0),
            tiempoExpiracionSegundos: payload.readUInt32LE(4)
        };
    }

    // --- L3: Dominio Sensores (RPSensor) ---

    static codificarFiltroPorMac(macHex: string, limiteRegistros: number): Buffer {
        const b = Buffer.alloc(10);
        this.macHexABuffer8(macHex).copy(b, 0);
        b.writeUInt16LE(limiteRegistros & 0xFFFF, 8);
        return b;
    }

    static codificarFiltroPorMacYFecha(
        macHex: string,
        desdeSeg: number,
        hastaSeg: number,
        limiteRegistros: number
    ): Buffer {
        const b = Buffer.alloc(20);
        this.macHexABuffer8(macHex).copy(b, 0);
        b.writeUInt32LE(desdeSeg >>> 0, 8);
        b.writeUInt32LE(hastaSeg >>> 0, 12);
        b.writeUInt16LE(limiteRegistros & 0xFFFF, 16);
        return b;
    }

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

    // --- L3: Dominio Sistema (RPSistema) ---

    static codificarConfiguracionWiFi(
        ssidSta: string,
        claveSta: string,
        ssidAp: string,
        claveAp: string
    ): Buffer {
        const b = Buffer.alloc(192);
        this.escribirCString(b, 0, 32, ssidSta);
        this.escribirCString(b, 32, 64, claveSta);
        this.escribirCString(b, 96, 32, ssidAp);
        this.escribirCString(b, 128, 64, claveAp);
        return b;
    }

    // ResConfiguracionWiFiDTO en ESP32:
    // ssidSta[32] (0..31) + ssidAp[32] (32..63) + ipSta[16] (64..79) +
    // ipAp[16] (80..95) + conectadoARouter (96) + puntoAccesoActivo (97) = 98 bytes
    static decodificarConfiguracionWiFi(payload: Buffer): ConfiguracionWiFiCliente {
        if (payload.length < 98) {
            throw new Error(`ProtocolError: ResConfiguracionWiFiDTO incompleto (${payload.length}/98 bytes)`);
        }
        return {
            ssidSta: this.leerCString(payload, 0, 32),
            ssidAp: this.leerCString(payload, 32, 32),
            ipSta: this.leerCString(payload, 64, 16),
            ipAp: this.leerCString(payload, 80, 16),
            conectadoARouter: payload.readUInt8(96) !== 0,
            puntoAccesoActivo: payload.readUInt8(97) !== 0
        };
    }

    // DiagnosticoSistemaDTO en ESP32: uint32_t LE heapLibreBytes (0..3) + uint32_t LE uptimeSegundos (4..7) = 8 bytes
    static decodificarDiagnosticoSistema(payload: Buffer): DiagnosticoSistemaCliente {
        if (payload.length < 8) {
            throw new Error("ProtocolError: DiagnosticoSistemaDTO incompleto");
        }
        return {
            heapLibreBytes: payload.readUInt32LE(0),
            uptimeSegundos: payload.readUInt32LE(4)
        };
    }

    // --- L3: Dominio Usuarios (RPUsuario) ---

    static codificarConsultaUsuarioNombre(usuario: string): Buffer {
        const b = Buffer.alloc(20);
        this.escribirCString(b, 0, 20, usuario);
        return b;
    }

    static codificarFiltroListadoUsuarios(
        limiteRegistros = 32,
        offsetRegistros = 0,
        soloActivos = false
    ): Buffer {
        const b = Buffer.alloc(8);
        b.writeUInt16LE(limiteRegistros & 0xFFFF, 0);
        b.writeUInt16LE(offsetRegistros & 0xFFFF, 2);
        b.writeUInt8(soloActivos ? 1 : 0, 4);
        return b;
    }

    static codificarCrearUsuario(usuario: string, clavePlana: string, rol = 1): Buffer {
        const b = Buffer.alloc(52);
        this.escribirCString(b, 0, 20, usuario);
        this.escribirCString(b, 20, 28, clavePlana);
        b.writeUInt8(rol & 0xFF, 48);
        return b;
    }

    static codificarActualizarUsuario(usuario: string, nuevoRol: number, activa: boolean): Buffer {
        const b = Buffer.alloc(24);
        this.escribirCString(b, 0, 20, usuario);
        b.writeUInt8(nuevoRol & 0xFF, 20);
        b.writeUInt8(activa ? 1 : 0, 21);
        return b;
    }

    static codificarCambiarContrasena(usuario: string, claveActual: string, claveNueva: string): Buffer {
        const b = Buffer.alloc(76);
        this.escribirCString(b, 0, 20, usuario);
        this.escribirCString(b, 20, 28, claveActual);
        this.escribirCString(b, 48, 28, claveNueva);
        return b;
    }

    static decodificarPerfilUsuario(payload: Buffer): PerfilUsuarioCliente {
        if (payload.length < 68) {
            throw new Error("ProtocolError: PerfilUsuarioDTO incompleto");
        }
        return {
            idUsuario: payload.readInt32LE(0),
            usuario: this.leerCString(payload, 4, 20),
            rol: payload.readUInt8(24),
            intentosFallidos: payload.readUInt8(25),
            bloqueado: payload.readUInt8(26) !== 0,
            activa: payload.readUInt8(27) !== 0,
            creadoEl: this.leerCString(payload, 28, 20),
            actualizadoEl: this.leerCString(payload, 48, 20)
        };
    }

    static decodificarListaUsuarios(payload: Buffer): ResumenUsuarioCliente[] {
        if (payload.length < 4) {
            return [];
        }
        const cantidad = Math.min(payload.readUInt8(0), 32);
        const usuarios: ResumenUsuarioCliente[] = [];

        for (let i = 0; i < cantidad; i++) {
            const base = 4 + i * 28;
            if (base + 28 > payload.length) {
                break;
            }
            usuarios.push({
                idUsuario: payload.readInt32LE(base),
                usuario: this.leerCString(payload, base + 4, 16),
                rol: payload.readUInt8(base + 20),
                intentosFallidos: payload.readUInt8(base + 21),
                bloqueado: payload.readUInt8(base + 22) !== 0,
                activa: payload.readUInt8(base + 23) !== 0,
                actualizadoEpoch: payload.readUInt32LE(base + 24)
            });
        }
        return usuarios;
    }

    static decodificarEstadisticasUsuarios(payload: Buffer): EstadisticasUsuariosCliente {
        if (payload.length < 16) {
            throw new Error("ProtocolError: EstadisticasUsuariosDTO incompleto");
        }
        return {
            totalUsuarios: payload.readInt32LE(0),
            usuariosActivos: payload.readInt32LE(4),
            usuariosBloqueados: payload.readInt32LE(8),
            maxIntentosFallidos: payload.readInt32LE(12)
        };
    }
    
}

export interface PerfilUsuarioCliente {
    idUsuario: number;
    usuario: string;
    rol: number;
    intentosFallidos: number;
    bloqueado: boolean;
    activa: boolean;
    creadoEl: string;
    actualizadoEl: string;
}

export interface ResumenUsuarioCliente {
    idUsuario: number;
    usuario: string;
    rol: number;
    intentosFallidos: number;
    bloqueado: boolean;
    activa: boolean;
    actualizadoEpoch: number;
}

export interface EstadisticasUsuariosCliente {
    totalUsuarios: number;
    usuariosActivos: number;
    usuariosBloqueados: number;
    maxIntentosFallidos: number;
}