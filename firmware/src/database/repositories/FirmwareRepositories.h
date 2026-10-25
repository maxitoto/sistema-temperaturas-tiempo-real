// Archivo: ./src/database/repositories/FirmwareRepositories.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "database/orm/ClienteORM.h"

namespace Repositories {

    using namespace EsquemaFisicoDB;
    using namespace SGBD;
    using namespace ORM;

    // =========================================================================
    // 1. SENSOR REPOSITORY (Catálogo de sensores físicos y sus umbrales)
    // =========================================================================
    class SensorFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit SensorFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        size_t listarActivos(FilaSensor* out, size_t maxItems, const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            size_t n = 0;
            orm->sensor.recorrerActivos([&](const FilaSensor& f) -> bool {
                out[n++] = f;
                return n < maxItems;
            }, sesion);
            return n;
        }

        bool buscarPorId(uint16_t idSensor, FilaSensor& out, const ContextoSesionSGBD& sesion) const {
            return orm ? orm->sensor.buscarPorId(idSensor, out, true, sesion) : false;
        }

        bool buscarPorHex(const char* hex, FilaSensor& out, const ContextoSesionSGBD& sesion) const {
            return orm ? orm->buscarSensorPorHex(hex, out, sesion) : false;
        }

        bool guardarSensor(const char* hex, float errSup, float errInf, float umbSup, float umbInf,
                           uint32_t ahoraSeg, FilaSensor& outGuardado, const ContextoSesionSGBD& sesion) {
            if (!orm || !hex) return false;
            FilaSensor existente = {};
            if (orm->buscarSensorPorHex(hex, existente, sesion)) {
                existente.error_superior  = errSup;
                existente.error_inferior  = errInf;
                existente.umbral_superior = umbSup;
                existente.umbral_inferior = umbInf;
                if (!orm->sensor.actualizarPorId(existente.id_sensor, existente, ahoraSeg, sesion)) return false;
                outGuardado = existente;
                return true;
            }
            FilaSensor nuevo = {};
            strncpy(nuevo.hexadecimal, hex, sizeof(nuevo.hexadecimal) - 1);
            nuevo.error_superior  = errSup;
            nuevo.error_inferior  = errInf;
            nuevo.umbral_superior = umbSup;
            nuevo.umbral_inferior = umbInf;
            if (!orm->sensor.crear(nuevo, ahoraSeg, sesion)) return false;
            outGuardado = nuevo;
            return true;
        }

