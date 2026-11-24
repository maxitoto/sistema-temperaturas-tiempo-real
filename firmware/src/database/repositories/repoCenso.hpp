#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"
#include "repoSensor.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct CensoDTO {
        PgAuto       idCenso              = 0;
        PgInt        idSensor             = 0;
        PgFloat      temperaturaCruda     = 0.0f;
        PgFloat      temperaturaCorregida = 0.0f;
        EstadoSensor estadoSensor         = EstadoSensor::OK;
        PgBigInt     fechaCreacion        = 0;
    };

    struct NuevaLecturaCensoDTO {
        PgInt        idSensor             = 0;
        PgFloat      temperaturaCruda     = 0.0f;
        PgFloat      temperaturaCorregida = 0.0f;
        EstadoSensor estadoSensor         = EstadoSensor::OK;
        PgBigInt     fechaCreacion        = 0;
    };

    struct ResumenEstadisticoCensoDTO {
        PgInt   totalLecturas       = 0;
        PgFloat temperaturaMinima   = 0.0f;
        PgFloat temperaturaMaxima   = 0.0f;
        PgFloat temperaturaPromedio = 0.0f;
    };

    class RepoCenso {
    private:
        static constexpr const char* TABLA = "Censo";

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static int64_t resolverTimestamp(int64_t tsProvisto) {
            return (tsProvisto > 0) ? tsProvisto : epochActual();
        }

    public:
        static CensoDTO mapear(const PgRowView& r) {
            CensoDTO s{};
            s.idCenso              = r.getInt32("id_censo");
            s.idSensor             = r.getInt32("id_sensor");
            s.temperaturaCruda     = r.getFloat("temperatura_cruda");
            s.temperaturaCorregida = r.getFloat("temperatura_corregida");
            s.estadoSensor         = r.getEnum<EstadoSensor>("estado_sensor", EstadoSensor::OK);
            s.fechaCreacion        = r.getInt64("fecha_creacion");
            return s;
        }

        static CensoDTO registrarLectura(
            int32_t idSensor,
            float temperaturaCruda,
            float temperaturaCorregida,
            EstadoSensor estado = EstadoSensor::OK,
            int64_t fechaCreacion = 0
        ) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            PgRowBuffer row(schema);
            row.setInt32("id_sensor",             idSensor);
            row.setFloat("temperatura_cruda",     temperaturaCruda);
            row.setFloat("temperatura_corregida", temperaturaCorregida);
            row.setEnum("estado_sensor",          estado);
            row.setInt64("fecha_creacion",        resolverTimestamp(fechaCreacion));

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar la lectura en Censo");
            }
            return mapear(row.view());
        }

        static uint16_t registrarLoteLecturas(const std::vector<NuevaLecturaCensoDTO>& lote) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            if (lote.empty()) return 0;

            auto& st = PgBinaryStorage::getInstance();
            TransaccionBatch tx;
            uint16_t insertados = 0;

            for (const auto& item : lote) {
                PgRowBuffer row(schema);
                row.setInt32("id_sensor",             item.idSensor);
                row.setFloat("temperatura_cruda",     item.temperaturaCruda);
                row.setFloat("temperatura_corregida", item.temperaturaCorregida);
                row.setEnum("estado_sensor",          item.estadoSensor);
                row.setInt64("fecha_creacion",        resolverTimestamp(item.fechaCreacion));
                if (st.appendRecord(TABLA, row)) {
                    insertados++;
                }
            }
            if (!tx.confirmar()) {
                throw std::runtime_error("IOError: fallo commitBatch en Censo");
            }
            return insertados;
        }

        static bool obtenerPorId(int32_t idCenso, CensoDTO& outDto) {
            asegurarEsquemaTabla(TABLA);
            bool ok = false;
            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_censo") == idCenso) {
                    outDto = mapear(r);
                    ok = true;
                    return false;
                }
                return true;
            });
            return ok;
        }

        // Paginación rápida inversa (idSensor == 0 trae todos los sensores)
        static std::vector<CensoDTO> listarPaginados(
            int32_t idSensor = 0,
            uint16_t limite = 32,
            uint16_t offset = 0
        ) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0 || limite > 32) limite = 32;

            std::vector<CensoDTO> out;
            out.reserve(limite);
            uint32_t matched = 0;

            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (idSensor > 0 && r.getInt32("id_sensor") != idSensor) return true;
                if (matched >= offset) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                matched++;
                return true;
            });
            return out;
        }

        static std::vector<CensoDTO> listarPorIdSensor(
            int32_t idSensor,
            uint16_t limite = 32,
            uint16_t offset = 0
        ) {
            return listarPaginados(idSensor, limite, offset);
        }

        static std::vector<CensoDTO> listarPorIdSensorYRangoFecha(
            int32_t idSensor,
            int64_t timestampDesde,
            int64_t timestampHasta,
            uint16_t limite = 32
        ) {
            asegurarEsquemaTabla(TABLA);
            if (timestampDesde > timestampHasta) {
                throw std::invalid_argument("ValueError: timestampDesde no puede ser mayor que timestampHasta");
            }
            if (limite == 0 || limite > 32) limite = 32;

            std::vector<CensoDTO> out;
            out.reserve(limite);

            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_sensor") != idSensor) return true;
                const int64_t ts = r.getInt64("fecha_creacion");
                if (ts >= timestampDesde && ts <= timestampHasta) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                return true;
            });
            return out;
        }

        static std::vector<CensoDTO> listarPorMac(const char* macHex, uint16_t limite = 32) {
            SensorDTO sensor = RepoSensor::exigirPorMac(macHex);
            return listarPorIdSensor(sensor.idSensor, limite, 0);
        }

        static std::vector<CensoDTO> listarPorMacYRangoFecha(
            const char* macHex,
            int64_t timestampDesde,
            int64_t timestampHasta,
            uint16_t limite = 32
        ) {
            SensorDTO sensor = RepoSensor::exigirPorMac(macHex);
            return listarPorIdSensorYRangoFecha(sensor.idSensor, timestampDesde, timestampHasta, limite);
        }

        static ResumenEstadisticoCensoDTO obtenerEstadisticasPorSensor(int32_t idSensor) {
            asegurarEsquemaTabla(TABLA);
            ResumenEstadisticoCensoDTO out{};
            double suma = 0.0;
            bool primero = true;

            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_sensor") != idSensor) return true;
                const float t = r.getFloat("temperatura_corregida");
                out.totalLecturas++;
                suma += static_cast<double>(t);
                if (primero) {
                    out.temperaturaMinima = t;
                    out.temperaturaMaxima = t;
                    primero = false;
                } else {
                    if (t < out.temperaturaMinima) out.temperaturaMinima = t;
                    if (t > out.temperaturaMaxima) out.temperaturaMaxima = t;
                }
                return true;
            });

            if (out.totalLecturas > 0) {
                out.temperaturaPromedio = static_cast<float>(suma / static_cast<double>(out.totalLecturas));
            }
            return out;
        }

        static void eliminarPorId(int32_t idCenso) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_censo") == idCenso; },
                true
            );
        }

        static uint16_t purgarAnterioresA(int64_t timestampLimite) {
            asegurarEsquemaTabla(TABLA);
            return PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt64("fecha_creacion") < timestampLimite; },
                true
            );
        }
    };

} // namespace DbRuntime::Repositories