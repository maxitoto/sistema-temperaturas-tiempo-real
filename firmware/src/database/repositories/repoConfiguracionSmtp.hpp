#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ConfiguracionSmtp;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

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
        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

    public:
        static ConfiguracionSmtpDTO mapear(const PgTuple& r) {
            ConfiguracionSmtpDTO dto{};
            r["id_configuracion_smtp"] >> dto.idConfiguracionSmtp;
            r["host_smtp"]             >> dto.hostSmtp;
            r["puerto"]                >> dto.puerto;
            r["usar_tls"]              >> dto.usarTls;
            r["usuario"]               >> dto.usuario;
            r["password_cifrado"]      >> dto.passwordCifrado;
            r["email_remitente"]       >> dto.emailRemitente;
            r["nombre_remitente"]      >> dto.nombreRemitente;
            r["activa"]                >> dto.activa;
            r["fecha_creacion"]        >> dto.fechaCreacion;
            r["fecha_modificacion"]    >> dto.fechaModificacion;
            return dto;
        }

        static bool obtenerOInicializar(ConfiguracionSmtpDTO& outDto) {
            asegurarCatalogo();

            PgTuple fila;
            if (ConfiguracionSmtp.filter_eq("id_configuracion_smtp", 1).first(fila)) {
                outDto = mapear(fila);
                return true;
            }

            auto res = ConfiguracionSmtp.create(
                {
                    PgCell("id_configuracion_smtp", static_cast<int32_t>(1)),
                    PgCell("host_smtp",             ""),
                    PgCell("puerto",                static_cast<uint32_t>(465)),
                    PgCell("usar_tls",              true),
                    PgCell("usuario",               ""),
                    PgCell("password_cifrado",      ""),
                    PgCell("email_remitente",       ""),
                    PgCell("nombre_remitente",      "Laboratorio UNTDF"),
                    PgCell("activa",                true)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                return false;
            }

            outDto = mapear(res.returningRows[0]);
            return true;
        }

        static ConfiguracionSmtpDTO exigirConfiguracion() {
            ConfiguracionSmtpDTO dto{};
            if (!obtenerOInicializar(dto)) {
                lanzarUltimoErrorDB();
            }
            return dto;
        }

        static ConfiguracionSmtpDTO actualizarTodo(const ConfiguracionSmtpDTO& dto) {
            ConfiguracionSmtpDTO actual{};
            obtenerOInicializar(actual);

            if (dto.puerto == 0 || dto.puerto > 65535) {
                throw std::invalid_argument("ValueError: el puerto SMTP debe estar entre 1 y 65535");
            }

            auto res = ConfiguracionSmtp.filter_eq("id_configuracion_smtp", 1)
                .returning({"*"})
                .update({
                    PgCell("host_smtp",        dto.hostSmtp.c_str()),
                    PgCell("puerto",           dto.puerto),
                    PgCell("usar_tls",         dto.usarTls),
                    PgCell("usuario",          dto.usuario.c_str()),
                    PgCell("password_cifrado", dto.passwordCifrado.c_str()),
                    PgCell("email_remitente",  dto.emailRemitente.c_str()),
                    PgCell("nombre_remitente", dto.nombreRemitente.c_str()),
                    PgCell("activa",           dto.activa)
                });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }
    };

}