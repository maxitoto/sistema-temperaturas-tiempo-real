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

        static bool tuplePrecedes(const PgTuple& a, const PgTuple& b, const std::vector<OrderBySpec>& orderBy) {
            for (const auto& ob : orderBy) {
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

        // Ejecución DQL directa en streaming: consume 1 sola tupla en RAM para agregaciones o LIMIT 1
        std::vector<PgTuple> executeSelect(const SelectQueryPlan& plan) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            // Caso 1: Agregaciones globales (obtenerEstadisticas...) calculadas al vuelo sin guardar filas en RAM
            if (plan.groupByColumns.empty() && !plan.aggregates.empty()) {
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
                return { outAll };
            }

            // Caso 2: Consulta de filas acotada estrictamente a (limitCount + offsetCount) en RAM
            const size_t maxKeep = (plan.limitCount > 0)
                ? std::min(Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM,
                           static_cast<size_t>(plan.limitCount + std::max<int32_t>(0, plan.offsetCount)))
                : Storage::RuntimeStorageAndIndexManager::MAX_SELECT_ROWS_RAM;

            std::vector<PgTuple> workingRows;
            workingRows.reserve(maxKeep);

            storage.forEachRowStreaming(plan.fromTable, [&](const PgTuple& r, uint32_t) -> bool {
                if (!evaluatePredicateChain(r, plan.wherePredicates)) return true;

                if (plan.orderBy.empty()) {
                    workingRows.push_back(r);
                    if (workingRows.size() >= maxKeep) return false;
                } else {
                    auto pos = std::lower_bound(
                        workingRows.begin(),
                        workingRows.end(),
                        r,
                        [&](const PgTuple& existing, const PgTuple& candidate) {
                            return tuplePrecedes(existing, candidate, plan.orderBy);
                        }
                    );
                    if (workingRows.size() < maxKeep) {
                        workingRows.insert(pos, r);
                    } else if (pos != workingRows.end()) {
                        workingRows.pop_back();
                        workingRows.insert(pos, r);
                    }
                }
                return true;
            });

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

            if (!plan.selectColumns.empty() && plan.selectColumns[0] != "*") {
                for (auto& r : workingRows) {
                    PgTuple p;
                    p.reserve(plan.selectColumns.size());
                    for (const auto& col : plan.selectColumns) {
                        const PgCell* c = r.findCell(col);
                        if (c) p.set(col, c->rawText, c->type);
                        else   p.set(col, "NULL", PgDataType::TEXT);
                    }
                    r = std::move(p);
                }
            }

            return workingRows;
        }
    };

} // namespace DbRuntime::Engine