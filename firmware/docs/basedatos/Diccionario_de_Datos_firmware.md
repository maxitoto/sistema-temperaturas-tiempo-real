-- ============================================================================
-- 1. TIPOS ENUMERADOS ENRIQUECIDOS
-- ============================================================================

CREATE TYPE "EstadoSensor" AS ENUM (
  'OK',                       -- 0
  'ALARMA_SUPERIOR',          -- 1
  'ALARMA_INFERIOR',          -- 2
  'SLOT_VACIO',               -- -1
  'DIRECCION_ROM_INVALIDA',   -- -2
  'FAMILIA_NO_COMPATIBLE',    -- -3
  'SCRATCHPAD_VACIO_CEROS',   -- -4
  'ERROR_CRC_SCRATCHPAD',     -- -5
  'CONVERSION_PENDIENTE',     -- -6
  'TIMEOUT_CONVERSION',       -- -7
  'FALLO_ESCRITURA_EEPROM',   -- -8
  'DESCONECTADO',             -- -127
  'ALIMENTACION_INSUFICIENTE',-- -250 (127.94 C)
  'REINICIO_ENERGIA_85C',     -- -251 (Glitch 85.00 C)
  'CORTOCIRCUITO_VDD',        -- -252
  'CORTOCIRCUITO_GND',        -- -253
  'CIRCUITO_ABIERTO'          -- -254
);

CREATE TYPE "EstadoBus" AS ENUM (
  'OK',
  'LINEA_EN_CORTO_GND',
  'SIN_PULSO_PRESENCIA',
  'TOPOLOGIA_MODIFICADA',
  'FALLO_CONVERSION_GLOBAL',
  'LIMITE_SENSORES_EXCEDIDO'
);

CREATE TYPE "EventoGestor" AS ENUM (
  'NINGUNO',
  'BUS_INICIALIZADO',
  'NUEVO_SENSOR_DETECTADO',
  'SENSOR_DESCONECTADO_EN_CALIENTE',
  'CONVERSION_COMPLETADA',
  'ERROR_BUS_ELECTRICO',
  'ALARMA_DETECTADA_EN_BUS'
);

CREATE TYPE "CriticidadEvento" AS ENUM (
  'CRITICIDAD_BAJA',
  'CRITICIDAD_MEDIA',
  'CRITICIDAD_CRITICA'
);

CREATE TYPE "TipoRecepcionCorreo" AS ENUM (
  'RECIBIR_TODOS',
  'RECIBIR_EVENTOS',
  'RECIBIR_EVENTOS_CRITICOS'
);

CREATE TYPE "EstadoEnvioCorreo" AS ENUM (
  'ENVIO_PENDIENTE',
  'ENVIO_ENVIADO',
  'ENVIO_FALLIDO'
);

CREATE TYPE "RolUsuario" AS ENUM (
  'ROL_ANONIMO',
  'ROL_LECTOR',
  'ROL_OPERADOR',
  'ROL_ADMIN'
);

CREATE TYPE "MedioConexion" AS ENUM (
  'MEDIO_INTERNO',
  'MEDIO_USB',
  'MEDIO_WIFI_STA',
  'MEDIO_WIFI_AP'
);

CREATE TYPE "ResultadoAuditoria" AS ENUM (
  'RESULTADO_EXITO',
  'RESULTADO_DENEGADO_ROL',
  'RESULTADO_CREDENCIAL_INVALIDA',
  'RESULTADO_INTRUSION_TERMINAL',
  'RESULTADO_ERROR_VALIDACION'
);

-- ============================================================================
-- 2. TABLAS DE CONFIGURACIÓN Y BOOTSTRAP (EL ESP32 LAS LEE AL DESPERTAR)
-- ============================================================================

