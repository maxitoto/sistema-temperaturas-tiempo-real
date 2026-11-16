#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::Correos;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct CorreoDTO {
        PgAuto              idCorreo              = 0;
        PgChar<24>          nombre                = "";
        PgChar<24>          apellido              = "";
        PgChar<48>          correoElectronico     = "";
        TipoRecepcionCorreo tipoRecepcionMensajes = TipoRecepcionCorreo::RECIBIR_TODOS;
        EstadoEnvioCorreo   ultimoEstadoEnvio     = EstadoEnvioCorreo::PENDIENTE;
        PgBool              activa                = true;
        PgDateTime          fechaCreacion         = "";
        PgDateTime          fechaModificacion     = "";
    };

    class RepoCorreos {
    private:
        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

        static bool esEmailValido(const char* email) {
            if (email == nullptr) return false;
            const char* arroba = strchr(email, '@');
            if (arroba == nullptr || arroba == email) return false;
            const char* punto = strchr(arroba, '.');
            return (punto != nullptr && *(punto + 1) != '\0');
        }

    public:
        static CorreoDTO mapear(const PgTuple& r) {
            CorreoDTO c{};
            r["id_correo"]               >> c.idCorreo;
            r["nombre"]                  >> c.nombre;
            r["apellido"]                >> c.apellido;
            r["correo_electronico"]      >> c.correoElectronico;
            r["tipo_recepcion_mensajes"] >> c.tipoRecepcionMensajes;
            r["ultimo_estado_envio"]     >> c.ultimoEstadoEnvio;
            r["activa"]                  >> c.activa;
            r["fecha_creacion"]          >> c.fechaCreacion;
            r["fecha_modificacion"]      >> c.fechaModificacion;
            return c;
        }

        static bool obtenerPorEmail(const char* email, CorreoDTO& outDto) {
            asegurarCatalogo();
            if (email == nullptr || email[0] == '\0') return false;

            PgTuple fila;
            if (!Correos.filter_eq("correo_electronico", String(email)).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
        }

        static CorreoDTO exigirPorEmail(const char* email) {
            CorreoDTO c{};
            if (!obtenerPorEmail(email, c)) {
                throw std::runtime_error("LookupError: el destinatario de correo no existe");
            }
            return c;
        }

        static std::vector<CorreoDTO> listarPaginados(
            uint16_t limite = 32,
            uint16_t offset = 0,
            bool soloActivos = false
        ) {
            asegurarCatalogo();

            auto q = Correos.objects();
            if (soloActivos) {
                q.filter_eq("activa", "TRUE");
            }
            return q.order_by("id_correo")
                    .limit(static_cast<int32_t>(limite))
                    .offset(static_cast<int32_t>(offset))
                    .all_as<CorreoDTO>(&RepoCorreos::mapear);
        }

        static std::vector<CorreoDTO> listarDestinatariosParaCriticidad(CriticidadEvento criticidad) {
            asegurarCatalogo();

            auto activos = listarPaginados(32, 0, true);
            if (criticidad == CriticidadEvento::ALTA) {
                return activos;
            }

            std::vector<CorreoDTO> filtrados;
            for (const auto& d : activos) {
                if (criticidad == CriticidadEvento::NULA) {
                    if (d.tipoRecepcionMensajes == TipoRecepcionCorreo::RECIBIR_TODOS) {
                        filtrados.push_back(d);
                    }
                } else {
                    if (d.tipoRecepcionMensajes != TipoRecepcionCorreo::RECIBIR_EVENTOS_CRITICOS) {
                        filtrados.push_back(d);
                    }
                }
            }
            return filtrados;
        }

        static CorreoDTO crearCorreo(
            const char* nombre,
            const char* apellido,
            const char* email,
            TipoRecepcionCorreo tipoRecepcion = TipoRecepcionCorreo::RECIBIR_TODOS
        ) {
            asegurarCatalogo();

            if (nombre == nullptr || nombre[0] == '\0') {
                throw std::invalid_argument("ValueError: el nombre del destinatario es obligatorio");
            }
            if (!esEmailValido(email)) {
                throw std::invalid_argument("ValueError: el formato de correo_electronico es invalido");
            }

            auto res = Correos.create(
                {
                    PgCell("nombre",                  nombre),
                    PgCell("apellido",                apellido ? apellido : ""),
                    PgCell("correo_electronico",      email),
                    PgCell("tipo_recepcion_mensajes", tipoRecepcion),
                    PgCell("ultimo_estado_envio",     EstadoEnvioCorreo::PENDIENTE),
                    PgCell("activa",                  true)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static CorreoDTO actualizarPorEmail(
            const char* email,
            const char* nombre,
            const char* apellido,
            TipoRecepcionCorreo tipoRecepcion,
            bool activa
        ) {
            asegurarCatalogo();

            auto res = Correos.filter_eq("correo_electronico", String(email ? email : ""))
                .returning({"*"})
                .update({
                    PgCell("nombre",                  nombre ? nombre : ""),
                    PgCell("apellido",                apellido ? apellido : ""),
                    PgCell("tipo_recepcion_mensajes", tipoRecepcion),
                    PgCell("activa",                  activa)
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el correo a actualizar no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static CorreoDTO registrarEstadoEnvioPorEmail(const char* email, EstadoEnvioCorreo estado) {
            asegurarCatalogo();

            auto res = Correos.filter_eq("correo_electronico", String(email ? email : ""))
                .returning({"*"})
                .update({ PgCell("ultimo_estado_envio", estado) });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static void eliminarPorEmail(const char* email) {
            asegurarCatalogo();

            auto res = Correos.filter_eq("correo_electronico", String(email ? email : "")).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.affectedRows == 0) {
                throw std::runtime_error("LookupError: el destinatario a eliminar no existe");
            }
        }
    };

}