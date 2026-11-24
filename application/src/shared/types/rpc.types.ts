export type MedioActivoCliente = "NINGUNO" | "UART-S" | "WIFI";

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

// --- L1 / L2 Conexión y Autenticación ---

export interface PerfilEnlaceUartsCliente {
  versionServidor: number;
  mtuBytes: number;
}

export interface SesionIniciadaCliente {
  rolAsignado: number;
  tiempoExpiracionSegundos: number;
}

// --- Dominio 1: Hardware, Red, MicroSD, Alarmas y BIOS RTOS ---

export interface InfoRedPublicaDTO {
  ipSta: string;
  ipAp: string;
  conectadoARouter: boolean;
  puntoAccesoActivo: boolean;
  rssiDbm: number;
  clientesAutenticados: number;
}

export interface ConfiguracionWiFiCliente {
  ssidSta: string;
  ssidAp: string;
  ipSta: string;
  ipAp: string;
  conectadoARouter: boolean;
  puntoAccesoActivo: boolean;
}

export interface DiagnosticoSistemaCliente {
  heapLibreBytes: number;
  uptimeSegundos: number;
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

export interface ResApagadoSeguroDTO {
  apagadoIniciado: boolean;
  alarmaSilenciada: boolean;
  ledApagado: boolean;
  tareaDeCierreLanzada: boolean;
  motivo: string;
}

// --- Dominio 2: Sensores, Censos, Usuarios, Configuración y Auditorías ---

export interface LecturaSensorCliente {
  timestampMs: number; // Unix timestamp en segundos (uint32_t)
  idSensor: number;
  macHex: string;
  temperaturaCorregida: number;
}

export interface PerfilSensorCliente {
  idSensor: number;
  mac: string;
  nombre: string;
  ubicacion: string;
  descripcion: string;
  umbralSup: number;
  umbralInf: number;
  errorSup: number;
  errorInf: number;
  offset: number;
  activa: boolean;
  fechaCreacion: string;
  fechaModificacion: string;
}

export interface ResumenSensorCliente {
  idSensor: number;
  mac: string;
  nombre: string;
  ubicacion: string;
  umbralSup: number;
  umbralInf: number;
  offset: number;
  activa: boolean;
}

export interface EstadisticasSensoresCliente {
  totalSensores: number;
  sensoresActivos: number;
  sensoresInactivos: number;
}

export interface ResumenEstadisticoCensoCliente {
  totalLecturas: number;
  temperaturaMinima: number;
  temperaturaMaxima: number;
  temperaturaPromedio: number;
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

export interface CensoRowDTO {
  idCenso: number;
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: EstadoSensor;
  fechaCreacion: number; // Unix timestamp en segundos
}

export interface FiltroPaginadoCensoDTO {
  idSensor: number; // 0 = todos
  limiteRegistros: number;
  offsetRegistros: number;
}

export interface CrearCensoManualDTO {
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: EstadoSensor;
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

export interface ConfiguracionSistemaDTO {
  idConfiguracionSistema: number;
  zonaHorariaMin: number;
  retencionCensoDias: number;
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

export interface ResultadoEnvioSmtpDTO {
  codigoSmtp: number;
  exito: boolean;
  correosEnviados: number;
  eventosActualizados: number;
  detalle: string;
}

export type CrearOActualizarCorreoDTO = Omit<CorreoItemDTO, "ultimoEstadoEnvio">;

export interface ServidorNtpItemDTO {
  idServer: number;
  server: string;
  prioridad: number;
  activa: boolean;
}

export interface AuditoriaSensorRowDTO {
  idAuditoriaSensor: number;
  idSensor: number;
  idCenso: number;
  estadoPrevioSensor: EstadoSensor;
  estadoNuevoSensor: EstadoSensor;
  estadoBus: EstadoBus;
  tipoEventoSensor: TipoEvento;
  criticidadSensor: CriticidadEvento;
  estadoReporte: EstadoEnvioCorreo;
  activa: boolean;
  fechaCreacion: string;
}

export interface EstadisticasAuditoriaSensoresDTO {
  totalEventos: number;
  eventosCriticos: number;
  reportesPendientes: number;
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

// --- Dominio 3: Diagrama ER y Terminal SQL ---

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

// --- Contrato completo de window.service.client ---

export interface ServiceClientAPI {
  // Transporte L1 y Auth L2
  listarPuertosSerie(): Promise<string[]>;
  conectarPorSerie(rutaPuerto: string): Promise<PerfilEnlaceUartsCliente>;
  conectarPorWiFi(ipServidor: string): Promise<void>;
  desconectar(): Promise<void>;
  estaConectado(): Promise<boolean>;
  obtenerMedioActivo(): Promise<MedioActivoCliente>;
  obtenerLogsVisor(): Promise<string[]>;
  limpiarLogsVisor(): Promise<void>;
  invalidarCache(): Promise<void>;
  iniciarSesion(usuario: string, clavePlana: string): Promise<SesionIniciadaCliente>;
  cerrarSesion(): Promise<void>;

