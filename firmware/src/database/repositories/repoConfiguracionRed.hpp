#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

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
        static constexpr const char* TABLA = "ConfiguracionRed";

    public:
        static ConfiguracionRedDTO mapear(const PgRowView& r) {
            ConfiguracionRedDTO dto{};
            dto.id_configuracion_red = r.getInt32("id_configuracion_red", 1);
            dto.ssid_sta             = r.getCStr("ssid_sta");
            dto.password_sta         = r.getCStr("password_sta");
            dto.ssid_ap              = r.getCStr("ssid_ap");
            dto.password_ap          = r.getCStr("password_ap");
            dto.creado_el            = r.getCStr("creado_el");
            dto.actualizado_el       = r.getCStr("actualizado_el");
            return dto;
        }

        static String ultimoError() {
            return DbCommon::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat();
        }

        static bool obtenerOInicializar(ConfiguracionRedDTO& outDto) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            bool encontrado = false;
            st.scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                outDto = mapear(r);
                encontrado = true;
                return false;
            });
            if (encontrado) return true;

            PgRowBuffer row(schema);
            row.setText("ssid_sta",     "");
            row.setText("password_sta", "");
            row.setText("ssid_ap",      "ESP32-SecureAP");
            row.setText("password_ap",  "ESP32secure1234@");

            if (!st.appendRecord(TABLA, row)) {
                return false;
            }
            outDto = mapear(row.view());
            return true;
        }

        static bool actualizarTodo(const ConfiguracionRedDTO& dto) {
            ConfiguracionRedDTO actual{};
            if (!obtenerOInicializar(actual)) return false;

            const int32_t idTarget = (dto.id_configuracion_red > 0) ? dto.id_configuracion_red : actual.id_configuracion_red;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool {
                    return r.getInt32("id_configuracion_red") == idTarget;
                },
                [&](PgRowBuffer& row) {
                    row.setText("ssid_sta",     dto.ssid_sta.c_str());
                    row.setText("password_sta", dto.password_sta.c_str());
                    row.setText("ssid_ap",      dto.ssid_ap.c_str());
                    row.setText("password_ap",  dto.password_ap.c_str());
                }
            );
            return n > 0;
        }
    };

} // namespace DbRuntime::Repositories