#pragma once
#include <Arduino.h>
#include "PgSqlEngine.hpp"

namespace DbSql::Schema {

    // ========================================================================
    // 1. DEFINICIÓN SQL PURA DE LOS 10 TIPOS ENUM (orden 0..N = PgDtoTypes.hpp)
    // ========================================================================
    static constexpr const char* DDL_ENUMS_SQL[] = {
        "CREATE TYPE IF NOT EXISTS enum_rol_usuario AS ENUM ('NINGUNO', 'NO_ADMIN', 'ADMIN');",

        "CREATE TYPE IF NOT EXISTS criticidad_evento_enum AS ENUM ('NULA', 'BAJA', 'MEDIA', 'ALTA');",

        "CREATE TYPE IF NOT EXISTS tipo_evento_enum AS ENUM ('INFORMATIVO', 'ADVERTENCIA', 'EXCEPCION');",

        "CREATE TYPE IF NOT EXISTS tipo_recepcion_correo_enum AS ENUM ('RECIBIR_TODOS', 'RECIBIR_EVENTOS', 'RECIBIR_EVENTOS_CRITICOS');",

        "CREATE TYPE IF NOT EXISTS estado_envio_correo_enum AS ENUM ('PENDIENTE', 'ENVIADO', 'FALLIDO');",

        "CREATE TYPE IF NOT EXISTS medio_conexion_enum AS ENUM ('INTERNO', 'UART_S', 'WIFI_STA', 'WIFI_AP');",

        "CREATE TYPE IF NOT EXISTS resultado_auditoria_enum AS ENUM ('EXITO', 'DENEGADO_ROL', 'CREDENCIAL_INVALIDA', 'USUARIO_BLOQUEADO', 'INTRUSION_TERMINAL', 'ERROR_VALIDACION');",

        "CREATE TYPE IF NOT EXISTS fuente_tiempo_enum AS ENUM ('SIN_SINCRONIZAR', 'NTP', 'RTC', 'CLIENTE');",

        "CREATE TYPE IF NOT EXISTS estado_sensor_enum AS ENUM ("
            "'OK', 'ALARMA_SUPERIOR', 'ALARMA_INFERIOR', 'SLOT_VACIO', "
            "'DIRECCION_ROM_INVALIDA', 'FAMILIA_NO_COMPATIBLE', 'SCRATCHPAD_VACIO_CEROS', "
            "'ERROR_CRC_SCRATCHPAD', 'CONVERSION_PENDIENTE', 'TIMEOUT_CONVERSION', "
            "'FALLO_ESCRITURA_EEPROM', 'DESCONECTADO', 'ALIMENTACION_INSUFICIENTE', "
            "'REINICIO_ENERGIA_85C', 'CORTOCIRCUITO_VDD', 'CORTOCIRCUITO_GND', 'CIRCUITO_ABIERTO');",

        "CREATE TYPE IF NOT EXISTS estado_bus_enum AS ENUM ("
            "'OK', 'LINEA_EN_CORTO_GND', 'SIN_PULSO_PRESENCIA', "
            "'TOPOLOGIA_MODIFICADA', 'FALLO_CONVERSION_GLOBAL', 'LIMITE_SENSORES_EXCEDIDO');"
    };

    // ========================================================================
    // 2. DEFINICIÓN SQL PURA DE LAS 12 TABLAS DEL SISTEMA
    // ========================================================================
    static constexpr const char* DDL_TABLES_SQL[] = {
        // 1. ConfiguracionRed
        "CREATE TABLE IF NOT EXISTS ConfiguracionRed ("
            "id_configuracion_red SERIAL PRIMARY KEY, "
            "ssid_sta VARCHAR(35) NOT NULL DEFAULT '', "
            "password_sta VARCHAR(64) NOT NULL DEFAULT '', "
            "ssid_ap VARCHAR(35) NOT NULL DEFAULT 'ESP32-SecureAP', "
            "password_ap VARCHAR(64) NOT NULL DEFAULT 'ESP32secure1234@', "
            "creado_el TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "actualizado_el TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ");",

        // 2. ConfiguracionSmtp
        "CREATE TABLE IF NOT EXISTS ConfiguracionSmtp ("
            "id_configuracion_smtp INTEGER PRIMARY KEY DEFAULT 1, "
            "host_smtp VARCHAR(40) NOT NULL DEFAULT '', "
            "puerto INTEGER NOT NULL DEFAULT 465, "
            "usar_tls BOOLEAN NOT NULL DEFAULT TRUE, "
            "usuario VARCHAR(48) NOT NULL DEFAULT '', "
            "password_cifrado VARCHAR(160) NOT NULL DEFAULT '', "
            "email_remitente VARCHAR(48) NOT NULL DEFAULT '', "
            "nombre_remitente VARCHAR(32) NOT NULL DEFAULT 'Laboratorio UNTDF', "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "fecha_modificacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ");",

