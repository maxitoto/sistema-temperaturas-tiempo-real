#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ModelManager;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::ConflictAction;
    using DbNotRuntime::Standard::PgDataType;

    // 1. DTO plano para que el resto del sistema no dependa de PgTuple ni del ORM
    struct FalsoDTO {
        int32_t  idFalso;
        char     codigo[24];
        float    temperatura;
        bool     activa;
        uint32_t fechaCreacion;
    };

    // 2. Instancia del Manager estilo Django (equivalente a Falso.objects)
    inline const ModelManager Falso("Falso");

    class RepoFalso {
    private:
        // Mapper estático compatible con QuerySet::all_as<FalsoDTO> y PgTuple::mapToStruct
        static FalsoDTO mapear(const PgTuple& fila) {
            FalsoDTO dto{};
            dto.idFalso       = fila.getInt("id_falso", -1);
            dto.temperatura   = fila.getFloat("temperatura", 0.0f);
            dto.activa        = fila.getBool("activa", false);
            dto.fechaCreacion = static_cast<uint32_t>(fila.getBigInt("fecha_creacion", 0));

            String cod = fila.getText("codigo", "");
            strncpy(dto.codigo, cod.c_str(), sizeof(dto.codigo) - 1);
            dto.codigo[sizeof(dto.codigo) - 1] = '\0';
            return dto;
        }

    public:
        // =====================================================================
        // CREATE: Falso.create({...}, {"id_falso"})
        // =====================================================================
        static bool crear(const FalsoDTO& nuevo, int32_t& idGeneradoOut) {
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            auto res = Falso.create({
                {"codigo",         String(nuevo.codigo),         PgDataType::VARCHAR},
                {"temperatura",    String(nuevo.temperatura, 2), PgDataType::REAL},
                {"activa",         nuevo.activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN},
                {"fecha_creacion", String(static_cast<unsigned long>(nuevo.fechaCreacion)), PgDataType::BIGINT}
            }, {"id_falso"});

            if (!res.success) {
                idGeneradoOut = -1;
                return false;
            }

            idGeneradoOut = !res.returningRows.empty()
                ? res.returningRows[0].getInt("id_falso", -1)
                : -1;
            return true;
        }

        // =====================================================================
        // READ (1 registro por PK): Falso.filter_eq("id_falso", id).first(fila)
        // =====================================================================
        static bool obtenerPorId(int32_t idFalso, FalsoDTO& outDto) {
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            PgTuple fila;
            if (!Falso.filter_eq("id_falso", idFalso).first(fila)) {
                return false;
            }

            outDto = mapear(fila);
            return true;
        }

        // =====================================================================
        // QUERY CON 2 FILTROS + ORDEN + LÍMITE:
        // Equivale en Django a:
        // Falso.objects.filter(codigo=cod, temperatura__gte=tempMin)
        //              .order_by("-fecha_creacion")[:limite]
        // =====================================================================
        static std::vector<FalsoDTO> buscarPorCodigoYTempMinima(
            const char* codigoBuscado,
            float tempMinima,
            int32_t limite = 32
        ) {
            if (!codigoBuscado || limite <= 0) return {};
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            return Falso
                .filter_eq("codigo", String(codigoBuscado))
                .filter("temperatura", FilterOp::GTE, String(tempMinima, 2))
                .order_by("-fecha_creacion")
                .limit(limite)
                .all_as<FalsoDTO>(&RepoFalso::mapear);
        }

        // =====================================================================
        // UPDATE: Falso.filter_eq("id_falso", id).update({...})
        // =====================================================================
        static bool actualizarTemperatura(int32_t idFalso, float nuevaTemp, bool activa = true) {
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            auto res = Falso
                .filter_eq("id_falso", idFalso)
                .update({
                    {"temperatura", String(nuevaTemp, 2),   PgDataType::REAL},
                    {"activa",      activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN}
                });

            return res.success && (res.affectedRows > 0);
        }

        // =====================================================================
        // UPSERT (INSERT ... ON CONFLICT DO UPDATE): Falso.upsert(...)
        // =====================================================================
        static bool guardarOActualizarPorCodigo(const FalsoDTO& dto) {
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            auto res = Falso.upsert(
                {
                    {"codigo",         String(dto.codigo),         PgDataType::VARCHAR},
                    {"temperatura",    String(dto.temperatura, 2), PgDataType::REAL},
                    {"activa",         dto.activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN},
                    {"fecha_creacion", String(static_cast<unsigned long>(dto.fechaCreacion)), PgDataType::BIGINT}
                },
                ConflictAction::DO_UPDATE,
                {
                    {"temperatura",    String(dto.temperatura, 2), PgDataType::REAL},
                    {"activa",         dto.activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN}
                }
            );

            return res.success;
        }

        // =====================================================================
        // DELETE: Falso.filter_eq("id_falso", id).remove()
        // =====================================================================
        static bool eliminarPorId(int32_t idFalso) {
            TarjetaSD::ScopedLock lock(TarjetaSD::getInstance().obtenerMutex());

            auto res = Falso
                .filter_eq("id_falso", idFalso)
                .remove();

            return res.success && (res.affectedRows > 0);
        }
    };

} // namespace DbRuntime::Repositories