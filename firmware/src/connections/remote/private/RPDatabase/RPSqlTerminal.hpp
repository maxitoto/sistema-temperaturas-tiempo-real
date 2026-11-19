#ifndef RP_SQL_TERMINAL_H
#define RP_SQL_TERMINAL_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/common/DiagnosticConsole.hpp"
#include "../../../../database/common/PhysicalSchemaCatalog.hpp"
#include "../../../../database/DB_DDL/engine/ModuloDDL.hpp"
#include "../../../../database/DB_DQL_DML/engine/ModuloDQL.hpp"
#include "../../../../database/DB_DQL_DML/engine/ModuloDML.hpp"
#include "../../../../database/DB_DQL_DML/orm/DjangoManager.hpp"

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
}; // 72 bytes (19 cols * 72 = 1368 B < 1536 B)

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

    using DbNotRuntime::Engine::PhysicalSchemaCatalog;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::ModuloDDL;
    using DbNotRuntime::Standard::PgDataType;
    using DbNotRuntime::Standard::DataTypeUtils;
    using DbNotRuntime::Standard::SqlState;
    using DbRuntime::Engine::ModuloDQL;
    using DbRuntime::Engine::ModuloDML;
    using DbRuntime::Engine::SelectQueryPlan;
    using DbRuntime::Engine::PredicateCondition;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::PgCell;

    inline String limpiarComillas(const String& s) {
        String t = s;
        t.trim();
        if (t.length() >= 2 && ((t.startsWith("'") && t.endsWith("'")) || (t.startsWith("\"") && t.endsWith("\"")))) {
            return t.substring(1, t.length() - 1);
        }
        return t;
    }

    inline std::vector<String> dividirPorComaRespetandoComillas(const String& src) {
        std::vector<String> partes;
        String actual = "";
        bool enSimple = false;
        bool enDoble  = false;
        for (size_t i = 0; i < src.length(); ++i) {
            char c = src[i];
            if (c == '\'' && !enDoble) enSimple = !enSimple;
            else if (c == '"' && !enSimple) enDoble = !enDoble;

            if (c == ',' && !enSimple && !enDoble) {
                actual.trim();
                partes.push_back(actual);
                actual = "";
            } else {
                actual += c;
            }
        }
        actual.trim();
        if (!actual.isEmpty()) partes.push_back(actual);
        return partes;
    }

    inline bool parsearCondicionSimpleWhere(const String& whereStr, std::vector<PredicateCondition>& outPreds) {
        String w = whereStr;
        w.trim();
        if (w.isEmpty()) return true;

        PredicateCondition cond;
        const char* ops[] = { ">=", "<=", "<>", "!=", "=", ">", "<", " ILIKE ", " LIKE " };
        FilterOp    fOps[]= { FilterOp::GTE, FilterOp::LTE, FilterOp::NEQ, FilterOp::NEQ,
                              FilterOp::EQ,  FilterOp::GT,  FilterOp::LT,  FilterOp::ILIKE, FilterOp::LIKE };

        String wUpper = w;
        wUpper.toUpperCase();

        for (size_t i = 0; i < 9; ++i) {
            int pos = wUpper.indexOf(ops[i]);
            if (pos > 0) {
                cond.columnOrExpr = limpiarComillas(w.substring(0, pos));
                cond.op           = fOps[i];
                cond.valueA       = limpiarComillas(w.substring(pos + strlen(ops[i])));
                outPreds.push_back(cond);
                return true;
            }
        }
        return false;
    }

    inline PgDataType resolverTipoSqlDDL(const String& typeToken, uint16_t& outLen) {
        outLen = 0;
        String u = typeToken;
        u.trim();
        u.toUpperCase();

        int pOpen = u.indexOf('(');
        int pClose = u.indexOf(')');
        if (pOpen > 0 && pClose > pOpen) {
            outLen = static_cast<uint16_t>(u.substring(pOpen + 1, pClose).toInt());
            u = u.substring(0, pOpen);
            u.trim();
        }

        if (u == "SMALLINT" || u == "INT2" || u == "SMALLSERIAL") return PgDataType::SMALLINT;
        if (u == "INTEGER" || u == "INT" || u == "INT4" || u == "SERIAL") return PgDataType::INTEGER;
        if (u == "BIGINT" || u == "INT8" || u == "BIGSERIAL") return PgDataType::BIGINT;
        if (u == "REAL" || u == "FLOAT4" || u == "FLOAT") return PgDataType::REAL;
        if (u == "DOUBLE" || u == "DOUBLE PRECISION" || u == "FLOAT8") return PgDataType::DOUBLE_PRECISION;
        if (u == "BOOLEAN" || u == "BOOL") return PgDataType::BOOLEAN;
        if (u == "CHAR" || u == "CHARACTER") return PgDataType::CHAR;
        if (u == "VARCHAR" || u == "CHARACTER VARYING") {
            if (outLen == 0) outLen = 64;
            return PgDataType::VARCHAR;
        }
        if (u == "TEXT") return PgDataType::TEXT;
        if (u == "DATE") return PgDataType::DATE;
        if (u == "TIME") return PgDataType::TIME;
        if (u == "TIMESTAMP") return PgDataType::TIMESTAMP;
        if (u == "TIMESTAMPTZ") return PgDataType::TIMESTAMP_TZ;
        if (u == "JSON") return PgDataType::JSON;
        return PgDataType::VARCHAR;
    }

    // ========================================================================
    // 1. getStructure() -> Nodos de Tablas y Relaciones FK para el Gráfico ER
    // ========================================================================
    inline auto obtenerEstructuraBaseDatos = llamadoSemantico.alMenosUnaVez<void, EstructuraCompletaDbDTO>(
        "obtenerEstructuraBaseDatos",
        []() -> EstructuraCompletaDbDTO {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.getAllTables().empty()) {
                DbRuntime::Orm::runtimeDb.bindToPhysicalCatalog();
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
            out.totalEnums        = 10;
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // ========================================================================
    // 2. getTableStructure(tabla) -> Columnas detalladas de una tabla
    // ========================================================================
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
                item.esIdentity     = (c.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE);
                item.esForeignKey   = c.isForeignKey;
                item.fkTabla        = c.referencedTable.c_str();
                item.fkColumna      = c.referencedColumn.c_str();
                if (!out.agregar(item)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // ========================================================================
    // 3. executeSql(sql) -> Terminal SQL Interactiva PostgreSQL (Solo ADMIN)
    // ========================================================================
    inline auto ejecutarSqlTerminal = llamadoSemantico.exactamenteUnaVez<ConsultaSqlTerminalDTO, ResultadoSqlTerminalDTO>(
        "ejecutarSqlTerminal",
        [](const ConsultaSqlTerminalDTO& entrada) -> ResultadoSqlTerminalDTO {
            ResultadoSqlTerminalDTO res{};
            const int64_t t0 = esp_timer_get_time();

            String sql = String(entrada.sentenciaSql.c_str());
            sql.trim();
            while (sql.endsWith(";")) {
                sql.remove(sql.length() - 1);
                sql.trim();
            }

            if (sql.isEmpty()) {
                res.exito            = false;
                res.sqlState         = SqlState::SYNTAX_ERROR;
                res.comandoEjecutado = "EMPTY";
                res.salidaPsql       = "ERROR:  empty query string [SQLSTATE 42601]";
                return res;
            }

            String upper = sql;
            upper.toUpperCase();
            auto& console = DdlDiagnosticConsole::getInstance();
            auto& cat     = PhysicalSchemaCatalog::getInstance();

            // --- A. SHOW TABLES / \dt ---
            if (upper == "SHOW TABLES" || upper == "\\DT") {
                String txt = " List of relations (pg_catalog)\n Schema |          Name          | Cols \n--------+------------------------+------\n";
                uint16_t count = 0;
                for (const auto& kv : cat.getAllTables()) {
                    char rowBuf[64];
                    snprintf(rowBuf, sizeof(rowBuf), " public | %-22s | %4u\n",
                             kv.second.tableName.c_str(),
                             static_cast<unsigned>(kv.second.columns.size()));
                    txt += rowBuf;
                    count++;
                }
                txt += "(" + String(count) + " rows)";
                res.exito             = true;
                res.sqlState          = "00000";
                res.comandoEjecutado  = "SHOW TABLES";
                res.filasAfectadas    = count;
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = txt.c_str();
                return res;
            }

            // --- B. SELECT ... FROM ... ---
            if (upper.startsWith("SELECT ")) {
                int posFrom = upper.indexOf(" FROM ");
                if (posFrom < 0) {
                    console.raiseError(SqlState::SYNTAX_ERROR, "SELECT", "", "", "missing FROM-clause entry for SELECT");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "SELECT";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                SelectQueryPlan plan;
                String colsPart = sql.substring(7, posFrom);
                colsPart.trim();
                if (colsPart != "*") {
                    plan.selectColumns = dividirPorComaRespetandoComillas(colsPart);
                }

                String tail      = sql.substring(posFrom + 6);
                String tailUpper = tail;
                tailUpper.toUpperCase();

                int posWhere = tailUpper.indexOf(" WHERE ");
                int posOrder = tailUpper.indexOf(" ORDER BY ");
                int posLimit = tailUpper.indexOf(" LIMIT ");

                int endTable = tail.length();
                if (posWhere >= 0 && posWhere < endTable) endTable = posWhere;
                if (posOrder >= 0 && posOrder < endTable) endTable = posOrder;
                if (posLimit >= 0 && posLimit < endTable) endTable = posLimit;

                plan.fromTable = limpiarComillas(tail.substring(0, endTable));
                if (!cat.hasTable(plan.fromTable)) {
                    console.raiseError(SqlState::UNDEFINED_TABLE, "SELECT", plan.fromTable, "",
                                       "relation \"" + plan.fromTable + "\" does not exist");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "SELECT";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                if (posWhere >= 0) {
                    int endWhere = tail.length();
                    if (posOrder > posWhere && posOrder < endWhere) endWhere = posOrder;
                    if (posLimit > posWhere && posLimit < endWhere) endWhere = posLimit;
                    parsearCondicionSimpleWhere(tail.substring(posWhere + 7, endWhere), plan.wherePredicates);
                }

                if (posOrder >= 0) {
                    int endOrder = (posLimit > posOrder) ? posLimit : static_cast<int>(tail.length());
                    String obRaw = tail.substring(posOrder + 10, endOrder);
                    obRaw.trim();
                    DbRuntime::Engine::OrderBySpec ob;
                    if (obRaw.endsWith(" DESC") || obRaw.endsWith(" desc")) {
                        ob.column    = limpiarComillas(obRaw.substring(0, obRaw.length() - 5));
                        ob.direction = DbNotRuntime::Standard::SortDirection::DESC;
                    } else if (obRaw.endsWith(" ASC") || obRaw.endsWith(" asc")) {
                        ob.column    = limpiarComillas(obRaw.substring(0, obRaw.length() - 4));
                        ob.direction = DbNotRuntime::Standard::SortDirection::ASC;
                    } else {
                        ob.column    = limpiarComillas(obRaw);
                    }
                    plan.orderBy.push_back(ob);
                }

                plan.limitCount = 16;
                if (posLimit >= 0) {
                    int lim = tail.substring(posLimit + 7).toInt();
                    if (lim > 0 && lim <= 32) plan.limitCount = lim;
                }

                auto filas = ModuloDQL::getInstance().executeSelect(plan);
                String outTxt = "";
                if (!filas.empty()) {
                    for (size_t c = 0; c < filas[0].allCells().size(); ++c) {
                        if (c > 0) outTxt += " | ";
                        outTxt += filas[0].allCells()[c].columnName;
                    }
                    outTxt += "\n------------------------------------------------\n";
                    for (const auto& r : filas) {
                        for (size_t c = 0; c < r.allCells().size(); ++c) {
                            if (c > 0) outTxt += " | ";
                            outTxt += r.allCells()[c].rawText;
                        }
                        outTxt += "\n";
                        if (outTxt.length() > 920) {
                            outTxt += "... (truncated to fit 1KB terminal buffer)\n";
                            break;
                        }
                    }
                }
                outTxt += "(" + String(static_cast<unsigned>(filas.size())) + " rows)";

                res.exito             = true;
                res.sqlState          = "00000";
                res.comandoEjecutado  = "SELECT";
                res.filasAfectadas    = static_cast<uint16_t>(filas.size());
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = outTxt.c_str();
                return res;
            }

            // --- C. INSERT INTO <tabla> (cols) VALUES (vals) ---
            if (upper.startsWith("INSERT INTO ")) {
                int posValues = upper.indexOf(" VALUES");
                int p1Open    = sql.indexOf('(');
                int p1Close   = sql.indexOf(')');
                int p2Open    = (posValues > 0) ? sql.indexOf('(', posValues) : -1;
                int p2Close   = (p2Open > 0)    ? sql.lastIndexOf(')')        : -1;

                if (posValues < 0 || p1Open < 0 || p1Close < p1Open || p2Open < 0 || p2Close < p2Open) {
                    console.raiseError(SqlState::SYNTAX_ERROR, "INSERT", "", "",
                                       "syntax error in INSERT INTO (expected: INSERT INTO table (col1, col2) VALUES (val1, val2))");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "INSERT";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                String tableName = limpiarComillas(sql.substring(12, p1Open));
                auto cols = dividirPorComaRespetandoComillas(sql.substring(p1Open + 1, p1Close));
                auto vals = dividirPorComaRespetandoComillas(sql.substring(p2Open + 1, p2Close));

                if (cols.size() != vals.size() || cols.empty()) {
                    console.raiseError(SqlState::SYNTAX_ERROR, "INSERT", tableName, "",
                                       "INSERT has more target columns than expressions or vice versa");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "INSERT";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                const auto* schema = cat.getTable(tableName);
                PgTuple tuple;
                for (size_t i = 0; i < cols.size(); ++i) {
                    String cName = limpiarComillas(cols[i]);
                    String vLit  = limpiarComillas(vals[i]);
                    const auto* colDef = schema ? schema->findColumn(cName) : nullptr;
                    tuple.set(cName, vLit, colDef ? colDef->dataType : PgDataType::TEXT);
                }

                auto dmlRes = ModuloDML::getInstance().insertRow(tableName, std::move(tuple));
                res.exito             = dmlRes.success;
                res.sqlState          = dmlRes.success ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "INSERT";
                res.filasAfectadas    = dmlRes.affectedRows;
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = dmlRes.success ? "INSERT 0 1" : console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- D. UPDATE <tabla> SET col=val WHERE ... ---
            if (upper.startsWith("UPDATE ")) {
                int posSet = upper.indexOf(" SET ");
                if (posSet < 0) {
                    console.raiseError(SqlState::SYNTAX_ERROR, "UPDATE", "", "", "missing SET clause in UPDATE");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "UPDATE";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                String tableName = limpiarComillas(sql.substring(7, posSet));
                int posWhere     = upper.indexOf(" WHERE ", posSet);
                String setPart   = (posWhere > 0) ? sql.substring(posSet + 5, posWhere) : sql.substring(posSet + 5);
                String wherePart = (posWhere > 0) ? sql.substring(posWhere + 7)         : "";

                auto rawAssigns = dividirPorComaRespetandoComillas(setPart);
                std::vector<PgCell> assigns;
                for (const auto& a : rawAssigns) {
                    int eq = a.indexOf('=');
                    if (eq > 0) {
                        assigns.emplace_back(limpiarComillas(a.substring(0, eq)), limpiarComillas(a.substring(eq + 1)));
                    }
                }

                std::vector<PredicateCondition> preds;
                parsearCondicionSimpleWhere(wherePart, preds);

                auto dmlRes = ModuloDML::getInstance().updateRows(tableName, assigns, preds, {}, preds.empty());
                res.exito             = dmlRes.success;
                res.sqlState          = dmlRes.success ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "UPDATE";
                res.filasAfectadas    = dmlRes.affectedRows;
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = dmlRes.success
                                        ? (String("UPDATE ") + String(dmlRes.affectedRows)).c_str()
                                        : console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- E. DELETE FROM <tabla> [WHERE ...] ---
            if (upper.startsWith("DELETE FROM ")) {
                int posWhere     = upper.indexOf(" WHERE ");
                String tableName = limpiarComillas((posWhere > 0) ? sql.substring(12, posWhere) : sql.substring(12));
                String wherePart = (posWhere > 0) ? sql.substring(posWhere + 7) : "";

                std::vector<PredicateCondition> preds;
                parsearCondicionSimpleWhere(wherePart, preds);

                auto dmlRes = ModuloDML::getInstance().deleteRows(tableName, preds, {}, preds.empty());
                res.exito             = dmlRes.success;
                res.sqlState          = dmlRes.success ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "DELETE";
                res.filasAfectadas    = dmlRes.affectedRows;
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = dmlRes.success
                                        ? (String("DELETE ") + String(dmlRes.affectedRows)).c_str()
                                        : console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- F. CREATE TABLE [IF NOT EXISTS] <tabla> (...) ---
            if (upper.startsWith("CREATE TABLE ")) {
                int pOpen  = sql.indexOf('(');
                int pClose = sql.lastIndexOf(')');
                if (pOpen < 0 || pClose <= pOpen) {
                    console.raiseError(SqlState::SYNTAX_ERROR, "CREATE TABLE", "", "", "missing column definitions in CREATE TABLE");
                    res.exito            = false;
                    res.sqlState         = console.last().returnedSqlState;
                    res.comandoEjecutado = "CREATE TABLE";
                    res.salidaPsql       = console.last().toPsqlFormat().c_str();
                    return res;
                }

                String header = sql.substring(13, pOpen);
                header.trim();
                bool ifNotExists = false;
                String headerUp = header;
                headerUp.toUpperCase();
                if (headerUp.startsWith("IF NOT EXISTS ")) {
                    ifNotExists = true;
                    header = header.substring(14);
                    header.trim();
                }

                String tableName = limpiarComillas(header);
                auto builder = ModuloDDL::getInstance().createTable(tableName, DbNotRuntime::Standard::TablePersistence::PERMANENT, ifNotExists);

                auto colDefs = dividirPorComaRespetandoComillas(sql.substring(pOpen + 1, pClose));
                for (const auto& rawDef : colDefs) {
                    String d = rawDef;
                    d.trim();
                    int sp = d.indexOf(' ');
                    if (sp <= 0) continue;

                    String colName = limpiarComillas(d.substring(0, sp));
                    String rest    = d.substring(sp + 1);
                    rest.trim();

                    String restUp = rest;
                    restUp.toUpperCase();

                    int nextSp = rest.indexOf(' ');
                    int parenClose = rest.indexOf(')');
                    if (parenClose > 0 && parenClose > nextSp) {
                        nextSp = rest.indexOf(' ', parenClose);
                    }

                    String typeTok = (nextSp > 0) ? rest.substring(0, nextSp) : rest;
                    uint16_t maxLen = 0;
                    PgDataType dt = resolverTipoSqlDDL(typeTok, maxLen);

                    builder.column(colName, dt, maxLen);
                    if (restUp.indexOf("SERIAL") >= 0 || restUp.indexOf("IDENTITY") >= 0) {
                        builder.generatedAsIdentity(DbNotRuntime::Standard::IdentityGeneration::BY_DEFAULT);
                    }
                    if (restUp.indexOf("PRIMARY KEY") >= 0) {
                        builder.primaryKey();
                    }
                    if (restUp.indexOf("NOT NULL") >= 0) {
                        builder.notNull();
                    }
                    if (restUp.indexOf("UNIQUE") >= 0) {
                        builder.unique();
                    }
                }

                bool ok = builder.execute();
                res.exito             = ok;
                res.sqlState          = ok ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "CREATE TABLE";
                res.filasAfectadas    = 0;
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- G. DROP TABLE [IF EXISTS] <tabla> [CASCADE] ---
            if (upper.startsWith("DROP TABLE ")) {
                String rest = sql.substring(11);
                rest.trim();
                String restUp = rest;
                restUp.toUpperCase();

                bool ifExists = false;
                if (restUp.startsWith("IF EXISTS ")) {
                    ifExists = true;
                    rest = rest.substring(10);
                    rest.trim();
                    restUp = rest;
                    restUp.toUpperCase();
                }

                bool cascade = false;
                if (restUp.endsWith(" CASCADE")) {
                    cascade = true;
                    rest = rest.substring(0, rest.length() - 8);
                    rest.trim();
                } else if (restUp.endsWith(" RESTRICT")) {
                    rest = rest.substring(0, rest.length() - 9);
                    rest.trim();
                }

                bool ok = ModuloDDL::getInstance().dropTable(
                    limpiarComillas(rest),
                    ifExists,
                    cascade ? DbNotRuntime::Standard::ReferentialAction::CASCADE
                            : DbNotRuntime::Standard::ReferentialAction::RESTRICT
                );
                res.exito             = ok;
                res.sqlState          = ok ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "DROP TABLE";
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- H. TRUNCATE [TABLE] <tabla> [CASCADE] ---
            if (upper.startsWith("TRUNCATE ")) {
                String rest = sql.substring(9);
                rest.trim();
                String restUp = rest;
                restUp.toUpperCase();
                if (restUp.startsWith("TABLE ")) {
                    rest = rest.substring(6);
                    rest.trim();
                    restUp = rest;
                    restUp.toUpperCase();
                }
                bool cascade = restUp.endsWith(" CASCADE");
                if (cascade) {
                    rest = rest.substring(0, rest.length() - 8);
                    rest.trim();
                }
                bool ok = ModuloDDL::getInstance().truncateTable(
                    limpiarComillas(rest),
                    cascade ? DbNotRuntime::Standard::ReferentialAction::CASCADE
                            : DbNotRuntime::Standard::ReferentialAction::RESTRICT
                );
                res.exito             = ok;
                res.sqlState          = ok ? "00000" : console.last().returnedSqlState;
                res.comandoEjecutado  = "TRUNCATE TABLE";
                res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                res.salidaPsql        = console.last().toPsqlFormat().c_str();
                return res;
            }

            // --- I. ALTER TABLE <tabla> ... ---
            if (upper.startsWith("ALTER TABLE ")) {
                String rest = sql.substring(12);
                rest.trim();
                int sp = rest.indexOf(' ');
                if (sp > 0) {
                    String tableName = limpiarComillas(rest.substring(0, sp));
                    String opRest    = rest.substring(sp + 1);
                    opRest.trim();
                    String opUp      = opRest;
                    opUp.toUpperCase();

                    bool ok = false;
                    if (opUp.startsWith("DROP COLUMN ")) {
                        String colName = limpiarComillas(opRest.substring(12));
                        ok = ModuloDDL::getInstance().alterTableDropColumn(tableName, colName);
                    } else if (opUp.startsWith("RENAME TO ")) {
                        String newName = limpiarComillas(opRest.substring(10));
                        ok = ModuloDDL::getInstance().alterTableRenameTo(tableName, newName);
                    } else if (opUp.startsWith("ADD COLUMN ")) {
                        String def = opRest.substring(11);
                        def.trim();
                        int spCol = def.indexOf(' ');
                        if (spCol > 0) {
                            String colName = limpiarComillas(def.substring(0, spCol));
                            String typeTok = def.substring(spCol + 1);
                            uint16_t maxLen = 0;
                            DbNotRuntime::Standard::ColumnConstraintDef newCol;
                            newCol.columnName = colName;
                            newCol.dataType   = resolverTipoSqlDDL(typeTok, maxLen);
                            newCol.maxLength  = maxLen;
                            ok = ModuloDDL::getInstance().alterTableAddColumn(tableName, newCol);
                        }
                    } else {
                        res.exito             = false;
                        res.sqlState          = SqlState::FEATURE_NOT_SUPPORTED;
                        res.comandoEjecutado  = "ALTER TABLE";
                        res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                        res.salidaPsql        = "NOTICE:  implementado proximamente [SQLSTATE 0A000]";
                        return res;
                    }

                    res.exito             = ok;
                    res.sqlState          = ok ? "00000" : console.last().returnedSqlState;
                    res.comandoEjecutado  = "ALTER TABLE";
                    res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                    res.salidaPsql        = console.last().toPsqlFormat().c_str();
                    return res;
                }
            }

            // --- J. Cualquier otra cláusula SQL aún no implementada ---
            console.raiseError(
                SqlState::FEATURE_NOT_SUPPORTED,
                "SQL TERMINAL",
                "",
                "",
                "implementado proximamente",
                "Comandos activos: SELECT, INSERT INTO, UPDATE, DELETE FROM, CREATE TABLE, ALTER TABLE, DROP TABLE, TRUNCATE, SHOW TABLES."
            );
            res.exito             = false;
            res.sqlState          = SqlState::FEATURE_NOT_SUPPORTED;
            res.comandoEjecutado  = "NOT_IMPLEMENTED";
            res.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
            res.salidaPsql        = console.last().toPsqlFormat().c_str();
            return res;
        },
        RolUsuario::ADMIN
    );

} // namespace RPSqlTerminal

#endif