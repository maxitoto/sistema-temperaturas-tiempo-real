#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct CalibracionDTO {
        PgInt    idCalibracion       = 1;
        PgFloat  altura              = 115.0f;
        PgFloat  presionAtmosferica  = 1013.25f;
        PgFloat  tempCongelacionRef  = 0.0f;
        PgFloat  tempEbullicionRef   = 100.0f;
        PgBool   activa              = true;
        PgBigInt fechaCreacion       = 0;
        PgBigInt fechaModificacion   = 0;
    };

    class RepoCalibracion {
    private:
        static constexpr const char* TABLA = "Calibracion";

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

    public:
        static CalibracionDTO mapear(const PgRowView& r) {
            CalibracionDTO dto{};
            dto.idCalibracion      = r.getInt32("id_calibracion", 1);
            dto.altura             = r.getFloat("altura", 115.0f);
            dto.presionAtmosferica = r.getFloat("presion_atmosferica", 1013.25f);
            dto.tempCongelacionRef = r.getFloat("temp_congelacion_ref", 0.0f);
            dto.tempEbullicionRef  = r.getFloat("temp_ebullicion_ref", 100.0f);
            dto.activa             = r.getBool("activa", true);
            dto.fechaCreacion      = r.getInt64("fecha_creacion", 0);
            dto.fechaModificacion  = r.getInt64("fecha_modificacion", 0);
            return dto;
        }

        static bool obtenerOInicializar(CalibracionDTO& outDto) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            bool encontrado = false;
            st.scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_calibracion") == 1) {
                    outDto = mapear(r);
                    encontrado = true;
                    return false;
                }
                return true;
            });
            if (encontrado) return true;

            const int64_t ahora = epochActual();
            PgRowBuffer row(schema);
            row.setInt32("id_calibracion",       1);
            row.setFloat("altura",               115.0f);
            row.setFloat("presion_atmosferica",  1013.25f);
            row.setFloat("temp_congelacion_ref", 0.0f);
            row.setFloat("temp_ebullicion_ref",  100.0f);
            row.setBool("activa",                true);
            row.setInt64("fecha_creacion",       ahora);
            row.setInt64("fecha_modificacion",   ahora);

            if (!st.appendRecord(TABLA, row)) {
                return false;
            }
            outDto = mapear(row.view());
            return true;
        }

        static CalibracionDTO exigirCalibracion() {
            CalibracionDTO dto{};
            if (!obtenerOInicializar(dto)) {
                throw std::runtime_error("IOError: no se pudo leer Calibracion");
            }
            return dto;
        }

        static CalibracionDTO actualizarTodo(const CalibracionDTO& dto) {
            if (dto.altura < -500.0f || dto.altura > 9000.0f) {
                throw std::invalid_argument("ValueError: altura fuera de rango (-500..9000 m)");
            }
            if (dto.presionAtmosferica < 300.0f || dto.presionAtmosferica > 1100.0f) {
                throw std::invalid_argument("ValueError: presion_atmosferica fuera de rango (300..1100 hPa)");
            }

            CalibracionDTO actual{};
            obtenerOInicializar(actual);

            const int64_t ahora = epochActual();
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [](const PgRowView& r) -> bool { return r.getInt32("id_calibracion") == 1; },
                [&](PgRowBuffer& row) {
                    row.setFloat("altura",               dto.altura);
                    row.setFloat("presion_atmosferica",  dto.presionAtmosferica);
                    row.setFloat("temp_congelacion_ref", dto.tempCongelacionRef);
                    row.setFloat("temp_ebullicion_ref",  dto.tempEbullicionRef);
                    row.setBool("activa",                dto.activa);
                    row.setInt64("fecha_modificacion",   ahora);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("IOError: no se pudo actualizar Calibracion");
            }
            return mapear(updated.view());
        }
    };

} // namespace DbRuntime::Repositories