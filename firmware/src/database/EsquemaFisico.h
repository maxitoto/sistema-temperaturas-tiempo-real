// Archivo: ./src/database/EsquemaFisico.h
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace EsquemaFisicoDB {

    enum EstadoSensor : uint8_t {
        ESTADO_OK               = 0,
        ESTADO_DESCONEXION_WIRE = 1,
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
        RECIBIR_TODOS            = 0,
        RECIBIR_EVENTOS          = 1,
        RECIBIR_EVENTOS_CRITICOS = 2
    };

    enum EstadoEnvioCorreo : uint8_t {
        ENVIO_PENDIENTE = 0,
        ENVIO_ENVIADO   = 1,
        ENVIO_FALLIDO   = 2
    };

    enum RolUsuario : uint8_t {
        ROL_REGULAR = 0,
        ROL_ADMIN   = 1
    };

    // =========================================================================
    // 1. TABLA FUSIONADA: sensor (244 bytes - 0 bytes de padding manual)
    // =========================================================================
    struct FilaSensor {
        // Grupo 4B (32 bytes: Offsets 0..31)
        float    correcion_superior;      // Offset 0  (4B)
        float    correcion_inferior;      // Offset 4  (4B)
        float    altura;                  // Offset 8  (4B)
        float    presion_atmosferica;     // Offset 12 (4B)
        float    umbral_superior;         // Offset 16 (4B)
        float    umbral_inferior;         // Offset 20 (4B)
        uint32_t fecha_creacion;          // Offset 24 (4B)
        uint32_t fecha_modificacion;      // Offset 28 (4B)
        // Grupo 2B (2 bytes: Offset 32..33)
        uint16_t id_sensor;               // Offset 32 (2B)
        // Grupo 1B / char[] (210 bytes: Offsets 34..243)
        uint8_t  activa;                  // Offset 34 (1B)
        char     hexadecimal[17];         // Offset 35 (17B) -> 32+2+1+17 = 52
        char     nombre[32];              // Offset 52 (32B)
        char     descripcion[64];         // Offset 84 (64B)
        char     ubicacion[32];           // Offset 148 (32B)
        char     ubicacion_descripcion[64]; // Offset 180 (64B) -> Total = 244B
    };
    static_assert(sizeof(FilaSensor) == 244, "FilaSensor debe medir exactamente 244 bytes");

    // =========================================================================
    // 2. TABLA: senso (28 bytes)
    // =========================================================================
    struct FilaSenso {
        uint32_t id_senso;
        uint16_t id_sensor;
        uint8_t  _pad0[2];
        float    temperatura_cruda;
        float    temperatura_corregida;
        uint8_t  estado_sensor;
        uint8_t  activa;
        uint8_t  _pad1[2];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaSenso) == 28, "FilaSenso debe medir exactamente 28 bytes");

    // =========================================================================
    // 3. TABLA: evento_sensor (24 bytes)
    // =========================================================================
    struct FilaEventoSensor {
        uint32_t id_evento_sensor;
        uint16_t id_sensor;
        uint8_t  _pad0[2];
        uint32_t id_senso;
        uint8_t  estado_previo_sensor;
        uint8_t  evento;
        uint8_t  criticidad;
        uint8_t  activa;
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaEventoSensor) == 24, "FilaEventoSensor debe medir exactamente 24 bytes");

    // =========================================================================
    // 4. TABLA: auditoria (64 bytes)
    // =========================================================================
    struct FilaAuditoria {
        uint32_t id_auditoria;
        uint16_t id_usuario;
        char     accion[24];
        char     detalle[24];
        uint8_t  activa;
        uint8_t  _pad0[1];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaAuditoria) == 64, "FilaAuditoria debe medir exactamente 64 bytes");

    // =========================================================================
    // 5. TABLA: correos (112 bytes)
    // =========================================================================
    struct FilaCorreo {
        uint16_t id_correo;
        char     nombre[24];
        char     apellido[24];
        char     correo_electronico[48];
        uint8_t  tipo_recepcion_mensajes;
        uint8_t  estado_envio;
        uint8_t  activa;
        uint8_t  _pad0[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaCorreo) == 112, "FilaCorreo debe medir exactamente 112 bytes");

    // =========================================================================
    // 6. TABLA: usuarios (64 bytes)
    // =========================================================================
    struct FilaUsuario {
        uint16_t id_usuario;
        char     usuario[20];
        uint8_t  contrasena[32];
        uint8_t  rol;
        uint8_t  activa;
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaUsuario) == 64, "FilaUsuario debe medir exactamente 64 bytes");

    // =========================================================================
    // 7. TABLA SINGLETON (PK=1): configuracion_smtp (192 bytes)
    // =========================================================================
    struct FilaConfiguracionSMTP {
        uint8_t  id_configuracion_smtp;
        char     host_smtp[40];
        uint8_t  _pad0[1];
        uint16_t puerto;
        char     usuario[40];
        char     password[48];
        char     email_remitente[48];
        uint8_t  activa;
        uint8_t  _pad1[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaConfiguracionSMTP) == 192, "FilaConfiguracionSMTP debe medir 192 bytes");

    // =========================================================================
    // 8. TABLA: servidores_ntp (52 bytes)
    // =========================================================================
    struct FilaServidorNTP {
        uint8_t  id_server;
        char     server[40];
        uint8_t  activa;
        uint8_t  _pad0[2];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaServidorNTP) == 52, "FilaServidorNTP debe medir 52 bytes");

    // =========================================================================
    // 9. TABLA SINGLETON (PK=1): lts (208 bytes)
    // =========================================================================
    struct FilaLTS {
        uint8_t  id_lts;
        char     nombre_certificado[24];
        char     certificado[64];
        uint8_t  clave_privada[32];
        char     cadena_certificados[64];
        uint8_t  _pad0[3];
        uint32_t fecha_inicio_vigencia;
        uint32_t fecha_fin_vigencia;
        uint8_t  activa;
        uint8_t  _pad1[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaLTS) == 208, "FilaLTS debe medir 208 bytes");

    // =========================================================================
    // 10. TABLA SINGLETON (PK=1): configuracion_conectividad (216 bytes)
    // =========================================================================
    struct FilaConfiguracionConectividad {
        // Grupo 4B (12 bytes: Offsets 0..11)
        uint32_t fecha_creacion;                // Offset 0  (4B)
        uint32_t fecha_modificacion;            // Offset 4  (4B)
        uint32_t baudios_usb;                   // Offset 8  (4B)
        // Grupo 2B (4 bytes: Offsets 12..15)
        uint16_t puerto_tcp;                    // Offset 12 (2B)
        uint16_t puerto_udp;                    // Offset 14 (2B)
        // Grupo 1B / char[] (198 bytes: Offsets 16..213)
        uint8_t  id_configuracion_conectividad; // Offset 16 (1B)
        uint8_t  activa;                        // Offset 17 (1B)
        char     SSID_STA[33];                  // Offset 18 (33B)
        char     STA_password[65];              // Offset 51 (65B)
        char     SSID_AP[33];                   // Offset 116 (33B)
        char     AP_password[65];               // Offset 149 (65B)
        uint8_t  _padFinal[2];                  // Offset 214 (2B) -> Total = 216B
    };
    static_assert(sizeof(FilaConfiguracionConectividad) == 216, "FilaConfiguracionConectividad debe medir 216 bytes");

}
