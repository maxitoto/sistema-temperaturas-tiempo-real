#pragma once
#include <Arduino.h>
#include <cstring>
#include "../DB_DQL_DML/orm/DjangoManager.hpp"
#include "../common/DiagnosticConsole.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ConfiguracionRed;
    using DbRuntime::Orm::PgTuple;
    using DbRuntime::Orm::PgCell;
    using DbRuntime::Orm::DmlExecutionResult;

    // Ajustado al DDL: CharField(35) -> char[36] | CharField(64) -> char[65]
    struct ConfiguracionRedDTO {
        int32_t id_configuracion_red = 0;
        char ssid_sta[36]            = {0};
        char password_sta[65]         = {0};
        char ssid_ap[36]             = {0};
        char password_ap[65]         = {0};
        char creado_el[24]           = {0};
        char actualizado_el[24]      = {0};
    };

    class RepoConfiguracionRed {
    private:
        // Si corregiste el typo en PhysicalModelsDefinition.hpp, cambiá a "password_sta"
        static constexpr const char* COL_PASSWORD_STA = "password_sta";

        template <size_t N>
        static void copiarTexto(char (&destino)[N], const String& origen) {
            strncpy(destino, origen.c_str(), N - 1);
            destino[N - 1] = '\0';
        }

        static ConfiguracionRedDTO mapearTupla(const PgTuple& row) {
            ConfiguracionRedDTO dto;
            dto.id_configuracion_red = row.getInt("id_configuracion_red", 0);
            copiarTexto(dto.ssid_sta,       row.getText("ssid_sta", ""));
            copiarTexto(dto.password_sta,    row.getText(COL_PASSWORD_STA, ""));
            copiarTexto(dto.ssid_ap,        row.getText("ssid_ap", "ESP32-SecureAP"));
            copiarTexto(dto.password_ap,    row.getText("password_ap", "ESP32secure1234@"));
            copiarTexto(dto.creado_el,      row.getText("creado_el", ""));
            copiarTexto(dto.actualizado_el, row.getText("actualizado_el", ""));
            return dto;
        }

    public:
        static bool obtenerOInicializar(ConfiguracionRedDTO& outDto) {
            PgTuple row;
            if (ConfiguracionRed.objects().order_by("id_configuracion_red").first(row)) {
                outDto = mapearTupla(row);
                return true;
            }

            // Se pasan explícitos para garantizar NOT NULL sin depender de DEF= en el catálogo
            std::vector<PgCell> iniciales = {
                PgCell("ssid_sta",       ""),
                PgCell(COL_PASSWORD_STA, ""),
                PgCell("ssid_ap",        "ESP32-SecureAP"),
                PgCell("password_ap",    "ESP32secure1234@")
            };

            DmlExecutionResult ins = ConfiguracionRed.create(iniciales, {"*"});
            if (!ins.success || ins.returningRows.empty()) {
                return false;
            }

            outDto = mapearTupla(ins.returningRows[0]);
            return true;
        }

        static bool actualizarTodo(const ConfiguracionRedDTO& dto) {
            std::vector<PgCell> cambios = {
                PgCell("ssid_sta",       dto.ssid_sta),
                PgCell(COL_PASSWORD_STA, dto.password_sta),
                PgCell("ssid_ap",        dto.ssid_ap),
                PgCell("password_ap",    dto.password_ap)
            };
            DmlExecutionResult res = ConfiguracionRed
                .filter_eq("id_configuracion_red", dto.id_configuracion_red)
                .update(cambios);

            return res.success && res.affectedRows > 0;
        }

        static String ultimoError() {
            return DbCommon::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat();
        }
    };

    using repoConfiguracionRed = RepoConfiguracionRed;

} // namespace DbRuntime::Repositories