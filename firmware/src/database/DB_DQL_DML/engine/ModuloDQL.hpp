#pragma once
#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "../../common/DiagnosticConsole.hpp"
#include "../../common/PhysicalSchemaCatalog.hpp"
#include "../telemetry/RuntimeCostProfiler.hpp"
#include "../standard/PostgresDmlDqlKeywords.hpp"
#include "../storage/RuntimeIndexAccessPath.hpp"
#include "AccessPathPlanner.hpp"

namespace DbRuntime::Engine {

    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::PgCell;
    using DbNotRuntime::Standard::PgDataType;
    using DbNotRuntime::Standard::SqlState;

    // Puntero a función ligero para subconsultas correlacionadas LATERAL sin sobrecargar la pila.
    using LateralGeneratorFn = std::vector<PgTuple> (*)(const PgTuple& leftRow);

    // Representa una unión relacional JOIN (incluyendo NATURAL, CROSS, FULL OUTER y LATERAL).
    struct JoinClauseDef {
        Standard::JoinType joinType = Standard::JoinType::INNER;
        String rightTable = "";
        String leftColumn = "";
        String rightColumn = "";
        std::vector<String> usingColumns;
        LateralGeneratorFn lateralFn = nullptr;
        String lateralAlias = "lateral_sub";
    };

    // Representa una función de agregación con soporte para FILTER (WHERE ...) y WITHIN GROUP (ORDER BY ...).
    struct AggregateSpec {
        Standard::AggregateFunc func = Standard::AggregateFunc::COUNT_ALL;
        String targetColumn = "*";
        String alias = "agg_val";
        bool hasFilter = false;
        PredicateCondition filterCondition;
        float percentileFraction = 0.5f; // Usado por PERCENTILE_CONT(p) WITHIN GROUP (ORDER BY col)
        DbNotRuntime::Standard::SortDirection withinGroupDir = DbNotRuntime::Standard::SortDirection::ASC;
    };

    // Representa una función de ventana analítica OVER (PARTITION BY ... ORDER BY ... ROWS/RANGE BETWEEN ...).
    struct WindowSpec {
        Standard::WindowFunc func = Standard::WindowFunc::ROW_NUMBER;
        String targetColumn = "";
        String alias = "win_val";
        std::vector<String> partitionByColumns;
        String orderByColumn = "";
        DbNotRuntime::Standard::SortDirection orderDir = DbNotRuntime::Standard::SortDirection::ASC;
        Standard::WindowFrameUnit frameUnit = Standard::WindowFrameUnit::ROWS;
        Standard::WindowBoundType startBound = Standard::WindowBoundType::UNBOUNDED_PRECEDING;
        Standard::WindowBoundType endBound = Standard::WindowBoundType::CURRENT_ROW;
        int16_t startOffset = 0;
        int16_t endOffset = 0;
        String defaultLagLeadValue = "NULL";
    };

    // Representa un criterio de ordenamiento con control explícito de NULLS FIRST / NULLS LAST.
    struct OrderBySpec {
        String column = "";
        DbNotRuntime::Standard::SortDirection direction = DbNotRuntime::Standard::SortDirection::ASC;
        DbNotRuntime::Standard::NullsOrder nullsOrder = DbNotRuntime::Standard::NullsOrder::LAST;
    };

    // Representa una expresión de tabla común (CTE: WITH [RECURSIVE] name AS (...)).
    struct CteDefinition {
        String cteName = "";
        bool isRecursive = false;
        std::vector<PgTuple> baseRows;
        LateralGeneratorFn recursiveStepFn = nullptr;
        uint8_t maxRecursionDepth = 16;
    };

    // Representa una operación de conjunto (UNION, INTERSECT, EXCEPT con DISTINCT o ALL).
    struct SetOperationAttachment {
        Standard::SetOpType opType = Standard::SetOpType::UNION_DISTINCT;
        std::vector<PgTuple> rightRows;
    };

    // Especificación completa y determinista de un plan lógico SELECT en PostgreSQL.
    struct SelectQueryPlan {
        String fromTable = "";
        std::vector<CteDefinition> ctes;
        std::vector<String> selectColumns; // Vacío o {"*"} significa todas las columnas
        bool selectDistinct = false;
        std::vector<String> distinctOnColumns; // DISTINCT ON (col1, col2)
        std::vector<JoinClauseDef> joins;
        std::vector<PredicateCondition> wherePredicates;
        std::vector<String> groupByColumns;
        Standard::GroupingMode groupingMode = Standard::GroupingMode::SIMPLE;
        std::vector<AggregateSpec> aggregates;
        std::vector<PredicateCondition> havingPredicates;
        std::vector<WindowSpec> windows;
        std::vector<SetOperationAttachment> setOperations;
        std::vector<OrderBySpec> orderBy;
        int32_t limitCount = -1;  // Cubre tanto LIMIT n como FETCH FIRST n ROWS ONLY
        int32_t offsetCount = 0;
    };

    class ModuloDQL {
    private:
        Telemetry::OperationCostMetrics lastCost;

        ModuloDQL() = default;

        // Compara una cadena contra un patrón SQL LIKE / ILIKE soportando comodines '%' y '_'.
        static bool matchLikePattern(const char* str, const char* pat, bool caseInsensitive) {
            while (*pat) {
                if (*pat == '%') {
                    while (*pat == '%') pat++;
                    if (!*pat) return true;
                    while (*str) {
                        if (matchLikePattern(str, pat, caseInsensitive)) return true;
                        str++;
                    }
                    return false;
                }
                if (!*str) return false;
                if (*pat != '_') {
                    char a = caseInsensitive ? tolower(*str) : *str;
                    char b = caseInsensitive ? tolower(*pat) : *pat;
                    if (a != b) return false;
                }
                str++;
                pat++;
            }
            return *str == '\0';
        }

