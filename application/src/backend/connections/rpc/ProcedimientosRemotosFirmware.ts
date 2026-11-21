// ============================================================================
// ADAPTADOR AL TRANSPORTE RPC EXISTENTE DE ELECTRON
// ============================================================================

import {
  clienteFirmware,
  OpcionesInvocacionRPC,
  PrioridadRPC,
} from "./FirmwareRPCClient";

import { limpiarFloat32 } from "./SchemaBuilder";

// Mapa de política de caché (ms) para consultas que NO cambian a cada instante.
// Cualquier procedimiento de escritura (no listado aquí) corre con prioridad ALTA e invalida la caché.
const POLITICA_LECTURA_POR_NOMBRE: Record<string, { cacheTtlMs: number }> = {
  // En vivo / tiempo real
  obtenerEstadoAlarmasActivas:          { cacheTtlMs: 0 },
  obtenerEstadoTareasRTOS:              { cacheTtlMs: 0 },
  obtenerInfoCompletaSistemaESP:        { cacheTtlMs: 0 },
  obtenerEstadoHardwareDirecto:         { cacheTtlMs: 0 },
  verificarIntegridadAuditoriaAuth:     { cacheTtlMs: 0 },

  // Telemetría semi-estática y listados paginados
  obtenerInfoRedPublica:                { cacheTtlMs: 5000 },
  obtenerEspacioMicroSD:                { cacheTtlMs: 30000 },
  leerHelpLogBios:                      { cacheTtlMs: 10000 },
  obtenerEstadisticasAuditoriaSensores: { cacheTtlMs: 20000 },
  listarAuditoriaSensorPorMac:          { cacheTtlMs: 15000 },
  listarSensosPaginados:                { cacheTtlMs: 15000 },
  listarAuditoriaSistema:               { cacheTtlMs: 15000 },
  listarAuditoriaAuth:                  { cacheTtlMs: 15000 },

  // Configuración y esquema
  obtenerCalibracionAmbiente:           { cacheTtlMs: 60000 },
  obtenerConfiguracionSistema:          { cacheTtlMs: 60000 },
  obtenerConfiguracionSmtp:             { cacheTtlMs: 60000 },
  listarCorreos:                        { cacheTtlMs: 60000 },
  listarServidoresNtp:                  { cacheTtlMs: 60000 },
  obtenerAuditoriaAuthPorId:            { cacheTtlMs: 60000 },
  obtenerEstructuraBaseDatos:           { cacheTtlMs: 120000 },
  obtenerColumnasTablaEstructura:       { cacheTtlMs: 120000 },
};

export interface InvocadorBinarioRPC {
  invocarPorId(
    idProcedimiento: number,
    payload?: Uint8Array,
    opciones?: OpcionesInvocacionRPC
  ): Promise<Uint8Array>;
}

// Equivalente exacto a hashNombreProcedimiento() en RPC_base.hpp
export function hashNombreProcedimiento(nombre: string): number {
  let h = 2166136261 >>> 0;
  for (let i = 0; i < nombre.length; i++) {
    h ^= nombre.charCodeAt(i) & 0xff;
    h = Math.imul(h, 16777619) >>> 0;
  }
  const id15 = (h ^ (h >>> 15)) & 0x7fff;
  return id15 < 0x0300 ? id15 + 0x0300 : id15;
}

// ============================================================================
// 2. ENUMS COMPARTIDOS CON PgDtoTypes.hpp Y RPSistema.hpp
// ============================================================================

export enum RolUsuario {
  NINGUNO = 0,
  NO_ADMIN = 1,
  ADMIN = 2,
}

export enum CriticidadEvento {
  NULA = 0,
  BAJA = 1,
  MEDIA = 2,
  ALTA = 3,
}

export enum TipoEvento {
  INFORMATIVO = 0,
  ADVERTENCIA = 1,
  EXCEPCION = 2,
}

export enum TipoRecepcionCorreo {
  RECIBIR_TODOS = 0,
  RECIBIR_EVENTOS = 1,
  RECIBIR_EVENTOS_CRITICOS = 2,
}

export enum EstadoEnvioCorreo {
  PENDIENTE = 0,
  ENVIADO = 1,
  FALLIDO = 2,
}

export enum MedioConexion {
  INTERNO = 0,
  UART_S = 1,
  WIFI_STA = 2,
  WIFI_AP = 3,
}

export enum ResultadoAuditoria {
  EXITO = 0,
  DENEGADO_ROL = 1,
  CREDENCIAL_INVALIDA = 2,
  USUARIO_BLOQUEADO = 3,
  INTRUSION_TERMINAL = 4,
  ERROR_VALIDACION = 5,
}

export enum FuenteTiempo {
  SIN_SINCRONIZAR = 0,
  NTP = 1,
  RTC = 2,
  CLIENTE = 3,
}

export enum EstadoSensor {
  OK = 0,
  ALARMA_SUPERIOR = 1,
  ALARMA_INFERIOR = 2,
  SLOT_VACIO = 3,
  DIRECCION_ROM_INVALIDA = 4,
  FAMILIA_NO_COMPATIBLE = 5,
  SCRATCHPAD_VACIO_CEROS = 6,
  ERROR_CRC_SCRATCHPAD = 7,
  CONVERSION_PENDIENTE = 8,
  TIMEOUT_CONVERSION = 9,
  FALLO_ESCRITURA_EEPROM = 10,
  DESCONECTADO = 11,
  ALIMENTACION_INSUFICIENTE = 12,
  REINICIO_ENERGIA_85C = 13,
  CORTOCIRCUITO_VDD = 14,
  CORTOCIRCUITO_GND = 15,
  CIRCUITO_ABIERTO = 16,
}

export enum EstadoBus {
  OK = 0,
  LINEA_EN_CORTO_GND = 1,
  SIN_PULSO_PRESENCIA = 2,
  TOPOLOGIA_MODIFICADA = 3,
  FALLO_CONVERSION_GLOBAL = 4,
  LIMITE_SENSORES_EXCEDIDO = 5,
}

export enum AccionHardwareDirecto {
  NINGUNA = 0,
  LED_ENCENDER = 1,
  LED_APAGAR = 2,
  BUZZ_SILENCIAR_ALARMA = 3,
  BUZZ_TONO_PRUEBA = 4,
  OLED_PUBLICAR_MENSAJE = 5,
  OLED_LIMPIAR = 6,
  SENSORES_REESCANEAR = 7,
}

// ============================================================================
// 3. DTOs DE LOS 3 DOMINIOS
// ============================================================================

