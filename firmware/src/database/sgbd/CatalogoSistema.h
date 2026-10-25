// Archivo: ./src/database/sgbd/CatalogoSistema.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "database/EsquemaFisico.h"

namespace SGBD {

    enum IdTabla : uint8_t {
        TABLA_SENSOR = 0,
        TABLA_SENSO,
        TABLA_EVENTO_SENSOR,
        TABLA_AUDITORIA,
        TABLA_CORREOS,
        TABLA_USUARIOS,
        TABLA_CONFIG_SMTP,
        TABLA_SERVIDORES_NTP,
        TABLA_LTS,
        TABLA_CONFIG_CONECTIVIDAD,
        TOTAL_TABLAS_SISTEMA
    };

    enum OperacionSGBD : uint8_t {
        OP_DDL_ESTRUCTURA = 0,
        OP_DQL_LECTURA    = 1,
        OP_DML_ESCRITURA  = 2,
        OP_DCL_PERMISOS   = 3
    };

    struct MetadatoTabla {
        IdTabla     id;
        const char* nombre;
        const char* rutaArchivo;
        const char* rutaTmp;
        size_t      tamanoFila;
        bool        esSingleton;        // true si la tabla solo admite PK = 1
        uint8_t     rolMinimoEscritura; // EsquemaFisicoDB::ROL_REGULAR o ROL_ADMIN
    };

    class CatalogoSistema {
      private:
        MetadatoTabla tablas[TOTAL_TABLAS_SISTEMA];

      public:
        static const char* directorioBase() { return "/db"; }

        CatalogoSistema() {
            using namespace EsquemaFisicoDB;
            tablas[TABLA_SENSOR]              = { TABLA_SENSOR,              "sensor",              "/db/sensor.bin",   "/db/sensor.tmp",   sizeof(FilaSensor),                    false, ROL_ADMIN   };
            tablas[TABLA_SENSO]               = { TABLA_SENSO,               "senso",               "/db/senso.bin",    "/db/senso.tmp",    sizeof(FilaSenso),                     false, ROL_REGULAR };
            tablas[TABLA_EVENTO_SENSOR]       = { TABLA_EVENTO_SENSOR,       "evento_sensor",       "/db/evento.bin",   "/db/evento.tmp",   sizeof(FilaEventoSensor),              false, ROL_REGULAR };
            tablas[TABLA_AUDITORIA]           = { TABLA_AUDITORIA,           "auditoria",           "/db/audit.bin",    "/db/audit.tmp",    sizeof(FilaAuditoria),                 false, ROL_REGULAR };
            tablas[TABLA_CORREOS]             = { TABLA_CORREOS,             "correos",             "/db/correos.bin",  "/db/correos.tmp",  sizeof(FilaCorreo),                    false, ROL_ADMIN   };
            tablas[TABLA_USUARIOS]            = { TABLA_USUARIOS,            "usuarios",            "/db/usuarios.bin", "/db/usuarios.tmp", sizeof(FilaUsuario),                   false, ROL_ADMIN   };
            tablas[TABLA_CONFIG_SMTP]         = { TABLA_CONFIG_SMTP,         "configuracion_smtp",  "/db/smtp.bin",     "/db/smtp.tmp",     sizeof(FilaConfiguracionSMTP),         true,  ROL_ADMIN   };
            tablas[TABLA_SERVIDORES_NTP]      = { TABLA_SERVIDORES_NTP,      "servidores_ntp",      "/db/ntp.bin",      "/db/ntp.tmp",      sizeof(FilaServidorNTP),               false, ROL_ADMIN   };
            tablas[TABLA_LTS]                 = { TABLA_LTS,                 "lts",                 "/db/lts.bin",      "/db/lts.tmp",      sizeof(FilaLTS),                       true,  ROL_ADMIN   };
            tablas[TABLA_CONFIG_CONECTIVIDAD] = { TABLA_CONFIG_CONECTIVIDAD, "config_conectividad", "/db/wifi.bin",     "/db/wifi.tmp",     sizeof(FilaConfiguracionConectividad), true,  ROL_ADMIN   };
        }

        const MetadatoTabla* obtener(IdTabla id) const {
            if (id >= TOTAL_TABLAS_SISTEMA) return nullptr;
            return &tablas[id];
        }

        // Permite que DCL modifique privilegios en el catálogo (GRANT / REVOKE)
        bool cambiarRolMinimoEscritura(IdTabla id, uint8_t nuevoRolMinimo) {
            if (id >= TOTAL_TABLAS_SISTEMA) return false;
            tablas[id].rolMinimoEscritura = nuevoRolMinimo;
            return true;
        }
    };

}