#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ConfiguracionRed;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct ConfiguracionRedDTO {
        PgAuto     id_configuracion_red = 1;
        PgChar<35> ssid_sta             = "";
        PgChar<64> password_sta         = "";
        PgChar<35> ssid_ap              = "ESP32-SecureAP";
        PgChar<64> password_ap          = "ESP32secure1234@";
        PgDateTime creado_el            = "";
        PgDateTime actualizado_el       = "";
    };

    class RepoConfiguracionRed {
    private:
        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static ConfiguracionRedDTO mapear(const PgTuple& r) {
            ConfiguracionRedDTO dto{};
            r["id_configuracion_red"] >> dto.id_configuracion_red;
            r["ssid_sta"]             >> dto.ssid_sta;
            r["password_sta"]         >> dto.password_sta;
            r["ssid_ap"]              >> dto.ssid_ap;
            r["password_ap"]          >> dto.password_ap;
            r["creado_el"]            >> dto.creado_el;
            r["actualizado_el"]       >> dto.actualizado_el;
            return dto;
        }

    public:
        static String ultimoError() {
            return DdlDiagnosticConsole::getInstance().last().toPsqlFormat();
        }

        static bool obtenerOInicializar(ConfiguracionRedDTO& outDto) {
            asegurarCatalogo();

            PgTuple fila;
            if (ConfiguracionRed.objects().order_by("id_configuracion_red").first(fila)) {
                outDto = mapear(fila);
                return true;
            }

            auto res = ConfiguracionRed.create(
                {
                    PgCell("ssid_sta",     ""),
                    PgCell("password_sta", ""),
                    PgCell("ssid_ap",      "ESP32-SecureAP"),
                    PgCell("password_ap",  "ESP32secure1234@")
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                return false;
            }

            outDto = mapear(res.returningRows[0]);
            return true;
        }

        static bool actualizarTodo(const ConfiguracionRedDTO& dto) {
            asegurarCatalogo();

            const int32_t idTarget = (dto.id_configuracion_red > 0) ? dto.id_configuracion_red : 1;
            auto res = ConfiguracionRed
                .filter_eq("id_configuracion_red", idTarget)
                .update({
                    PgCell("ssid_sta",     dto.ssid_sta.c_str()),
                    PgCell("password_sta", dto.password_sta.c_str()),
                    PgCell("ssid_ap",      dto.ssid_ap.c_str()),
                    PgCell("password_ap",  dto.password_ap.c_str())
                });

            return res.success && res.affectedRows > 0;
        }
    };

}