export interface InfoRedPublicaDTO {
  ipSta: string;
  ipAp: string;
  conectadoARouter: boolean;
  puntoAccesoActivo: boolean;
  rssiDbm: number;
  clientesAutenticados: number;
}

export interface EspacioMicroSdDTO {
  totalKb: number;
  usadoKb: number;
  libreKb: number;
  inicializada: boolean;
}

export interface SensorEnAlarmaItemDTO {
  slot: number;
  idSensor: number;
  mac: string;
  temperaturaCorregida: number;
  umbralSuperior: number;
  umbralInferior: number;
  estado: EstadoSensor;
}

export interface EstadoAlarmasActivasDTO {
  hayAlarmaActiva: boolean;
  bocinaSonando: boolean;
  ledAlarmaEncendido: boolean;
  estadoBus: EstadoBus;
  totalSensoresEnAlarma: number;
  sensoresAfectados: SensorEnAlarmaItemDTO[];
}

export interface InfoCompletaEspDTO {
  heapLibreBytes: number;
  heapMinimoHistoricoBytes: number;
  heapMaxBloqueBytes: number;
  heapTotalBytes: number;
  frecuenciaCpuMhz: number;
  flashTamanoBytes: number;
  uptimeSegundos: number;
  incidentesBios: number;
  revisionChip: number;
  modeloChip: string;
  motivoUltimoReset: string;
}

export interface TelemetriaTareaRtosDTO {
  nombre: string;
  nucleo: number;
  prioridad: number;
  periodoMs: number;
  stackReservadoBytes: number;
  stackLibreMinBytes: number;
  ciclosEjecutados: number;
  maxTiempoEjecucionUs: number;
  msDesdeUltimoLatido: number;
  reiniciosBios: number;
  activa: boolean;
  enEjecucionPaso: boolean;
}

export interface EstadoHardwareDirectoDTO {
  sdInicializada: boolean;
  oledInicializada: boolean;
  ledEncendido: boolean;
  buzzerEncendido: boolean;
  alarmaFisicaActiva: boolean;
  estadoBus1Wire: EstadoBus;
  sensoresDetectadosBus: number;
  sensoresAsignados: number;
}

export interface ComandoHardwareDirectoDTO {
  accion: AccionHardwareDirecto;
  frecuenciaHz?: number;
  duracionMs?: number;
  lineaOled1?: string;
  lineaOled2?: string;
}

export interface ReporteHelpLogBiosDTO {
  incidentesTotalesBios: number;
  ultimosRegistrosLog: string;
}

export interface CalibracionAmbienteDTO {
  idCalibracion: number;
  altura: number;
  presionAtmosferica: number;
  tempCongelacionRef: number;
  tempEbullicionRef: number;
  activa: boolean;
  fechaCreacion: number;
  fechaModificacion: number;
}

export type ActualizarCalibracionAmbienteInputDTO = Omit<
  CalibracionAmbienteDTO,
  "idCalibracion" | "fechaCreacion" | "fechaModificacion"
>;

export interface EstadisticasAuditoriaSensoresDTO {
  totalEventos: number;
  eventosCriticos: number;
  reportesPendientes: number;
}

export interface SensoRowDTO {
  idSenso: number;
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: EstadoSensor;
  fechaCreacion: number; // Unix timestamp en segundos
}

export interface FiltroPaginadoSensoDTO {
  idSensor: number; // 0 = todos
  limiteRegistros: number;
  offsetRegistros: number;
}

export interface CrearSensoManualDTO {
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: EstadoSensor;
}

export interface AuditoriaSensorRowDTO {
  idAuditoriaSensor: number;
  idSensor: number;
  idSenso: number;
  estadoPrevioSensor: EstadoSensor;
  estadoNuevoSensor: EstadoSensor;
  estadoBus: EstadoBus;
  tipoEventoSensor: TipoEvento;
  criticidadSensor: CriticidadEvento;
  estadoReporte: EstadoEnvioCorreo;
  activa: boolean;
  fechaCreacion: string;
}

export interface ConfiguracionSistemaDTO {
  idConfiguracionSistema: number;
  zonaHorariaMin: number;
  retencionSensoDias: number;
  retencionEventosDias: number;
  retencionAuditoriaDias: number;
  sdLibreMinimoKb: number;
  correoCooldownS: number;
  activa: boolean;
  fechaModificacion: number;
}

export type ActualizarConfigSistemaInputDTO = Omit<
  ConfiguracionSistemaDTO,
  "idConfiguracionSistema" | "fechaModificacion"
>;

export interface ConfiguracionSmtpDTO {
  idConfiguracionSmtp: number;
  hostSmtp: string;
  puerto: number;
  usarTls: boolean;
  usuario: string;
  passwordCifrado: string;
  emailRemitente: string;
  nombreRemitente: string;
  activa: boolean;
  fechaCreacion: string;
  fechaModificacion: string;
}

export type ActualizarConfigSmtpInputDTO = Omit<
  ConfiguracionSmtpDTO,
  "idConfiguracionSmtp" | "fechaCreacion" | "fechaModificacion"
>;

export interface CorreoItemDTO {
  idCorreo: number;
  nombre: string;
  apellido: string;
  correoElectronico: string;
  tipoRecepcionMensajes: TipoRecepcionCorreo;
  ultimoEstadoEnvio: EstadoEnvioCorreo;
  activa: boolean;
}

export type CrearOActualizarCorreoDTO = Omit<CorreoItemDTO, "ultimoEstadoEnvio">;

export interface ServidorNtpItemDTO {
  idServer: number;
  server: string;
  prioridad: number;
  activa: boolean;
}

export interface AuditoriaSistemaRowDTO {
  idAuditoriaSistema: number;
  criticidad: CriticidadEvento;
  tipoEventoSistema: TipoEvento;
  estadoBus: EstadoBus;
  espacioSdLibreKb: number;
  heapLibreBytes: number;
  resetReason: string;
  versionFirmware: string;
  detalle: string;
  internet: boolean;
  estadoReporte: EstadoEnvioCorreo;
  bootId: number;
  uptimeS: number;
  fuenteTiempo: FuenteTiempo;
  activa: boolean;
  fechaCreacion: string;
}

export interface ResumenAuditoriaAuthDTO {
  idAuditoriaAuth: number;
  seq: number;
  idUsuario: number;
  usuarioIntentado: string;
  accion: string;
  medioConexion: MedioConexion;
  origenRemoto: string;
  resultado: ResultadoAuditoria;
  criticidad: CriticidadEvento;
  fechaCreacion: number;
}

