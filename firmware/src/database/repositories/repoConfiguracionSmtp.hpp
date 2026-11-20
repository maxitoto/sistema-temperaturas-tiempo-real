#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct ConfiguracionSmtpDTO {
        PgInt       idConfiguracionSmtp = 1;
        PgChar<40>  hostSmtp            = "";
        PgPosInt    puerto              = 465;
        PgBool      usarTls             = true;
        PgChar<48>  usuario             = "";
        PgChar<160> passwordCifrado     = "";
        PgChar<48>  emailRemitente      = "";
        PgChar<32>  nombreRemitente     = "Laboratorio UNTDF";
        PgBool      activa              = true;
        PgDateTime  fechaCreacion       = "";
        PgDateTime  fechaModificacion   = "";
    };

    class RepoConfiguracionSmtp {
    private:
        static constexpr const char* TABLA = "ConfiguracionSmtp";

    public:
        static ConfiguracionSmtpDTO mapear(const PgRowView& r) {
            ConfiguracionSmtpDTO dto{};
            dto.idConfiguracionSmtp = r.getInt32("id_configuracion_smtp", 1);
            dto.hostSmtp            = r.getCStr("host_smtp");
            dto.puerto              = r.getUInt32("puerto", 465);
            dto.usarTls             = r.getBool("usar_tls", true);
            dto.usuario             = r.getCStr("usuario");
            dto.passwordCifrado     = r.getCStr("password_cifrado");
            dto.emailRemitente      = r.getCStr("email_remitente");
            dto.nombreRemitente     = r.getCStr("nombre_remitente");
            dto.activa              = r.getBool("activa", true);
            dto.fechaCreacion       = r.getCStr("fecha_creacion");
            dto.fechaModificacion   = r.getCStr("fecha_modificacion");
            return dto;
        }

        static bool obtenerOInicializar(ConfiguracionSmtpDTO& outDto) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            bool encontrado = false;
            st.scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_configuracion_smtp") == 1) {
                    outDto = mapear(r);
                    encontrado = true;
                    return false;
                }
                return true;
            });
            if (encontrado) return true;

            PgRowBuffer row(schema);
            row.setInt32("id_configuracion_smtp", 1);
            row.setText("host_smtp",              "");
            row.setUInt32("puerto",               465);
            row.setBool("usar_tls",               true);
            row.setText("usuario",                "");
            row.setText("password_cifrado",       "");
            row.setText("email_remitente",        "");
            row.setText("nombre_remitente",       "Laboratorio UNTDF");
            row.setBool("activa",                 true);

            if (!st.appendRecord(TABLA, row)) {
                return false;
            }
            outDto = mapear(row.view());
            return true;
        }

        static ConfiguracionSmtpDTO exigirConfiguracion() {
            ConfiguracionSmtpDTO dto{};
            if (!obtenerOInicializar(dto)) {
                throw std::runtime_error("IOError: no se pudo leer ConfiguracionSmtp");
            }
            return dto;
        }

        static ConfiguracionSmtpDTO actualizarTodo(const ConfiguracionSmtpDTO& dto) {
            if (dto.puerto == 0 || dto.puerto > 65535) {
                throw std::invalid_argument("ValueError: el puerto SMTP debe estar entre 1 y 65535");
            }

            ConfiguracionSmtpDTO actual{};
            obtenerOInicializar(actual);

            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [](const PgRowView& r) -> bool { return r.getInt32("id_configuracion_smtp") == 1; },
                [&](PgRowBuffer& row) {
                    row.setText("host_smtp",        dto.hostSmtp.c_str());
                    row.setUInt32("puerto",         dto.puerto);
                    row.setBool("usar_tls",         dto.usarTls);
                    row.setText("usuario",          dto.usuario.c_str());
                    row.setText("password_cifrado", dto.passwordCifrado.c_str());
                    row.setText("email_remitente",  dto.emailRemitente.c_str());
                    row.setText("nombre_remitente", dto.nombreRemitente.c_str());
                    row.setBool("activa",           dto.activa);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("IOError: no se pudo actualizar ConfiguracionSmtp");
            }
            return mapear(updated.view());
        }
    };

} // namespace DbRuntime::Repositories