  // Dominio 1: Sistema, Hardware, Red y BIOS RTOS
  obtenerInfoRedPublica(): Promise<InfoRedPublicaDTO>;
  obtenerEspacioMicroSD(): Promise<EspacioMicroSdDTO>;
  obtenerEstadoAlarmasActivas(): Promise<EstadoAlarmasActivasDTO>;
  ejecutarDiagnosticoSistema(): Promise<DiagnosticoSistemaCliente>;
  silenciarAlarmaActiva(): Promise<EstadoHardwareDirectoDTO>;
  obtenerInfoCompletaSistemaESP(): Promise<InfoCompletaEspDTO>;
  obtenerEstadoTareasRTOS(): Promise<TelemetriaTareaRtosDTO[]>;
  obtenerEstadoHardwareDirecto(): Promise<EstadoHardwareDirectoDTO>;
  controlarHardwareDirecto(cmd: ComandoHardwareDirectoDTO): Promise<EstadoHardwareDirectoDTO>;
  leerHelpLogBios(): Promise<ReporteHelpLogBiosDTO>;
  obtenerConfiguracionWiFi(): Promise<ConfiguracionWiFiCliente>;
  actualizarConfiguracionWiFi(
    ssidSta: string,
    claveSta: string,
    ssidAp: string,
    claveAp: string
  ): Promise<ConfiguracionWiFiCliente>;
  reiniciarHardware(): Promise<void>;
  solicitarApagadoSeguro(): Promise<ResApagadoSeguroDTO>;

  // Dominio 2: Sensores y Censos
  suscribirSensoresTiempoReal(): Promise<LecturaSensorCliente[]>;
  obtenerUltimoLoteTiempoReal(): Promise<LecturaSensorCliente[]>;
  listarSensores(
    limiteRegistros?: number,
    offsetRegistros?: number,
    soloActivos?: boolean
  ): Promise<ResumenSensorCliente[]>;
  obtenerSensorPorMac(mac: string): Promise<PerfilSensorCliente>;
  obtenerCensosPorMac(macHex: string, limiteRegistros?: number): Promise<LecturaSensorCliente[]>;
  obtenerCensosPorMacYFecha(
    macHex: string,
    timestampDesde: number,
    timestampHasta: number,
    limiteRegistros?: number
  ): Promise<LecturaSensorCliente[]>;
  obtenerCensosPorMacYFechaFragmentado(
    macHex: string,
    timestampDesde: number,
    timestampHasta: number,
    maxRegistros?: number
  ): Promise<LecturaSensorCliente[]>;
  listarCensosPaginados(filtro: FiltroPaginadoCensoDTO): Promise<CensoRowDTO[]>;
  listarPicosCensos(
    idSensor: number,
    orden: "ALTOS" | "BAJOS",
    limite?: number,
    maxMuestras?: number
  ): Promise<CensoRowDTO[]>;
  obtenerEstadisticasSensores(): Promise<EstadisticasSensoresCliente>;
  obtenerEstadisticasCensoPorMac(mac: string): Promise<ResumenEstadisticoCensoCliente>;
  obtenerCalibracionAmbiente(): Promise<CalibracionAmbienteDTO>;
  actualizarMetadatosSensor(
    mac: string,
    nombre: string,
    ubicacion: string,
    descripcion: string,
    activa?: boolean
  ): Promise<PerfilSensorCliente>;
  actualizarCalibracionSensor(
    mac: string,
    umbralSup: number,
    umbralInf: number,
    errorSup: number,
    errorInf: number,
    offset: number
  ): Promise<PerfilSensorCliente>;
  eliminarSensorPorMac(mac: string): Promise<void>;
  actualizarCalibracionAmbiente(
    dto: ActualizarCalibracionAmbienteInputDTO
  ): Promise<CalibracionAmbienteDTO>;
  crearCensoManual(dto: CrearCensoManualDTO): Promise<CensoRowDTO>;
  eliminarCensoPorId(idCenso: number): Promise<void>;
  purgarCensosAnterioresA(timestampLimiteSeg: number): Promise<number>;