CREATE TABLE "calibracion_fw" (
  "id_calibracion" uint8 PRIMARY KEY DEFAULT 1,
  "altura" float NOT NULL DEFAULT 0,
  "presion_atmosferica" float NOT NULL DEFAULT 1013.25,
  "temp_congelacion_ref" float NOT NULL DEFAULT 0.0,
  "temp_ebullicion_ref" float NOT NULL DEFAULT 100.0,
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "lts_fw" (
  "id_lts" uint8 PRIMARY KEY DEFAULT 1,
  "nombre_certificado" varchar(24) NOT NULL,
  "certificado_fingerprint" varchar(64) NOT NULL,
  "clave_aes_canal" varchar(64) NOT NULL,
  "secreto_hmac_sesion" varchar(64) NOT NULL,
  "cadena_certificados" varchar(64),
  "fecha_inicio_vigencia" uint32,
  "fecha_fin_vigencia" uint32,
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "configuracion_conectividad_fw" (
  "id_configuracion_conectividad" uint8 PRIMARY KEY DEFAULT 1,
  "SSID_STA" varchar(33) NOT NULL,
  "STA_password_aes" varchar(128),
  "SSID_AP" varchar(33) NOT NULL,
  "AP_password_aes" varchar(128),
  "puerto_tcp" uint16 NOT NULL DEFAULT 80,
  "puerto_udp" uint16 NOT NULL DEFAULT 81,
  "baudios_usb" uint32 NOT NULL DEFAULT 115200,
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "servidores_ntp_fw" (
  "id_server" SERIAL PRIMARY KEY,
  "server" varchar(40) UNIQUE NOT NULL,
  "prioridad" uint8 NOT NULL DEFAULT 1,
  "timeout_ms" uint16 NOT NULL DEFAULT 3000,
  "fallos_consecutivos" uint16 NOT NULL DEFAULT 0,
  "ultimo_sync_exitoso" uint32 DEFAULT 0,
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "configuracion_smtp_fw" (
  "id_configuracion_smtp" uint8 PRIMARY KEY DEFAULT 1,
  "host_smtp" varchar(40) NOT NULL,
  "puerto" uint16 NOT NULL DEFAULT 465,
  "usar_tls" uint8 NOT NULL DEFAULT 1,
  "usuario" varchar(48) NOT NULL,
  "password_aes" varchar(128) NOT NULL,
  "email_remitente" varchar(48) NOT NULL,
  "nombre_remitente" varchar(32) DEFAULT 'Alerta Heladera Lab',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

-- ============================================================================
-- 3. TABLAS DE SEGURIDAD, USUARIOS, CORREOS Y AUDITORÍA
-- ============================================================================

CREATE TABLE "usuarios_fw" (
  "id_usuario" SERIAL PRIMARY KEY,
  "usuario" varchar(20) UNIQUE NOT NULL,
  "salt" varchar(16) NOT NULL,
  "contrasena_hash" varchar(64) NOT NULL,
  "rol" "RolUsuario" NOT NULL DEFAULT 'ROL_LECTOR',
  "intentos_fallidos" uint8 NOT NULL DEFAULT 0,
  "bloqueado" uint8 NOT NULL DEFAULT 0,
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "correos_fw" (
  "id_correo" SERIAL PRIMARY KEY,
  "nombre" varchar(24) NOT NULL,
  "apellido" varchar(24),
  "correo_electronico" varchar(48) UNIQUE NOT NULL,
  "tipo_recepcion_mensajes" "TipoRecepcionCorreo" NOT NULL DEFAULT 'RECIBIR_TODOS',
  "ultimo_estado_envio" "EstadoEnvioCorreo" NOT NULL DEFAULT 'ENVIO_PENDIENTE',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "auditoria_fw" (
  "id_auditoria" SERIAL PRIMARY KEY,
  "id_usuario" uint16 NOT NULL,
  "accion" varchar(32) NOT NULL,
  "tabla_afectada" varchar(32),
  "valor_previo" varchar(64),
  "valor_nuevo" varchar(64),
  "detalle" varchar(120) NOT NULL,
  "medio_conexion" "MedioConexion" NOT NULL DEFAULT 'MEDIO_INTERNO',
  "origen_remoto" varchar(24) DEFAULT 'LOCAL',
  "resultado" "ResultadoAuditoria" NOT NULL DEFAULT 'RESULTADO_EXITO',
  "criticidad" "CriticidadEvento" NOT NULL DEFAULT 'CRITICIDAD_BAJA',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32
);

-- ============================================================================
-- 4. TABLAS DE SENSORES, TELEMETRÍA Y EVENTOS (MISION CRÍTICA)
-- ============================================================================

CREATE TABLE "sensor_fw" (
  "id_sensor" SERIAL PRIMARY KEY,
  "hexadecimal" char(16) UNIQUE NOT NULL,
  "mac2" char(2) NOT NULL,
  "nombre" varchar(32) NOT NULL,
  "descripcion" varchar(64),
  "ubicacion" varchar(32) NOT NULL,
  "ubicacion_descripcion" varchar(64),
  "offset_calibracion" float NOT NULL DEFAULT 0.0,
  "correcion_superior" float NOT NULL DEFAULT 0.0,
  "correcion_inferior" float NOT NULL DEFAULT 0.0,
  "umbral_superior" int8 NOT NULL DEFAULT 126,
  "umbral_inferior" int8 NOT NULL DEFAULT -126,
  "resolucion_bits" uint8 NOT NULL DEFAULT 12,
  "alimentacion_parasita" uint8 NOT NULL DEFAULT 0,
  "estado_actual" "EstadoSensor" NOT NULL DEFAULT 'OK',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "senso_fw" (
  "id_senso" SERIAL PRIMARY KEY,
  "id_sensor" uint16 NOT NULL,
  "temperatura_cruda" float NOT NULL,
  "temperatura_corregida" float NOT NULL,
  "estado_sensor" "EstadoSensor" NOT NULL DEFAULT 'OK',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32
);

CREATE TABLE "evento_sensor_fw" (
  "id_evento_sensor" SERIAL PRIMARY KEY,
  "id_sensor" uint16 NOT NULL,
  "id_senso" uint32 NOT NULL,
  "estado_previo_sensor" "EstadoSensor" NOT NULL,
  "evento" "EstadoSensor" NOT NULL,
  "criticidad" "CriticidadEvento" NOT NULL,
  "notificado_smtp" "EstadoEnvioCorreo" NOT NULL DEFAULT 'ENVIO_PENDIENTE',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32,
  "fecha_modificacion" uint32
);

CREATE TABLE "evento_sistema_fw" (
  "id_evento_sistema" SERIAL PRIMARY KEY,
  "estado_bus" "EstadoBus" NOT NULL DEFAULT 'OK',
  "evento_gestor" "EventoGestor" NOT NULL DEFAULT 'NINGUNO',
  "criticidad" "CriticidadEvento" NOT NULL DEFAULT 'CRITICIDAD_MEDIA',
  "espacio_sd_libre_kb" uint32 DEFAULT 0,
  "heap_libre_bytes" uint32 DEFAULT 0,
  "detalle" varchar(96) NOT NULL,
  "notificado_smtp" "EstadoEnvioCorreo" NOT NULL DEFAULT 'ENVIO_PENDIENTE',
  "activa" uint8 DEFAULT 1,
  "fecha_creacion" uint32
);

-- ============================================================================
-- 5. CLAVES FORÁNEAS E INTEGRIDAD REFERENCIAL
-- ============================================================================

ALTER TABLE "senso_fw" ADD FOREIGN KEY ("id_sensor") REFERENCES "sensor_fw" ("id_sensor") ON DELETE CASCADE;
ALTER TABLE "evento_sensor_fw" ADD FOREIGN KEY ("id_sensor") REFERENCES "sensor_fw" ("id_sensor") ON DELETE CASCADE;
ALTER TABLE "evento_sensor_fw" ADD FOREIGN KEY ("id_senso") REFERENCES "senso_fw" ("id_senso") ON DELETE CASCADE;
ALTER TABLE "auditoria_fw" ADD FOREIGN KEY ("id_usuario") REFERENCES "usuarios_fw" ("id_usuario") ON DELETE RESTRICT;

-- ============================================================================
-- 6. ÍNDICES DE OPTIMIZACIÓN DE CONSULTAS
-- ============================================================================

CREATE INDEX "idx_sensor_hex"          ON "sensor_fw" ("hexadecimal");
CREATE INDEX "idx_senso_sensor_fecha"  ON "senso_fw" ("id_sensor", "fecha_creacion" DESC);
CREATE INDEX "idx_evento_sensor_crit"  ON "evento_sensor_fw" ("criticidad", "fecha_creacion" DESC);
CREATE INDEX "idx_evento_smtp_pend"    ON "evento_sensor_fw" ("notificado_smtp");
CREATE INDEX "idx_auditoria_fecha"     ON "auditoria_fw" ("fecha_creacion" DESC);
CREATE INDEX "idx_auditoria_usuario"   ON "auditoria_fw" ("id_usuario");
CREATE INDEX "idx_ntp_prioridad"       ON "servidores_ntp_fw" ("prioridad" ASC);