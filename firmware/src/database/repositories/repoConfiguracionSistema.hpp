#pragma once
#include <Arduino.h>
#include <time.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ConfiguracionSistema;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct ConfiguracionSistemaDTO {
        PgInt    idConfiguracionSistema = 1;
        PgInt    zonaHorariaMin         = -180;
        PgPosInt retencionSensoDias     = 365;
        PgPosInt retencionEventosDias   = 730;
        PgPosInt retencionAuditoriaDias = 1095;
        PgPosInt sdLibreMinimoKb        = 51200;
        PgPosInt correoCooldownS        = 900;
        PgBool   activa                 = true;
        PgBigInt fechaModificacion      = 0;
    };

    class RepoConfiguracionSistema {
    private:
        static int64_t epochActual() {
            const time_t t = time(nullptr);
            return (t > 100000) ? static_cast<int64_t>(t) : static_cast<int64_t>(millis() / 1000UL);
        }

        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

    public:
        static ConfiguracionSistemaDTO mapear(const PgTuple& r) {
            ConfiguracionSistemaDTO dto{};
            r["id_configuracion_sistema"] >> dto.idConfiguracionSistema;
            r["zona_horaria_min"]         >> dto.zonaHorariaMin;
            r["retencion_senso_dias"]     >> dto.retencionSensoDias;
            r["retencion_eventos_dias"]   >> dto.retencionEventosDias;
            r["retencion_auditoria_dias"] >> dto.retencionAuditoriaDias;
            r["sd_libre_minimo_kb"]       >> dto.sdLibreMinimoKb;
            r["correo_cooldown_s"]        >> dto.correoCooldownS;
            r["activa"]                   >> dto.activa;
            r["fecha_modificacion"]       >> dto.fechaModificacion;
            return dto;
        }

        static bool obtenerOInicializar(ConfiguracionSistemaDTO& outDto) {
            asegurarCatalogo();

            PgTuple fila;
            if (ConfiguracionSistema.filter_eq("id_configuracion_sistema", 1).first(fila)) {
                outDto = mapear(fila);
                return true;
            }

            const int64_t ahora = epochActual();
            auto res = ConfiguracionSistema.create(
                {
                    PgCell("id_configuracion_sistema", static_cast<int32_t>(1)),
                    PgCell("zona_horaria_min",         static_cast<int32_t>(-180)),
                    PgCell("retencion_senso_dias",     static_cast<uint32_t>(365)),
                    PgCell("retencion_eventos_dias",   static_cast<uint32_t>(730)),
                    PgCell("retencion_auditoria_dias", static_cast<uint32_t>(1095)),
                    PgCell("sd_libre_minimo_kb",       static_cast<uint32_t>(51200)),
                    PgCell("correo_cooldown_s",        static_cast<uint32_t>(900)),
                    PgCell("activa",                   true),
                    PgCell("fecha_modificacion",       ahora)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                return false;
            }

            outDto = mapear(res.returningRows[0]);
            return true;
        }

        static ConfiguracionSistemaDTO exigirConfiguracion() {
            ConfiguracionSistemaDTO dto{};
            if (!obtenerOInicializar(dto)) {
                lanzarUltimoErrorDB();
            }
            return dto;
        }

        static ConfiguracionSistemaDTO actualizarTodo(const ConfiguracionSistemaDTO& dto) {
            ConfiguracionSistemaDTO actual{};
            obtenerOInicializar(actual);

            const int64_t ahora = epochActual();
            auto res = ConfiguracionSistema.filter_eq("id_configuracion_sistema", 1)
                .returning({"*"})
                .update({
                    PgCell("zona_horaria_min",         dto.zonaHorariaMin),
                    PgCell("retencion_senso_dias",     dto.retencionSensoDias),
                    PgCell("retencion_eventos_dias",   dto.retencionEventosDias),
                    PgCell("retencion_auditoria_dias", dto.retencionAuditoriaDias),
                    PgCell("sd_libre_minimo_kb",       dto.sdLibreMinimoKb),
                    PgCell("correo_cooldown_s",        dto.correoCooldownS),
                    PgCell("activa",                   dto.activa),
                    PgCell("fecha_modificacion",       ahora)
                });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }
    };

}