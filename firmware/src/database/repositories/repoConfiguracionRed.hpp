#pragma once
#include <Arduino.h>
#include <string.h>
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

        // Actualización independiente solo de STA (no modifica SoftAP; conserva clave si llega vacía)
        static bool actualizarSta(const char* ssidSta, const char* passwordSta, bool conservarClaveSiVacia = true) {
            ConfiguracionRedDTO actual{};
            if (!obtenerOInicializar(actual)) return false;

            const char* ssidFinal   = ssidSta ? ssidSta : "";
            const bool esRedAbierta = (passwordSta != nullptr && strcmp(passwordSta, "__OPEN__") == 0);
            const bool claveVacia   = (!passwordSta || passwordSta[0] == '\0');

            if (!esRedAbierta && !claveVacia && strlen(passwordSta) < 8) {
                throw std::invalid_argument("ValueError: la clave WiFi STA debe tener al menos 8 caracteres (WPA2)");
            }

            const char* claveFinal = esRedAbierta
                ? ""
                : ((claveVacia && conservarClaveSiVacia && ssidFinal[0] != '\0')
                    ? actual.password_sta.c_str()
                    : (passwordSta ? passwordSta : ""));

            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool {
                    return r.getInt32("id_configuracion_red") == actual.id_configuracion_red;
                },
                [&](PgRowBuffer& row) {
                    row.setText("ssid_sta",     ssidFinal);
                    row.setText("password_sta", claveFinal);
                }
            );
            return n > 0;
        }

        // Actualización independiente solo de SoftAP (no modifica STA; conserva clave si llega vacía)
        static bool actualizarAp(const char* ssidAp, const char* passwordAp, bool conservarClaveSiVacia = true) {
            ConfiguracionRedDTO actual{};
            if (!obtenerOInicializar(actual)) return false;

            if (!ssidAp || ssidAp[0] == '\0') {
                throw std::invalid_argument("ValueError: el SSID del SoftAP no puede estar vacio");
            }
            const bool claveVacia = (!passwordAp || passwordAp[0] == '\0');
            if (!claveVacia && strlen(passwordAp) < 8) {
                throw std::invalid_argument("ValueError: la clave del SoftAP debe tener al menos 8 caracteres (WPA2)");
            }

            const char* claveFinal = (claveVacia && conservarClaveSiVacia)
                ? actual.password_ap.c_str()
                : (passwordAp ? passwordAp : "");

            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool {
                    return r.getInt32("id_configuracion_red") == actual.id_configuracion_red;
                },
                [&](PgRowBuffer& row) {
                    row.setText("ssid_ap",     ssidAp);
                    row.setText("password_ap", claveFinal);
                }
            );
            return n > 0;
        }

        // Compatible con el contrato combinado: nunca pisa contraseñas existentes si llegan vacías
        static bool actualizarTodo(const ConfiguracionRedDTO& dto) {
            ConfiguracionRedDTO actual{};
            if (!obtenerOInicializar(actual)) return false;

            const char* ssidStaFinal = dto.ssid_sta.vacio() ? actual.ssid_sta.c_str() : dto.ssid_sta.c_str();
            const char* passStaFinal = dto.password_sta.vacio() ? actual.password_sta.c_str() : dto.password_sta.c_str();
            const char* ssidApFinal  = dto.ssid_ap.vacio() ? actual.ssid_ap.c_str() : dto.ssid_ap.c_str();
            const char* passApFinal  = dto.password_ap.vacio() ? actual.password_ap.c_str() : dto.password_ap.c_str();

            const int32_t idTarget = (dto.id_configuracion_red > 0) ? dto.id_configuracion_red : actual.id_configuracion_red;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool {
                    return r.getInt32("id_configuracion_red") == idTarget;
                },
                [&](PgRowBuffer& row) {
                    row.setText("ssid_sta",     ssidStaFinal);
                    row.setText("password_sta", passStaFinal);
                    row.setText("ssid_ap",      ssidApFinal);
                    row.setText("password_ap",  passApFinal);
                }
            );
            return n > 0;
        }
    };

} // namespace DbRuntime::Repositories