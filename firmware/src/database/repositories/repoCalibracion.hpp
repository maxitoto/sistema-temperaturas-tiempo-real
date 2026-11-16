#pragma once
#include <Arduino.h>
#include <time.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::Calibracion;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

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

    public:
        static CalibracionDTO mapear(const PgTuple& r) {
            CalibracionDTO dto{};
            r["id_calibracion"]       >> dto.idCalibracion;
            r["altura"]               >> dto.altura;
            r["presion_atmosferica"]  >> dto.presionAtmosferica;
            r["temp_congelacion_ref"] >> dto.tempCongelacionRef;
            r["temp_ebullicion_ref"]  >> dto.tempEbullicionRef;
            r["activa"]               >> dto.activa;
            r["fecha_creacion"]       >> dto.fechaCreacion;
            r["fecha_modificacion"]   >> dto.fechaModificacion;
            return dto;
        }

        static bool obtenerOInicializar(CalibracionDTO& outDto) {
            asegurarCatalogo();

            PgTuple fila;
            if (Calibracion.filter_eq("id_calibracion", 1).first(fila)) {
                outDto = mapear(fila);
                return true;
            }

            const int64_t ahora = epochActual();
            auto res = Calibracion.create(
                {
                    PgCell("id_calibracion",       static_cast<int32_t>(1)),
                    PgCell("altura",               0.0f),
                    PgCell("presion_atmosferica",  1013.25f),
                    PgCell("temp_congelacion_ref", 0.0f),
                    PgCell("temp_ebullicion_ref",  100.0f),
                    PgCell("activa",               true),
                    PgCell("fecha_creacion",       ahora),
                    PgCell("fecha_modificacion",   ahora)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                return false;
            }

            outDto = mapear(res.returningRows[0]);
            return true;
        }

        static CalibracionDTO exigirCalibracion() {
            CalibracionDTO dto{};
            if (!obtenerOInicializar(dto)) {
                lanzarUltimoErrorDB();
            }
            return dto;
        }

        static CalibracionDTO actualizarTodo(const CalibracionDTO& dto) {
            CalibracionDTO actual{};
            obtenerOInicializar(actual);

            const int64_t ahora = epochActual();
            auto res = Calibracion.filter_eq("id_calibracion", 1)
                .returning({"*"})
                .update({
                    PgCell("altura",               dto.altura),
                    PgCell("presion_atmosferica",  dto.presionAtmosferica),
                    PgCell("temp_congelacion_ref", dto.tempCongelacionRef),
                    PgCell("temp_ebullicion_ref",  dto.tempEbullicionRef),
                    PgCell("activa",               dto.activa),
                    PgCell("fecha_modificacion",   ahora)
                });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }
    };

}