  // Dominio 2: Usuarios
  obtenerUsuarioPorNombre(usuario: string): Promise<PerfilUsuarioCliente>;
  cambiarContrasenaUsuario(
    usuario: string,
    claveActual: string,
    claveNueva: string
  ): Promise<PerfilUsuarioCliente>;
  listarUsuarios(
    limiteRegistros?: number,
    offsetRegistros?: number,
    soloActivos?: boolean
  ): Promise<ResumenUsuarioCliente[]>;
  crearUsuario(usuario: string, clavePlana: string, rol?: number): Promise<PerfilUsuarioCliente>;
  actualizarRolYEstadoUsuario(
    usuario: string,
    nuevoRol: number,
    activa: boolean
  ): Promise<PerfilUsuarioCliente>;
  desbloquearUsuario(usuario: string): Promise<PerfilUsuarioCliente>;
  eliminarUsuario(usuario: string): Promise<void>;
  obtenerEstadisticasUsuarios(): Promise<EstadisticasUsuariosCliente>;

  // Dominio 2: Configuración, SMTP, Correos y NTP
  obtenerConfiguracionSistema(): Promise<ConfiguracionSistemaDTO>;
  actualizarConfiguracionSistema(
    dto: ActualizarConfigSistemaInputDTO
  ): Promise<ConfiguracionSistemaDTO>;
  obtenerConfiguracionSmtp(): Promise<ConfiguracionSmtpDTO>;
  actualizarConfiguracionSmtp(dto: ActualizarConfigSmtpInputDTO): Promise<ConfiguracionSmtpDTO>;
  listarCorreos(): Promise<CorreoItemDTO[]>;
  guardarCorreo(dto: CrearOActualizarCorreoDTO): Promise<CorreoItemDTO>;
  eliminarCorreoPorId(idCorreo: number): Promise<void>;
  enviarCorreoPruebaSmtp(destinatario?: string): Promise<ResultadoEnvioSmtpDTO>;
  despacharAlertasCorreoAhora(): Promise<ResultadoEnvioSmtpDTO>;
  listarServidoresNtp(): Promise<ServidorNtpItemDTO[]>;
  guardarServidorNtp(dto: ServidorNtpItemDTO): Promise<ServidorNtpItemDTO>;
  eliminarServidorNtpPorId(idServer: number): Promise<void>;

  // Dominio 2: Auditorías
  listarAuditoriaSensorPorMac(
    macHex: string,
    limiteRegistros?: number
  ): Promise<AuditoriaSensorRowDTO[]>;
  obtenerEstadisticasAuditoriaSensores(): Promise<EstadisticasAuditoriaSensoresDTO>;
  actualizarEstadoReporteAuditoriaSensor(
    idAuditoriaSensor: number,
    nuevoEstado: EstadoEnvioCorreo
  ): Promise<AuditoriaSensorRowDTO>;
  eliminarAuditoriaSensorPorId(idAuditoriaSensor: number): Promise<void>;
  listarAuditoriaSistema(
    limiteRegistros?: number,
    offsetRegistros?: number
  ): Promise<AuditoriaSistemaRowDTO[]>;
  eliminarAuditoriaSistemaPorId(idAuditoriaSistema: number): Promise<void>;
  purgarAuditoriaSistema(fechaHoraIsoLimite: string): Promise<number>;
  listarAuditoriaAuth(
    limiteRegistros?: number,
    offsetRegistros?: number
  ): Promise<ResumenAuditoriaAuthDTO[]>;
  obtenerAuditoriaAuthPorId(idAuditoriaAuth: number): Promise<AuditoriaAuthDetalleDTO>;
  verificarIntegridadAuditoriaAuth(): Promise<ResultadoIntegridadAuthDTO>;
  eliminarAuditoriaAuthPorId(idAuditoriaAuth: number): Promise<void>;
  purgarAuditoriaAuth(timestampLimiteSeg: number): Promise<number>;

  // Dominio 3: Estructura ER y Terminal SQL
  obtenerEstructuraBaseDatos(): Promise<EstructuraCompletaDbDTO>;
  obtenerColumnasTablaEstructura(nombreTabla: string): Promise<ColumnaEstructuraDTO[]>;
  ejecutarSqlTerminal(sentenciaSql: string): Promise<ResultadoSqlTerminalDTO>;
}