export interface AuditoriaAuthDetalleDTO extends ResumenAuditoriaAuthDTO {
  tablaAfectada: string;
  valorPrevio: string;
  valorNuevo: string;
  detalle: string;
  bootId: number;
  uptimeS: number;
  fuenteTiempo: FuenteTiempo;
  hashPrevio: string;
  hashRegistro: string;
}

export interface ResultadoIntegridadAuthDTO {
  cadenaIntegra: boolean;
  registrosVerificados: number;
}

export interface NodoTablaEstructuraDTO {
  nombreTabla: string;
  pkColumna: string;
  totalColumnas: number;
  totalIndices: number;
}

export interface RelacionFkEstructuraDTO {
  tablaOrigen: string;
  columnaOrigen: string;
  tablaDestino: string;
  columnaDestino: string;
}

export interface EstructuraCompletaDbDTO {
  totalTablas: number;
  totalRelacionesFk: number;
  totalEnums: number;
  tablas: NodoTablaEstructuraDTO[];
  relacionesFk: RelacionFkEstructuraDTO[];
}

export interface ColumnaEstructuraDTO {
  nombreColumna: string;
  tipoSql: string;
  longitudMaxima: number;
  esPrimaryKey: boolean;
  esNotNull: boolean;
  esUnique: boolean;
  esIdentity: boolean;
  esForeignKey: boolean;
  fkTabla: string;
  fkColumna: string;
}

export interface ResultadoSqlTerminalDTO {
  exito: boolean;
  sqlState: string;
  comandoEjecutado: string;
  filasAfectadas: number;
  tiempoEjecucionUs: number;
  salidaPsql: string;
}

// ============================================================================
// 4. PRIMITIVAS DE CODIFICACIÓN Y DECODIFICACIÓN BINARIA (XTENSA 32/64-BIT LE)
// ============================================================================

const utf8Encoder = new TextEncoder();
const utf8Decoder = new TextDecoder("utf-8");

function leerTextoFijo(buf: Uint8Array, offset: number, maxLen: number): string {
  const sub = buf.subarray(offset, offset + maxLen);
  const zeroIdx = sub.indexOf(0);
  const slice = zeroIdx >= 0 ? sub.subarray(0, zeroIdx) : sub;
  return utf8Decoder.decode(slice);
}

function escribirTextoFijo(
  buf: Uint8Array,
  offset: number,
  maxLen: number,
  texto?: string
): void {
  buf.fill(0, offset, offset + maxLen);
  if (!texto) return;
  const bytes = utf8Encoder.encode(texto);
  const n = Math.min(bytes.length, maxLen - 1);
  buf.set(bytes.subarray(0, n), offset);
}

function escribirMacHex8(buf: Uint8Array, offset: number, macHex: string): void {
  const limpio = String(macHex ?? "")
    .replace(/[^0-9A-Fa-f]/g, "")
    .padEnd(16, "0")
    .slice(0, 16);
  for (let i = 0; i < 8; i++) {
    buf[offset + i] = parseInt(limpio.slice(i * 2, i * 2 + 2), 16) & 0xff;
  }
}

function deserializarPgList<T>(
  buf: Uint8Array,
  offsetInicioItems: 1 | 2 | 4 | 8,
  tamanoItem: number,
  decodificarItem: (vista: DataView, bytes: Uint8Array, baseOffset: number) => T
): T[] {
  if (buf.byteLength < 1) return [];
  const cantidad = buf[0];
  const vista = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const resultado: T[] = [];

  for (let i = 0; i < cantidad; i++) {
    const pos = offsetInicioItems + i * tamanoItem;
    if (pos + tamanoItem > buf.byteLength) break;
    resultado.push(decodificarItem(vista, buf, pos));
  }
  return resultado;
}

// ============================================================================
// 5. DECODIFICADORES DE SALIDA POR PROCEDIMIENTO
// ============================================================================

function decodificarEstadoHardware(buf: Uint8Array): EstadoHardwareDirectoDTO {
  return {
    sdInicializada: buf[0] !== 0,
    oledInicializada: buf[1] !== 0,
    ledEncendido: buf[2] !== 0,
    buzzerEncendido: buf[3] !== 0,
    alarmaFisicaActiva: buf[4] !== 0,
    estadoBus1Wire: buf[5] as EstadoBus,
    sensoresDetectadosBus: buf[6],
    sensoresAsignados: buf[7],
  };
}

function decodificarCalibracionAmbiente(buf: Uint8Array): CalibracionAmbienteDTO {
  const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  return {
    idCalibracion: v.getInt32(0, true),
    altura: limpiarFloat32(v.getFloat32(4, true)),
    presionAtmosferica: limpiarFloat32(v.getFloat32(8, true)),
    tempCongelacionRef: limpiarFloat32(v.getFloat32(12, true)),
    tempEbullicionRef: limpiarFloat32(v.getFloat32(16, true)),
    activa: buf[20] !== 0,
    fechaCreacion: Number(v.getBigInt64(24, true)),
    fechaModificacion: Number(v.getBigInt64(32, true)),
  };
}

function decodificarAuditoriaSensor(
  v: DataView,
  buf: Uint8Array,
  base: number
): AuditoriaSensorRowDTO {
  return {
    idAuditoriaSensor: v.getInt32(base + 0, true),
    idSensor: v.getInt32(base + 4, true),
    idSenso: v.getInt32(base + 8, true),
    estadoPrevioSensor: buf[base + 12] as EstadoSensor,
    estadoNuevoSensor: buf[base + 13] as EstadoSensor,
    estadoBus: buf[base + 14] as EstadoBus,
    tipoEventoSensor: buf[base + 15] as TipoEvento,
    criticidadSensor: buf[base + 16] as CriticidadEvento,
    estadoReporte: buf[base + 17] as EstadoEnvioCorreo,
    activa: buf[base + 18] !== 0,
    fechaCreacion: leerTextoFijo(buf, base + 19, 20),
  };
}

function decodificarConfigSistema(buf: Uint8Array): ConfiguracionSistemaDTO {
  const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  return {
    idConfiguracionSistema: v.getInt32(0, true),
    zonaHorariaMin: v.getInt32(4, true),
    retencionSensoDias: v.getUint32(8, true),
    retencionEventosDias: v.getUint32(12, true),
    retencionAuditoriaDias: v.getUint32(16, true),
    sdLibreMinimoKb: v.getUint32(20, true),
    correoCooldownS: v.getUint32(24, true),
    activa: buf[28] !== 0,
    fechaModificacion: Number(v.getBigInt64(32, true)),
  };
}

