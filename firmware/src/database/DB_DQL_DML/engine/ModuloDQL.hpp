#pragma once
#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
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

    using LateralGeneratorFn = std::vector<PgTuple> (*)(const PgTuple& leftRow);

    struct JoinClauseDef {
        Standard::JoinType joinType = Standard::JoinType::INNER;
        String rightTable = "";
        String leftColumn = "";
        String rightColumn = "";
        std::vector<String> usingColumns;
        LateralGeneratorFn lateralFn = nullptr;
        String lateralAlias = "lateral_sub";
    };

    struct AggregateSpec {
        Standard::AggregateFunc func = Standard::AggregateFunc::COUNT_ALL;
        String targetColumn = "*";
        String alias = "agg_val";
        bool hasFilter = false;
        PredicateCondition filterCondition;
        float percentileFraction = 0.5f;
        DbNotRuntime::Standard::SortDirection withinGroupDir = DbNotRuntime::Standard::SortDirection::ASC;
    };

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

    struct OrderBySpec {
        String column = "";
        DbNotRuntime::Standard::SortDirection direction = DbNotRuntime::Standard::SortDirection::ASC;
        DbNotRuntime::Standard::NullsOrder nullsOrder = DbNotRuntime::Standard::NullsOrder::LAST;
    };

    struct CteDefinition {
        String cteName = "";
        bool isRecursive = false;
        std::vector<PgTuple> baseRows;
        LateralGeneratorFn recursiveStepFn = nullptr;
        uint8_t maxRecursionDepth = 16;
    };

    struct SetOperationAttachment {
        Standard::SetOpType opType = Standard::SetOpType::UNION_DISTINCT;
        std::vector<PgTuple> rightRows;
    };

    struct SelectQueryPlan {
        String fromTable = "";
        std::vector<CteDefinition> ctes;
        std::vector<String> selectColumns;
        bool selectDistinct = false;
        std::vector<String> distinctOnColumns;
        std::vector<JoinClauseDef> joins;
        std::vector<PredicateCondition> wherePredicates;
        std::vector<String> groupByColumns;
        Standard::GroupingMode groupingMode = Standard::GroupingMode::SIMPLE;
        std::vector<AggregateSpec> aggregates;
        std::vector<PredicateCondition> havingPredicates;
        std::vector<WindowSpec> windows;
        std::vector<SetOperationAttachment> setOperations;
        std::vector<OrderBySpec> orderBy;
        int32_t limitCount = -1;
        int32_t offsetCount = 0;
    };

    class ModuloDQL {
    private:
        // Clave compacta POD de 40 B (cero heap) para ordenar y paginar por byteOffset
        struct RowOrderKey {
            uint32_t byteOffset = 0;
            uint8_t  kind       = 0; // 0=NULL, 1=INT64, 2=DOUBLE, 3=TEXT
            union {
                int64_t i64;
                double  f64;
            } num{};
            char txtKey[24]{};
        };

        static constexpr size_t MAX_SORT_WINDOW_SINGLE_PASS = 160; // 160 * 40 B = 6.4 KB máx.
        static constexpr size_t CURSOR_ADVANCE_STEP         = 128; // 128 * 40 B = 5.1 KB máx.

        Telemetry::OperationCostMetrics lastCost;

        ModuloDQL() = default;

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

        static int compareTypedCellWithLiteral(const PgCell& cell, const String& literal) {
            return DbCommon::Standard::DataTypeUtils::compareUnionWithLiteral(cell.type, cell.num, cell.rawText, literal);
        }

        static RowOrderKey extractOrderKey(const PgTuple& row, uint32_t byteOffset, const OrderBySpec& ob) {
            RowOrderKey k{};
            k.byteOffset = byteOffset;
            const PgCell* c = row.findCell(ob.column);
            if (!c || c->isNull) {
                k.kind = 0;
                return k;
            }
            if (DbCommon::Standard::DataTypeUtils::isIntegerType(c->type) ||
                DbCommon::Standard::DataTypeUtils::isTemporalOrEpochComparable(c->type)) {
                k.kind    = 1;
                k.num.i64 = c->num.i64;
                return k;
            }
            if (DbCommon::Standard::DataTypeUtils::isFloatingOrDecimalType(c->type)) {
                k.kind    = 2;
                k.num.f64 = c->num.f64;
                return k;
            }
            if (c->type == PgDataType::BOOLEAN) {
                k.kind    = 1;
                k.num.i64 = c->num.b ? 1 : 0;
                return k;
            }
            k.kind = 3;
            strncpy(k.txtKey, c->rawText.c_str(), sizeof(k.txtKey) - 1);
            k.txtKey[sizeof(k.txtKey) - 1] = '\0';
            return k;
        }

        static bool orderKeyPrecedes(const RowOrderKey& a, const RowOrderKey& b, const OrderBySpec& ob) {
            const bool nullA = (a.kind == 0);
            const bool nullB = (b.kind == 0);
            if (nullA && nullB) return a.byteOffset < b.byteOffset;
            if (nullA) return (ob.nullsOrder == DbNotRuntime::Standard::NullsOrder::FIRST);
            if (nullB) return (ob.nullsOrder != DbNotRuntime::Standard::NullsOrder::FIRST);

            int cmp = 0;
            if (a.kind == 1 && b.kind == 1) {
                cmp = (a.num.i64 < b.num.i64) ? -1 : ((a.num.i64 > b.num.i64) ? 1 : 0);
            } else if (a.kind == 2 && b.kind == 2) {
                if (fabs(a.num.f64 - b.num.f64) < 0.00001) cmp = 0;
                else cmp = (a.num.f64 < b.num.f64) ? -1 : 1;
            } else {
                cmp = strcmp(a.txtKey, b.txtKey);
            }

            if (cmp != 0) {
                return (ob.direction == DbNotRuntime::Standard::SortDirection::ASC) ? (cmp < 0) : (cmp > 0);
            }
            return a.byteOffset < b.byteOffset;
        }

        static PgTuple projectColumnsIfNeeded(const PgTuple& r, const std::vector<String>& selectColumns) {
            if (selectColumns.empty() || selectColumns[0] == "*") {
                return r;
            }
            PgTuple p;
            p.reserve(selectColumns.size());
            for (const auto& col : selectColumns) {
                const PgCell* c = r.findCell(col);
                if (c) p.set(col, c->rawText, c->type);
                else   p.set(col, "NULL", PgDataType::TEXT);
            }
            return p;
        }

        // Pasada 1: selecciona solo los byteOffsets de la página [offset .. offset + limit) usando <= 6.4 KB
        static std::vector<uint32_t> selectMatchingOffsets(const SelectQueryPlan& plan) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            const size_t offset = static_cast<size_t>(std::max<int32_t>(0, plan.offsetCount));
            const size_t limit  = (plan.limitCount > 0)
                ? std::min<size_t>(static_cast<size_t>(plan.limitCount), Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM)
                : Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM;

            std::vector<uint32_t> outOffsets;
            outOffsets.reserve(limit);

            // Sin ORDER BY: saltea `offset` coincidencias en streaming y corta apenas junta `limit`
            if (plan.orderBy.empty()) {
                size_t matched = 0;
                storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t byteOff) -> bool {
                    if (!evaluatePredicateChain(r, plan.wherePredicates)) return true;
                    if (matched >= offset) {
                        outOffsets.push_back(byteOff);
                        if (outOffsets.size() >= limit) return false;
                    }
                    matched++;
                    return true;
                });
                return outOffsets;
            }

            // Con ORDER BY: soporta cualquier offset avanzando por ventanas acotadas en RAM
            const OrderBySpec& ob = plan.orderBy[0];
            bool hasFloor = false;
            RowOrderKey floorKey{};
            size_t remainingOffset = offset;

            std::vector<RowOrderKey> topKeys;

            while (true) {
                const bool esVentanaFinal = (remainingOffset + limit <= MAX_SORT_WINDOW_SINGLE_PASS);
                const size_t batchNeed = esVentanaFinal
                    ? (remainingOffset + limit)
                    : std::min<size_t>(remainingOffset, CURSOR_ADVANCE_STEP);

                topKeys.clear();
                topKeys.reserve(std::min<size_t>(batchNeed, 32));

                storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t byteOff) -> bool {
                    if (!evaluatePredicateChain(r, plan.wherePredicates)) return true;

                    RowOrderKey cand = extractOrderKey(r, byteOff, ob);
                    if (hasFloor && !orderKeyPrecedes(floorKey, cand, ob)) {
                        return true; // Ya fue consumido en una ventana anterior
                    }

                    if (topKeys.size() == batchNeed && !orderKeyPrecedes(cand, topKeys.back(), ob)) {
                        return true; // Descarte O(1) sin búsqueda binaria
                    }

                    auto pos = std::lower_bound(
                        topKeys.begin(),
                        topKeys.end(),
                        cand,
                        [&](const RowOrderKey& existing, const RowOrderKey& incoming) {
                            return orderKeyPrecedes(existing, incoming, ob);
                        }
                    );

                    if (topKeys.size() < batchNeed) {
                        topKeys.insert(pos, cand);
                    } else if (pos != topKeys.end()) {
                        topKeys.pop_back();
                        topKeys.insert(pos, cand);
                    }
                    return true;
                });

                if (esVentanaFinal) {
                    if (remainingOffset < topKeys.size()) {
                        const size_t fin = std::min<size_t>(topKeys.size(), remainingOffset + limit);
                        for (size_t i = remainingOffset; i < fin; ++i) {
                            outOffsets.push_back(topKeys[i].byteOffset);
                        }
                    }
                    break;
                }

                // Ventana de avance: si hay menos filas que el salto, no existe esa página
                if (topKeys.size() < batchNeed) {
                    break;
                }
                floorKey = topKeys.back();
                hasFloor = true;
                remainingOffset -= batchNeed;
            }

            return outOffsets;
        }

        static PgTuple executeGlobalAggregation(const SelectQueryPlan& plan) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            const size_t nAgg = plan.aggregates.size();
            std::vector<int64_t> counts(nAgg, 0);
            std::vector<double>  sums(nAgg, 0.0);
            std::vector<PgCell>  bestCell(nAgg);
            std::vector<bool>    hasBest(nAgg, false);

            storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t) -> bool {
                if (!evaluatePredicateChain(r, plan.wherePredicates)) return true;

                for (size_t i = 0; i < nAgg; ++i) {
                    const auto& ag = plan.aggregates[i];
                    if (ag.hasFilter && !evaluateCondition(r, ag.filterCondition)) continue;

                    if (ag.func == Standard::AggregateFunc::COUNT_ALL) {
                        counts[i]++;
                    } else {
                        const PgCell* c = r.findCell(ag.targetColumn);
                        if (c && !c->isNull) {
                            counts[i]++;
                            sums[i] += c->asDouble();
                            if (!hasBest[i]) {
                                bestCell[i] = *c;
                                hasBest[i]  = true;
                            } else {
                                int cmp = compareTypedCellWithLiteral(*c, bestCell[i].rawText);
                                if ((ag.func == Standard::AggregateFunc::MIN && cmp < 0) ||
                                    (ag.func == Standard::AggregateFunc::MAX && cmp > 0)) {
                                    bestCell[i] = *c;
                                }
                            }
                        }
                    }
                }
                return true;
            });

            PgTuple outAll;
            outAll.reserve(nAgg);
            for (size_t i = 0; i < nAgg; ++i) {
                const auto& ag = plan.aggregates[i];
                switch (ag.func) {
                    case Standard::AggregateFunc::COUNT_ALL:
                    case Standard::AggregateFunc::COUNT_COL:
                    case Standard::AggregateFunc::COUNT_DISTINCT:
                        outAll.set(ag.alias, String(static_cast<long>(counts[i])), PgDataType::BIGINT);
                        break;
                    case Standard::AggregateFunc::SUM:
                    case Standard::AggregateFunc::AVG: {
                        if (counts[i] == 0) {
                            outAll.set(ag.alias, "0.00", PgDataType::REAL);
                        } else {
                            double val = (ag.func == Standard::AggregateFunc::AVG) ? (sums[i] / counts[i]) : sums[i];
                            outAll.set(ag.alias, String(static_cast<float>(val), 2), PgDataType::REAL);
                        }
                        break;
                    }
                    case Standard::AggregateFunc::MIN:
                    case Standard::AggregateFunc::MAX:
                        if (!hasBest[i]) {
                            outAll.set(ag.alias, "NULL", PgDataType::TEXT);
                        } else {
                            outAll.set(ag.alias, bestCell[i].rawText, bestCell[i].type);
                        }
                        break;
                    default:
                        outAll.set(ag.alias, "0.00", PgDataType::REAL);
                        break;
                }
            }
            return outAll;
        }

    public:
        static ModuloDQL& getInstance() {
            static ModuloDQL instance;
            return instance;
        }

        const Telemetry::OperationCostMetrics& getLastCost() const { return lastCost; }

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
            if (isCellNull) return false;

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

        // Conteo real en streaming con 0 bytes de heap acumulados
        size_t executeCount(const SelectQueryPlan& plan) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            size_t total = 0;
            storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t) -> bool {
                if (evaluatePredicateChain(r, plan.wherePredicates)) {
                    total++;
                }
                return true;
            });
            return total;
        }

        // Ejecuta SELECT entregando 1 sola PgTuple en RAM a la vez al consumidor (usado por all_as y HMAC)
        template <typename ConsumerFn>
        void executeSelectStreaming(const SelectQueryPlan& plan, ConsumerFn consumer) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();

            if (plan.groupByColumns.empty() && !plan.aggregates.empty()) {
                PgTuple agg = executeGlobalAggregation(plan);
                consumer(agg);
                return;
            }

            std::vector<uint32_t> offsets = selectMatchingOffsets(plan);
            if (offsets.empty()) return;

            const bool needsProj = !plan.selectColumns.empty() && plan.selectColumns[0] != "*";
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            storage.forEachRowAtOffsets(plan.fromTable, offsets.data(), offsets.size(), [&](const PgTuple& r) -> bool {
                if (needsProj) {
                    PgTuple proj = projectColumnsIfNeeded(r, plan.selectColumns);
                    return consumer(proj);
                }
                return consumer(r);
            });
        }

        // Ejecución estándar retornando vector (usada por Terminal SQL, .all() y .first())
        std::vector<PgTuple> executeSelect(const SelectQueryPlan& plan) {
            std::vector<PgTuple> out;
            executeSelectStreaming(plan, [&](const PgTuple& r) -> bool {
                // Cortafuegos de memoria si la Terminal SQL pide muchas filas de tablas muy anchas
                if (!out.empty() && ESP.getMaxAllocHeap() < 8192) {
                    return false;
                }
                out.push_back(r);
                return true;
            });
            return out;
        }
    };

} // namespace DbRuntime::Engine