#ifndef RP_SQL_TERMINAL_H
#define RP_SQL_TERMINAL_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/common/PhysicalSchemaCatalog.hpp"
#include "../../../../database/sql/PgSqlEngine.hpp"
#include "../../../../database/sql/SchemaPostgreSQL.hpp"

// ============================================================================
// 1. DTOs PARA EL DIAGRAMA DE ESTRUCTURA DE TABLAS (getStructure)
// ============================================================================

struct NodoTablaEstructuraDTO {
    PgChar<24> nombreTabla;   // 0..23
    PgChar<20> pkColumna;     // 24..43
    PgUInt8    totalColumnas; // 44
    PgUInt8    totalIndices;  // 45
}; // 46 bytes

struct RelacionFkEstructuraDTO {
    PgChar<20> tablaOrigen;    // 0..19
    PgChar<20> columnaOrigen;  // 20..39
    PgChar<20> tablaDestino;   // 40..59
    PgChar<20> columnaDestino; // 60..79
}; // 80 bytes

struct EstructuraCompletaDbDTO {
    PgUInt8                 totalTablas;       // 0
    PgUInt8                 totalRelacionesFk; // 1
    PgUInt8                 totalEnums;        // 2
    NodoTablaEstructuraDTO  tablas[14];        // 3..646   (644 B)
    RelacionFkEstructuraDTO relacionesFk[8];   // 647..1286 (640 B)
}; // 1287 bytes (< 1536 B)

struct ConsultaTablaEstructuraDTO {
    PgChar<32> nombreTabla;
};

struct ColumnaEstructuraDTO {
    PgChar<24>    nombreColumna;  // 0..23
    PgChar<16>    tipoSql;        // 24..39
    PgPosSmallInt longitudMaxima; // 40..41 (uint16)
    PgBool        esPrimaryKey;   // 42
    PgBool        esNotNull;      // 43
    PgBool        esUnique;       // 44
    PgBool        esIdentity;     // 45
    PgBool        esForeignKey;   // 46
    PgChar<12>    fkTabla;        // 47..58
    PgChar<12>    fkColumna;      // 59..70
    // 1 byte padding (71)
}; // 72 bytes

using ListaColumnasEstructuraDTO = PgList<ColumnaEstructuraDTO, 20>;

// ============================================================================
// 2. DTOs PARA LA TERMINAL SQL INTERACTIVA (executeSql)
// ============================================================================

struct ConsultaSqlTerminalDTO {
    PgChar<384> sentenciaSql;
};

struct ResultadoSqlTerminalDTO {
    PgBool        exito;
    PgChar<8>     sqlState;
    PgChar<28>    comandoEjecutado;
    PgPosSmallInt filasAfectadas;
    PgPosInt      tiempoEjecucionUs;
    PgChar<1024>  salidaPsql;
};

namespace RPSqlTerminal {

    using DbCommon::Engine::PhysicalSchemaCatalog;
    using DbCommon::Standard::DataTypeUtils;
    using DbCommon::Standard::IdentityGeneration;
    using DbCommon::Standard::PgDataType;

    // 1. Nodos de Tablas y Relaciones FK para el Gráfico ER
    inline auto obtenerEstructuraBaseDatos = llamadoSemantico.alMenosUnaVez<void, EstructuraCompletaDbDTO>(
        "obtenerEstructuraBaseDatos",
        []() -> EstructuraCompletaDbDTO {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.getAllTables().empty()) {
                DbSql::Schema::bindOrBootstrapDatabase();
            }

            EstructuraCompletaDbDTO out{};
            uint8_t idxT  = 0;
            uint8_t idxFk = 0;

            for (const auto& kv : cat.getAllTables()) {
                if (idxT >= 14) break;
                const auto& tb = kv.second;
                NodoTablaEstructuraDTO& nodo = out.tablas[idxT++];
                nodo.nombreTabla   = tb.tableName.c_str();
                nodo.totalColumnas = static_cast<uint8_t>(tb.columns.size());

                uint8_t nIdx = 0;
                for (const auto& idxPair : cat.getAllIndexes()) {
                    if (idxPair.second.tableName.equalsIgnoreCase(tb.tableName)) nIdx++;
                }
                nodo.totalIndices = nIdx;

                for (const auto& col : tb.columns) {
                    if (col.isPrimaryKey) {
                        nodo.pkColumna = col.columnName.c_str();
                    }
                    if (col.isForeignKey && idxFk < 8) {
                        RelacionFkEstructuraDTO& rel = out.relacionesFk[idxFk++];
                        rel.tablaOrigen    = tb.tableName.c_str();
                        rel.columnaOrigen  = col.columnName.c_str();
                        rel.tablaDestino   = col.referencedTable.c_str();
                        rel.columnaDestino = col.referencedColumn.c_str();
                    }
                }
            }

            out.totalTablas       = idxT;
            out.totalRelacionesFk = idxFk;
            out.totalEnums        = static_cast<uint8_t>(cat.getAllEnums().size());
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Columnas detalladas de una tabla
    inline auto obtenerColumnasTablaEstructura = llamadoSemantico.alMenosUnaVez<ConsultaTablaEstructuraDTO, ListaColumnasEstructuraDTO>(
        "obtenerColumnasTablaEstructura",
        [](const ConsultaTablaEstructuraDTO& in) -> ListaColumnasEstructuraDTO {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            const auto* tb = cat.getTable(in.nombreTabla.c_str());
            if (!tb) {
                throw std::runtime_error("UndefinedTable: la tabla solicitada no existe en pg_catalog");
            }

            ListaColumnasEstructuraDTO out{};
            for (const auto& c : tb->columns) {
                ColumnaEstructuraDTO item{};
                item.nombreColumna  = c.columnName.c_str();
                item.tipoSql        = (c.dataType == PgDataType::ENUM)
                                      ? c.customTypeName.c_str()
                                      : DataTypeUtils::toString(c.dataType);
                item.longitudMaxima = c.maxLength;
                item.esPrimaryKey   = c.isPrimaryKey;
                item.esNotNull      = c.isNotNull;
                item.esUnique       = c.isUnique;
                item.esIdentity     = (c.identityMode != IdentityGeneration::NONE);
                item.esForeignKey   = c.isForeignKey;
                item.fkTabla        = c.referencedTable.c_str();
                item.fkColumna      = c.referencedColumn.c_str();
                if (!out.agregar(item)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 3. Terminal SQL Interactiva PostgreSQL (Solo ADMIN)
    inline auto ejecutarSqlTerminal = llamadoSemantico.exactamenteUnaVez<ConsultaSqlTerminalDTO, ResultadoSqlTerminalDTO>(
        "ejecutarSqlTerminal",
        [](const ConsultaSqlTerminalDTO& entrada) -> ResultadoSqlTerminalDTO {
            const DbSql::SqlExecResult& r = DbSql::PgSqlEngine::getInstance().execute(entrada.sentenciaSql.c_str());
            ResultadoSqlTerminalDTO res{};
            res.exito             = r.exito;
            res.sqlState          = r.sqlState;
            res.comandoEjecutado  = r.comandoEjecutado;
            res.filasAfectadas    = r.filasAfectadas;
            res.tiempoEjecucionUs = r.tiempoEjecucionUs;
            res.salidaPsql        = r.salidaPsql;
            return res;
        },
        RolUsuario::ADMIN
    );

} // namespace RPSqlTerminal

#endif