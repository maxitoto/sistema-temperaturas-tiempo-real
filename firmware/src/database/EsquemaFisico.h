// Archivo: ./src/database/EsquemaFisicoDB.h
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace EsquemaFisicoDB {

    // =========================================================================
    // ENUMS DE DOMINIO (1 Byte en Flash, 0 fragmentación)
    // =========================================================================
    enum EstadoSensor : uint8_t {
        ESTADO_OK               = 0,
        ESTADO_DESCONEXION_WIRE = 1, // -127°C en el canal 1-Wire
        ESTADO_RUIDO            = 2,
        ESTADO_SUPERO_INFERIOR  = 3,
        ESTADO_SUPERO_SUPERIOR  = 4
    };

    enum CriticidadEvento : uint8_t {
        CRITICIDAD_BAJA    = 0,
        CRITICIDAD_MEDIA   = 1,
        CRITICIDAD_CRITICA = 2
    };

    enum TipoRecepcionCorreo : uint8_t {
        RECIBIR_TODOS            = 0, // "todos"
        RECIBIR_EVENTOS          = 1, // "eventos"
        RECIBIR_EVENTOS_CRITICOS = 2  // "eventos criticos"
    };

    enum EstadoEnvioCorreo : uint8_t {
        ENVIO_PENDIENTE = 0, // "pendiente"
        ENVIO_ENVIADO   = 1, // "enviado"
        ENVIO_FALLIDO   = 2  // "fallido"
    };

    enum RolUsuario : uint8_t {
        ROL_REGULAR = 0, // "regular"
        ROL_ADMIN   = 1  // "admin"
    };

    // =========================================================================
    // 1. TABLA: sensor (44 bytes por fila)
    // Definición intrínseca del sensor (sin temperatura actual ni ubicación)
    // =========================================================================
    struct __attribute__((packed)) FilaSensor {
        char     hexadecimal[17];    // PK: 16 chars hex + '\0'
        uint16_t id_sensor;          // UNIQUE, INCREMENT
        float    error_superior;     // default: 0.0f
        float    error_inferior;     // default: 0.0f
        float    umbral_superior;    // default: 126.0f
        float    umbral_inferior;    // default: -126.0f
        uint8_t  activa;             // default: 1
        uint32_t fecha_creacion;     // Unix timestamp (seg)
        uint32_t fecha_modificacion; // Unix timestamp (seg)
    };

    // =========================================================================
    // 2. TABLA: senso (24 bytes por fila)
    // Evaluación periódica del sensor en el tiempo (ej. cada 2 minutos)
    // =========================================================================
    struct __attribute__((packed)) FilaSenso {
        uint32_t id_senso;              // PK, INCREMENT
        uint16_t id_sensor;             // FK -> FilaSensor.id_sensor
        float    temperatura_cruda;     // NOT NULL
        float    temperatura_corregida; // NOT NULL
        uint8_t  estado_sensor;         // EstadoSensor (OK, -127, ruido, umbral...)
        uint8_t  activa;                // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 3. TABLA: evento_sensor (22 bytes por fila)
    // Confirmación de cambio de estado tras período anti-falsos positivos
    // =========================================================================
    struct __attribute__((packed)) FilaEventoSensor {
        uint32_t id_evento_sensor;     // PK, INCREMENT
        uint16_t id_sensor;            // FK -> FilaSensor.id_sensor
        uint32_t id_senso;             // FK -> FilaSenso.id_senso
        uint8_t  estado_previo_sensor; // EstadoSensor anterior
        uint8_t  evento;               // EstadoSensor nuevo confirmado
        uint8_t  criticidad;           // CriticidadEvento
        uint8_t  activa;               // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 4. TABLA: auditoria (63 bytes por fila)
    // Registro de acciones de usuarios en el firmware (umbrales, login, reset)
    // =========================================================================
    struct __attribute__((packed)) FilaAuditoria {
        uint32_t id_auditoria;       // PK, INCREMENT
        uint16_t id_usuario;         // FK -> FilaUsuario.id_usuario
        char     accion[24];         // Ej: "CAMBIO_UMBRAL", "LOGIN", "REINICIO"
        char     detalle[24];        // Ej: "Sensor #1: -18 a -20"
        uint8_t  activa;             // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 5. TABLA: correos (109 bytes por fila)
    // Destinatarios de notificaciones por correo electrónico
    // =========================================================================
    struct __attribute__((packed)) FilaCorreo {
        uint16_t id_correo;               // PK, INCREMENT
        char     nombre[24];              // NOT NULL
        char     apellido[24];            // NULLABLE ("" si vacío)
        char     correo_electronico[48];  // NOT NULL
        uint8_t  tipo_recepcion_mensajes; // default: RECIBIR_TODOS (0)
        uint8_t  estado_envio;            // default: ENVIO_PENDIENTE (0)
        uint8_t  activa;                  // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 6. TABLA: usuarios (64 bytes por fila)
    // Autenticación y autorización de acciones en el servidor del firmware
    // =========================================================================
    struct __attribute__((packed)) FilaUsuario {
        uint16_t id_usuario;         // PK, INCREMENT
        char     usuario[20];        // UNIQUE, NOT NULL
        uint8_t  contrasena[32];     // Hash SHA-256 binario
        uint8_t  rol;                // default: ROL_REGULAR (0)
        uint8_t  activa;             // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 7. TABLA SINGLETON (PK=1): configuracion_smtp (188 bytes)
    // Se reemplaza el registro 1 conservando fecha_creacion
    // =========================================================================
    struct __attribute__((packed)) FilaConfiguracionSMTP {
        uint8_t  id_configuracion_smtp; // PK, default: 1
        char     host_smtp[40];         // NOT NULL
        uint16_t puerto;                // NOT NULL (ej. 587)
        char     usuario[40];           // NOT NULL
        char     password[48];          // NOT NULL
        char     email_remitente[48];   // NOT NULL
        uint8_t  activa;                // default: 1
        uint32_t fecha_creacion;        // Inmutable al reemplazar
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 8. TABLA: servidores_ntp (50 bytes por fila)
    // Lista persistente de servidores NTP
    // =========================================================================
    struct __attribute__((packed)) FilaServidorNTP {
        uint8_t  id_server;          // PK, INCREMENT
        char     server[40];         // NOT NULL (ej. "pool.ntp.org")
        uint8_t  activa;             // default: 1
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 9. TABLA SINGLETON (PK=1): lts (202 bytes)
    // Certificado y clave privada persistidos en el firmware
    // =========================================================================
    struct __attribute__((packed)) FilaLTS {
        uint8_t  id_lts;                  // PK, default: 1
        char     nombre_certificado[24];  // NOT NULL
        char     certificado[64];         // NOT NULL
        uint8_t  clave_privada[32];       // NOT NULL
        char     cadena_certificados[64]; // NULLABLE
        uint32_t fecha_inicio_vigencia;
        uint32_t fecha_fin_vigencia;
        uint8_t  activa;                  // default: 1
        uint32_t fecha_creacion;          // Inmutable al reemplazar
        uint32_t fecha_modificacion;
    };

    // =========================================================================
    // 10. TABLA SINGLETON (PK=1): configuracion_conectividad (206 bytes)
    // Credenciales Wi-Fi STA y AP persistidas para sobrevivir reinicios
    // =========================================================================
    struct __attribute__((packed)) FilaConfiguracionConectividad {
        uint8_t  id_configuracion_conectividad; // PK, default: 1
        char     SSID_STA[33];                  // default: ""
        char     STA_password[65];              // default: ""
        char     SSID_AP[33];                   // default: ""
        char     AP_password[65];               // default: ""
        uint16_t puerto_udp;                    // default: 81
        uint8_t  activa;                        // default: 1
        uint32_t fecha_creacion;                // Inmutable al reemplazar
        uint32_t fecha_modificacion;
    };

}