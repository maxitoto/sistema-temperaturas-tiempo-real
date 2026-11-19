import {
  CriticidadEvento,
  EstadoBus,
  EstadoEnvioCorreo,
  EstadoSensor,
  FuenteTiempo,
  MedioConexion,
  ResultadoAuditoria,
  RolUsuario,
  TipoEvento,
  TipoRecepcionCorreo,
} from "#types/rpc.types";

export const LABEL_ROL: Record<number, string> = {
  [RolUsuario.NINGUNO]: "NINGUNO",
  [RolUsuario.NO_ADMIN]: "NO_ADMIN (Operador)",
  [RolUsuario.ADMIN]: "ADMIN (Administrador)",
};

export const LABEL_ESTADO_SENSOR: Record<number, string> = {
  [EstadoSensor.OK]: "OK",
  [EstadoSensor.ALARMA_SUPERIOR]: "ALARMA_SUPERIOR (TH)",
  [EstadoSensor.ALARMA_INFERIOR]: "ALARMA_INFERIOR (TL)",
  [EstadoSensor.SLOT_VACIO]: "SLOT_VACIO",
  [EstadoSensor.DIRECCION_ROM_INVALIDA]: "DIRECCION_ROM_INVALIDA",
  [EstadoSensor.FAMILIA_NO_COMPATIBLE]: "FAMILIA_NO_COMPATIBLE",
  [EstadoSensor.SCRATCHPAD_VACIO_CEROS]: "SCRATCHPAD_VACIO_CEROS",
  [EstadoSensor.ERROR_CRC_SCRATCHPAD]: "ERROR_CRC_SCRATCHPAD",
  [EstadoSensor.CONVERSION_PENDIENTE]: "CONVERSION_PENDIENTE",
  [EstadoSensor.TIMEOUT_CONVERSION]: "TIMEOUT_CONVERSION",
  [EstadoSensor.FALLO_ESCRITURA_EEPROM]: "FALLO_ESCRITURA_EEPROM",
  [EstadoSensor.DESCONECTADO]: "DESCONECTADO",
  [EstadoSensor.ALIMENTACION_INSUFICIENTE]: "ALIMENTACION_INSUFICIENTE",
  [EstadoSensor.REINICIO_ENERGIA_85C]: "REINICIO_ENERGIA_85C",
  [EstadoSensor.CORTOCIRCUITO_VDD]: "CORTOCIRCUITO_VDD",
  [EstadoSensor.CORTOCIRCUITO_GND]: "CORTOCIRCUITO_GND",
  [EstadoSensor.CIRCUITO_ABIERTO]: "CIRCUITO_ABIERTO",
};

export const LABEL_ESTADO_BUS: Record<number, string> = {
  [EstadoBus.OK]: "OK",
  [EstadoBus.LINEA_EN_CORTO_GND]: "LINEA_EN_CORTO_GND",
  [EstadoBus.SIN_PULSO_PRESENCIA]: "SIN_PULSO_PRESENCIA",
  [EstadoBus.TOPOLOGIA_MODIFICADA]: "TOPOLOGIA_MODIFICADA",
  [EstadoBus.FALLO_CONVERSION_GLOBAL]: "FALLO_CONVERSION_GLOBAL",
  [EstadoBus.LIMITE_SENSORES_EXCEDIDO]: "LIMITE_SENSORES_EXCEDIDO",
};

export const LABEL_CRITICIDAD: Record<number, string> = {
  [CriticidadEvento.NULA]: "NULA",
  [CriticidadEvento.BAJA]: "BAJA",
  [CriticidadEvento.MEDIA]: "MEDIA",
  [CriticidadEvento.ALTA]: "ALTA",
};

export const LABEL_TIPO_EVENTO: Record<number, string> = {
  [TipoEvento.INFORMATIVO]: "INFORMATIVO",
  [TipoEvento.ADVERTENCIA]: "ADVERTENCIA",
  [TipoEvento.EXCEPCION]: "EXCEPCION",
};

export const LABEL_TIPO_RECEPCION: Record<number, string> = {
  [TipoRecepcionCorreo.RECIBIR_TODOS]: "RECIBIR_TODOS",
  [TipoRecepcionCorreo.RECIBIR_EVENTOS]: "RECIBIR_EVENTOS",
  [TipoRecepcionCorreo.RECIBIR_EVENTOS_CRITICOS]: "RECIBIR_EVENTOS_CRITICOS",
};

export const LABEL_ESTADO_ENVIO: Record<number, string> = {
  [EstadoEnvioCorreo.PENDIENTE]: "PENDIENTE",
  [EstadoEnvioCorreo.ENVIADO]: "ENVIADO",
  [EstadoEnvioCorreo.FALLIDO]: "FALLIDO",
};

export const LABEL_MEDIO_CONEXION: Record<number, string> = {
  [MedioConexion.INTERNO]: "INTERNO",
  [MedioConexion.UART_S]: "UART_S",
  [MedioConexion.WIFI_STA]: "WIFI_STA",
  [MedioConexion.WIFI_AP]: "WIFI_AP",
};

export const LABEL_RESULTADO_AUDITORIA: Record<number, string> = {
  [ResultadoAuditoria.EXITO]: "EXITO",
  [ResultadoAuditoria.DENEGADO_ROL]: "DENEGADO_ROL",
  [ResultadoAuditoria.CREDENCIAL_INVALIDA]: "CREDENCIAL_INVALIDA",
  [ResultadoAuditoria.USUARIO_BLOQUEADO]: "USUARIO_BLOQUEADO",
  [ResultadoAuditoria.INTRUSION_TERMINAL]: "INTRUSION_TERMINAL",
  [ResultadoAuditoria.ERROR_VALIDACION]: "ERROR_VALIDACION",
};

export const LABEL_FUENTE_TIEMPO: Record<number, string> = {
  [FuenteTiempo.SIN_SINCRONIZAR]: "SIN_SINCRONIZAR",
  [FuenteTiempo.NTP]: "NTP",
  [FuenteTiempo.RTC]: "RTC",
  [FuenteTiempo.CLIENTE]: "CLIENTE",
};