function decodificarConfigSmtp(buf: Uint8Array): ConfiguracionSmtpDTO {
  const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  return {
    idConfiguracionSmtp: v.getInt32(0, true),
    hostSmtp: leerTextoFijo(buf, 4, 40),
    puerto: v.getUint32(44, true),
    usarTls: buf[48] !== 0,
    usuario: leerTextoFijo(buf, 49, 48),
    passwordCifrado: leerTextoFijo(buf, 97, 160),
    emailRemitente: leerTextoFijo(buf, 257, 48),
    nombreRemitente: leerTextoFijo(buf, 305, 32),
    activa: buf[337] !== 0,
    fechaCreacion: leerTextoFijo(buf, 338, 20),
    fechaModificacion: leerTextoFijo(buf, 358, 20),
  };
}

function decodificarCorreoItem(
  v: DataView,
  buf: Uint8Array,
  base: number
): CorreoItemDTO {
  return {
    idCorreo: v.getInt32(base + 0, true),
    nombre: leerTextoFijo(buf, base + 4, 24),
    apellido: leerTextoFijo(buf, base + 28, 24),
    correoElectronico: leerTextoFijo(buf, base + 52, 48),
    tipoRecepcionMensajes: buf[base + 100] as TipoRecepcionCorreo,
    ultimoEstadoEnvio: buf[base + 101] as EstadoEnvioCorreo,
    activa: buf[base + 102] !== 0,
  };
}

function decodificarServidorNtpItem(
  v: DataView,
  buf: Uint8Array,
  base: number
): ServidorNtpItemDTO {
  return {
    idServer: v.getInt32(base + 0, true),
    server: leerTextoFijo(buf, base + 4, 40),
    prioridad: v.getInt16(base + 44, true),
    activa: buf[base + 46] !== 0,
  };
}

function decodificarSensoRow(
  v: DataView,
  buf: Uint8Array,
  base: number
): SensoRowDTO {
  return {
    idSenso: v.getInt32(base + 0, true),
    idSensor: v.getInt32(base + 4, true),
    temperaturaCruda: limpiarFloat32(v.getFloat32(base + 8, true)),
    temperaturaCorregida: limpiarFloat32(v.getFloat32(base + 12, true)),
    estadoSensor: buf[base + 16] as EstadoSensor,
    fechaCreacion: Number(v.getBigInt64(base + 24, true)),
  };
}

// ============================================================================
// 6. CLIENTE DE PROCEDIMIENTOS REMOTOS (DOMINIOS 1, 2 Y 3)
// ============================================================================

export class ProcedimientosRemotosFirmware {
  constructor(private readonly rpc: InvocadorBinarioRPC) {}

  // Dentro de ProcedimientosRemotosFirmware (reemplaza líneas 611-617):
  private invocar(
    nombreProcedimiento: string,
    payload?: Uint8Array
  ): Promise<Uint8Array> {
    const idProc = hashNombreProcedimiento(nombreProcedimiento);
    const politica = POLITICA_LECTURA_POR_NOMBRE[nombreProcedimiento];
    const opciones: OpcionesInvocacionRPC = politica
      ? {
          deduplicable: true,
          prioridad: PrioridadRPC.NORMAL,
          cacheTtlMs: politica.cacheTtlMs,
          invalidaCache: false,
        }
      : {
          deduplicable: false,
          prioridad: PrioridadRPC.ALTA,
          cacheTtlMs: 0,
          invalidaCache: true,
        };
    return this.rpc.invocarPorId(idProc, payload, opciones);
  }

  // ==========================================================================
  // DOMINIO 1: HARDWARE Y SISTEMA (RPSistema.hpp)
  // ==========================================================================

