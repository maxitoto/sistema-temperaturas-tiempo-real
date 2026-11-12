#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ModelManager;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbNotRuntime::Standard::PgDataType;

    struct FalsoDTO {
        int32_t  idFalso;
        char     codigo[24];
        float    temperatura;
        bool     activa;
        uint32_t fechaCreacion;
    };

    inline const ModelManager Falso("Falso");

    class RepoFalso {
    private:
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
        // CREATE individual (concurrencia protegida automáticamente dentro del motor)
        static bool crear(const FalsoDTO& nuevo, int32_t& idGeneradoOut) {
            auto res = Falso.create({
                {"codigo",         String(nuevo.codigo),         PgDataType::VARCHAR},
                {"temperatura",    String(nuevo.temperatura, 2), PgDataType::REAL},
                {"activa",         nuevo.activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN},
                {"fecha_creacion", String(static_cast<unsigned long>(nuevo.fechaCreacion)), PgDataType::BIGINT}
            }, {"id_falso"});

            idGeneradoOut = (res.success && !res.returningRows.empty())
                ? res.returningRows[0].getInt("id_falso", -1)
                : -1;
            return res.success;
        }

        // BATCH CREATE (inserta N registros y toca la MicroSD una sola vez al cerrar TransaccionBatch)
        static uint16_t crearEnLote(const FalsoDTO* lote, size_t cantidad) {
            if (!lote || cantidad == 0) return 0;
            TransaccionBatch tx;
            uint16_t insertados = 0;

            for (size_t i = 0; i < cantidad; ++i) {
                int32_t idDummy = -1;
                if (crear(lote[i], idDummy)) {
                    insertados++;
                }
            }
            return tx.confirmar() ? insertados : 0;
        }

        // READ por PK
        static bool obtenerPorId(int32_t idFalso, FalsoDTO& outDto) {
            PgTuple fila;
            if (!Falso.filter_eq("id_falso", idFalso).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
        }

        // QUERY con 2 filtros (usa el índice de "codigo" automáticamente y filtra temperatura >= tempMinima)
        static std::vector<FalsoDTO> buscarPorCodigoYTempMinima(
            const char* codigoBuscado,
            float tempMinima,
            int32_t limite = 32
        ) {
            if (!codigoBuscado || limite <= 0) return {};
            return Falso
                .filter_eq("codigo", String(codigoBuscado))
                .filter("temperatura", FilterOp::GTE, String(tempMinima, 2))
                .order_by("-fecha_creacion")
                .limit(limite)
                .all_as<FalsoDTO>(&RepoFalso::mapear);
        }

        // UPDATE
        static bool actualizarTemperatura(int32_t idFalso, float nuevaTemp, bool activa = true) {
            auto res = Falso
                .filter_eq("id_falso", idFalso)
                .update({
                    {"temperatura", String(nuevaTemp, 2),   PgDataType::REAL},
                    {"activa",      activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN}
                });
            return res.success && (res.affectedRows > 0);
        }

        // DELETE
        static bool eliminarPorId(int32_t idFalso) {
            auto res = Falso.filter_eq("id_falso", idFalso).remove();
            return res.success && (res.affectedRows > 0);
        }
    };

} // namespace DbRuntime::Repositories