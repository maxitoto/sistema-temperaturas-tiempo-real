#pragma once
#include <Arduino.h>
#include <vector>
#include <algorithm>
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

        static bool validateCandidateTuple(const DbNotRuntime::Engine::TablePhysicalSchema& schema,
                                           Storage::PgTuple& candidate,
                                           const char* commandName,
                                           bool isInsert) {
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();

            for (const auto& cell : candidate.allCells()) {
                if (!schema.findColumn(cell.columnName)) {
                    return console.raiseError(
                        SqlState::UNDEFINED_COLUMN, commandName, schema.tableName, cell.columnName,
                        "column \"" + String(cell.columnName) + "\" of relation \"" + schema.tableName + "\" does not exist"
                    );
                }
            }

            for (const auto& colDef : schema.columns) {
                Storage::PgCell* cell = candidate.findCellMutable(colDef.columnName);
                bool wasProvided = (cell != nullptr && !cell->rawText.equalsIgnoreCase("DEFAULT"));

                if (colDef.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE) {
                    if (wasProvided && isInsert && colDef.identityMode == DbNotRuntime::Standard::IdentityGeneration::ALWAYS) {
                        return console.raiseError(
                            SqlState::SYNTAX_ERROR, commandName, schema.tableName, colDef.columnName,
                            "cannot insert a non-DEFAULT value into column \"" + colDef.columnName + "\"",
                            "Column \"" + colDef.columnName + "\" is GENERATED ALWAYS AS IDENTITY."
                        );
                    }
                    if (!wasProvided && isInsert) {
                        candidate.set(colDef.columnName, "NULL", colDef.dataType);
                        continue;
                    }
                }

                // Si es UPDATE y la columna tiene auto_now=true (y no fue sobrescrita manualmente), estampa la fecha/hora actual
                if (!isInsert && colDef.autoNow && !wasProvided) {
                    String kw = (colDef.dataType == PgDataType::DATE) ? "CURRENT_DATE"
                              : (colDef.dataType == PgDataType::TIME || colDef.dataType == PgDataType::TIME_TZ) ? "CURRENT_TIME"
                              : "CURRENT_TIMESTAMP";
                    candidate.set(colDef.columnName, kw, colDef.dataType);
                    cell = candidate.findCellMutable(colDef.columnName);
                    wasProvided = true;
                }

                if (!wasProvided) {
                    if (colDef.hasDefault) {
                        candidate.set(colDef.columnName, colDef.defaultValue, colDef.dataType);
                    } else if (colDef.dataType == PgDataType::DOMAIN_TYPE) {
                        const auto* dm = cat.getDomain(colDef.customTypeName);
                        if (dm && dm->hasDefault) {
                            candidate.set(colDef.columnName, dm->defaultValue, dm->baseType);
                        } else {
                            candidate.set(colDef.columnName, "NULL", colDef.dataType);
                        }
                    } else {
                        candidate.set(colDef.columnName, "NULL", colDef.dataType);
                    }
                    cell = candidate.findCellMutable(colDef.columnName);
                } else {
                    PgDataType effectiveType = colDef.dataType;
                    if (effectiveType == PgDataType::DOMAIN_TYPE) {
                        const auto* dm = cat.getDomain(colDef.customTypeName);
                        if (dm) effectiveType = dm->baseType;
                    }
                    cell->setValue(cell->rawText, effectiveType);
                }

                bool domainNotNull = false;
                PgDataType typeToValidate = colDef.dataType;
                uint16_t maxLenToValidate = colDef.maxLength;
                uint8_t precToValidate    = colDef.precision;
                uint8_t scaleToValidate   = colDef.scale;

                if (colDef.dataType == PgDataType::DOMAIN_TYPE) {
                    const auto* dm = cat.getDomain(colDef.customTypeName);
                    if (dm) {
                        domainNotNull    = dm->notNull;
                        typeToValidate   = dm->baseType;
                        maxLenToValidate = dm->maxLength;
                        precToValidate   = dm->precision;
                        scaleToValidate  = dm->scale;
                    }
                }

                if ((colDef.isNotNull || domainNotNull) && (!cell || cell->isNull)) {
                    return console.raiseError(
                        SqlState::NOT_NULL_VIOLATION, commandName, schema.tableName, colDef.columnName,
                        "null value in column \"" + colDef.columnName + "\" of relation \"" + schema.tableName + "\" violates not-null constraint"
                    );
                }

                if (cell && !cell->isNull) {
                    const DbNotRuntime::Standard::EnumTypeDefinition* enumDef =
                        (typeToValidate == PgDataType::ENUM) ? cat.getEnum(colDef.customTypeName) : nullptr;
                    String stateOut, msgOut;
                    if (!DbNotRuntime::Standard::DataTypeUtils::validateLiteral(
                            typeToValidate, cell->rawText, maxLenToValidate, precToValidate, scaleToValidate,
                            enumDef, stateOut, msgOut)) {
                        return console.raiseError(
                            stateOut.c_str(), commandName, schema.tableName, colDef.columnName, msgOut
                        );
                    }
                }

                if (colDef.hasCheck && colDef.checkFn && cell && !cell->isNull) {
                    if (!colDef.checkFn(cell->rawText)) {
                        return console.raiseError(
                            SqlState::CHECK_VIOLATION, commandName, schema.tableName, colDef.columnName,
                            "new row for relation \"" + schema.tableName + "\" violates check constraint \"" + colDef.checkConstraintName + "\"",
                            "", colDef.checkConstraintName
                        );
                    }
                }

                if (colDef.isForeignKey && cell && !cell->isNull) {
                    const auto* parentIdx = storage.findBestIndexForColumn(colDef.referencedTable, colDef.referencedColumn);
                    bool parentExists = false;
                    if (parentIdx) {
                        parentExists = !parentIdx->lookupEqual(cell->rawText).empty();
                    } else {
                        for (const auto& pr : storage.getTableRows(colDef.referencedTable)) {
                            if (pr.getText(colDef.referencedColumn.c_str()) == cell->rawText) {
                                parentExists = true;
                                break;
                            }
                        }
                    }
                    if (!parentExists) {
                        return console.raiseError(
                            SqlState::FOREIGN_KEY_VIOLATION, commandName, schema.tableName, colDef.columnName,
                            "insert or update on table \"" + schema.tableName + "\" violates foreign key constraint \"" + colDef.fkConstraintName + "\"",
                            "Key (" + colDef.columnName + ")=(" + cell->rawText + ") is not present in table \"" + colDef.referencedTable + "\".",
                            colDef.fkConstraintName
                        );
                    }
                }
            }
            return true;
        }

        static int32_t findUniqueConflictRowOffset(const String& tableName,
                                                   const Storage::PgTuple& candidate,
                                                   int32_t ignoreRowOffset,
                                                   String& outIndexName) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            for (const auto& idx : storage.getIndexesOfTable(tableName)) {
                if (!idx.isUnique && !idx.isPrimary) continue;
                char key[32];
                if (!idx.buildCompositeKey(candidate, key, sizeof(key))) continue;
                std::vector<uint32_t> matches = idx.lookupEqual(String(key));
                for (uint32_t pos : matches) {
                    if (static_cast<int32_t>(pos) != ignoreRowOffset) {
                        outIndexName = idx.indexName;
                        return static_cast<int32_t>(pos);
                    }
                }
            }
            return -1;
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

        static void snapshotTableGraph(const String& tableName, const String& skipTable,
                                       std::map<String, std::vector<Storage::PgTuple>>& snap) {
            if (tableName.equalsIgnoreCase(skipTable) || snap.find(tableName) != snap.end()) return;
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            snap[tableName] = storage.getTableRows(tableName);
            for (const auto& dep : cat.findDependentTables(tableName)) snapshotTableGraph(dep, skipTable, snap);
        }

        static void restoreSnapshot(const std::map<String, std::vector<Storage::PgTuple>>& snap) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            for (const auto& kv : snap) {
                storage.replaceTableRows(kv.first, kv.second);
            }
        }

    public:
        static ModuloDML& getInstance() {
            static ModuloDML instance;
            return instance;
        }

        const Telemetry::OperationCostMetrics& getLastCost() const { return lastCost; }

        DmlExecutionResult insertRow(const String& tableName,
                                     Storage::PgTuple inputRow,
                                     const OnConflictClause& onConflict = {},
                                     const std::vector<String>& returningColumns = {}) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            {
                Telemetry::ScopedCostProbe probe(lastCost, "INSERT", tableName);
                const auto* schema = cat.getTable(tableName);
                if (!schema) {
                    console.raiseError(SqlState::UNDEFINED_TABLE, "INSERT", tableName, "", "relation \"" + tableName + "\" does not exist");
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                if (!validateCandidateTuple(*schema, inputRow, "INSERT", true)) {
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                String conflictIdxName = "";
                int32_t conflictOffset = findUniqueConflictRowOffset(tableName, inputRow, -1, conflictIdxName);
                lastCost.scanType = Telemetry::AccessScanType::UNIQUE_INDEX_LOOKUP;
                strncpy(lastCost.indexUsed, conflictIdxName.c_str(), sizeof(lastCost.indexUsed) - 1);

                if (conflictOffset >= 0) {
                    if (onConflict.action == Standard::ConflictAction::DO_NOTHING) {
                        result.success = true;
                        result.affectedRows = 0;
                        console.reportSuccess("INSERT 0 0", tableName, "ON CONFLICT DO NOTHING");
                    } else if (onConflict.action == Standard::ConflictAction::DO_UPDATE) {
                        std::vector<Storage::PgTuple> allRows;
                        bool updatedOk = false;
                        bool validationFailed = false;
                        Storage::PgTuple updatedTuple;

                        storage.forEachRowStreaming(tableName, [&](const Storage::PgTuple& row, uint32_t offset) -> bool {
                            if (static_cast<int32_t>(offset) == conflictOffset &&
                                ModuloDQL::evaluatePredicateChain(row, onConflict.whereCondition)) {
                                Storage::PgTuple candidate = row;
                                for (const auto& assign : onConflict.updateAssignments) {
                                    const auto* colDef = schema->findColumn(assign.columnName);
                                    candidate.set(assign.columnName, assign.rawText, colDef ? colDef->dataType : PgDataType::TEXT);
                                }
                                if (!validateCandidateTuple(*schema, candidate, "INSERT ON CONFLICT DO UPDATE", false)) {
                                    validationFailed = true;
                                    return false;
                                }
                                updatedTuple = candidate;
                                allRows.push_back(std::move(candidate));
                                updatedOk = true;
                            } else {
                                allRows.push_back(row);
                            }
                            return true;
                        });

                        if (validationFailed) {
                            probe.finish(); result.cost = lastCost;
                            return result;
                        }
                        if (updatedOk) {
                            if (!storage.replaceTableRows(tableName, allRows)) {
                                console.raiseError(SqlState::IO_ERROR, "INSERT ON CONFLICT DO UPDATE", tableName, "", "could not persist updated table to SD");
                                probe.finish(); result.cost = lastCost;
                                return result;
                            }
                            result.affectedRows = 1;
                            lastCost.rowsProduced = 1;
                            if (!returningColumns.empty()) {
                                result.returningRows.push_back(projectReturning(updatedTuple, returningColumns));
                            }
                        }
                        result.success = true;
                        console.reportSuccess("INSERT 0 1", tableName, "ON CONFLICT DO UPDATE");
                    } else {
                        console.raiseError(
                            SqlState::UNIQUE_VIOLATION, "INSERT", tableName, "",
                            "duplicate key value violates unique constraint \"" + conflictIdxName + "\"",
                            "", conflictIdxName
                        );
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }
                } else {
                    for (const auto& colDef : schema->columns) {
                        if (colDef.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE) {
                            const Storage::PgCell* c = inputRow.findCell(colDef.columnName);
                            if (!c || c->isNull) {
                                int64_t nextVal = storage.nextIdentityValue(
                                    schema->tableName, colDef.columnName, colDef.identityStart, colDef.identityIncrement
                                );
                                inputRow.set(colDef.columnName, String(static_cast<long>(nextVal)), colDef.dataType);
                            }
                        }
                    }
                    if (!storage.appendRowAndIndex(tableName, inputRow)) {
                        console.raiseError(SqlState::IO_ERROR, "INSERT", tableName, "", "could not append row to SD");
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }

                    result.success = true;
                    result.affectedRows = 1;
                    lastCost.rowsProduced = 1;
                    if (!returningColumns.empty()) {
                        result.returningRows.push_back(projectReturning(inputRow, returningColumns));
                    }
                    console.reportSuccess("INSERT 0 1", tableName);
                }
            }
            result.cost = lastCost;
            return result;
        }

        DmlExecutionResult updateRows(const String& tableName,
                                      const std::vector<Storage::PgCell>& assignments,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalUpdate = false,
                                      const String& fromAuxTable = "") {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            {
                Telemetry::ScopedCostProbe probe(lastCost, "UPDATE", tableName);
                const auto* schema = cat.getTable(tableName);
                if (!schema) {
                    console.raiseError(SqlState::UNDEFINED_TABLE, "UPDATE", tableName, "", "relation \"" + tableName + "\" does not exist");
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                if (whereFilters.empty() && !allowUnconditionalUpdate) {
                    console.raiseError(
                        SqlState::SYNTAX_ERROR, "UPDATE", tableName, "",
                        "unconditional UPDATE without WHERE clause is blocked by deterministic runtime policy",
                        "Add a .filter(...) clause or pass allowUnconditionalUpdate=true explicitly."
                    );
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                for (const auto& assign : assignments) {
                    if (!schema->findColumn(assign.columnName)) {
                        console.raiseError(
                            SqlState::UNDEFINED_COLUMN, "UPDATE", tableName, assign.columnName,
                            "column \"" + String(assign.columnName) + "\" of relation \"" + tableName + "\" does not exist"
                        );
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }
                }

                ChosenAccessPlan accessPlan = AccessPathPlanner::chooseBestAccessPath(tableName, whereFilters, lastCost);
                std::vector<uint32_t> indexedOffsets;
                if (accessPlan.indexPtr != nullptr) {
                    indexedOffsets = accessPlan.indexPtr->lookupFirstColumnPrefix(accessPlan.lookupValue);
                }

                const auto& fromRows = fromAuxTable.isEmpty() ? std::vector<Storage::PgTuple>{} : storage.getTableRows(fromAuxTable);
                std::vector<Storage::PgTuple> finalRows;
                std::vector<Storage::PgTuple> stagedModified;
                uint16_t examined = 0;
                bool aborted = false;

                storage.forEachRowStreaming(tableName, [&](const Storage::PgTuple& currentRow, uint32_t offset) -> bool {
                    bool inIndex = true;
                    if (accessPlan.indexPtr != nullptr) {
                        inIndex = std::find(indexedOffsets.begin(), indexedOffsets.end(), offset) != indexedOffsets.end();
                    }
                    if (!inIndex) {
                        finalRows.push_back(currentRow);
                        return true;
                    }

                    examined++;
                    bool matchesWhere = false;
                    if (!fromAuxTable.isEmpty()) {
                        for (const auto& fr : fromRows) {
                            Storage::PgTuple joined = currentRow;
                            for (const auto& fc : fr.allCells()) {
                                joined.set(fromAuxTable + "." + String(fc.columnName), fc.rawText, fc.type);
                            }
                            if (ModuloDQL::evaluatePredicateChain(joined, whereFilters)) {
                                matchesWhere = true;
                                break;
                            }
                        }
                    } else {
                        matchesWhere = ModuloDQL::evaluatePredicateChain(currentRow, whereFilters);
                    }

                    if (!matchesWhere) {
                        finalRows.push_back(currentRow);
                        return true;
                    }

                    Storage::PgTuple candidate = currentRow;
                    for (const auto& assign : assignments) {
                        const auto* colDef = schema->findColumn(assign.columnName);
                        candidate.set(assign.columnName, assign.rawText, colDef->dataType);
                    }

                    if (!validateCandidateTuple(*schema, candidate, "UPDATE", false)) {
                        aborted = true;
                        return false;
                    }

                    String conflictIdx = "";
                    bool uniqueViolation =
                        findUniqueConflictRowOffset(tableName, candidate, static_cast<int32_t>(offset), conflictIdx) >= 0;

                    if (!uniqueViolation && !stagedModified.empty()) {
                        for (const auto& ix : storage.getIndexesOfTable(tableName)) {
                            if (!ix.isUnique && !ix.isPrimary) continue;
                            char kNew[32];
                            if (!ix.buildCompositeKey(candidate, kNew, sizeof(kNew))) continue;
                            for (const auto& st : stagedModified) {
                                char kSt[32];
                                if (ix.buildCompositeKey(st, kSt, sizeof(kSt)) && strcmp(kNew, kSt) == 0) {
                                    conflictIdx = ix.indexName;
                                    uniqueViolation = true;
                                    break;
                                }
                            }
                            if (uniqueViolation) break;
                        }
                    }

                    if (uniqueViolation) {
                        console.raiseError(
                            SqlState::UNIQUE_VIOLATION, "UPDATE", tableName, "",
                            "duplicate key value violates unique constraint \"" + conflictIdx + "\"",
                            "", conflictIdx
                        );
                        aborted = true;
                        return false;
                    }

                    result.affectedRows++;
                    if (!returningColumns.empty()) {
                        result.returningRows.push_back(projectReturning(candidate, returningColumns));
                    }
                    stagedModified.push_back(candidate);
                    finalRows.push_back(std::move(candidate));
                    return true;
                });

                lastCost.rowsExamined = examined;
                if (aborted) {
                    result.affectedRows = 0;
                    result.returningRows.clear();
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                if (result.affectedRows > 0) {
                    if (!storage.replaceTableRows(tableName, finalRows)) {
                        result.affectedRows = 0;
                        result.returningRows.clear();
                        console.raiseError(SqlState::IO_ERROR, "UPDATE", tableName, "",
                                           "could not persist table \"" + tableName + "\"; UPDATE rolled back");
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }
                }

                lastCost.rowsProduced = result.affectedRows;
                result.success = true;
                console.reportSuccess("UPDATE", tableName, String(result.affectedRows) + " rows updated");
            }
            result.cost = lastCost;
            return result;
        }

        DmlExecutionResult deleteRows(const String& tableName,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalDelete = false,
                                      const String& usingAuxTable = "") {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            {
                Telemetry::ScopedCostProbe probe(lastCost, "DELETE", tableName);
                const auto* schema = cat.getTable(tableName);
                if (!schema) {
                    console.raiseError(SqlState::UNDEFINED_TABLE, "DELETE", tableName, "", "relation \"" + tableName + "\" does not exist");
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                if (whereFilters.empty() && !allowUnconditionalDelete) {
                    console.raiseError(
                        SqlState::SYNTAX_ERROR, "DELETE", tableName, "",
                        "unconditional DELETE without WHERE clause is blocked by deterministic runtime policy",
                        "Add a .filter(...) clause or use TRUNCATE TABLE."
                    );
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                AccessPathPlanner::chooseBestAccessPath(tableName, whereFilters, lastCost);

                const auto& usingRows = usingAuxTable.isEmpty() ? std::vector<Storage::PgTuple>{} : storage.getTableRows(usingAuxTable);
                std::vector<Storage::PgTuple> keptRows;
                std::vector<Storage::PgTuple> toDelete;
                uint16_t examined = 0;

                storage.forEachRowStreaming(tableName, [&](const Storage::PgTuple& r, uint32_t) -> bool {
                    examined++;
                    bool matches = false;
                    if (!usingAuxTable.isEmpty()) {
                        for (const auto& ur : usingRows) {
                            Storage::PgTuple joined = r;
                            for (const auto& uc : ur.allCells()) {
                                joined.set(usingAuxTable + "." + String(uc.columnName), uc.rawText, uc.type);
                            }
                            if (ModuloDQL::evaluatePredicateChain(joined, whereFilters)) {
                                matches = true;
                                break;
                            }
                        }
                    } else {
                        matches = ModuloDQL::evaluatePredicateChain(r, whereFilters);
                    }

                    if (matches) {
                        toDelete.push_back(r);
                    } else {
                        keptRows.push_back(r);
                    }
                    return true;
                });

                lastCost.rowsExamined = examined;
                std::vector<String> childTables = cat.findDependentTables(tableName);

                bool hasCascadingActions = false;
                for (const auto& delRow : toDelete) {
                    for (const auto& childName : childTables) {
                        const auto* childSchema = cat.getTable(childName);
                        if (!childSchema) continue;
                        for (const auto& childCol : childSchema->columns) {
                            if (!childCol.isForeignKey || !childCol.referencedTable.equalsIgnoreCase(tableName)) continue;

                            if (childCol.onDelete != DbNotRuntime::Standard::ReferentialAction::RESTRICT &&
                                childCol.onDelete != DbNotRuntime::Standard::ReferentialAction::NO_ACTION) {
                                hasCascadingActions = true;
                                continue;
                            }

                            String parentKeyVal = delRow.getText(childCol.referencedColumn.c_str());
                            PredicateCondition fkCond;
                            fkCond.columnOrExpr = childCol.columnName;
                            fkCond.op           = Standard::FilterOp::EQ;
                            fkCond.valueA       = parentKeyVal;

                            const auto* childIdx = storage.findBestIndexForColumn(childName, childCol.columnName);
                            bool hasDependentRow = false;
                            if (childIdx) {
                                hasDependentRow = !childIdx->lookupEqual(parentKeyVal).empty();
                            } else {
                                for (const auto& cr : storage.getTableRows(childName)) {
                                    if (ModuloDQL::evaluateCondition(cr, fkCond)) { hasDependentRow = true; break; }
                                }
                            }
                            if (hasDependentRow) {
                                console.raiseError(
                                    SqlState::FOREIGN_KEY_VIOLATION, "DELETE", tableName, childCol.referencedColumn,
                                    "update or delete on table \"" + tableName + "\" violates foreign key constraint \""
                                    + childCol.fkConstraintName + "\" on table \"" + childName + "\"",
                                    "Key (" + childCol.referencedColumn + ")=(" + parentKeyVal + ") is still referenced from table \"" + childName + "\".",
                                    childCol.fkConstraintName
                                );
                                probe.finish(); result.cost = lastCost;
                                return result;
                            }
                        }
                    }
                }

                std::map<String, std::vector<Storage::PgTuple>> snapshot;
                if (hasCascadingActions) {
                    for (const auto& childName : childTables) snapshotTableGraph(childName, tableName, snapshot);
                }

                bool phase2Failed = false;
                for (const auto& delRow : toDelete) {
                    for (const auto& childName : childTables) {
                        const auto* childSchema = cat.getTable(childName);
                        if (!childSchema) continue;
                        for (const auto& childCol : childSchema->columns) {
                            if (!childCol.isForeignKey || !childCol.referencedTable.equalsIgnoreCase(tableName)) continue;

                            PredicateCondition fkCond;
                            fkCond.columnOrExpr = childCol.columnName;
                            fkCond.op           = Standard::FilterOp::EQ;
                            fkCond.valueA       = delRow.getText(childCol.referencedColumn.c_str());

                            if (childCol.onDelete == DbNotRuntime::Standard::ReferentialAction::CASCADE) {
                                if (!deleteRows(childName, {fkCond}, {}, true).success) phase2Failed = true;
                            } else if (childCol.onDelete == DbNotRuntime::Standard::ReferentialAction::SET_NULL) {
                                if (!updateRows(childName, {Storage::PgCell(childCol.columnName, "NULL", childCol.dataType)}, {fkCond}, {}, true).success) phase2Failed = true;
                            } else if (childCol.onDelete == DbNotRuntime::Standard::ReferentialAction::SET_DEFAULT) {
                                if (!updateRows(childName, {Storage::PgCell(childCol.columnName, childCol.defaultValue, childCol.dataType)}, {fkCond}, {}, true).success) phase2Failed = true;
                            }
                            if (phase2Failed) break;
                        }
                        if (phase2Failed) break;
                    }
                    if (phase2Failed) break;

                    result.affectedRows++;
                    if (!returningColumns.empty()) {
                        result.returningRows.push_back(projectReturning(delRow, returningColumns));
                    }
                }

                if (phase2Failed) {
                    restoreSnapshot(snapshot);
                    result.affectedRows = 0;
                    result.returningRows.clear();
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                if (!toDelete.empty()) {
                    if (!storage.replaceTableRows(tableName, keptRows)) {
                        restoreSnapshot(snapshot);
                        result.affectedRows = 0;
                        result.returningRows.clear();
                        console.raiseError(SqlState::IO_ERROR, "DELETE", tableName, "",
                                           "could not persist table \"" + tableName + "\"; DELETE rolled back");
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }
                }

                lastCost.rowsProduced = result.affectedRows;
                result.success = true;
                console.reportSuccess("DELETE", tableName, String(result.affectedRows) + " rows deleted");
            }
            result.cost = lastCost;
            return result;
        }

        DmlExecutionResult mergeRows(const String& targetTable,
                                     const std::vector<Storage::PgTuple>& sourceRows,
                                     const String& joinKeyTargetCol,
                                     const String& joinKeySourceCol,
                                     const std::vector<MergeWhenClause>& whenClauses) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult result;
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();

            {
                Telemetry::ScopedCostProbe probe(lastCost, "MERGE", targetTable);
                if (!cat.hasTable(targetTable)) {
                    console.raiseError(SqlState::UNDEFINED_TABLE, "MERGE", targetTable, "", "relation \"" + targetTable + "\" does not exist");
                    probe.finish(); result.cost = lastCost;
                    return result;
                }

                for (const auto& srcRow : sourceRows) {
                    String keyVal = srcRow.getText(joinKeySourceCol.c_str());
                    PredicateCondition matchCond;
                    matchCond.columnOrExpr = joinKeyTargetCol;
                    matchCond.op           = Standard::FilterOp::EQ;
                    matchCond.valueA       = keyVal;

                    ChosenAccessPlan plan = AccessPathPlanner::chooseBestAccessPath(targetTable, {matchCond}, lastCost);
                    bool matched = false;
                    if (plan.indexPtr) {
                        matched = !plan.indexPtr->lookupEqual(keyVal).empty();
                    } else {
                        for (const auto& tr : Storage::RuntimeStorageAndIndexManager::getInstance().getTableRows(targetTable)) {
                            if (ModuloDQL::evaluateCondition(tr, matchCond)) { matched = true; break; }
                        }
                    }

                    for (const auto& clause : whenClauses) {
                        if (clause.isMatched && matched) {
                            if (clause.action == Standard::MergeActionType::UPDATE_SET) {
                                auto r = updateRows(targetTable, clause.assignmentsOrValues, {matchCond});
                                if (!r.success) { probe.finish(); result.cost = lastCost; return result; }
                                result.affectedRows += r.affectedRows;
                            } else if (clause.action == Standard::MergeActionType::DELETE_ROW) {
                                auto r = deleteRows(targetTable, {matchCond});
                                if (!r.success) { probe.finish(); result.cost = lastCost; return result; }
                                result.affectedRows += r.affectedRows;
                            }
                            break;
                        } else if (!clause.isMatched && !matched) {
                            if (clause.action == Standard::MergeActionType::INSERT_VALUES) {
                                Storage::PgTuple toInsert = srcRow;
                                for (const auto& c : clause.assignmentsOrValues) {
                                    toInsert.set(c.columnName, c.rawText, c.type);
                                }
                                auto r = insertRow(targetTable, toInsert);
                                if (!r.success) { probe.finish(); result.cost = lastCost; return result; }
                                result.affectedRows += r.affectedRows;
                            }
                            break;
                        }
                    }
                }
                lastCost.rowsProduced = result.affectedRows;
                result.success = true;
                console.reportSuccess("MERGE", targetTable, String(result.affectedRows) + " rows merged");
            }
            result.cost = lastCost;
            return result;
        }

        DmlExecutionResult truncateTable(const String& tableName,
                                         DbNotRuntime::Standard::ReferentialAction behavior = DbNotRuntime::Standard::ReferentialAction::RESTRICT) {
            auto dbLock = Storage::RuntimeStorageAndIndexManager::lockEngine();
            DmlExecutionResult res;
            Telemetry::ScopedCostProbe probe(lastCost, "TRUNCATE", tableName);

            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();

            if (!cat.hasTable(tableName)) {
                console.raiseError(SqlState::UNDEFINED_TABLE, "TRUNCATE TABLE", tableName, "",
                                   "table \"" + tableName + "\" does not exist");
                probe.finish(); res.cost = lastCost;
                return res;
            }

            std::vector<String> dependents = cat.findDependentTables(tableName);
            if (!dependents.empty() && behavior != DbNotRuntime::Standard::ReferentialAction::CASCADE) {
                console.raiseError(SqlState::FEATURE_NOT_SUPPORTED, "TRUNCATE TABLE", tableName, "",
                                   "cannot truncate a table referenced in a foreign key constraint",
                                   "Truncate table \"" + dependents[0] + "\" at the same time, or use TRUNCATE ... CASCADE.");
                probe.finish(); res.cost = lastCost;
                return res;
            }

            uint16_t countBefore = static_cast<uint16_t>(storage.getTableRows(tableName).size());
            storage.replaceTableRows(tableName, {});

            if (behavior == DbNotRuntime::Standard::ReferentialAction::CASCADE) {
                for (const auto& dep : dependents) {
                    countBefore += static_cast<uint16_t>(storage.getTableRows(dep).size());
                    storage.replaceTableRows(dep, {});
                }
            }

            res.success           = true;
            res.affectedRows      = countBefore;
            lastCost.rowsProduced = countBefore;
            console.reportSuccess("TRUNCATE TABLE", tableName);
            probe.finish(); res.cost = lastCost;
            return res;
        }
    };

} // namespace DbRuntime::Engine