        // 3. Calibracion
        "CREATE TABLE IF NOT EXISTS Calibracion ("
            "id_calibracion INTEGER PRIMARY KEY DEFAULT 1, "
            "altura REAL NOT NULL DEFAULT 115.0, "
            "presion_atmosferica REAL NOT NULL DEFAULT 1013.25, "
            "temp_congelacion_ref REAL NOT NULL DEFAULT 0.0, "
            "temp_ebullicion_ref REAL NOT NULL DEFAULT 100.0, "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion BIGINT NOT NULL DEFAULT 0, "
            "fecha_modificacion BIGINT NOT NULL DEFAULT 0"
        ");",

        // 4. ServidoresNtp
        "CREATE TABLE IF NOT EXISTS ServidoresNtp ("
            "id_server SERIAL PRIMARY KEY, "
            "server VARCHAR(40) NOT NULL UNIQUE, "
            "prioridad SMALLINT NOT NULL DEFAULT 1, "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion BIGINT NOT NULL DEFAULT 0, "
            "fecha_modificacion BIGINT NOT NULL DEFAULT 0"
        ");",

        // 5. ConfiguracionSistema
        "CREATE TABLE IF NOT EXISTS ConfiguracionSistema ("
            "id_configuracion_sistema INTEGER PRIMARY KEY DEFAULT 1, "
            "zona_horaria_min INTEGER NOT NULL DEFAULT -180, "
            "retencion_censo_dias INTEGER NOT NULL DEFAULT 365, "
            "retencion_eventos_dias INTEGER NOT NULL DEFAULT 730, "
            "retencion_auditoria_dias INTEGER NOT NULL DEFAULT 1095, "
            "sd_libre_minimo_kb INTEGER NOT NULL DEFAULT 51200, "
            "correo_cooldown_s INTEGER NOT NULL DEFAULT 900, "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_modificacion BIGINT NOT NULL DEFAULT 0"
        ");",

        // 6. Usuario
        "CREATE TABLE IF NOT EXISTS Usuario ("
            "id_usuario SERIAL PRIMARY KEY, "
            "usuario VARCHAR(20) NOT NULL UNIQUE, "
            "salt VARCHAR(32) NOT NULL DEFAULT 'SALT', "
            "contrasena_hash VARCHAR(64) NOT NULL, "
            "rol enum_rol_usuario NOT NULL DEFAULT 'NINGUNO', "
            "intentos_fallidos SMALLINT NOT NULL DEFAULT 0, "
            "bloqueado BOOLEAN NOT NULL DEFAULT FALSE, "
            "bloqueado_desde BIGINT DEFAULT 0, "
            "bloqueado_hasta BIGINT DEFAULT 0, "
            "fecha_ultimo_login BIGINT DEFAULT 0, "
            "fecha_cambio_password BIGINT DEFAULT 0, "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "creado_el TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "actualizado_el TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ");",

