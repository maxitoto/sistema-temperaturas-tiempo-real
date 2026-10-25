// Archivo: ./src/backend/stubs/contratos.ts
import { OpcionesLlamada } from "../communication/layers/CapaAplicacion";

// ---------- Credencial unificada de Seguridad (Certificado TLS + Usuario/PIN) ----------
export interface CredencialRpcDTO {
  emisorCertificado?: string;
  firmaCertificado?: string;
  username: string;
  pinHash: string;
}

// ---------- DTOs de Dominio del Firmware ----------
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

export interface SensorFirmwareDTO {
  idSensor: number;
  hexadecimal: string;
  errorSuperior: number;
  errorInferior: number;
  umbralSuperior: number;
  umbralInferior: number;
  activa: number;
  fechaCreacion: number;
  fechaModificacion: number;
}

export interface GuardarSensorFirmwareInput {
  credencial: CredencialRpcDTO;
  hexadecimal: string;
  errorSuperior: number;
  errorInferior: number;
  umbralSuperior: number;
  umbralInferior: number;
}

export interface SensoFirmwareDTO {
  idSenso: number;
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: number;
  fechaCreacion: number;
}

export interface EventoSensorFirmwareDTO {
  idEventoSensor: number;
  idSensor: number;
  idSenso: number;
  estadoPrevioSensor: number;
  evento: number;
  criticidad: number;
  fechaCreacion: number;
}

export interface AuditoriaFirmwareRowDTO {
  idAuditoria: number;
  idUsuario: number;
  accion: string;
  detalle: string;
  fechaCreacion: number;
}

export interface CorreoFirmwareDTO {
  idCorreo: number;
  nombre: string;
  apellido: string;
  correoElectronico: string;
  tipoRecepcionMensajes: number;
  estadoEnvio: number;
  fechaModificacion: number;
}

export interface CrearCorreoFirmwareInput {
  credencial: CredencialRpcDTO;
  nombre: string;
  apellido?: string;
  correoElectronico: string;
  tipoRecepcionMensajes?: number;
}

export interface ConfigSmtpFirmwareResumenDTO {
  configurado: boolean;
  hostSmtp: string;
  puerto: number;
  usuario: string;
  emailRemitente: string;
  fechaModificacion: number;
}

export interface ConfigRedYNtpFirmwareDTO {
  ssidSta: string;
  ssidAp: string;
  fechaModificacion: number;
  servidoresNtp: string[];
}

export interface GuardarRedFirmwareInput {
  credencial: CredencialRpcDTO;
  ssidSta: string;
  passwordStaPlana: string;
  ssidAp: string;
  passwordApPlana: string;
}

export interface RespuestaReloj {
  unixSeg: number;
  zonaHoraria: string;
}

// ---------- Contrato: servicio que EXPONE el firmware (backend lo llama) ----------
export interface IFirmware {
  sumarYLeerEstado: (base: number, factor: number, opts?: OpcionesLlamada) => Promise<RespuestaCalculoFirmware>;
  guardarConfiguracionSegura: (dto: SolicitudConfigSeguraDTO, opts?: OpcionesLlamada) => Promise<RespuestaAuditoriaFirmwareDTO>;

  // Consultas DQL y Mutaciones DML hacia la MicroSD del Firmware
  listarSensoresFirmware: (cred: CredencialRpcDTO, opts?: OpcionesLlamada) => Promise<SensorFirmwareDTO[]>;
  guardarSensorFirmware: (input: GuardarSensorFirmwareInput, opts?: OpcionesLlamada) => Promise<SensorFirmwareDTO>;
  listarUltimosSensosFirmware: (cred: CredencialRpcDTO, idSensor?: number, limite?: number, opts?: OpcionesLlamada) => Promise<SensoFirmwareDTO[]>;
  listarUltimosEventosFirmware: (cred: CredencialRpcDTO, idSensor?: number, limite?: number, opts?: OpcionesLlamada) => Promise<EventoSensorFirmwareDTO[]>;
  listarUltimasAuditoriasFirmware: (cred: CredencialRpcDTO, limite?: number, opts?: OpcionesLlamada) => Promise<AuditoriaFirmwareRowDTO[]>;
  listarCorreosFirmware: (cred: CredencialRpcDTO, opts?: OpcionesLlamada) => Promise<CorreoFirmwareDTO[]>;
  crearCorreoFirmware: (input: CrearCorreoFirmwareInput, opts?: OpcionesLlamada) => Promise<CorreoFirmwareDTO>;
  obtenerConfigSmtpFirmware: (cred: CredencialRpcDTO, opts?: OpcionesLlamada) => Promise<ConfigSmtpFirmwareResumenDTO>;
  obtenerConfigRedYNtpFirmware: (cred: CredencialRpcDTO, opts?: OpcionesLlamada) => Promise<ConfigRedYNtpFirmwareDTO>;
  guardarConfigRedFirmware: (input: GuardarRedFirmwareInput, opts?: OpcionesLlamada) => Promise<{ ssidSta: string; ssidAp: string; fechaModificacion: number }>;
}

// ---------- Contrato: servicio que EXPONE el backend (firmware lo llama) ----------
export interface IBackend {
  sincronizarReloj(uptimeEspMs: number, motivo: string, certEsp: string, idNodo: number): RespuestaReloj;
}

// ---------- Nombres de procedimientos ----------
export const RPC = {
  SUMAR_Y_ESTADO: "RPC_SUMAR_Y_ESTADO",
  GUARDAR_CONFIG_SEGURA: "RPC_GUARDAR_CONFIG_SEGURA",
  SYNC_RELOJ: "RPC_SYNC_RELOJ",
  DB_LISTAR_SENSORES: "RPC_DB_LISTAR_SENSORES",
  DB_GUARDAR_SENSOR: "RPC_DB_GUARDAR_SENSOR",
  DB_ULTIMOS_SENSOS: "RPC_DB_ULTIMOS_SENSOS",
  DB_ULTIMOS_EVENTOS: "RPC_DB_ULTIMOS_EVENTOS",
  DB_ULTIMAS_AUDITORIAS: "RPC_DB_ULTIMAS_AUDITORIAS",
  DB_LISTAR_CORREOS: "RPC_DB_LISTAR_CORREOS",
  DB_CREAR_CORREO: "RPC_DB_CREAR_CORREO",
  DB_OBTENER_SMTP: "RPC_DB_OBTENER_SMTP",
  DB_OBTENER_RED_Y_NTP: "RPC_DB_OBTENER_RED_Y_NTP",
  DB_GUARDAR_RED: "RPC_DB_GUARDAR_RED",
} as const;