        // Compara dos celdas según su tipo nativo en hardware (entero 64b, FPU float o lexicográfico) devolviendo <0, 0, >0.
        static int compareTypedCellWithLiteral(const PgCell& cell, const String& literal) {
            return DbCommon::Standard::DataTypeUtils::compareUnionWithLiteral(cell.type, cell.num, cell.rawText, literal);
        }

    public:
        // Obtiene la instancia única del motor de consultas DQL.
        static ModuloDQL& getInstance() {
            static ModuloDQL instance;
            return instance;
        }

        // Devuelve el reporte de costo físico (Stack, CPU, Tiempo y camino de acceso) de la última consulta.
        const Telemetry::OperationCostMetrics& getLastCost() const { return lastCost; }

        // Evalúa una condición atómica sobre una tupla tipada respetando la semántica de NULL en PostgreSQL.
        static bool evaluateCondition(const PgTuple& tuple, const PredicateCondition& cond) {
            bool rawResult = false;

            if (cond.op == Standard::FilterOp::EXISTS_SUBQUERY ||
                cond.op == Standard::FilterOp::ANY_SUBQUERY ||
                cond.op == Standard::FilterOp::ALL_SUBQUERY) {
                rawResult = cond.subqueryFn ? cond.subqueryFn(tuple) : false;
                return cond.negated ? !rawResult : rawResult;
            }

            const PgCell* cell = tuple.findCell(cond.columnOrExpr);
            bool isCellNull = (!cell || cell->isNull);

            if (cond.op == Standard::FilterOp::IS_NULL) {
                rawResult = isCellNull;
                return cond.negated ? !rawResult : rawResult;
            }
            if (cond.op == Standard::FilterOp::IS_NOT_NULL) {
                rawResult = !isCellNull;
                return cond.negated ? !rawResult : rawResult;
            }

            // En SQL estándar cualquier comparación ordinaria contra NULL evalúa a UNKNOWN (falso en WHERE)
            if (isCellNull) return false;


            // Comparación columna-columna: el lado derecho se resuelve desde la misma tupla
            String rhs = cond.valueA;
            if (cond.valueIsColumn) {
                const PgCell* rhsCell = tuple.findCell(cond.valueA);
                if (!rhsCell || rhsCell->isNull) return false;
                rhs = rhsCell->rawText;
            }

            switch (cond.op) {
                case Standard::FilterOp::EQ:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) == 0);
                    break;
                case Standard::FilterOp::NEQ:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) != 0);
                    break;
                case Standard::FilterOp::GT:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) > 0);
                    break;
                case Standard::FilterOp::GTE:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) >= 0);
                    break;
                case Standard::FilterOp::LT:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) < 0);
                    break;
                case Standard::FilterOp::LTE:
                    rawResult = (compareTypedCellWithLiteral(*cell, rhs) <= 0);
                    break;
                case Standard::FilterOp::BETWEEN:
                    rawResult = (compareTypedCellWithLiteral(*cell, cond.valueA) >= 0 &&
                                 compareTypedCellWithLiteral(*cell, cond.valueB) <= 0);
                    break;
                case Standard::FilterOp::LIKE:
                case Standard::FilterOp::SIMILAR_TO:
                    rawResult = matchLikePattern(cell->rawText.c_str(), cond.valueA.c_str(), false);
                    break;
                case Standard::FilterOp::ILIKE:
                    rawResult = matchLikePattern(cell->rawText.c_str(), cond.valueA.c_str(), true);
                    break;
                case Standard::FilterOp::IN_LIST:
                    for (const auto& item : cond.valueList) {
                        if (compareTypedCellWithLiteral(*cell, item) == 0) { rawResult = true; break; }
                    }
                    break;
                case Standard::FilterOp::NOT_IN_LIST:
                    rawResult = true;
                    for (const auto& item : cond.valueList) {
                        if (compareTypedCellWithLiteral(*cell, item) == 0) { rawResult = false; break; }
                    }
                    break;
                default:
                    rawResult = false;
                    break;
            }
            return cond.negated ? !rawResult : rawResult;
        }

        // Evalúa una cadena de predicados unidos por conectores lógicos AND / OR.
        static bool evaluatePredicateChain(const PgTuple& tuple, const std::vector<PredicateCondition>& preds) {
            if (preds.empty()) return true;
            bool acc = true;
            for (size_t i = 0; i < preds.size(); ++i) {
                bool cur = evaluateCondition(tuple, preds[i]);
                if (i == 0) {
                    acc = cur;
                } else if (preds[i].connector == Standard::LogicConnector::OR_OP) {
                    acc = acc || cur;
                } else {
                    acc = acc && cur;
                }
            }
            return acc;
        }

    private:
        // Valida que las columnas usadas en SELECT y WHERE existan en el catálogo o CTEs (SQLSTATE 42703 / 42702).
        bool validateQueryColumnsAgainstCatalog(const SelectQueryPlan& plan) {
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();

            bool isCte = false;
            for (const auto& cte : plan.ctes) {
                if (cte.cteName.equalsIgnoreCase(plan.fromTable)) { isCte = true; break; }
            }

            const auto* mainSchema = cat.getTable(plan.fromTable);
            if (!mainSchema && !isCte) {
                return console.raiseError(
                    SqlState::UNDEFINED_TABLE, "SELECT", plan.fromTable, "",
                    "relation \"" + plan.fromTable + "\" does not exist"
                );
            }

            // Verifica tablas unidas por JOIN
            std::vector<const DbNotRuntime::Engine::TablePhysicalSchema*> activeSchemas;
            if (mainSchema) activeSchemas.push_back(mainSchema);

            for (const auto& j : plan.joins) {
                if (j.joinType == Standard::JoinType::LATERAL_JOIN) continue;
                const auto* jSchema = cat.getTable(j.rightTable);
                if (!jSchema) {
                    return console.raiseError(
                        SqlState::UNDEFINED_TABLE, "SELECT JOIN", j.rightTable, "",
                        "relation \"" + j.rightTable + "\" does not exist"
                    );
                }
                activeSchemas.push_back(jSchema);
            }

            // Valida existencia y no-ambigüedad de columnas en WHERE si el esquema proviene del catálogo
            if (!activeSchemas.empty()) {
                for (const auto& cond : plan.wherePredicates) {
                    if (!cond.keyColumnCandidate()) continue;
                    int dot = cond.columnOrExpr.indexOf('.');
                    if (dot != -1) {
                        String tPrefix = cond.columnOrExpr.substring(0, dot);
                        String cName   = cond.columnOrExpr.substring(dot + 1);
                        const auto* targetSchema = cat.getTable(tPrefix);
                        if (!targetSchema || !targetSchema->findColumn(cName)) {
                            return console.raiseError(
                                SqlState::UNDEFINED_COLUMN, "SELECT WHERE", tPrefix, cName,
                                "column \"" + cond.columnOrExpr + "\" does not exist"
                            );
                        }
                    } else {
                        uint8_t matches = 0;
                        for (const auto* sc : activeSchemas) {
                            if (sc->findColumn(cond.columnOrExpr)) matches++;
                        }
                        if (matches == 0 && !isCte) {
                            return console.raiseError(
                                SqlState::UNDEFINED_COLUMN, "SELECT WHERE", plan.fromTable, cond.columnOrExpr,
                                "column \"" + cond.columnOrExpr + "\" does not exist"
                            );
                        }
                        if (matches > 1) {
                            return console.raiseError(
                                SqlState::AMBIGUOUS_COLUMN, "SELECT WHERE", plan.fromTable, cond.columnOrExpr,
                                "column reference \"" + cond.columnOrExpr + "\" is ambiguous",
                                "Qualify the column with the table name: \"Table." + cond.columnOrExpr + "\"."
                            );
                        }
                    }
                }
            }
            return true;
        }

        // Materializa una CTE (incluyendo WITH RECURSIVE con punto fijo acotado) devolviendo sus tuplas.
        static std::vector<PgTuple> materializeCte(const CteDefinition& cte) {
            std::vector<PgTuple> result = cte.baseRows;
            if (!cte.isRecursive || !cte.recursiveStepFn) return result;

            std::vector<PgTuple> workingTable = cte.baseRows;
            uint8_t depth = 0;
            while (!workingTable.empty() && depth < cte.maxRecursionDepth) {
                std::vector<PgTuple> nextWave;
                for (const auto& row : workingTable) {
                    std::vector<PgTuple> generated = cte.recursiveStepFn(row);
                    for (auto& g : generated) {
                        result.push_back(g);
                        nextWave.push_back(std::move(g));
                    }
                }
                workingTable = std::move(nextWave);
                depth++;
            }
            return result;
        }

        // Prefija las celdas de una tupla con "Tabla.columna" además del nombre corto para resolver Joins sin colisión.
        static PgTuple enrichTupleWithPrefix(const PgTuple& src, const String& tableName) {
            PgTuple out;
            out.reserve(src.size() * 2);
            for (const auto& c : src.allCells()) {
                out.set(c.columnName, c.rawText, c.type);
                out.set(tableName + "." + String(c.columnName), c.rawText, c.type);
            }
            return out;
        }

        // Ejecuta la fase de agregación sobre un conjunto de filas agrupadas.
        static void computeAggregateOnGroup(const std::vector<PgTuple>& groupRows,
                                            const AggregateSpec& ag,
                                            PgTuple& outRow) {
            std::vector<const PgCell*> validCells;
            size_t countRows = 0;

            for (const auto& r : groupRows) {
                if (ag.hasFilter && !evaluateCondition(r, ag.filterCondition)) continue;
                if (ag.func == Standard::AggregateFunc::COUNT_ALL) {
                    countRows++;
                } else {
                    const PgCell* c = r.findCell(ag.targetColumn);
                    if (c && !c->isNull) {
                        validCells.push_back(c);
                        countRows++;
                    }
                }
            }

            switch (ag.func) {
                case Standard::AggregateFunc::COUNT_ALL:
                case Standard::AggregateFunc::COUNT_COL:
                    outRow.set(ag.alias, String(static_cast<long>(countRows)), PgDataType::BIGINT);
                    break;
                case Standard::AggregateFunc::COUNT_DISTINCT: {
                    std::vector<String> seen;
                    for (const auto* c : validCells) {
                        bool exists = false;
                        for (const auto& s : seen) if (s == c->rawText) { exists = true; break; }
                        if (!exists) seen.push_back(c->rawText);
                    }
                    outRow.set(ag.alias, String(static_cast<long>(seen.size())), PgDataType::BIGINT);
                    break;
                }
                case Standard::AggregateFunc::SUM:
                case Standard::AggregateFunc::AVG: {
                    if (validCells.empty()) {
                        outRow.set(ag.alias, "0.00", PgDataType::REAL);
                        break;
                    }
                    float sum = 0.0f;
                    for (const auto* c : validCells) sum += c->asFloat();
                    float val = (ag.func == Standard::AggregateFunc::AVG) ? (sum / validCells.size()) : sum;
                    outRow.set(ag.alias, String(val, 2), PgDataType::REAL);
                    break;
                }
                case Standard::AggregateFunc::MIN:
                case Standard::AggregateFunc::MAX: {
                    if (validCells.empty()) {
                        outRow.set(ag.alias, "NULL", PgDataType::TEXT);
                        break;
                    }
                    const PgCell* best = validCells[0];
                    for (size_t i = 1; i < validCells.size(); ++i) {
                        int cmp = compareTypedCellWithLiteral(*validCells[i], best->rawText);
                        if ((ag.func == Standard::AggregateFunc::MIN && cmp < 0) ||
                            (ag.func == Standard::AggregateFunc::MAX && cmp > 0)) {
                            best = validCells[i];
                        }
                    }
                    outRow.set(ag.alias, best->rawText, best->type);
                    break;
                }
                case Standard::AggregateFunc::PERCENTILE_CONT: {
                    if (validCells.empty()) {
                        outRow.set(ag.alias, "NULL", PgDataType::REAL);
                        break;
                    }
                    std::vector<float> vals;
                    vals.reserve(validCells.size());
                    for (const auto* c : validCells) vals.push_back(c->asFloat());
                    std::sort(vals.begin(), vals.end());
                    if (ag.withinGroupDir == DbNotRuntime::Standard::SortDirection::DESC) {
                        std::reverse(vals.begin(), vals.end());
                    }
                    float pos = ag.percentileFraction * (vals.size() - 1);
                    size_t lo = static_cast<size_t>(floorf(pos));
                    size_t hi = static_cast<size_t>(ceilf(pos));
                    float interp = vals[lo] + (pos - lo) * (vals[hi] - vals[lo]);
                    outRow.set(ag.alias, String(interp, 2), PgDataType::REAL);
                    break;
                }
                default:
                    break;
            }
        }

        // Ejecuta todas las funciones de ventana analítica OVER (PARTITION BY ... ORDER BY ... ROWS BETWEEN ...).
        static void evaluateWindowFunctions(std::vector<PgTuple>& rows, const std::vector<WindowSpec>& windows) {
            if (rows.empty() || windows.empty()) return;

            for (const auto& win : windows) {
                // Agrupa los índices de fila por clave de partición PARTITION BY
                std::map<String, std::vector<size_t>> partitions;
                for (size_t i = 0; i < rows.size(); ++i) {
                    String partKey = "";
                    for (const auto& pCol : win.partitionByColumns) {
                        partKey += rows[i].getText(pCol.c_str(), "NULL") + "|";
                    }
                    partitions[partKey].push_back(i);
                }

                for (auto& partPair : partitions) {
                    auto& idxList = partPair.second;
                    // Ordena internamente la partición si se especificó ORDER BY en la ventana
                    if (!win.orderByColumn.isEmpty()) {
                        std::sort(idxList.begin(), idxList.end(), [&](size_t a, size_t b) {
                            const PgCell* cA = rows[a].findCell(win.orderByColumn);
                            const PgCell* cB = rows[b].findCell(win.orderByColumn);
                            if (!cA || cA->isNull) return false;
                            if (!cB || cB->isNull) return true;
                            int cmp = compareTypedCellWithLiteral(*cA, cB->rawText);
                            return (win.orderDir == DbNotRuntime::Standard::SortDirection::ASC) ? (cmp < 0) : (cmp > 0);
                        });
                    }

                    int32_t currentRank = 1;
                    int32_t currentDenseRank = 1;

                    for (size_t pos = 0; pos < idxList.size(); ++pos) {
                        size_t rowIdx = idxList[pos];
                        if (pos > 0 && !win.orderByColumn.isEmpty()) {
                            const PgCell* prevC = rows[idxList[pos - 1]].findCell(win.orderByColumn);
                            const PgCell* currC = rows[rowIdx].findCell(win.orderByColumn);
                            bool tied = (prevC && currC && compareTypedCellWithLiteral(*currC, prevC->rawText) == 0);
                            if (!tied) {
                                currentRank = static_cast<int32_t>(pos + 1);
                                currentDenseRank++;
                            }
                        }

                        // Determina los límites físicos del frame ROWS BETWEEN startBound AND endBound
                        int startIdx = 0;
                        int endIdx   = static_cast<int>(idxList.size()) - 1;
                        if (win.startBound == Standard::WindowBoundType::CURRENT_ROW) startIdx = static_cast<int>(pos);
                        else if (win.startBound == Standard::WindowBoundType::N_PRECEDING) startIdx = std::max(0, static_cast<int>(pos) - win.startOffset);
                        else if (win.startBound == Standard::WindowBoundType::N_FOLLOWING) startIdx = std::min(endIdx, static_cast<int>(pos) + win.startOffset);

                        if (win.endBound == Standard::WindowBoundType::CURRENT_ROW) endIdx = static_cast<int>(pos);
                        else if (win.endBound == Standard::WindowBoundType::N_PRECEDING) endIdx = std::max(0, static_cast<int>(pos) - win.endOffset);
                        else if (win.endBound == Standard::WindowBoundType::N_FOLLOWING) endIdx = std::min(static_cast<int>(idxList.size()) - 1, static_cast<int>(pos) + win.endOffset);

                        switch (win.func) {
                            case Standard::WindowFunc::ROW_NUMBER:
                                rows[rowIdx].set(win.alias, String(static_cast<long>(pos + 1)), PgDataType::BIGINT);
                                break;
                            case Standard::WindowFunc::RANK:
                                rows[rowIdx].set(win.alias, String(currentRank), PgDataType::BIGINT);
                                break;
                            case Standard::WindowFunc::DENSE_RANK:
                                rows[rowIdx].set(win.alias, String(currentDenseRank), PgDataType::BIGINT);
                                break;
                            case Standard::WindowFunc::LAG: {
                                int offset = (win.startOffset > 0) ? win.startOffset : 1;
                                int targetPos = static_cast<int>(pos) - offset;
                                String val = (targetPos >= 0)
                                    ? rows[idxList[targetPos]].getText(win.targetColumn.c_str(), win.defaultLagLeadValue.c_str())
                                    : win.defaultLagLeadValue;
                                rows[rowIdx].set(win.alias, val, PgDataType::TEXT);
                                break;
                            }
                            case Standard::WindowFunc::LEAD: {
                                int offset = (win.endOffset > 0) ? win.endOffset : 1;
                                int targetPos = static_cast<int>(pos) + offset;
                                String val = (targetPos < static_cast<int>(idxList.size()))
                                    ? rows[idxList[targetPos]].getText(win.targetColumn.c_str(), win.defaultLagLeadValue.c_str())
                                    : win.defaultLagLeadValue;
                                rows[rowIdx].set(win.alias, val, PgDataType::TEXT);
                                break;
                            }
                            case Standard::WindowFunc::FIRST_VALUE:
                                if (startIdx <= endIdx) {
                                    rows[rowIdx].set(win.alias, rows[idxList[startIdx]].getText(win.targetColumn.c_str(), "NULL"), PgDataType::TEXT);
                                }
                                break;
                            case Standard::WindowFunc::LAST_VALUE:
                                if (startIdx <= endIdx) {
                                    rows[rowIdx].set(win.alias, rows[idxList[endIdx]].getText(win.targetColumn.c_str(), "NULL"), PgDataType::TEXT);
                                }
                                break;
                            case Standard::WindowFunc::SUM_OVER:
                            case Standard::WindowFunc::AVG_OVER: {
                                float sum = 0.0f;
                                int count = 0;
                                for (int k = startIdx; k <= endIdx; ++k) {
                                    sum += rows[idxList[k]].getFloat(win.targetColumn.c_str(), 0.0f);
                                    count++;
                                }
                                float res = (win.func == Standard::WindowFunc::AVG_OVER && count > 0) ? (sum / count) : sum;
                                rows[rowIdx].set(win.alias, String(res, 2), PgDataType::REAL);
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Compara si dos tuplas son idénticas en todos sus valores proyectados para operaciones de conjunto.
        static bool areTuplesEqual(const PgTuple& a, const PgTuple& b) {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i) {
                if (a.allCells()[i].rawText != b.allCells()[i].rawText) return false;
            }
            return true;
        }

    public:
        // Ejecuta un plan SELECT completo usando los índices del DDL cuando están disponibles y midiendo el costo en hardware.
        std::vector<PgTuple> executeSelect(const SelectQueryPlan& plan) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            Telemetry::ScopedCostProbe probe(lastCost, "SELECT", plan.fromTable);
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();

            if (!validateQueryColumnsAgainstCatalog(plan)) {
                throw std::runtime_error(console.last().toPsqlFormat().c_str());
            }

            // El SGBD impone su propio límite físico de 32 registros por consulta
            if (plan.limitCount > static_cast<int32_t>(Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM)) {
                console.raiseError(
                    SqlState::INVALID_PARAMETER_VALUE, "SELECT LIMIT", plan.fromTable, "",
                    "limit (" + String(plan.limitCount) + ") exceeds SGBD maximum batch size of " +
                    String(Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM) + " rows",
                    "Divide the query into batches of at most 32 rows using LIMIT and OFFSET."
                );
                throw std::runtime_error(console.last().toPsqlFormat().c_str());
            }

            // 1. Resolución de fuente de datos (CTE o Tabla Física con Planificador de Índices)
            std::vector<PgTuple> workingRows;
            bool loadedFromCte = false;
            for (const auto& cte : plan.ctes) {
                if (cte.cteName.equalsIgnoreCase(plan.fromTable)) {
                    workingRows = materializeCte(cte);
                    lastCost.scanType = Telemetry::AccessScanType::SEQ_SCAN;
                    lastCost.rowsExamined = static_cast<uint16_t>(workingRows.size());
                    loadedFromCte = true;
                    break;
                }
            }

            const bool hasJoins = !plan.joins.empty();

            if (!loadedFromCte) {
                auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
                ChosenAccessPlan accessPlan = AccessPathPlanner::chooseBestAccessPath(plan.fromTable, plan.wherePredicates, lastCost);

                const size_t maxFilasSeguras = (plan.limitCount > 0 && plan.orderBy.empty() && plan.aggregates.empty())
                    ? static_cast<size_t>(plan.limitCount + plan.offsetCount)
                    : Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM;

                if (accessPlan.indexPtr != nullptr) {
                    // INDEX SCAN / PK LOOKUP / UNIQUE LOOKUP en O(log N) + seek(byteOffset) directo en MicroSD
                    std::vector<uint32_t> offsets = accessPlan.indexPtr->lookupFirstColumnPrefix(accessPlan.lookupValue);
                    lastCost.rowsExamined = static_cast<uint16_t>(offsets.size());
                    for (uint32_t byteOff : offsets) {
                        PgTuple rawRow;
                        if (!storage.readRowAtOffset(plan.fromTable, byteOff, rawRow)) continue;
                        PgTuple candidate = hasJoins ? enrichTupleWithPrefix(rawRow, plan.fromTable) : std::move(rawRow);
                        if (hasJoins || evaluatePredicateChain(candidate, plan.wherePredicates)) {
                            if (workingRows.size() >= Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM) {
                                workingRows.erase(workingRows.begin());
                            }
                            workingRows.push_back(std::move(candidate));
                            if (workingRows.size() >= maxFilasSeguras && plan.orderBy.empty()) break;
                        }
                    }
                } else {
                    // SEQ SCAN en streaming desde la MicroSD (1 sola PgTuple en RAM a la vez)
                    uint16_t examinadas = 0;
                    storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t) -> bool {
                        examinadas++;
                        PgTuple candidate = hasJoins ? enrichTupleWithPrefix(r, plan.fromTable) : r;
                        if (hasJoins || evaluatePredicateChain(candidate, plan.wherePredicates)) {
                            if (workingRows.size() >= Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM) {
                                workingRows.erase(workingRows.begin());
                            }
                            workingRows.push_back(std::move(candidate));
                            if (workingRows.size() >= maxFilasSeguras && plan.orderBy.empty()) return false;
                        }
                        return true;
                    });
                    lastCost.rowsExamined = examinadas;
                }
            } else if (!hasJoins) {
                std::vector<PgTuple> filtered;
                for (auto& r : workingRows) {
                    if (evaluatePredicateChain(r, plan.wherePredicates)) filtered.push_back(std::move(r));
                }
                workingRows = std::move(filtered);
            }

            // 2. Ejecución de JOINS (INNER, LEFT, RIGHT, FULL OUTER, CROSS, NATURAL y LATERAL)
            if (hasJoins) {
                auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
                for (const auto& j : plan.joins) {
                    std::vector<PgTuple> joinedResult;

                    if (j.joinType == Standard::JoinType::LATERAL_JOIN && j.lateralFn) {
                        for (const auto& leftRow : workingRows) {
                            std::vector<PgTuple> latRows = j.lateralFn(leftRow);
                            for (const auto& lr : latRows) {
                                PgTuple combined = leftRow;
                                for (const auto& c : lr.allCells()) {
                                    combined.set(c.columnName, c.rawText, c.type);
                                    combined.set(j.lateralAlias + "." + String(c.columnName), c.rawText, c.type);
                                }
                                joinedResult.push_back(std::move(combined));
                            }
                        }
                        workingRows = std::move(joinedResult);
                        continue;
                    }

                    const auto& rightRaw = storage.getTableRows(j.rightTable);
                    lastCost.rowsExamined += static_cast<uint16_t>(rightRaw.size());
                    std::vector<bool> rightMatched(rightRaw.size(), false);

                    String leftColKey = j.leftColumn;
                    String rightColKey = j.rightColumn;

                    // Autodetección para NATURAL JOIN o USING (...)
                    if (j.joinType == Standard::JoinType::NATURAL_INNER && !workingRows.empty() && !rightRaw.empty()) {
                        for (const auto& lc : workingRows[0].allCells()) {
                            if (strchr(lc.columnName, '.') == nullptr && rightRaw[0].findCell(lc.columnName)) {
                                leftColKey  = lc.columnName;
                                rightColKey = lc.columnName;
                                break;
                            }
                        }
                    } else if (!j.usingColumns.empty()) {
                        leftColKey  = j.usingColumns[0];
                        rightColKey = j.usingColumns[0];
                    }

                    for (const auto& lRow : workingRows) {
                        bool matchedLeft = false;
                        const PgCell* lCell = lRow.findCell(leftColKey);

                        for (size_t rIdx = 0; rIdx < rightRaw.size(); ++rIdx) {
                            const auto& rRow = rightRaw[rIdx];
                            bool isMatch = false;

                            if (j.joinType == Standard::JoinType::CROSS) {
                                isMatch = true;
                            } else {
                                const PgCell* rCell = rRow.findCell(rightColKey);
                                if (lCell && rCell && !lCell->isNull && !rCell->isNull) {
                                    isMatch = (compareTypedCellWithLiteral(*lCell, rCell->rawText) == 0);
                                }
                            }

                            if (isMatch) {
                                matchedLeft = true;
                                rightMatched[rIdx] = true;
                                PgTuple combined = lRow;
                                for (const auto& rc : rRow.allCells()) {
                                    if (!combined.findCell(rc.columnName)) {
                                        combined.set(rc.columnName, rc.rawText, rc.type);
                                    }
                                    combined.set(j.rightTable + "." + String(rc.columnName), rc.rawText, rc.type);
                                }
                                joinedResult.push_back(std::move(combined));
                            }
                        }

                        if (!matchedLeft && (j.joinType == Standard::JoinType::LEFT_OUTER || j.joinType == Standard::JoinType::FULL_OUTER)) {
                            joinedResult.push_back(lRow);
                        }
                    }

                    if (j.joinType == Standard::JoinType::RIGHT_OUTER || j.joinType == Standard::JoinType::FULL_OUTER) {
                        for (size_t rIdx = 0; rIdx < rightRaw.size(); ++rIdx) {
                            if (!rightMatched[rIdx]) {
                                joinedResult.push_back(enrichTupleWithPrefix(rightRaw[rIdx], j.rightTable));
                            }
                        }
                    }
                    workingRows = std::move(joinedResult);
                }

                // Aplica predicados WHERE post-join
                std::vector<PgTuple> postWhere;
                for (auto& r : workingRows) {
                    if (evaluatePredicateChain(r, plan.wherePredicates)) postWhere.push_back(std::move(r));
                }
                workingRows = std::move(postWhere);
            }

            // 3. Agrupación GROUP BY (SIMPLE, ROLLUP, CUBE) + Agregaciones (FILTER, WITHIN GROUP) + HAVING
            if (!plan.groupByColumns.empty() || !plan.aggregates.empty()) {
                                std::vector<PgTuple> aggregatedRows;
                const size_t nCols = plan.groupByColumns.size();

                if (nCols == 0) {
                    // Agregación global sin GROUP BY
                    PgTuple outAll;
                    for (const auto& ag : plan.aggregates) computeAggregateOnGroup(workingRows, ag, outAll);
                    if (evaluatePredicateChain(outAll, plan.havingPredicates)) {
                        aggregatedRows.push_back(std::move(outAll));
                    }
                } else {
                    // Conjuntos de agrupación: bit i = columna i agrupada (1) o NULL (0). Máx. 16 columnas.
                    std::vector<uint32_t> groupingSets;
                    const uint32_t fullMask = (1u << nCols) - 1u;
                    if (plan.groupingMode == Standard::GroupingMode::ROLLUP) {
                        for (int k = static_cast<int>(nCols); k >= 0; --k) groupingSets.push_back((1u << k) - 1u);
                    } else if (plan.groupingMode == Standard::GroupingMode::CUBE) {
                        for (int32_t m = static_cast<int32_t>(fullMask); m >= 0; --m) groupingSets.push_back(static_cast<uint32_t>(m));
                    } else {
                        groupingSets.push_back(fullMask);
                    }

                    for (uint32_t mask : groupingSets) {
                        std::map<String, std::vector<PgTuple>> groups;
                        for (const auto& r : workingRows) {
                            String gKey = "";
                            for (size_t i = 0; i < nCols; ++i) {
                                if (mask & (1u << i)) gKey += r.getText(plan.groupByColumns[i].c_str(), "NULL") + "\x1F";
                            }
                            groups[gKey].push_back(r);
                        }
                        for (const auto& gPair : groups) {
                            const auto& gList = gPair.second;
                            PgTuple outG;
                            for (size_t i = 0; i < nCols; ++i) {
                                const String& gCol = plan.groupByColumns[i];
                                if (mask & (1u << i)) {
                                    const PgCell* c = gList[0].findCell(gCol);
                                    if (c) outG.set(gCol, c->rawText, c->type);
                                } else {
                                    outG.set(gCol, "NULL", PgDataType::TEXT);
                                }
                            }
                            for (const auto& ag : plan.aggregates) computeAggregateOnGroup(gList, ag, outG);
                            if (evaluatePredicateChain(outG, plan.havingPredicates)) {
                                aggregatedRows.push_back(std::move(outG));
                            }
                        }
                    }
                }
                workingRows = std::move(aggregatedRows);
            }

            // 4. Evaluación de funciones de ventana analítica OVER (...)
            if (!plan.windows.empty()) {
                evaluateWindowFunctions(workingRows, plan.windows);
            }

            // 5. Proyección de columnas solicitadas en SELECT
            if (!plan.selectColumns.empty() && plan.selectColumns[0] != "*") {
                std::vector<PgTuple> projected;
                projected.reserve(workingRows.size());
                for (const auto& r : workingRows) {
                    PgTuple p;
                    p.reserve(plan.selectColumns.size() + plan.aggregates.size() + plan.windows.size());
                    for (const auto& col : plan.selectColumns) {
                        const PgCell* c = r.findCell(col);
                        int dot = col.indexOf('.');
                        String outName = (dot != -1) ? col.substring(dot + 1) : col;
                        if (c) p.set(outName, c->rawText, c->type);
                        else   p.set(outName, "NULL", PgDataType::TEXT);
                    }
                    for (const auto& ag : plan.aggregates) {
                        const PgCell* c = r.findCell(ag.alias);
                        if (c && !p.findCell(ag.alias)) p.set(ag.alias, c->rawText, c->type);
                    }
                    for (const auto& w : plan.windows) {
                        const PgCell* c = r.findCell(w.alias);
                        if (c && !p.findCell(w.alias)) p.set(w.alias, c->rawText, c->type);
                    }
                    projected.push_back(std::move(p));
                }
                workingRows = std::move(projected);
            } else if (hasJoins) {
                // Limpia las claves duplicadas con prefijo "Tabla." cuando se selecciona "*"
                for (auto& r : workingRows) {
                    auto& cells = r.allCellsMutable();
                    for (auto it = cells.begin(); it != cells.end(); ) {
                        if (strchr(it->columnName, '.') != nullptr) it = cells.erase(it);
                        else ++it;
                    }
                }
            }

            // 6. Operaciones de conjuntos (UNION, INTERSECT, EXCEPT con DISTINCT / ALL)
            for (const auto& setOp : plan.setOperations) {
                std::vector<PgTuple> nextSet;
                switch (setOp.opType) {
                    case Standard::SetOpType::UNION_ALL:
                        nextSet = std::move(workingRows);
                        for (const auto& rr : setOp.rightRows) nextSet.push_back(rr);
                        break;
                    case Standard::SetOpType::UNION_DISTINCT:
                        for (const auto& lr : workingRows) {
                            bool dup = false;
                            for (const auto& ex : nextSet) if (areTuplesEqual(lr, ex)) { dup = true; break; }
                            if (!dup) nextSet.push_back(lr);
                        }
                        for (const auto& rr : setOp.rightRows) {
                            bool dup = false;
                            for (const auto& ex : nextSet) if (areTuplesEqual(rr, ex)) { dup = true; break; }
                            if (!dup) nextSet.push_back(rr);
                        }
                        break;
                    case Standard::SetOpType::INTERSECT_DISTINCT:
                    case Standard::SetOpType::INTERSECT_ALL:
                        for (const auto& lr : workingRows) {
                            bool inRight = false;
                            for (const auto& rr : setOp.rightRows) if (areTuplesEqual(lr, rr)) { inRight = true; break; }
                            if (inRight) {
                                if (setOp.opType == Standard::SetOpType::INTERSECT_DISTINCT) {
                                    bool dup = false;
                                    for (const auto& ex : nextSet) if (areTuplesEqual(lr, ex)) { dup = true; break; }
                                    if (!dup) nextSet.push_back(lr);
                                } else {
                                    nextSet.push_back(lr);
                                }
                            }
                        }
                        break;
                    case Standard::SetOpType::EXCEPT_DISTINCT:
                    case Standard::SetOpType::EXCEPT_ALL:
                        for (const auto& lr : workingRows) {
                            bool inRight = false;
                            for (const auto& rr : setOp.rightRows) if (areTuplesEqual(lr, rr)) { inRight = true; break; }
                            if (!inRight) {
                                if (setOp.opType == Standard::SetOpType::EXCEPT_DISTINCT) {
                                    bool dup = false;
                                    for (const auto& ex : nextSet) if (areTuplesEqual(lr, ex)) { dup = true; break; }
                                    if (!dup) nextSet.push_back(lr);
                                } else {
                                    nextSet.push_back(lr);
                                }
                            }
                        }
                        break;
                }
                workingRows = std::move(nextSet);
            }

            // 7. Ordenamiento ORDER BY con soporte explícito para NULLS FIRST y NULLS LAST
            if (!plan.orderBy.empty()) {
                std::sort(workingRows.begin(), workingRows.end(), [&](const PgTuple& a, const PgTuple& b) {
                    for (const auto& ob : plan.orderBy) {
                        const PgCell* cA = a.findCell(ob.column);
                        const PgCell* cB = b.findCell(ob.column);
                        bool nullA = (!cA || cA->isNull);
                        bool nullB = (!cB || cB->isNull);
                        if (nullA && nullB) continue;
                        if (nullA) return (ob.nullsOrder == DbNotRuntime::Standard::NullsOrder::FIRST);
                        if (nullB) return (ob.nullsOrder != DbNotRuntime::Standard::NullsOrder::FIRST);

                        int cmp = compareTypedCellWithLiteral(*cA, cB->rawText);
                        if (cmp != 0) {
                            return (ob.direction == DbNotRuntime::Standard::SortDirection::ASC) ? (cmp < 0) : (cmp > 0);
                        }
                    }
                    return false;
                });
            }

            // 8. Deduplicación DISTINCT y DISTINCT ON (...) de PostgreSQL
            if (plan.selectDistinct) {
                std::vector<PgTuple> dedup;
                for (auto& r : workingRows) {
                    bool seen = false;
                    for (const auto& d : dedup) if (areTuplesEqual(r, d)) { seen = true; break; }
                    if (!seen) dedup.push_back(std::move(r));
                }
                workingRows = std::move(dedup);
            } else if (!plan.distinctOnColumns.empty()) {
                std::vector<PgTuple> dedupOn;
                std::vector<String> seenKeys;
                for (auto& r : workingRows) {
                    String k = "";
                    for (const auto& col : plan.distinctOnColumns) k += r.getText(col.c_str(), "NULL") + "\x1F";
                    bool exists = false;
                    for (const auto& sk : seenKeys) if (sk == k) { exists = true; break; }
                    if (!exists) {
                        seenKeys.push_back(k);
                        dedupOn.push_back(std::move(r));
                    }
                }
                workingRows = std::move(dedupOn);
            }

            // 9. Paginación OFFSET y LIMIT / FETCH FIRST n ROWS ONLY
            if (plan.offsetCount > 0) {
                if (static_cast<size_t>(plan.offsetCount) >= workingRows.size()) {
                    workingRows.clear();
                } else {
                    workingRows.erase(workingRows.begin(), workingRows.begin() + plan.offsetCount);
                }
            }
            if (plan.limitCount >= 0 && workingRows.size() > static_cast<size_t>(plan.limitCount)) {
                workingRows.resize(static_cast<size_t>(plan.limitCount));
            }

            lastCost.rowsProduced = static_cast<uint16_t>(workingRows.size());
            console.reportSuccess("SELECT", plan.fromTable, String(workingRows.size()) + " rows");
            if (workingRows.size() > Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM) {
                console.raiseError(
                    SqlState::DATA_EXCEPTION, "SELECT", plan.fromTable, "",
                    "query returned more than " + String(Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM) +
                    " rows without pagination",
                    "Use .limit(32) and .offset(n) to fetch records in safe batches."
                );
                throw std::runtime_error(console.last().toPsqlFormat().c_str());
            }
            return workingRows;
        }
    };

} // namespace DbRuntime::Engine