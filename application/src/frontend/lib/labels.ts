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

export const TEMP_MIN_FISICA_DS18B20 = -55.0;
export const TEMP_MAX_FISICA_DS18B20 = 126.5;

export function esTemperaturaFisicaDS18B20(temp: number): boolean {
  return (
    Number.isFinite(temp) &&
    temp >= TEMP_MIN_FISICA_DS18B20 &&
    temp <= TEMP_MAX_FISICA_DS18B20
  );
}

/**
 * Traduce valores centinela fuera de rango físico (-127, -250..-254) a su EstadoSensor
 * para evitar que se confundan con temperaturas reales por debajo de TL (-55 °C).
 */
export function detectarEstadoPorSentinela(temp: number): EstadoSensor | null {
  if (!Number.isFinite(temp)) return EstadoSensor.DESCONECTADO;
  if (Math.abs(temp - -127) < 0.6) return EstadoSensor.DESCONECTADO;
  if (Math.abs(temp - -250) < 0.6) return EstadoSensor.ALIMENTACION_INSUFICIENTE;
  if (Math.abs(temp - -251) < 0.6) return EstadoSensor.REINICIO_ENERGIA_85C;
  if (Math.abs(temp - -252) < 0.6) return EstadoSensor.CORTOCIRCUITO_VDD;
  if (Math.abs(temp - -253) < 0.6) return EstadoSensor.CORTOCIRCUITO_GND;
  if (Math.abs(temp - -254) < 0.6) return EstadoSensor.CIRCUITO_ABIERTO;
  if (!esTemperaturaFisicaDS18B20(temp)) return EstadoSensor.DESCONECTADO;
  return null;
}

/**
 * Resuelve el estado real de una lectura en vivo:
 * 1) Prioriza el estado reportado por el firmware si hay alarma activa.
 * 2) Si la temperatura es un centinela o está fuera de [-55, 126.5] °C, devuelve el fallo (nunca ALARMA_INFERIOR).
 * 3) Solo evalúa TL/TH sobre temperaturas físicamente válidas.
 */
export function resolverEstadoLecturaVivo(
  temp: number | null | undefined,
  umbralInf: number | undefined,
  umbralSup: number | undefined,
  estadoAlarmaFw?: EstadoSensor
): EstadoSensor {
  if (estadoAlarmaFw !== undefined && estadoAlarmaFw !== EstadoSensor.OK) {
    return estadoAlarmaFw;
  }
  if (temp === null || temp === undefined) {
    return EstadoSensor.OK;
  }
  const estadoSentinela = detectarEstadoPorSentinela(temp);
  if (estadoSentinela !== null) {
    return estadoSentinela;
  }
  if (umbralSup !== undefined && temp >= umbralSup) {
    return EstadoSensor.ALARMA_SUPERIOR;
  }
  if (umbralInf !== undefined && temp <= umbralInf) {
    return EstadoSensor.ALARMA_INFERIOR;
  }
  return EstadoSensor.OK;
}

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
  [EstadoSensor.DESCONECTADO]: "SIN RESPUESTA (DESCONECTADO)",
  [EstadoSensor.ALIMENTACION_INSUFICIENTE]: "ALIMENTACION_INSUFICIENTE",
  [EstadoSensor.REINICIO_ENERGIA_85C]: "REINICIO_ENERGIA_85C",
  [EstadoSensor.CORTOCIRCUITO_VDD]: "CORTOCIRCUITO_VDD",
  [EstadoSensor.CORTOCIRCUITO_GND]: "CORTOCIRCUITO_GND",
  [EstadoSensor.CIRCUITO_ABIERTO]: "CIRCUITO_ABIERTO",
};

export const DESCRIPCION_ESTADO_SENSOR: Record<number, string> = {
  [EstadoSensor.OK]:
    "Comunicación 1-Wire estable y temperatura dentro del rango seguro [TL, TH].",
  [EstadoSensor.ALARMA_SUPERIOR]:
    "La temperatura corregida superó el umbral máximo (TH) configurado para este sensor.",
  [EstadoSensor.ALARMA_INFERIOR]:
    "La temperatura corregida descendió por debajo del umbral mínimo (TL) configurado.",
  [EstadoSensor.SLOT_VACIO]: "Ranura de memoria sin sensor físico asignado.",
  [EstadoSensor.DIRECCION_ROM_INVALIDA]:
    "La dirección ROM de 64 bits leída en el bus no pasó la verificación CRC.",
  [EstadoSensor.FAMILIA_NO_COMPATIBLE]:
    "El dispositivo conectado al bus 1-Wire no pertenece a la familia DS18B20 (0x28).",
  [EstadoSensor.SCRATCHPAD_VACIO_CEROS]:
    "El sensor devolvió solo ceros: posible fallo de alimentación o línea de datos retenida a GND.",
  [EstadoSensor.ERROR_CRC_SCRATCHPAD]:
    "Trama corrupta al leer el scratchpad: revisar ruido eléctrico, longitud del cable o resistencia pull-up de 4.7 kΩ.",
  [EstadoSensor.CONVERSION_PENDIENTE]:
    "El sensor aún está completando la conversión analógico-digital interna.",
  [EstadoSensor.TIMEOUT_CONVERSION]:
    "Se agotó el tiempo máximo esperando la lectura del sensor en el bus 1-Wire.",
  [EstadoSensor.FALLO_ESCRITURA_EEPROM]:
    "No se pudo confirmar la escritura de configuración en la EEPROM interna del DS18B20.",
  [EstadoSensor.DESCONECTADO]:
    "El sensor sigue registrado y vigilado en el sistema, pero dejó de responder en el bus 1-Wire (revisar cable amarillo de datos, empalmes o alimentación). Permanece en alerta roja hasta reconectarse.",
  [EstadoSensor.ALIMENTACION_INSUFICIENTE]:
    "Tensión insuficiente o conversión fallida (0x07FF / 127.94 °C): revisar línea VDD de 3.3 V.",
  [EstadoSensor.REINICIO_ENERGIA_85C]:
    "El DS18B20 devolvió 85.0 °C de Power-On Reset (corte breve en VDD o falso contacto).",
  [EstadoSensor.CORTOCIRCUITO_VDD]: "Línea de datos en cortocircuito contra VDD.",
  [EstadoSensor.CORTOCIRCUITO_GND]: "Línea de datos en cortocircuito contra masa (GND).",
  [EstadoSensor.CIRCUITO_ABIERTO]:
    "Línea de datos abierta o sin resistencia pull-up conectada.",
};

export function describirEstadoSensor(estado: number): string {
  return (
    DESCRIPCION_ESTADO_SENSOR[estado] ??
    "Estado de falla reportado por el controlador del bus 1-Wire."
  );
}

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