  obtenerInfoRedPublica = async (): Promise<InfoRedPublicaDTO> => {
    const buf = await this.invocar("obtenerInfoRedPublica");
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      ipSta: leerTextoFijo(buf, 0, 16),
      ipAp: leerTextoFijo(buf, 16, 16),
      conectadoARouter: buf[32] !== 0,
      puntoAccesoActivo: buf[33] !== 0,
      rssiDbm: v.getInt32(36, true),
      clientesAutenticados: buf[40],
    };
  };

  obtenerEspacioMicroSD = async (): Promise<EspacioMicroSdDTO> => {
    const buf = await this.invocar("obtenerEspacioMicroSD");
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      totalKb: v.getUint32(0, true),
      usadoKb: v.getUint32(4, true),
      libreKb: v.getUint32(8, true),
      inicializada: buf[12] !== 0,
    };
  };

  obtenerEstadoAlarmasActivas = async (): Promise<EstadoAlarmasActivasDTO> => {
    const buf = await this.invocar("obtenerEstadoAlarmasActivas");
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    const total = Math.min(buf[4], 6);
    const sensoresAfectados: SensorEnAlarmaItemDTO[] = [];

    for (let i = 0; i < total; i++) {
      const base = 8 + i * 44;
      sensoresAfectados.push({
        slot: buf[base + 0],
        idSensor: v.getInt32(base + 4, true),
        mac: leerTextoFijo(buf, base + 8, 17),
        temperaturaCorregida: limpiarFloat32(v.getFloat32(base + 28, true)),
        umbralSuperior: limpiarFloat32(v.getFloat32(base + 32, true)),
        umbralInferior: limpiarFloat32(v.getFloat32(base + 36, true)),
        estado: buf[base + 40] as EstadoSensor,
      });
    }

    return {
      hayAlarmaActiva: buf[0] !== 0,
      bocinaSonando: buf[1] !== 0,
      ledAlarmaEncendido: buf[2] !== 0,
      estadoBus: buf[3] as EstadoBus,
      totalSensoresEnAlarma: total,
      sensoresAfectados,
    };
  };

  silenciarAlarmaActiva = async (): Promise<EstadoHardwareDirectoDTO> => {
    const buf = await this.invocar("silenciarAlarmaActiva");
    return decodificarEstadoHardware(buf);
  };

  obtenerInfoCompletaSistemaESP = async (): Promise<InfoCompletaEspDTO> => {
    const buf = await this.invocar("obtenerInfoCompletaSistemaESP");
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      heapLibreBytes: v.getUint32(0, true),
      heapMinimoHistoricoBytes: v.getUint32(4, true),
      heapMaxBloqueBytes: v.getUint32(8, true),
      heapTotalBytes: v.getUint32(12, true),
      frecuenciaCpuMhz: v.getUint32(16, true),
      flashTamanoBytes: v.getUint32(20, true),
      uptimeSegundos: v.getUint32(24, true),
      incidentesBios: v.getUint32(28, true),
      revisionChip: buf[32],
      modeloChip: leerTextoFijo(buf, 33, 24),
      motivoUltimoReset: leerTextoFijo(buf, 57, 24),
    };
  };

  obtenerEstadoTareasRTOS = async (): Promise<TelemetriaTareaRtosDTO[]> => {
    const buf = await this.invocar("obtenerEstadoTareasRTOS");
    return deserializarPgList(buf, 4, 48, (v, b, base) => ({
      nombre: leerTextoFijo(b, base + 0, 16),
      nucleo: v.getInt8(base + 16),
      prioridad: b[base + 17],
      periodoMs: v.getUint32(base + 20, true),
      stackReservadoBytes: v.getUint32(base + 24, true),
      stackLibreMinBytes: v.getUint32(base + 28, true),
      ciclosEjecutados: v.getUint32(base + 32, true),
      maxTiempoEjecucionUs: v.getUint32(base + 36, true),
      msDesdeUltimoLatido: v.getUint32(base + 40, true),
      reiniciosBios: v.getUint16(base + 44, true),
      activa: b[base + 46] !== 0,
      enEjecucionPaso: b[base + 47] !== 0,
    }));
  };

  obtenerEstadoHardwareDirecto = async (): Promise<EstadoHardwareDirectoDTO> => {
    const buf = await this.invocar("obtenerEstadoHardwareDirecto");
    return decodificarEstadoHardware(buf);
  };

  controlarHardwareDirecto = async (
    cmd: ComandoHardwareDirectoDTO
  ): Promise<EstadoHardwareDirectoDTO> => {
    const payload = new Uint8Array(50);
    const v = new DataView(payload.buffer);
    payload[0] = cmd.accion;
    v.setUint16(2, cmd.frecuenciaHz ?? 1200, true);
    v.setUint16(4, cmd.duracionMs ?? 200, true);
    escribirTextoFijo(payload, 6, 22, cmd.lineaOled1);
    escribirTextoFijo(payload, 28, 22, cmd.lineaOled2);

    const buf = await this.invocar("controlarHardwareDirecto", payload);
    return decodificarEstadoHardware(buf);
  };

  leerHelpLogBios = async (): Promise<ReporteHelpLogBiosDTO> => {
    const buf = await this.invocar("leerHelpLogBios");
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      incidentesTotalesBios: v.getUint32(0, true),
      ultimosRegistrosLog: leerTextoFijo(buf, 4, 512),
    };
  };

  // ==========================================================================
  // DOMINIO 2: BASE DE DATOS CRUD COMPLETO (RPSensor, RPConfiguracion, RPAuditoria)
  // ==========================================================================

  obtenerCalibracionAmbiente = async (): Promise<CalibracionAmbienteDTO> => {
    const buf = await this.invocar("obtenerCalibracionAmbiente");
    return decodificarCalibracionAmbiente(buf);
  };

  actualizarCalibracionAmbiente = async (
    dto: ActualizarCalibracionAmbienteInputDTO
  ): Promise<CalibracionAmbienteDTO> => {
    const payload = new Uint8Array(20);
    const v = new DataView(payload.buffer);
    v.setFloat32(0, dto.altura, true);
    v.setFloat32(4, dto.presionAtmosferica, true);
    v.setFloat32(8, dto.tempCongelacionRef, true);
    v.setFloat32(12, dto.tempEbullicionRef, true);
    payload[16] = dto.activa ? 1 : 0;

    const buf = await this.invocar("actualizarCalibracionAmbiente", payload);
    return decodificarCalibracionAmbiente(buf);
  };

  listarAuditoriaSensorPorMac = async (
    macHex: string,
    limiteRegistros = 32
  ): Promise<AuditoriaSensorRowDTO[]> => {
    const payload = new Uint8Array(10);
    escribirMacHex8(payload, 0, macHex);
    new DataView(payload.buffer).setUint16(8, limiteRegistros, true);

    const buf = await this.invocar("listarAuditoriaSensorPorMac", payload);
    return deserializarPgList(buf, 4, 40, decodificarAuditoriaSensor);
  };

  obtenerEstadisticasAuditoriaSensores =
    async (): Promise<EstadisticasAuditoriaSensoresDTO> => {
      const buf = await this.invocar("obtenerEstadisticasAuditoriaSensores");
      const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
      return {
        totalEventos: v.getInt32(0, true),
        eventosCriticos: v.getInt32(4, true),
        reportesPendientes: v.getInt32(8, true),
      };
    };

  listarSensosPaginados = async (
    filtro: FiltroPaginadoSensoDTO
  ): Promise<SensoRowDTO[]> => {
    const payload = new Uint8Array(8);
    const v = new DataView(payload.buffer);
    v.setInt32(0, filtro.idSensor, true);
    v.setUint16(4, filtro.limiteRegistros, true);
    v.setUint16(6, filtro.offsetRegistros, true);

    const buf = await this.invocar("listarSensosPaginados", payload);
    return deserializarPgList(buf, 8, 32, decodificarSensoRow);
  };

  listarPicosSensos = async (
    idSensor: number,
    orden: "ALTOS" | "BAJOS",
    limite = 15,
    maxMuestras = 320
  ): Promise<SensoRowDTO[]> => {
    const tope = Math.max(32, Math.min(maxMuestras, 640));
    const muestra: SensoRowDTO[] = [];

    for (let offset = 0; offset < tope; offset += 32) {
      const lote = await this.listarSensosPaginados({
        idSensor,
        limiteRegistros: 32,
        offsetRegistros: offset,
      });
      if (lote.length === 0) break;
      muestra.push(...lote);
      if (lote.length < 32) break;
    }

    const validas = muestra.filter(
      (r) =>
        r.estadoSensor === EstadoSensor.OK ||
        r.estadoSensor === EstadoSensor.ALARMA_SUPERIOR ||
        r.estadoSensor === EstadoSensor.ALARMA_INFERIOR
    );

    validas.sort((a, b) =>
      orden === "ALTOS"
        ? b.temperaturaCorregida - a.temperaturaCorregida
        : a.temperaturaCorregida - b.temperaturaCorregida
    );

    return validas.slice(0, Math.max(1, Math.min(limite, 64)));
  };

  crearSensoManual = async (dto: CrearSensoManualDTO): Promise<SensoRowDTO> => {
    const payload = new Uint8Array(16);
    const v = new DataView(payload.buffer);
    v.setInt32(0, dto.idSensor, true);
    v.setFloat32(4, dto.temperaturaCruda, true);
    v.setFloat32(8, dto.temperaturaCorregida, true);
    payload[12] = dto.estadoSensor;

    const buf = await this.invocar("crearSensoManual", payload);
    const vr = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return decodificarSensoRow(vr, buf, 0);
  };

  eliminarSensoPorId = async (idSenso: number): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idSenso, true);
    await this.invocar("eliminarSensoPorId", payload);
  };

  purgarSensosAnterioresA = async (
    timestampLimiteSeg: number
  ): Promise<number> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setUint32(0, timestampLimiteSeg, true);
    const buf = await this.invocar("purgarSensosAnterioresA", payload);
    return new DataView(buf.buffer, buf.byteOffset, buf.byteLength).getUint16(
      0,
      true
    );
  };

  actualizarEstadoReporteAuditoriaSensor = async (
    idAuditoriaSensor: number,
    nuevoEstado: EstadoEnvioCorreo
  ): Promise<AuditoriaSensorRowDTO> => {
    const payload = new Uint8Array(8);
    const v = new DataView(payload.buffer);
    v.setInt32(0, idAuditoriaSensor, true);
    payload[4] = nuevoEstado;

    const buf = await this.invocar(
      "actualizarEstadoReporteAuditoriaSensor",
      payload
    );
    const vr = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return decodificarAuditoriaSensor(vr, buf, 0);
  };

  eliminarAuditoriaSensorPorId = async (
    idAuditoriaSensor: number
  ): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idAuditoriaSensor, true);
    await this.invocar("eliminarAuditoriaSensorPorId", payload);
  };

  obtenerConfiguracionSistema = async (): Promise<ConfiguracionSistemaDTO> => {
    const buf = await this.invocar("obtenerConfiguracionSistema");
    return decodificarConfigSistema(buf);
  };

  actualizarConfiguracionSistema = async (
    dto: ActualizarConfigSistemaInputDTO
  ): Promise<ConfiguracionSistemaDTO> => {
    const payload = new Uint8Array(28);
    const v = new DataView(payload.buffer);
    v.setInt32(0, dto.zonaHorariaMin, true);
    v.setUint32(4, dto.retencionSensoDias, true);
    v.setUint32(8, dto.retencionEventosDias, true);
    v.setUint32(12, dto.retencionAuditoriaDias, true);
    v.setUint32(16, dto.sdLibreMinimoKb, true);
    v.setUint32(20, dto.correoCooldownS, true);
    payload[24] = dto.activa ? 1 : 0;

    const buf = await this.invocar("actualizarConfiguracionSistema", payload);
    return decodificarConfigSistema(buf);
  };

  obtenerConfiguracionSmtp = async (): Promise<ConfiguracionSmtpDTO> => {
    const buf = await this.invocar("obtenerConfiguracionSmtp");
    return decodificarConfigSmtp(buf);
  };

  actualizarConfiguracionSmtp = async (
    dto: ActualizarConfigSmtpInputDTO
  ): Promise<ConfiguracionSmtpDTO> => {
    const payload = new Uint8Array(336);
    const v = new DataView(payload.buffer);
    escribirTextoFijo(payload, 0, 40, dto.hostSmtp);
    v.setUint32(40, dto.puerto, true);
    payload[44] = dto.usarTls ? 1 : 0;
    escribirTextoFijo(payload, 45, 48, dto.usuario);
    escribirTextoFijo(payload, 93, 160, dto.passwordCifrado);
    escribirTextoFijo(payload, 253, 48, dto.emailRemitente);
    escribirTextoFijo(payload, 301, 32, dto.nombreRemitente);
    payload[333] = dto.activa ? 1 : 0;

    const buf = await this.invocar("actualizarConfiguracionSmtp", payload);
    return decodificarConfigSmtp(buf);
  };

  listarCorreos = async (): Promise<CorreoItemDTO[]> => {
    const buf = await this.invocar("listarCorreos");
    return deserializarPgList(buf, 4, 104, decodificarCorreoItem);
  };

  guardarCorreo = async (
    dto: CrearOActualizarCorreoDTO
  ): Promise<CorreoItemDTO> => {
    const payload = new Uint8Array(104);
    const v = new DataView(payload.buffer);
    v.setInt32(0, dto.idCorreo, true);
    escribirTextoFijo(payload, 4, 24, dto.nombre);
    escribirTextoFijo(payload, 28, 24, dto.apellido);
    escribirTextoFijo(payload, 52, 48, dto.correoElectronico);
    payload[100] = dto.tipoRecepcionMensajes;
    payload[101] = dto.activa ? 1 : 0;

    const buf = await this.invocar("guardarCorreo", payload);
    const vr = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return decodificarCorreoItem(vr, buf, 0);
  };

  eliminarCorreoPorId = async (idCorreo: number): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idCorreo, true);
    await this.invocar("eliminarCorreoPorId", payload);
  };

  listarServidoresNtp = async (): Promise<ServidorNtpItemDTO[]> => {
    const buf = await this.invocar("listarServidoresNtp");
    return deserializarPgList(buf, 4, 48, decodificarServidorNtpItem);
  };

  guardarServidorNtp = async (
    dto: ServidorNtpItemDTO
  ): Promise<ServidorNtpItemDTO> => {
    const payload = new Uint8Array(48);
    const v = new DataView(payload.buffer);
    v.setInt32(0, dto.idServer, true);
    escribirTextoFijo(payload, 4, 40, dto.server);
    v.setInt16(44, dto.prioridad, true);
    payload[46] = dto.activa ? 1 : 0;

    const buf = await this.invocar("guardarServidorNtp", payload);
    const vr = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return decodificarServidorNtpItem(vr, buf, 0);
  };

  eliminarServidorNtpPorId = async (idServer: number): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idServer, true);
    await this.invocar("eliminarServidorNtpPorId", payload);
  };

  listarAuditoriaSistema = async (
    limiteRegistros = 6,
    offsetRegistros = 0
  ): Promise<AuditoriaSistemaRowDTO[]> => {
    const payload = new Uint8Array(4);
    const vp = new DataView(payload.buffer);
    vp.setUint16(0, limiteRegistros, true);
    vp.setUint16(2, offsetRegistros, true);

    const buf = await this.invocar("listarAuditoriaSistema", payload);
    return deserializarPgList(buf, 4, 188, (v, b, base) => ({
      idAuditoriaSistema: v.getInt32(base + 0, true),
      criticidad: b[base + 4] as CriticidadEvento,
      tipoEventoSistema: b[base + 5] as TipoEvento,
      estadoBus: b[base + 6] as EstadoBus,
      espacioSdLibreKb: v.getUint32(base + 8, true),
      heapLibreBytes: v.getUint32(base + 12, true),
      resetReason: leerTextoFijo(b, base + 16, 24),
      versionFirmware: leerTextoFijo(b, base + 40, 16),
      detalle: leerTextoFijo(b, base + 56, 96),
      internet: b[base + 152] !== 0,
      estadoReporte: b[base + 153] as EstadoEnvioCorreo,
      bootId: v.getUint32(base + 156, true),
      uptimeS: v.getUint32(base + 160, true),
      fuenteTiempo: b[base + 164] as FuenteTiempo,
      activa: b[base + 165] !== 0,
      fechaCreacion: leerTextoFijo(b, base + 166, 20),
    }));
  };

  eliminarAuditoriaSistemaPorId = async (
    idAuditoriaSistema: number
  ): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idAuditoriaSistema, true);
    await this.invocar("eliminarAuditoriaSistemaPorId", payload);
  };

  purgarAuditoriaSistema = async (
    fechaHoraIsoLimite: string
  ): Promise<number> => {
    const payload = new Uint8Array(20);
    escribirTextoFijo(payload, 0, 20, fechaHoraIsoLimite);
    const buf = await this.invocar("purgarAuditoriaSistema", payload);
    return new DataView(buf.buffer, buf.byteOffset, buf.byteLength).getUint16(
      0,
      true
    );
  };

  listarAuditoriaAuth = async (
    limiteRegistros = 16,
    offsetRegistros = 0
  ): Promise<ResumenAuditoriaAuthDTO[]> => {
    const payload = new Uint8Array(4);
    const vp = new DataView(payload.buffer);
    vp.setUint16(0, limiteRegistros, true);
    vp.setUint16(2, offsetRegistros, true);

    const buf = await this.invocar("listarAuditoriaAuth", payload);
    return deserializarPgList(buf, 4, 80, (v, b, base) => ({
      idAuditoriaAuth: v.getInt32(base + 0, true),
      seq: v.getUint32(base + 4, true),
      idUsuario: v.getInt32(base + 8, true),
      usuarioIntentado: leerTextoFijo(b, base + 12, 20),
      accion: leerTextoFijo(b, base + 32, 24),
      medioConexion: b[base + 56] as MedioConexion,
      origenRemoto: leerTextoFijo(b, base + 57, 16),
      resultado: b[base + 73] as ResultadoAuditoria,
      criticidad: b[base + 74] as CriticidadEvento,
      fechaCreacion: v.getUint32(base + 76, true),
    }));
  };

  obtenerAuditoriaAuthPorId = async (
    idAuditoriaAuth: number
  ): Promise<AuditoriaAuthDetalleDTO> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idAuditoriaAuth, true);

    const buf = await this.invocar("obtenerAuditoriaAuthPorId", payload);
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      idAuditoriaAuth: v.getInt32(0, true),
      seq: Number(v.getBigUint64(8, true)),
      idUsuario: v.getInt32(16, true),
      usuarioIntentado: leerTextoFijo(buf, 20, 20),
      accion: leerTextoFijo(buf, 40, 32),
      tablaAfectada: leerTextoFijo(buf, 72, 32),
      valorPrevio: leerTextoFijo(buf, 104, 64),
      valorNuevo: leerTextoFijo(buf, 168, 64),
      detalle: leerTextoFijo(buf, 232, 120),
      medioConexion: buf[352] as MedioConexion,
      origenRemoto: leerTextoFijo(buf, 353, 46),
      resultado: buf[399] as ResultadoAuditoria,
      criticidad: buf[400] as CriticidadEvento,
      bootId: v.getUint32(404, true),
      uptimeS: v.getUint32(408, true),
      fuenteTiempo: buf[412] as FuenteTiempo,
      fechaCreacion: Number(v.getBigInt64(416, true)),
      hashPrevio: leerTextoFijo(buf, 424, 64),
      hashRegistro: leerTextoFijo(buf, 488, 64),
    };
  };

  verificarIntegridadAuditoriaAuth =
    async (): Promise<ResultadoIntegridadAuthDTO> => {
      const buf = await this.invocar("verificarIntegridadAuditoriaAuth");
      const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
      return {
        cadenaIntegra: buf[0] !== 0,
        registrosVerificados: v.getUint16(2, true),
      };
    };

  eliminarAuditoriaAuthPorId = async (
    idAuditoriaAuth: number
  ): Promise<void> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setInt32(0, idAuditoriaAuth, true);
    await this.invocar("eliminarAuditoriaAuthPorId", payload);
  };

  purgarAuditoriaAuth = async (timestampLimiteSeg: number): Promise<number> => {
    const payload = new Uint8Array(4);
    new DataView(payload.buffer).setUint32(0, timestampLimiteSeg, true);
    const buf = await this.invocar("purgarAuditoriaAuth", payload);
    return new DataView(buf.buffer, buf.byteOffset, buf.byteLength).getUint16(
      0,
      true
    );
  };

  // ==========================================================================
  // DOMINIO 3: TERMINAL SQL INTERACTIVA Y DIAGRAMA ER (RPSqlTerminal.hpp)
  // ==========================================================================

  obtenerEstructuraBaseDatos = async (): Promise<EstructuraCompletaDbDTO> => {
    const buf = await this.invocar("obtenerEstructuraBaseDatos");
    const totalTablas = Math.min(buf[0], 14);
    const totalRelacionesFk = Math.min(buf[1], 8);
    const totalEnums = buf[2];

    const tablas: NodoTablaEstructuraDTO[] = [];
    for (let i = 0; i < totalTablas; i++) {
      const base = 3 + i * 46;
      tablas.push({
        nombreTabla: leerTextoFijo(buf, base + 0, 24),
        pkColumna: leerTextoFijo(buf, base + 24, 20),
        totalColumnas: buf[base + 44],
        totalIndices: buf[base + 45],
      });
    }

    const relacionesFk: RelacionFkEstructuraDTO[] = [];
    for (let i = 0; i < totalRelacionesFk; i++) {
      const base = 647 + i * 80;
      relacionesFk.push({
        tablaOrigen: leerTextoFijo(buf, base + 0, 20),
        columnaOrigen: leerTextoFijo(buf, base + 20, 20),
        tablaDestino: leerTextoFijo(buf, base + 40, 20),
        columnaDestino: leerTextoFijo(buf, base + 60, 20),
      });
    }

    return {
      totalTablas,
      totalRelacionesFk,
      totalEnums,
      tablas,
      relacionesFk,
    };
  };

  obtenerColumnasTablaEstructura = async (
    nombreTabla: string
  ): Promise<ColumnaEstructuraDTO[]> => {
    const payload = new Uint8Array(32);
    escribirTextoFijo(payload, 0, 32, nombreTabla);

    const buf = await this.invocar("obtenerColumnasTablaEstructura", payload);
    return deserializarPgList(buf, 2, 72, (v, b, base) => ({
      nombreColumna: leerTextoFijo(b, base + 0, 24),
      tipoSql: leerTextoFijo(b, base + 24, 16),
      longitudMaxima: v.getUint16(base + 40, true),
      esPrimaryKey: b[base + 42] !== 0,
      esNotNull: b[base + 43] !== 0,
      esUnique: b[base + 44] !== 0,
      esIdentity: b[base + 45] !== 0,
      esForeignKey: b[base + 46] !== 0,
      fkTabla: leerTextoFijo(b, base + 47, 12),
      fkColumna: leerTextoFijo(b, base + 59, 12),
    }));
  };

  ejecutarSqlTerminal = async (
    sentenciaSql: string
  ): Promise<ResultadoSqlTerminalDTO> => {
    const payload = new Uint8Array(384);
    escribirTextoFijo(payload, 0, 384, sentenciaSql);

    const buf = await this.invocar("ejecutarSqlTerminal", payload);
    const v = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    return {
      exito: buf[0] !== 0,
      sqlState: leerTextoFijo(buf, 1, 8),
      comandoEjecutado: leerTextoFijo(buf, 9, 28),
      filasAfectadas: v.getUint16(38, true),
      tiempoEjecucionUs: v.getUint32(40, true),
      salidaPsql: leerTextoFijo(buf, 44, 1024),
    };
  };
}

