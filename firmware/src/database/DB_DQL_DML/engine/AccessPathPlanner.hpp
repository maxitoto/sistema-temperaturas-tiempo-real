#pragma once
#include <Arduino.h>
#include <vector>
#include <cstring>
#include "../telemetry/RuntimeCostProfiler.hpp"
#include "../standard/PostgresDmlDqlKeywords.hpp"
#include "../storage/RuntimeIndexAccessPath.hpp"

namespace DbRuntime::Engine {

    // Puntero a función ligero (4 bytes en Xtensa) para evaluar subconsultas EXISTS / IN sin usar std::function en pila.
    using SubqueryEvaluatorFn = bool (*)(const Storage::PgTuple&);

    // Representa una condición atómica de filtrado en cláusulas WHERE, HAVING o FILTER.
    struct PredicateCondition {
        String columnOrExpr = "";
        Standard::FilterOp op = Standard::FilterOp::EQ;
        String valueA = "";
        String valueB = ""; // Usado en BETWEEN valueA AND valueB
        std::vector<String> valueList; // Usado en IN (...) / NOT IN (...)
        Standard::LogicConnector connector = Standard::LogicConnector::AND_OP;
        bool negated = false;
        SubqueryEvaluatorFn subqueryFn = nullptr; // Soporte ligero para EXISTS / ANY / ALL 
        bool valueIsColumn = false; // true: valueA es el nombre de otra columna (comparación columna-columna)

        // Determina si el predicado apunta a un identificador de columna simple apto para búsqueda por índice.
        bool keyColumnCandidate() const {
            return !columnOrExpr.isEmpty() && columnOrExpr.indexOf('(') == -1 && columnOrExpr.indexOf(' ') == -1;
        }

        // Extrae el nombre limpio de la columna eliminando el prefijo de tabla ("Tabla.col" -> "col") si existe.
        String cleanColumnName() const {
            int dot = columnOrExpr.indexOf('.');
            return (dot != -1) ? columnOrExpr.substring(dot + 1) : columnOrExpr;
        }
    };

    // Resultado de la decisión del planificador de caminos de acceso.
    struct ChosenAccessPlan {
        Telemetry::AccessScanType scanType = Telemetry::AccessScanType::SEQ_SCAN;
        const Storage::ActiveIndexStructure* indexPtr = nullptr;
        String lookupValue = "";
    };

    class AccessPathPlanner {
    public:
        // Inspecciona los filtros conjuntivos (AND) y selecciona el índice de menor costo disponible en el catálogo DDL.
        static ChosenAccessPlan chooseBestAccessPath(const String& tableName,
                                                     const std::vector<PredicateCondition>& filters,
                                                     Telemetry::OperationCostMetrics& outMetrics) {
            ChosenAccessPlan plan;
            plan.scanType = Telemetry::AccessScanType::SEQ_SCAN;
            outMetrics.scanType = Telemetry::AccessScanType::SEQ_SCAN;
            outMetrics.indexUsed[0] = '\0';

            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            // Si existe una condición de igualdad (EQ) no negada unida por AND sobre una columna indexada, usa Index Scan
            for (const auto& cond : filters) {
                if (cond.negated || cond.connector == Standard::LogicConnector::OR_OP) continue;
                if (cond.op == Standard::FilterOp::EQ && cond.keyColumnCandidate() && !cond.valueIsColumn &&
                    (cond.columnOrExpr.indexOf('.') == -1 || cond.columnOrExpr.startsWith(tableName + "."))) {
                    String cleanCol = cond.cleanColumnName();
                    const auto* idx = storage.findBestIndexForColumn(tableName, cleanCol);
                    if (idx && !idx->keyColumns.empty()) {
                        plan.indexPtr    = idx;
                        plan.lookupValue = cond.valueA;
                        if (idx->isPrimary) {
                            plan.scanType = Telemetry::AccessScanType::PRIMARY_KEY_LOOKUP;
                        } else if (idx->isUnique) {
                            plan.scanType = Telemetry::AccessScanType::UNIQUE_INDEX_LOOKUP;
                        } else {
                            plan.scanType = Telemetry::AccessScanType::INDEX_SCAN;
                        }
                        outMetrics.scanType = plan.scanType;
                        strncpy(outMetrics.indexUsed, idx->indexName.c_str(), sizeof(outMetrics.indexUsed) - 1);
                        return plan;
                    }
                }
            }
            return plan;
        }
    };

} // namespace DbRuntime::Engine