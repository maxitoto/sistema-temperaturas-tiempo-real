#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"
#include "repoSensor.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::Senso;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::RuntimeStorageAndIndexManager;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct SensoDTO {
        PgAuto       idSenso              = 0;
        PgInt        idSensor             = 0;
        PgFloat      temperaturaCruda     = 0.0f;
        PgFloat      temperaturaCorregida = 0.0f;
        EstadoSensor estadoSensor         = EstadoSensor::OK;
        PgBigInt     fechaCreacion        = 0;
    };

    struct NuevaLecturaSensoDTO {
        PgInt        idSensor             = 0;
        PgFloat      temperaturaCruda     = 0.0f;
        PgFloat      temperaturaCorregida = 0.0f;
        EstadoSensor estadoSensor         = EstadoSensor::OK;
        PgBigInt     fechaCreacion        = 0;
    };

    struct ResumenEstadisticoSensoDTO {
        PgInt   totalLecturas       = 0;
        PgFloat temperaturaMinima   = 0.0f;
        PgFloat temperaturaMaxima   = 0.0f;
        PgFloat temperaturaPromedio = 0.0f;
    };

    class RepoSenso {
    private:
        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static int64_t resolverTimestamp(int64_t tsProvisto) {
            return (tsProvisto > 0) ? tsProvisto : epochActual();
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
        static SensoDTO mapear(const PgTuple& r) {
            SensoDTO s{};
            r["id_senso"]              >> s.idSenso;
            r["id_sensor"]             >> s.idSensor;
            r["temperatura_cruda"]     >> s.temperaturaCruda;
            r["temperatura_corregida"] >> s.temperaturaCorregida;
            r["estado_sensor"]         >> s.estadoSensor;
            r["fecha_creacion"]        >> s.fechaCreacion;
            return s;
        }

        static SensoDTO registrarLectura(
            int32_t idSensor,
            float temperaturaCruda,
            float temperaturaCorregida,
            EstadoSensor estado = EstadoSensor::OK,
            int64_t fechaCreacion = 0
        ) {
            asegurarCatalogo();

            const int64_t ts = resolverTimestamp(fechaCreacion);
            auto res = Senso.create(
                {
                    PgCell("id_sensor",             idSensor),
                    PgCell("temperatura_cruda",     temperaturaCruda),
                    PgCell("temperatura_corregida", temperaturaCorregida),
                    PgCell("estado_sensor",         estado),
                    PgCell("fecha_creacion",        ts)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static uint16_t registrarLoteLecturas(const std::vector<NuevaLecturaSensoDTO>& lote) {
            asegurarCatalogo();
            if (lote.empty()) return 0;

            auto& storage = RuntimeStorageAndIndexManager::getInstance();
            storage.beginBatch();

            uint16_t insertados = 0;
            for (const auto& item : lote) {
                const int64_t ts = resolverTimestamp(item.fechaCreacion);
                auto res = Senso.create({
                    PgCell("id_sensor",             item.idSensor),
                    PgCell("temperatura_cruda",     item.temperaturaCruda),
                    PgCell("temperatura_corregida", item.temperaturaCorregida),
                    PgCell("estado_sensor",         item.estadoSensor),
                    PgCell("fecha_creacion",        ts)
                });
                if (res.success) {
                    insertados += res.affectedRows;
                }
            }

            if (!storage.commitBatch()) {
                lanzarUltimoErrorDB();
            }
            return insertados;
        }

        static bool obtenerPorId(int32_t idSenso, SensoDTO& outDto) {
            asegurarCatalogo();
            PgTuple fila;
            if (!Senso.filter_eq("id_senso", idSenso).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
        }

        static std::vector<SensoDTO> listarPorIdSensor(
            int32_t idSensor,
            uint16_t limite = 32,
            uint16_t offset = 0
        ) {
            asegurarCatalogo();

            return Senso.filter_eq("id_sensor", idSensor)
                        .order_by("-fecha_creacion")
                        .limit(static_cast<int32_t>(limite))
                        .offset(static_cast<int32_t>(offset))
                        .all_as<SensoDTO>(&RepoSenso::mapear);
        }

        static std::vector<SensoDTO> listarPorIdSensorYRangoFecha(
            int32_t idSensor,
            int64_t timestampDesde,
            int64_t timestampHasta,
            uint16_t limite = 32
        ) {
            asegurarCatalogo();

            if (timestampDesde > timestampHasta) {
                throw std::invalid_argument("ValueError: timestampDesde no puede ser mayor que timestampHasta");
            }

            char bufDesde[24]{};
            char bufHasta[24]{};
            snprintf(bufDesde, sizeof(bufDesde), "%lld", static_cast<long long>(timestampDesde));
            snprintf(bufHasta, sizeof(bufHasta), "%lld", static_cast<long long>(timestampHasta));

            return Senso.filter_eq("id_sensor", idSensor)
                        .filter("fecha_creacion", FilterOp::BETWEEN, String(bufDesde), String(bufHasta))
                        .order_by("-fecha_creacion")
                        .limit(static_cast<int32_t>(limite))
                        .all_as<SensoDTO>(&RepoSenso::mapear);
        }

        static std::vector<SensoDTO> listarPorMac(const char* macHex, uint16_t limite = 32) {
            SensorDTO sensor = RepoSensor::exigirPorMac(macHex);
            return listarPorIdSensor(sensor.idSensor, limite, 0);
        }

        static std::vector<SensoDTO> listarPorMacYRangoFecha(
            const char* macHex,
            int64_t timestampDesde,
            int64_t timestampHasta,
            uint16_t limite = 32
        ) {
            SensorDTO sensor = RepoSensor::exigirPorMac(macHex);
            return listarPorIdSensorYRangoFecha(sensor.idSensor, timestampDesde, timestampHasta, limite);
        }

        static ResumenEstadisticoSensoDTO obtenerEstadisticasPorSensor(int32_t idSensor) {
            asegurarCatalogo();

            PgTuple r;
            ResumenEstadisticoSensoDTO out{};
            if (Senso.filter_eq("id_sensor", idSensor)
                     .annotate("total",    AggregateFunc::COUNT_ALL)
                     .annotate("temp_min", AggregateFunc::MIN, "temperatura_corregida")
                     .annotate("temp_max", AggregateFunc::MAX, "temperatura_corregida")
                     .annotate("temp_avg", AggregateFunc::AVG, "temperatura_corregida")
                     .first(r)) {
                r["total"]    >> out.totalLecturas;
                r["temp_min"] >> out.temperaturaMinima;
                r["temp_max"] >> out.temperaturaMaxima;
                r["temp_avg"] >> out.temperaturaPromedio;
            }
            return out;
        }

        static uint16_t purgarAnterioresA(int64_t timestampLimite) {
            asegurarCatalogo();

            char bufLimite[24]{};
            snprintf(bufLimite, sizeof(bufLimite), "%lld", static_cast<long long>(timestampLimite));

            auto res = Senso.filter("fecha_creacion", FilterOp::LT, String(bufLimite)).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            return res.affectedRows;
        }
    };

}