        // 7. Sensor
        "CREATE TABLE IF NOT EXISTS Sensor ("
            "id_sensor SERIAL PRIMARY KEY, "
            "mac CHAR(16) NOT NULL UNIQUE, "
            "nombre VARCHAR(32) NOT NULL DEFAULT '', "
            "ubicacion VARCHAR(32) NOT NULL DEFAULT '', "
            "descripcion VARCHAR(128) NOT NULL DEFAULT '', "
            "umbral_sup REAL NOT NULL DEFAULT 126.0, "
            "umbral_inf REAL NOT NULL DEFAULT -55.0, "
            "error_sup REAL NOT NULL DEFAULT 0.0, "
            "error_inf REAL NOT NULL DEFAULT 0.0, "
            "offset REAL NOT NULL DEFAULT 0.0, "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "fecha_modificacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ");",

        // 8. Censo
        "CREATE TABLE IF NOT EXISTS Censo ("
            "id_censo SERIAL PRIMARY KEY, "
            "id_sensor INTEGER NOT NULL REFERENCES Sensor(id_sensor) ON DELETE RESTRICT, "
            "temperatura_cruda REAL NOT NULL DEFAULT 0.0, "
            "temperatura_corregida REAL NOT NULL DEFAULT 0.0, "
            "estado_sensor estado_sensor_enum NOT NULL DEFAULT 'OK', "
            "fecha_creacion BIGINT NOT NULL DEFAULT 0"
        ");",

        // 9. AuditoriaSensores
        "CREATE TABLE IF NOT EXISTS AuditoriaSensores ("
            "id_auditoria_sensor SERIAL PRIMARY KEY, "
            "id_sensor INTEGER NOT NULL REFERENCES Sensor(id_sensor) ON DELETE RESTRICT, "
            "id_censo INTEGER NOT NULL REFERENCES Censo(id_censo) ON DELETE CASCADE, "
            "estado_previo_sensor estado_sensor_enum NOT NULL DEFAULT 'OK', "
            "estado_nuevo_sensor estado_sensor_enum NOT NULL DEFAULT 'ALARMA_SUPERIOR', "
            "estado_bus estado_bus_enum NOT NULL DEFAULT 'OK', "
            "tipo_evento_sensor tipo_evento_enum NOT NULL DEFAULT 'ADVERTENCIA', "
            "criticidad_sensor criticidad_evento_enum NOT NULL DEFAULT 'ALTA', "
            "estado_reporte estado_envio_correo_enum NOT NULL DEFAULT 'PENDIENTE', "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");",

        // 10. AuditoriaSistema
        "CREATE TABLE IF NOT EXISTS AuditoriaSistema ("
            "id_auditoria_sistema SERIAL PRIMARY KEY, "
            "criticidad criticidad_evento_enum NOT NULL DEFAULT 'NULA', "
            "tipo_evento_sistema tipo_evento_enum NOT NULL DEFAULT 'INFORMATIVO', "
            "estado_bus estado_bus_enum NOT NULL DEFAULT 'OK', "
            "espacio_sd_libre_kb INTEGER NOT NULL DEFAULT 0, "
            "heap_libre_bytes INTEGER NOT NULL DEFAULT 0, "
            "reset_reason VARCHAR(24) NOT NULL DEFAULT '', "
            "version_firmware VARCHAR(16) NOT NULL DEFAULT '1.0.0', "
            "detalle VARCHAR(96) NOT NULL DEFAULT '', "
            "internet BOOLEAN NOT NULL DEFAULT FALSE, "
            "estado_reporte estado_envio_correo_enum NOT NULL DEFAULT 'PENDIENTE', "
            "boot_id INTEGER NOT NULL DEFAULT 0, "
            "uptime_s INTEGER NOT NULL DEFAULT 0, "
            "fuente_tiempo fuente_tiempo_enum NOT NULL DEFAULT 'SIN_SINCRONIZAR', "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");",

        // 11. Correos
        "CREATE TABLE IF NOT EXISTS Correos ("
            "id_correo SERIAL PRIMARY KEY, "
            "nombre VARCHAR(24) NOT NULL, "
            "apellido VARCHAR(24) NOT NULL DEFAULT '', "
            "correo_electronico VARCHAR(48) NOT NULL UNIQUE, "
            "tipo_recepcion_mensajes tipo_recepcion_correo_enum NOT NULL DEFAULT 'RECIBIR_TODOS', "
            "ultimo_estado_envio estado_envio_correo_enum NOT NULL DEFAULT 'PENDIENTE', "
            "activa BOOLEAN NOT NULL DEFAULT TRUE, "
            "fecha_creacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "fecha_modificacion TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ");",

        // 12. AuditoriaAuth
        "CREATE TABLE IF NOT EXISTS AuditoriaAuth ("
            "id_auditoria_auth SERIAL PRIMARY KEY, "
            "seq BIGINT NOT NULL UNIQUE, "
            "id_usuario INTEGER REFERENCES Usuario(id_usuario) ON DELETE RESTRICT, "
            "usuario_intentado VARCHAR(20) NOT NULL DEFAULT '', "
            "accion VARCHAR(32) NOT NULL, "
            "tabla_afectada VARCHAR(32) NOT NULL DEFAULT 'NINGUNA', "
            "valor_previo VARCHAR(64) NOT NULL DEFAULT '', "
            "valor_nuevo VARCHAR(64) NOT NULL DEFAULT '', "
            "detalle VARCHAR(120) NOT NULL DEFAULT '', "
            "medio_conexion medio_conexion_enum NOT NULL DEFAULT 'INTERNO', "
            "origen_remoto VARCHAR(46) NOT NULL DEFAULT 'LOCAL', "
            "resultado resultado_auditoria_enum NOT NULL DEFAULT 'EXITO', "
            "criticidad criticidad_evento_enum NOT NULL DEFAULT 'BAJA', "
            "boot_id INTEGER NOT NULL DEFAULT 0, "
            "uptime_s INTEGER NOT NULL DEFAULT 0, "
            "fuente_tiempo fuente_tiempo_enum NOT NULL DEFAULT 'SIN_SINCRONIZAR', "
            "fecha_creacion BIGINT NOT NULL DEFAULT 0, "
            "hash_previo VARCHAR(64) NOT NULL, "
            "hash_registro VARCHAR(64) NOT NULL"
        ");"
    };

    // ========================================================================
    // 3. ÍNDICES METADATA DEL CATÁLOGO (para el diagrama ER)
    // ========================================================================
    static constexpr const char* DDL_INDEXES_SQL[] = {
        "CREATE INDEX IF NOT EXISTS idx_ntp_activa_prioridad ON ServidoresNtp (activa, prioridad);",
        "CREATE INDEX IF NOT EXISTS idx_usuario_activa ON Usuario (activa);",
        "CREATE INDEX IF NOT EXISTS idx_sensor_activo ON Sensor (activa);",
        "CREATE INDEX IF NOT EXISTS idx_sensor_ubicacion ON Sensor (ubicacion);",
        "CREATE INDEX IF NOT EXISTS idx_censo_sensor_fecha ON Censo (id_sensor, fecha_creacion);",
        "CREATE INDEX IF NOT EXISTS idx_censo_fecha ON Censo (fecha_creacion);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sensor_fecha ON AuditoriaSensores (id_sensor, fecha_creacion);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sensor_reporte ON AuditoriaSensores (estado_reporte);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sensor_criticidad ON AuditoriaSensores (criticidad_sensor);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sis_fecha ON AuditoriaSistema (fecha_creacion);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sis_reporte ON AuditoriaSistema (estado_reporte);",
        "CREATE INDEX IF NOT EXISTS idx_aud_sis_criticidad ON AuditoriaSistema (criticidad);",
        "CREATE INDEX IF NOT EXISTS idx_correos_activa ON Correos (activa);",
        "CREATE INDEX IF NOT EXISTS idx_aud_auth_fecha ON AuditoriaAuth (fecha_creacion);",
        "CREATE INDEX IF NOT EXISTS idx_aud_auth_usuario_fecha ON AuditoriaAuth (id_usuario, fecha_creacion);"
    };

    inline bool migrateAllSqlSchema() {
        auto& engine = PgSqlEngine::getInstance();
        auto& cat    = DbCommon::Engine::PhysicalSchemaCatalog::getInstance();

        engine.setDeferredCatalogFlush(true);

        for (const char* stmt : DDL_ENUMS_SQL) {
            if (!engine.execute(stmt).exito) {
                engine.setDeferredCatalogFlush(false);
                return false;
            }
        }
        for (const char* stmt : DDL_TABLES_SQL) {
            if (!engine.execute(stmt).exito) {
                engine.setDeferredCatalogFlush(false);
                return false;
            }
        }
        for (const char* stmt : DDL_INDEXES_SQL) {
            if (!engine.execute(stmt).exito) {
                engine.setDeferredCatalogFlush(false);
                return false;
            }
        }

        engine.setDeferredCatalogFlush(false);
        return cat.flushCatalogToSingleFile();
    }

    // Carga PG_CATALOG_V2 desde SD; si no existe o detecta catálogo TSV viejo (V1), crea el esquema binario completo
    inline bool bindOrBootstrapDatabase() {
        auto& storage = DbRuntime::Storage::PgBinaryStorage::getInstance();
        if (storage.bindToPhysicalCatalog()) {
            return true;
        }
        DbCommon::Engine::PhysicalSchemaCatalog::getInstance().clearInMemory();
        return migrateAllSqlSchema();
    }

} // namespace DbSql::Schema