        bool desactivarPorId(uint16_t idSensor, uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            return orm ? orm->sensor.desactivarPorId(idSensor, ahoraSeg, sesion) : false;
        }
    };

    // =========================================================================
    // 2. SENSO REPOSITORY (Telemetría periódica en anillo circular)
    // =========================================================================
    class SensoFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit SensoFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        // Devuelve las últimas N mediciones en orden DESC (más recientes primero) en O(N)
        size_t listarUltimos(FilaSenso* out, size_t maxItems, uint16_t filtroIdSensor,
                             const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            uint32_t ultimoId = orm->senso.obtenerUltimoId();
            size_t encontrados = 0;
            size_t revisados = 0;

            for (uint32_t id = ultimoId; id >= 1 && encontrados < maxItems && revisados < ClienteORM::MAX_FILAS_SENSO; id--, revisados++) {
                FilaSenso f = {};
                if (orm->senso.buscarPorId(id, f, sesion)) {
                    if (filtroIdSensor == 0 || f.id_sensor == filtroIdSensor) {
                        out[encontrados++] = f;
                    }
                }
                if (id == 1) break;
            }
            return encontrados;
        }

        bool registrarLectura(uint16_t idSensor, float tCruda, float tCorregida, uint8_t estado,
                              uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            if (!orm) return false;
            FilaSenso f = {};
            f.id_sensor             = idSensor;
            f.temperatura_cruda     = tCruda;
            f.temperatura_corregida = tCorregida;
            f.estado_sensor         = estado;
            return orm->senso.registrar(f, ahoraSeg, sesion);
        }
    };

    // =========================================================================
    // 3. EVENTO SENSOR REPOSITORY (Transiciones de estado confirmadas)
    // =========================================================================
    class EventoSensorFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit EventoSensorFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        size_t listarUltimos(FilaEventoSensor* out, size_t maxItems, uint16_t filtroIdSensor,
                             const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            uint32_t ultimoId = orm->eventoSensor.obtenerUltimoId();
            size_t encontrados = 0;
            size_t revisados = 0;

            for (uint32_t id = ultimoId; id >= 1 && encontrados < maxItems && revisados < ClienteORM::MAX_FILAS_EVENTO; id--, revisados++) {
                FilaEventoSensor f = {};
                if (orm->eventoSensor.buscarPorId(id, f, sesion)) {
                    if (filtroIdSensor == 0 || f.id_sensor == filtroIdSensor) {
                        out[encontrados++] = f;
                    }
                }
                if (id == 1) break;
            }
            return encontrados;
        }
    };

    // =========================================================================
    // 4. AUDITORIA REPOSITORY (Historial de acciones en el Firmware)
    // =========================================================================
    class AuditoriaFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit AuditoriaFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        size_t listarUltimas(FilaAuditoria* out, size_t maxItems, const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            uint32_t ultimoId = orm->auditoria.obtenerUltimoId();
            size_t encontrados = 0;
            size_t revisados = 0;

            for (uint32_t id = ultimoId; id >= 1 && encontrados < maxItems && revisados < ClienteORM::MAX_FILAS_AUDITORIA; id--, revisados++) {
                FilaAuditoria f = {};
                if (orm->auditoria.buscarPorId(id, f, sesion)) {
                    out[encontrados++] = f;
                }
                if (id == 1) break;
            }
            return encontrados;
        }

        bool registrar(uint16_t idUsuario, const char* accion, const char* detalle,
                       uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            return orm ? orm->registrarAuditoria(idUsuario, accion, detalle, ahoraSeg, sesion) : false;
        }
    };

    // =========================================================================
    // 5. CORREOS REPOSITORY (Destinatarios de alertas por email)
    // =========================================================================
    class CorreoFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit CorreoFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        size_t listarActivos(FilaCorreo* out, size_t maxItems, const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            size_t n = 0;
            orm->correos.recorrerActivos([&](const FilaCorreo& f) -> bool {
                out[n++] = f;
                return n < maxItems;
            }, sesion);
            return n;
        }

        bool crearCorreo(const char* nombre, const char* apellido, const char* email,
                         uint8_t tipoRecepcion, uint32_t ahoraSeg, FilaCorreo& outCreado,
                         const ContextoSesionSGBD& sesion) {
            if (!orm || !nombre || !email) return false;
            FilaCorreo nuevo = {};
            strncpy(nuevo.nombre, nombre, sizeof(nuevo.nombre) - 1);
            if (apellido) strncpy(nuevo.apellido, apellido, sizeof(nuevo.apellido) - 1);
            strncpy(nuevo.correo_electronico, email, sizeof(nuevo.correo_electronico) - 1);
            nuevo.tipo_recepcion_mensajes = tipoRecepcion;
            nuevo.estado_envio            = ENVIO_PENDIENTE;
            if (!orm->correos.crear(nuevo, ahoraSeg, sesion)) return false;
            outCreado = nuevo;
            return true;
        }

        bool desactivarPorId(uint16_t idCorreo, uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            return orm ? orm->correos.desactivarPorId(idCorreo, ahoraSeg, sesion) : false;
        }
    };

    // =========================================================================
    // 6. CONFIGURACION SINGLETONS & NTP REPOSITORY (SMTP, WiFi, NTP, LTS)
    // =========================================================================
    class ConfiguracionFirmwareRepository {
      private:
        ClienteORM* orm;
      public:
        explicit ConfiguracionFirmwareRepository(ClienteORM* o = nullptr) : orm(o) {}
        void vincular(ClienteORM* o) { orm = o; }

        bool obtenerSmtp(FilaConfiguracionSMTP& out, const ContextoSesionSGBD& sesion) const {
            return orm ? orm->configSmtp.obtener(out, sesion) : false;
        }

        bool guardarSmtp(FilaConfiguracionSMTP& cfg, uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            return orm ? orm->configSmtp.guardar(cfg, ahoraSeg, sesion) : false;
        }

        bool obtenerConectividad(FilaConfiguracionConectividad& out, const ContextoSesionSGBD& sesion) const {
            return orm ? orm->conectividad.obtener(out, sesion) : false;
        }

        bool guardarConectividad(FilaConfiguracionConectividad& cfg, uint32_t ahoraSeg, const ContextoSesionSGBD& sesion) {
            return orm ? orm->conectividad.guardar(cfg, ahoraSeg, sesion) : false;
        }

        bool obtenerLts(FilaLTS& out, const ContextoSesionSGBD& sesion) const {
            return orm ? orm->lts.obtener(out, sesion) : false;
        }

        size_t listarServidoresNtp(FilaServidorNTP* out, size_t maxItems, const ContextoSesionSGBD& sesion) const {
            if (!orm || !out || maxItems == 0) return 0;
            size_t n = 0;
            orm->servidoresNtp.recorrerActivos([&](const FilaServidorNTP& f) -> bool {
                out[n++] = f;
                return n < maxItems;
            }, sesion);
            return n;
        }

        bool agregarServidorNtp(const char* hostNtp, uint32_t ahoraSeg, FilaServidorNTP& out,
                                const ContextoSesionSGBD& sesion) {
            if (!orm || !hostNtp) return false;
            FilaServidorNTP f = {};
            strncpy(f.server, hostNtp, sizeof(f.server) - 1);
            if (!orm->servidoresNtp.crear(f, ahoraSeg, sesion)) return false;
            out = f;
            return true;
        }
    };

    // Contenedor unificado de Repositorios del Firmware
    struct ContenedorRepositoriosFirmware {
        SensorFirmwareRepository        sensores;
        SensoFirmwareRepository         sensos;
        EventoSensorFirmwareRepository  eventos;
        AuditoriaFirmwareRepository     auditorias;
        CorreoFirmwareRepository        correos;
        ConfiguracionFirmwareRepository configuraciones;

        void vincularORM(ClienteORM& orm) {
            sensores.vincular(&orm);
            sensos.vincular(&orm);
            eventos.vincular(&orm);
            auditorias.vincular(&orm);
            correos.vincular(&orm);
            configuraciones.vincular(&orm);
        }
    };

}