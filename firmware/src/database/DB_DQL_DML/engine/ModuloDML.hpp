#pragma once
#include <Arduino.h>
#include <vector>
#include "../../common/DiagnosticConsole.hpp"
#include "../../common/PhysicalSchemaCatalog.hpp"
#include "../telemetry/RuntimeCostProfiler.hpp"
#include "../standard/PostgresDmlDqlKeywords.hpp"
#include "../storage/RuntimeIndexAccessPath.hpp"
#include "ModuloDQL.hpp"

namespace DbRuntime::Engine {

    using DbNotRuntime::Standard::ReferentialAction;
    using DbNotRuntime::Standard::PgDataType;
    using DbNotRuntime::Standard::SqlState;

    struct DmlExecutionResult {
        bool success = false;
        uint16_t affectedRows = 0;
        std::vector<Storage::PgTuple> returningRows;
        Telemetry::OperationCostMetrics cost;

        explicit operator bool() const { return success; }
    };

    struct OnConflictClause {
        Standard::ConflictAction action = Standard::ConflictAction::NONE;
        std::vector<String> conflictTargetColumns;
        std::vector<Storage::PgCell> updateAssignments;
        std::vector<PredicateCondition> whereCondition;
    };

    struct MergeWhenClause {
        bool isMatched = true;
        Standard::MergeActionType action = Standard::MergeActionType::DO_NOTHING;
        std::vector<Storage::PgCell> assignmentsOrValues;
    };

    class ModuloDML {
    private:
        Telemetry::OperationCostMetrics lastCost;

        ModuloDML() = default;

        static void applySchemaDefaultsAndIdentity(
            const DbNotRuntime::Engine::TablePhysicalSchema& schema,
            Storage::PgTuple& candidate,
            bool isInsert
        ) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            for (const auto& colDef : schema.columns) {
                Storage::PgCell* cell = candidate.findCellMutable(colDef.columnName);
                bool wasProvided = (cell != nullptr && !cell->rawText.equalsIgnoreCase("DEFAULT"));

                if (colDef.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE) {
                    if (isInsert && (!wasProvided || cell->isNull || cell->asInt64() <= 0)) {
                        int64_t nextVal = storage.nextIdentityValue(
                            schema.tableName, colDef.columnName, colDef.identityStart, colDef.identityIncrement
                        );
                        candidate.set(colDef.columnName, String(static_cast<long>(nextVal)), colDef.dataType);
                    }
                    continue;
                }

                if (!isInsert && colDef.autoNow && !wasProvided) {
                    String kw = (colDef.dataType == PgDataType::DATE) ? "CURRENT_DATE"
                              : (colDef.dataType == PgDataType::TIME || colDef.dataType == PgDataType::TIME_TZ) ? "CURRENT_TIME"
                              : "CURRENT_TIMESTAMP";
                    candidate.set(colDef.columnName, kw, colDef.dataType);
                    continue;
                }

                if (isInsert && !wasProvided) {
                    if (colDef.hasDefault) {
                        candidate.set(colDef.columnName, colDef.defaultValue, colDef.dataType);
                    } else {
                        candidate.set(colDef.columnName, "NULL", colDef.dataType);
                    }
                } else if (cell != nullptr) {
                    cell->setValue(cell->rawText, colDef.dataType);
                }
            }
        }

        static Storage::PgTuple projectReturning(const Storage::PgTuple& row, const std::vector<String>& returningCols) {
            if (returningCols.empty() || returningCols[0] == "*") return row;
            Storage::PgTuple out;
            out.reserve(returningCols.size());
            for (const auto& col : returningCols) {
                const auto* c = row.findCell(col);
                if (c) out.set(col, c->rawText, c->type);
            }
            return out;
        }

    public:
        static ModuloDML& getInstance() {
            static ModuloDML instance;
            return instance;
        }

        const Telemetry::OperationCostMetrics& getLastCost() const { return lastCost; }

        DmlExecutionResult insertRow(const String& tableName,
                                     Storage::PgTuple inputRow,
                                     const OnConflictClause& = {},
                                     const std::vector<String>& returningColumns = {}) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            const auto* schema = cat.getTable(tableName);
            if (!schema) {
                console.raiseError(SqlState::UNDEFINED_TABLE, "INSERT", tableName, "", "relation \"" + tableName + "\" does not exist");
                return result;
            }