// ============================================================================
// INSTANCIA EXPORTADA COMO OBJETO PLANO ENUMERABLE PARA clientServices.ts E IPC
// ============================================================================

// Al final del archivo (reemplaza líneas 1179-1195):
const instancia = new ProcedimientosRemotosFirmware({
  invocarPorId: async (
    idProcedimiento: number,
    payload?: Uint8Array,
    opciones?: OpcionesInvocacionRPC
  ): Promise<Uint8Array> => {
    const respuesta = await clienteFirmware.invocarPorId(
      idProcedimiento,
      payload ? Buffer.from(payload) : undefined,
      opciones
    );

    return new Uint8Array(
      respuesta.buffer,
      respuesta.byteOffset,
      respuesta.byteLength
    );
  },
});

// Objeto plano de funciones arrow: garantiza que { ...procedimientosRemotosFirmware }
// y Object.entries() en handles.ts expongan el 100% de los métodos sin exponer `rpc`
// y sin perder el contexto `this`.
export const procedimientosRemotosFirmware = {
  // Dominio 1: Hardware y Sistema
  obtenerInfoRedPublica: instancia.obtenerInfoRedPublica,
  obtenerEspacioMicroSD: instancia.obtenerEspacioMicroSD,
  obtenerEstadoAlarmasActivas: instancia.obtenerEstadoAlarmasActivas,
  silenciarAlarmaActiva: instancia.silenciarAlarmaActiva,
  obtenerInfoCompletaSistemaESP: instancia.obtenerInfoCompletaSistemaESP,
  obtenerEstadoTareasRTOS: instancia.obtenerEstadoTareasRTOS,
  obtenerEstadoHardwareDirecto: instancia.obtenerEstadoHardwareDirecto,
  controlarHardwareDirecto: instancia.controlarHardwareDirecto,
  leerHelpLogBios: instancia.leerHelpLogBios,

  // Dominio 2: CRUD Completo de Tablas
  obtenerCalibracionAmbiente: instancia.obtenerCalibracionAmbiente,
  actualizarCalibracionAmbiente: instancia.actualizarCalibracionAmbiente,
  listarAuditoriaSensorPorMac: instancia.listarAuditoriaSensorPorMac,
  obtenerEstadisticasAuditoriaSensores:
    instancia.obtenerEstadisticasAuditoriaSensores,
  listarSensosPaginados: instancia.listarSensosPaginados,
  listarPicosSensos: instancia.listarPicosSensos,
  crearSensoManual: instancia.crearSensoManual,
  eliminarSensoPorId: instancia.eliminarSensoPorId,
  purgarSensosAnterioresA: instancia.purgarSensosAnterioresA,
  actualizarEstadoReporteAuditoriaSensor:
    instancia.actualizarEstadoReporteAuditoriaSensor,
  eliminarAuditoriaSensorPorId: instancia.eliminarAuditoriaSensorPorId,
  obtenerConfiguracionSistema: instancia.obtenerConfiguracionSistema,
  actualizarConfiguracionSistema: instancia.actualizarConfiguracionSistema,
  obtenerConfiguracionSmtp: instancia.obtenerConfiguracionSmtp,
  actualizarConfiguracionSmtp: instancia.actualizarConfiguracionSmtp,
  listarCorreos: instancia.listarCorreos,
  guardarCorreo: instancia.guardarCorreo,
  eliminarCorreoPorId: instancia.eliminarCorreoPorId,
  listarServidoresNtp: instancia.listarServidoresNtp,
  guardarServidorNtp: instancia.guardarServidorNtp,
  eliminarServidorNtpPorId: instancia.eliminarServidorNtpPorId,
  listarAuditoriaSistema: instancia.listarAuditoriaSistema,
  eliminarAuditoriaSistemaPorId: instancia.eliminarAuditoriaSistemaPorId,
  purgarAuditoriaSistema: instancia.purgarAuditoriaSistema,
  listarAuditoriaAuth: instancia.listarAuditoriaAuth,
  obtenerAuditoriaAuthPorId: instancia.obtenerAuditoriaAuthPorId,
  verificarIntegridadAuditoriaAuth: instancia.verificarIntegridadAuditoriaAuth,
  eliminarAuditoriaAuthPorId: instancia.eliminarAuditoriaAuthPorId,
  purgarAuditoriaAuth: instancia.purgarAuditoriaAuth,

  // Dominio 3: Terminal SQL y Estructura ER
  obtenerEstructuraBaseDatos: instancia.obtenerEstructuraBaseDatos,
  obtenerColumnasTablaEstructura: instancia.obtenerColumnasTablaEstructura,
  ejecutarSqlTerminal: instancia.ejecutarSqlTerminal,
};