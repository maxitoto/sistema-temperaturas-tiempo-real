// ===== src/backend/stubs/contratos.ts =====
import { OpcionesLlamada } from "../communication/layers/CapaAplicacion";

// ---------- DTOs ----------
export interface RespuestaCalculoFirmware {
  resultado: number;
  estado: string;
  heapLibreBytes: number;
}

export interface SolicitudConfigSeguraDTO {
  emisorCertificado: string;
  firmaCertificado: string;
  username: string;
  pinHash: string;
  tempMin: number;
  tempMax: number;
  smtpPasswordPlana: string;
}

export interface RespuestaAuditoriaFirmwareDTO {
  idRegistro: number;
  tempMinGuardada: number;
  tempMaxGuardada: number;
  smtpDesencriptadaEnFirmware: string;
  modificadoPor: string;
  rolUsuario: string;
  medioUsado: string;
  totalFilasEnDB: number;
  hexCampoCifradoEnviado: string;
}

export interface RespuestaReloj {
  unixSeg: number;
  zonaHoraria: string;
}

// ---------- Contrato: servicio que EXPONE el firmware (backend lo llama) ----------
export interface IFirmware {
  sumarYLeerEstado(base: number, factor: number, opts?: OpcionesLlamada): Promise<RespuestaCalculoFirmware>;
  guardarConfiguracionSegura(dto: SolicitudConfigSeguraDTO, opts?: OpcionesLlamada): Promise<RespuestaAuditoriaFirmwareDTO>;
}

// ---------- Contrato: servicio que EXPONE el backend (firmware lo llama) ----------
export interface IBackend {
  sincronizarReloj(uptimeEspMs: number, motivo: string, certEsp: string, idNodo: number): RespuestaReloj;
}

// ---------- Nombres de procedimientos (una sola fuente de verdad) ----------
export const RPC = {
  SUMAR_Y_ESTADO: "RPC_SUMAR_Y_ESTADO",
  GUARDAR_CONFIG_SEGURA: "RPC_GUARDAR_CONFIG_SEGURA",
  SYNC_RELOJ: "RPC_SYNC_RELOJ",
} as const;