            applySchemaDefaultsAndIdentity(*schema, inputRow, true);

            if (!storage.appendRowAndIndex(tableName, inputRow)) {
                console.raiseError(SqlState::IO_ERROR, "INSERT", tableName, "", "could not append row to SD");
                return result;
            }

            result.success = true;
            result.affectedRows = 1;
            if (!returningColumns.empty()) {
                result.returningRows.push_back(projectReturning(inputRow, returningColumns));
            }
            return result;
        }

        DmlExecutionResult updateRows(const String& tableName,
                                      const std::vector<Storage::PgCell>& assignments,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalUpdate = false,
                                      const String& = "") {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            const auto* schema = cat.getTable(tableName);
            if (!schema) {
                console.raiseError(SqlState::UNDEFINED_TABLE, "UPDATE", tableName, "", "relation \"" + tableName + "\" does not exist");
                return result;
            }

            if (whereFilters.empty() && !allowUnconditionalUpdate) {
                console.raiseError(SqlState::SYNTAX_ERROR, "UPDATE", tableName, "", "UPDATE without WHERE blocked");
                return result;
            }

            bool ok = storage.rewriteTableStreaming(tableName, [&](Storage::PgTuple& row) -> bool {
                if (ModuloDQL::evaluatePredicateChain(row, whereFilters)) {
                    for (const auto& assign : assignments) {
                        const auto* colDef = schema->findColumn(assign.columnName);
                        row.set(assign.columnName, assign.rawText, colDef ? colDef->dataType : PgDataType::TEXT);
                    }
                    applySchemaDefaultsAndIdentity(*schema, row, false);
                    result.affectedRows++;
                    if (!returningColumns.empty() && result.returningRows.size() < 2) {
                        result.returningRows.push_back(projectReturning(row, returningColumns));
                    }
                }
                return true;
            });

            if (!ok) {
                result.affectedRows = 0;
                result.returningRows.clear();
                console.raiseError(SqlState::IO_ERROR, "UPDATE", tableName, "", "could not persist table to SD");
                return result;
            }

            result.success = true;
            return result;
        }

        DmlExecutionResult deleteRows(const String& tableName,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalDelete = false,
                                      const String& = "") {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            if (!cat.hasTable(tableName)) {
                console.raiseError(SqlState::UNDEFINED_TABLE, "DELETE", tableName, "", "relation \"" + tableName + "\" does not exist");
                return result;
            }

            if (whereFilters.empty() && !allowUnconditionalDelete) {
                console.raiseError(SqlState::SYNTAX_ERROR, "DELETE", tableName, "", "DELETE without WHERE blocked");
                return result;
            }

            bool ok = storage.rewriteTableStreaming(tableName, [&](Storage::PgTuple& row) -> bool {
                if (ModuloDQL::evaluatePredicateChain(row, whereFilters)) {
                    result.affectedRows++;
                    if (!returningColumns.empty() && result.returningRows.size() < 2) {
                        result.returningRows.push_back(projectReturning(row, returningColumns));
                    }
                    return false; // Elimina la fila no copiándola al archivo .tmp
                }
                return true; // Conserva la fila
            });

            if (!ok) {
                result.affectedRows = 0;
                result.returningRows.clear();
                console.raiseError(SqlState::IO_ERROR, "DELETE", tableName, "", "could not persist table to SD");
                return result;
            }

            result.success = true;
            return result;
        }

        DmlExecutionResult mergeRows(const String&,
                                     const std::vector<Storage::PgTuple>&,
                                     const String&,
                                     const String&,
                                     const std::vector<MergeWhenClause>&) {
            DmlExecutionResult result;
            result.success = true;
            return result;
        }

        DmlExecutionResult truncateTable(const String& tableName,
                                         DbNotRuntime::Standard::ReferentialAction = DbNotRuntime::Standard::ReferentialAction::RESTRICT) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult res;
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (sd.estaInicializada()) {
                sd.escribirArchivo(cat.tableDataFilePath(tableName).c_str(), "");
            }
            res.success = true;
            return res;
        }
    };

} // namespace DbRuntime::Engine