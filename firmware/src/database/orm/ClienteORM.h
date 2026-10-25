// Archivo: ./src/database/orm/ClienteORM.h
#pragma once
#include <string.h>
#include "database/EsquemaFisico.h"
#include "database/sgbd/MotorSGBD.h"
#include "database/orm/RepositorioTabla.h"

namespace ORM {

    using namespace EsquemaFisicoDB;
    using namespace SGBD;

    class ClienteORM {
      private:
        MotorSGBD* motor;

      public:
        // Capacidades de los anillos circulares en la tarjeta microSD
        static const size_t MAX_FILAS_SENSO     = 1024;
        static const size_t MAX_FILAS_EVENTO    = 256;
        static const size_t MAX_FILAS_AUDITORIA = 256;

        // =====================================================================
        // LOS 10 REPOSITORIOS TIPADOS (Igual que prisma.<tabla>)
        // =====================================================================
        ModeloCatalogo<FilaSensor,      uint16_t, &FilaSensor::id_sensor>           sensor;
        ModeloCircular<FilaSenso,       &FilaSenso::id_senso, MAX_FILAS_SENSO>      senso;
        ModeloCircular<FilaEventoSensor,&FilaEventoSensor::id_evento_sensor, MAX_FILAS_EVENTO> eventoSensor;
        ModeloCircular<FilaAuditoria,   &FilaAuditoria::id_auditoria, MAX_FILAS_AUDITORIA>     auditoria;
        ModeloCatalogo<FilaCorreo,      uint16_t, &FilaCorreo::id_correo>           correos;
        ModeloCatalogo<FilaUsuario,     uint16_t, &FilaUsuario::id_usuario>         usuarios;
        ModeloSingleton<FilaConfiguracionSMTP,         &FilaConfiguracionSMTP::id_configuracion_smtp>                 configSmtp;
        ModeloCatalogo<FilaServidorNTP, uint8_t,  &FilaServidorNTP::id_server>      servidoresNtp;
        ModeloSingleton<FilaLTS,                       &FilaLTS::id_lts>                                             lts;
        ModeloSingleton<FilaConfiguracionConectividad, &FilaConfiguracionConectividad::id_configuracion_conectividad> conectividad;

        explicit ClienteORM(MotorSGBD* motorSgbd)
            : motor(motorSgbd),
              sensor(motorSgbd, TABLA_SENSOR),
              senso(motorSgbd, TABLA_SENSO),
              eventoSensor(motorSgbd, TABLA_EVENTO_SENSOR),
              auditoria(motorSgbd, TABLA_AUDITORIA),
              correos(motorSgbd, TABLA_CORREOS),
              usuarios(motorSgbd, TABLA_USUARIOS),
              configSmtp(motorSgbd, TABLA_CONFIG_SMTP),
              servidoresNtp(motorSgbd, TABLA_SERVIDORES_NTP),
              lts(motorSgbd, TABLA_LTS),
              conectividad(motorSgbd, TABLA_CONFIG_CONECTIVIDAD) {}

        // Inicializa las 10 tablas en la SD y recupera los contadores autoincrementales circulares
        bool inicializar() {
            if (!motor || !motor->iniciar()) return false;
            senso.sincronizarSecuenciaInicial();
            eventoSensor.sincronizarSecuenciaInicial();
            auditoria.sincronizarSecuenciaInicial();
            return true;
        }

        // =====================================================================
        // HELPERS DE CONSULTA POR CLAVE NATURAL (WHERE hexadecimal / username)
        // =====================================================================
        bool buscarSensorPorHex(const char* hexBuscado, FilaSensor& salida,
                                const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!hexBuscado) return false;
            return sensor.buscarPrimero(salida, [&](const FilaSensor& f) {
                return strncmp(f.hexadecimal, hexBuscado, sizeof(f.hexadecimal)) == 0;
            }, true, sesion);
        }

        bool buscarUsuarioPorNombre(const char* username, FilaUsuario& salida,
                                    const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!username) return false;
            return usuarios.buscarPrimero(salida, [&](const FilaUsuario& f) {
                return strncmp(f.usuario, username, sizeof(f.usuario)) == 0;
            }, true, sesion);
        }

        // Registra rápidamente una fila en la tabla auditoria.bin
        bool registrarAuditoria(uint16_t idUsuario, const char* accionTxt, const char* detalleTxt,
                                uint32_t timestampActual,
                                const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            FilaAuditoria f = {};
            f.id_usuario = idUsuario;
            if (accionTxt)  strncpy(f.accion,  accionTxt,  sizeof(f.accion)  - 1);
            if (detalleTxt) strncpy(f.detalle, detalleTxt, sizeof(f.detalle) - 1);
            return auditoria.registrar(f, timestampActual, sesion);
        }
    };

}