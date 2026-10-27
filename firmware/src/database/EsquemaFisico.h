// Archivo: ./src/database/EsquemaFisico.h
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
    // 1. TABLA: sensor (48 bytes por fila - Alineación nativa 32-bit FPU)
    // =========================================================================
    struct FilaSensor {
        char     hexadecimal[17];    // Offset 0  (17B)
        uint8_t  _pad0[1];           // Offset 17 (1B padding explícito)
        uint16_t id_sensor;          // Offset 18 (2B)
        float    error_superior;     // Offset 20 (4B)
        float    error_inferior;     // Offset 24 (4B)
        float    umbral_superior;    // Offset 28 (4B)
        float    umbral_inferior;    // Offset 32 (4B)
        uint8_t  activa;             // Offset 36 (1B)
        uint8_t  _pad1[3];           // Offset 37 (3B padding explícito)
        uint32_t fecha_creacion;     // Offset 40 (4B)
        uint32_t fecha_modificacion; // Offset 44 (4B)
    };
    static_assert(sizeof(FilaSensor) == 48, "FilaSensor debe medir exactamente 48 bytes en ESP32");

    // =========================================================================
    // 2. TABLA: senso (28 bytes por fila - Alineación nativa 32-bit FPU)
    // =========================================================================
    struct FilaSenso {
        uint32_t id_senso;              // Offset 0  (4B)
        uint16_t id_sensor;             // Offset 4  (2B)
        uint8_t  _pad0[2];              // Offset 6  (2B padding explícito)
        float    temperatura_cruda;     // Offset 8  (4B)
        float    temperatura_corregida; // Offset 12 (4B)
        uint8_t  estado_sensor;         // Offset 16 (1B)
        uint8_t  activa;                // Offset 17 (1B)
        uint8_t  _pad1[2];              // Offset 18 (2B padding explícito)
        uint32_t fecha_creacion;        // Offset 20 (4B)
        uint32_t fecha_modificacion;    // Offset 24 (4B)
    };
    static_assert(sizeof(FilaSenso) == 28, "FilaSenso debe medir exactamente 28 bytes en ESP32");

    // =========================================================================
    // 3. TABLA: evento_sensor (24 bytes por fila)
    // =========================================================================
    struct FilaEventoSensor {
        uint32_t id_evento_sensor;     // Offset 0  (4B)
        uint16_t id_sensor;            // Offset 4  (2B)
        uint8_t  _pad0[2];             // Offset 6  (2B padding explícito)
        uint32_t id_senso;             // Offset 8  (4B)
        uint8_t  estado_previo_sensor; // Offset 12 (1B)
        uint8_t  evento;               // Offset 13 (1B)
        uint8_t  criticidad;           // Offset 14 (1B)
        uint8_t  activa;               // Offset 15 (1B)
        uint32_t fecha_creacion;       // Offset 16 (4B)
        uint32_t fecha_modificacion;   // Offset 20 (4B)
    };
    static_assert(sizeof(FilaEventoSensor) == 24, "FilaEventoSensor debe medir exactamente 24 bytes en ESP32");

    // =========================================================================
    // 4. TABLA: auditoria (64 bytes por fila)
    // =========================================================================
    struct FilaAuditoria {
        uint32_t id_auditoria;       // Offset 0  (4B)
        uint16_t id_usuario;         // Offset 4  (2B)
        char     accion[24];         // Offset 6  (24B)
        char     detalle[24];        // Offset 30 (24B)
        uint8_t  activa;             // Offset 54 (1B)
        uint8_t  _pad0[1];           // Offset 55 (1B padding explícito)
        uint32_t fecha_creacion;     // Offset 56 (4B)
        uint32_t fecha_modificacion; // Offset 60 (4B)
    };
    static_assert(sizeof(FilaAuditoria) == 64, "FilaAuditoria debe medir exactamente 64 bytes en ESP32");

    // =========================================================================
    // 5. TABLA: correos (112 bytes por fila)
    // =========================================================================
    struct FilaCorreo {
        uint16_t id_correo;               // Offset 0   (2B)
        char     nombre[24];              // Offset 2   (24B)
        char     apellido[24];            // Offset 26  (24B)
        char     correo_electronico[48];  // Offset 50  (48B)
        uint8_t  tipo_recepcion_mensajes; // Offset 98  (1B)
        uint8_t  estado_envio;            // Offset 99  (1B)
        uint8_t  activa;                  // Offset 100 (1B)
        uint8_t  _pad0[3];                // Offset 101 (3B padding explícito)
        uint32_t fecha_creacion;          // Offset 104 (4B)
        uint32_t fecha_modificacion;      // Offset 108 (4B)
    };
    static_assert(sizeof(FilaCorreo) == 112, "FilaCorreo debe medir exactamente 112 bytes en ESP32");

    // =========================================================================
    // 6. TABLA: usuarios (64 bytes por fila - Alineación natural exacta)
    // =========================================================================
    struct FilaUsuario {
        uint16_t id_usuario;         // Offset 0  (2B)
        char     usuario[20];        // Offset 2  (20B)
        uint8_t  contrasena[32];     // Offset 22 (32B - SHA-256 binario)
        uint8_t  rol;                // Offset 54 (1B)
        uint8_t  activa;             // Offset 55 (1B)
        uint32_t fecha_creacion;     // Offset 56 (4B)
        uint32_t fecha_modificacion; // Offset 60 (4B)
    };
    static_assert(sizeof(FilaUsuario) == 64, "FilaUsuario debe medir exactamente 64 bytes en ESP32");

    // =========================================================================
    // 7. TABLA SINGLETON (PK=1): configuracion_smtp (192 bytes)
    // =========================================================================
    struct FilaConfiguracionSMTP {
        uint8_t  id_configuracion_smtp; // Offset 0   (1B)
        char     host_smtp[40];         // Offset 1   (40B)
        uint8_t  _pad0[1];              // Offset 41  (1B padding explícito)
        uint16_t puerto;                // Offset 42  (2B)
        char     usuario[40];           // Offset 44  (40B)
        char     password[48];          // Offset 84  (48B)
        char     email_remitente[48];   // Offset 132 (48B)
        uint8_t  activa;                // Offset 180 (1B)
        uint8_t  _pad1[3];              // Offset 181 (3B padding explícito)
        uint32_t fecha_creacion;        // Offset 184 (4B)
        uint32_t fecha_modificacion;    // Offset 188 (4B)
    };
    static_assert(sizeof(FilaConfiguracionSMTP) == 192, "FilaConfiguracionSMTP debe medir exactamente 192 bytes en ESP32");

    // =========================================================================
    // 8. TABLA: servidores_ntp (52 bytes por fila)
    // =========================================================================
    struct FilaServidorNTP {
        uint8_t  id_server;          // Offset 0  (1B)
        char     server[40];         // Offset 1  (40B)
        uint8_t  activa;             // Offset 41 (1B)
        uint8_t  _pad0[2];           // Offset 42 (2B padding explícito)
        uint32_t fecha_creacion;     // Offset 44 (4B)
        uint32_t fecha_modificacion; // Offset 48 (4B)
    };
    static_assert(sizeof(FilaServidorNTP) == 52, "FilaServidorNTP debe medir exactamente 52 bytes en ESP32");

    // =========================================================================
    // 9. TABLA SINGLETON (PK=1): lts (208 bytes)
    // =========================================================================
    struct FilaLTS {
        uint8_t  id_lts;                  // Offset 0   (1B)
        char     nombre_certificado[24];  // Offset 1   (24B)
        char     certificado[64];         // Offset 25  (64B)
        uint8_t  clave_privada[32];       // Offset 89  (32B - PSK AES-256)
        char     cadena_certificados[64]; // Offset 121 (64B)
        uint8_t  _pad0[3];                // Offset 185 (3B padding explícito)
        uint32_t fecha_inicio_vigencia;   // Offset 188 (4B)
        uint32_t fecha_fin_vigencia;      // Offset 192 (4B)
        uint8_t  activa;                  // Offset 196 (1B)
        uint8_t  _pad1[3];                // Offset 197 (3B padding explícito)
        uint32_t fecha_creacion;          // Offset 200 (4B)
        uint32_t fecha_modificacion;      // Offset 204 (4B)
    };
    static_assert(sizeof(FilaLTS) == 208, "FilaLTS debe medir exactamente 208 bytes en ESP32");

    // =========================================================================
    // 10. TABLA SINGLETON (PK=1): configuracion_conectividad (212 bytes)
    // =========================================================================
    struct FilaConfiguracionConectividad {
        uint8_t  id_configuracion_conectividad; // Offset 0   (1B)
        char     SSID_STA[33];                  // Offset 1   (33B)
        char     STA_password[65];              // Offset 34  (65B)
        char     SSID_AP[33];                   // Offset 99  (33B)
        char     AP_password[65];               // Offset 132 (65B)
        uint8_t  _pad0[1];                      // Offset 197 (1B padding explícito)
        uint16_t puerto_udp;                    // Offset 198 (2B)
        uint8_t  activa;                        // Offset 200 (1B)
        uint8_t  _pad1[3];                      // Offset 201 (3B padding explícito)
        uint32_t fecha_creacion;                // Offset 204 (4B)
        uint32_t fecha_modificacion;            // Offset 208 (4B)
    };
    static_assert(sizeof(FilaConfiguracionConectividad) == 212, "FilaConfiguracionConectividad debe medir exactamente 212 bytes en ESP32");

}