#pragma once
#include <Arduino.h>
#include <time.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ServidoresNtp;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct ServidorNtpDTO {
        PgAuto     idServer          = 0;
        PgChar<40> server            = "";
        PgSmallInt prioridad         = 1;
        PgBool     activa            = true;
        PgBigInt   fechaCreacion     = 0;
        PgBigInt   fechaModificacion = 0;
    };

    class RepoServidoresNtp {
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

        static void asegurarSemillaPorDefecto() {
            if (ServidoresNtp.objects().exists()) {
                return;
            }
            const int64_t ahora = epochActual();
            ServidoresNtp.create({
                PgCell("server",             "pool.ntp.org"),
                PgCell("prioridad",          static_cast<int16_t>(1)),
                PgCell("activa",             true),
                PgCell("fecha_creacion",     ahora),
                PgCell("fecha_modificacion", ahora)
            });
            ServidoresNtp.create({
                PgCell("server",             "time.google.com"),
                PgCell("prioridad",          static_cast<int16_t>(2)),
                PgCell("activa",             true),
                PgCell("fecha_creacion",     ahora),
                PgCell("fecha_modificacion", ahora)
            });
        }

    public:
        static ServidorNtpDTO mapear(const PgTuple& r) {
            ServidorNtpDTO dto{};
            r["id_server"]          >> dto.idServer;
            r["server"]             >> dto.server;
            r["prioridad"]          >> dto.prioridad;
            r["activa"]             >> dto.activa;
            r["fecha_creacion"]     >> dto.fechaCreacion;
            r["fecha_modificacion"] >> dto.fechaModificacion;
            return dto;
        }

        static std::vector<ServidorNtpDTO> listarActivosPorPrioridad(uint16_t limite = 32) {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            return ServidoresNtp.filter_eq("activa", "TRUE")
                                .order_by("prioridad")
                                .limit(static_cast<int32_t>(limite))
                                .all_as<ServidorNtpDTO>(&RepoServidoresNtp::mapear);
        }

        static std::vector<ServidorNtpDTO> listarPaginados(uint16_t limite = 32, uint16_t offset = 0) {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            return ServidoresNtp.objects()
                                .order_by("prioridad")
                                .limit(static_cast<int32_t>(limite))
                                .offset(static_cast<int32_t>(offset))
                                .all_as<ServidorNtpDTO>(&RepoServidoresNtp::mapear);
        }

        static ServidorNtpDTO crearServidor(const char* host, int16_t prioridad = 1, bool activa = true) {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            if (host == nullptr || host[0] == '\0') {
                throw std::invalid_argument("ValueError: el host NTP no puede estar vacio");
            }

            const int64_t ahora = epochActual();
            auto res = ServidoresNtp.create(
                {
                    PgCell("server",             host),
                    PgCell("prioridad",          prioridad),
                    PgCell("activa",             activa),
                    PgCell("fecha_creacion",     ahora),
                    PgCell("fecha_modificacion", ahora)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static ServidorNtpDTO actualizarPorHost(const char* host, int16_t prioridad, bool activa) {
            asegurarCatalogo();

            const int64_t ahora = epochActual();
            auto res = ServidoresNtp.filter_eq("server", String(host ? host : ""))
                .returning({"*"})
                .update({
                    PgCell("prioridad",          prioridad),
                    PgCell("activa",             activa),
                    PgCell("fecha_modificacion", ahora)
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el servidor NTP solicitado no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static void eliminarPorHost(const char* host) {
            asegurarCatalogo();

            auto res = ServidoresNtp.filter_eq("server", String(host ? host : "")).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.affectedRows == 0) {
                throw std::runtime_error("LookupError: el servidor NTP a eliminar no existe");
            }
        }
    };

}