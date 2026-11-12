#pragma once
#include <Arduino.h>
#include <vector>
#include "../../DB_DDL/engine/DdlDiagnosticConsole.hpp"
#include "../../DB_DDL/engine/PhysicalSchemaCatalog.hpp"
#include "../telemetry/RuntimeCostProfiler.hpp"
#include "../standard/PostgresDmlDqlKeywords.hpp"
#include "../storage/RuntimeIndexAccessPath.hpp"
#include "ModuloDQL.hpp"

namespace DbRuntime::Engine {

    using DbNotRuntime::Standard::ReferentialAction;
    using DbNotRuntime::Standard::PgDataType;
    using DbNotRuntime::Standard::SqlState;

    // Resultado de ejecución de cualquier operación DML con soporte nativo para RETURNING y telemetría de costo.
    struct DmlExecutionResult {
        bool success = false;
        uint16_t affectedRows = 0;
        std::vector<Storage::PgTuple> returningRows;
        Telemetry::OperationCostMetrics cost;

        explicit operator bool() const { return success; }
    };

    // Especificación de cláusula ON CONFLICT (Upsert de PostgreSQL).
    struct OnConflictClause {
        Standard::ConflictAction action = Standard::ConflictAction::NONE;
        std::vector<String> conflictTargetColumns;
        std::vector<Storage::PgCell> updateAssignments;
        std::vector<PredicateCondition> whereCondition;
    };

    // Regla individual para la sentencia estándar MERGE INTO.
    struct MergeWhenClause {
        bool isMatched = true; // true = WHEN MATCHED, false = WHEN NOT MATCHED
        Standard::MergeActionType action = Standard::MergeActionType::DO_NOTHING;
        std::vector<Storage::PgCell> assignmentsOrValues;
    };

    class ModuloDML {
    private:
        Telemetry::OperationCostMetrics lastCost;

        ModuloDML() = default;

        // Valida que una tupla candidata cumpla todos los tipos, NOT NULL, CHECK y FOREIGN KEY del esquema DDL.
        static bool validateCandidateTuple(const DbNotRuntime::Engine::TablePhysicalSchema& schema,
                                           Storage::PgTuple& candidate,
                                           const char* commandName,
                                           bool isInsert) {
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            auto& console = DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance();

            // 1. Verifica que no se hayan enviado columnas inexistentes en la tabla (SQLSTATE 42703)
            for (const auto& cell : candidate.allCells()) {
                if (!schema.findColumn(cell.columnName)) {
                    return console.raiseError(
                        SqlState::UNDEFINED_COLUMN, commandName, schema.tableName, cell.columnName,
                        "column \"" + String(cell.columnName) + "\" of relation \"" + schema.tableName + "\" does not exist"
                    );
                }
            }

            // 2. Completa secuencias IDENTITY, valores DEFAULT y verifica restricciones columna por columna
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
                        // Reserva el slot en orden sin incrementar la secuencia hasta confirmar la inserción
                        candidate.set(colDef.columnName, "NULL", colDef.dataType);
                        continue;
                    }
                }

                if (!wasProvided) {
                    if (colDef.hasDefault) {
                        candidate.set(colDef.columnName, colDef.defaultValue, colDef.dataType);
                    } else {
                        candidate.set(colDef.columnName, "NULL", colDef.dataType);
                    }
                    cell = candidate.findCellMutable(colDef.columnName);
                } else {
                    // Asegura que la celda tenga sincronizada la unión numérica con el tipo declarado en el DDL
                    cell->setValue(cell->rawText, colDef.dataType);
                }

                // Validación NOT NULL (SQLSTATE 23502)
                if (colDef.isNotNull && (!cell || cell->isNull)) {
                    return console.raiseError(
                        SqlState::NOT_NULL_VIOLATION, commandName, schema.tableName, colDef.columnName,
                        "null value in column \"" + colDef.columnName + "\" of relation \"" + schema.tableName + "\" violates not-null constraint"
                    );
                }

                // Validación de tipo de dato, longitud máxima, precisión/escala y etiquetas ENUM (Clase 22)
                if (cell && !cell->isNull) {
                    const DbNotRuntime::Standard::EnumTypeDefinition* enumDef =
                        (colDef.dataType == PgDataType::ENUM) ? cat.getEnum(colDef.customTypeName) : nullptr;
                    String stateOut, msgOut;
                    if (!DbNotRuntime::Standard::DataTypeUtils::validateLiteral(
                            colDef.dataType, cell->rawText, colDef.maxLength, colDef.precision, colDef.scale,
                            enumDef, stateOut, msgOut)) {
                        return console.raiseError(
                            stateOut.c_str(), commandName, schema.tableName, colDef.columnName, msgOut
                        );
                    }
                }

                // Validación de predicado CHECK (SQLSTATE 23514)
                if (colDef.hasCheck && colDef.checkFn && cell && !cell->isNull) {
                    if (!colDef.checkFn(cell->rawText)) {
                        return console.raiseError(
                            SqlState::CHECK_VIOLATION, commandName, schema.tableName, colDef.columnName,
                            "new row for relation \"" + schema.tableName + "\" violates check constraint \"" + colDef.checkConstraintName + "\"",
                            "", colDef.checkConstraintName
                        );
                    }
                }

                // Validación de integridad referencial FOREIGN KEY hacia la tabla padre usando su índice (SQLSTATE 23503)
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

        // Busca si una tupla colisiona con algún índice UNIQUE o PRIMARY KEY devolviendo la posición de la fila en conflicto.
        static int32_t findUniqueConflictRowIndex(const String& tableName,
                                                  const Storage::PgTuple& candidate,
                                                  int32_t ignoreRowIndex,
                                                  String& outIndexName) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            for (const auto& idx : storage.getIndexesOfTable(tableName)) {
                if (!idx.isUnique && !idx.isPrimary) continue;
                String key = idx.buildCompositeKey(candidate);
                if (key.isEmpty()) continue;
                std::vector<uint32_t> matches = idx.lookupEqual(key);
                for (uint32_t pos : matches) {
                    if (static_cast<int32_t>(pos) != ignoreRowIndex) {
                        outIndexName = idx.indexName;
                        return static_cast<int32_t>(pos);
                    }
                }
            }
            return -1;
        }

        // Proyecta únicamente las columnas pedidas en la cláusula RETURNING.
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


        // Copia las filas de una tabla y, recursivamente, de sus dependientes, para poder revertir.
        static void snapshotTableGraph(const String& tableName, const String& skipTable,
                                       std::map<String, std::vector<Storage::PgTuple>>& snap) {
            if (tableName.equalsIgnoreCase(skipTable) || snap.find(tableName) != snap.end()) return;
            auto& cat     = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            snap[tableName] = storage.getTableRows(tableName);
            for (const auto& dep : cat.findDependentTables(tableName)) snapshotTableGraph(dep, skipTable, snap);
        }

        // Restaura las filas del snapshot, reconstruye índices y persiste (no emite diagnósticos).
        static void restoreSnapshot(const std::map<String, std::vector<Storage::PgTuple>>& snap) {
            auto& storage = Storage::RuntimeStorageAndIndexManager::getInstance();
            for (const auto& kv : snap) {
                storage.getTableRowsMutable(kv.first) = kv.second;
                storage.rebuildIndexesForTable(kv.first);
                storage.persistTableToSd(kv.first);
            }
        }

    public:
        // Devuelve la instancia única del motor DML.
        static ModuloDML& getInstance() {
            static ModuloDML instance;
            return instance;
        }

        const Telemetry::OperationCostMetrics& getLastCost() const { return lastCost; }

        // ---------------- INSERT INTO ... [ON CONFLICT ...] [RETURNING ...] ----------------
        // Inserta una fila validando tipos, constraints, gestionando conflictos Upsert y proyectando RETURNING.
        DmlExecutionResult insertRow(const String& tableName,
                                     Storage::PgTuple inputRow,
                                     const OnConflictClause& onConflict = {},
                                     const std::vector<String>& returningColumns = {}) {
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
                int32_t conflictPos = findUniqueConflictRowIndex(tableName, inputRow, -1, conflictIdxName);
                lastCost.scanType = Telemetry::AccessScanType::UNIQUE_INDEX_LOOKUP;
                strncpy(lastCost.indexUsed, conflictIdxName.c_str(), sizeof(lastCost.indexUsed) - 1);

                if (conflictPos >= 0) {
                    if (onConflict.action == Standard::ConflictAction::DO_NOTHING) {
                        result.success = true;
                        result.affectedRows = 0;
                        console.reportSuccess("INSERT 0 0", tableName, "ON CONFLICT DO NOTHING");
                    } else if (onConflict.action == Standard::ConflictAction::DO_UPDATE) {
                        auto& rows = storage.getTableRowsMutable(tableName);
                        Storage::PgTuple updated = rows[conflictPos];
                        if (ModuloDQL::evaluatePredicateChain(updated, onConflict.whereCondition)) {
                            for (const auto& assign : onConflict.updateAssignments) {
                                const auto* colDef = schema->findColumn(assign.columnName);
                                updated.set(assign.columnName, assign.rawText, colDef ? colDef->dataType : PgDataType::TEXT);
                            }
                            if (!validateCandidateTuple(*schema, updated, "INSERT ON CONFLICT DO UPDATE", false)) {
                                probe.finish(); result.cost = lastCost;
                                return result;
                            }
                            rows[conflictPos] = updated;
                            storage.rebuildIndexesForTable(tableName);
                            storage.persistTableToSd(tableName);
                            result.affectedRows = 1;
                            lastCost.rowsProduced = 1;
                            if (!returningColumns.empty()) {
                                result.returningRows.push_back(projectReturning(updated, returningColumns));
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
                    auto& rows = storage.getTableRowsMutable(tableName);
                    rows.push_back(inputRow);
                    storage.rebuildIndexesForTable(tableName);
                    storage.persistTableToSd(tableName);

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

        // ---------------- UPDATE ... SET ... [FROM ...] WHERE ... [RETURNING ...] ----------------
        // Actualiza filas determinísticamente exigiendo WHERE explícito (salvo allowUnconditional=true) y validando constraints.
        DmlExecutionResult updateRows(const String& tableName,
                                      const std::vector<Storage::PgCell>& assignments,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalUpdate = false,
                                      const String& fromAuxTable = "") {
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

                auto& rows = storage.getTableRowsMutable(tableName);
                ChosenAccessPlan accessPlan = AccessPathPlanner::chooseBestAccessPath(tableName, whereFilters, lastCost);

                std::vector<uint32_t> targetIndices;
                if (accessPlan.indexPtr != nullptr) {
                    targetIndices = accessPlan.indexPtr->lookupEqual(accessPlan.lookupValue);
                    lastCost.rowsExamined = static_cast<uint16_t>(targetIndices.size());
                } else {
                    lastCost.rowsExamined = static_cast<uint16_t>(rows.size());
                    for (uint32_t i = 0; i < rows.size(); ++i) targetIndices.push_back(i);
                }

                const auto& fromRows = storage.getTableRows(fromAuxTable);
                std::vector<std::pair<uint32_t, Storage::PgTuple>> stagedUpdates; // Fase 1: filas nuevas, sin tocar la tabla
                for (uint32_t idx : targetIndices) {
                    if (idx >= rows.size()) continue;
                    if (!fromAuxTable.isEmpty()) {
                        // UPDATE ... FROM: la fila se actualiza si existe alguna fila auxiliar que cumple el WHERE
                        bool matchedAux = false;
                        for (const auto& fr : fromRows) {
                            Storage::PgTuple joined = rows[idx];
                            for (const auto& fc : fr.allCells()) {
                                joined.set(fromAuxTable + "." + String(fc.columnName), fc.rawText, fc.type);
                            }
                            if (ModuloDQL::evaluatePredicateChain(joined, whereFilters)) { matchedAux = true; break; }
                        }
                        if (!matchedAux) continue;
                    } else if (!ModuloDQL::evaluatePredicateChain(rows[idx], whereFilters)) {
                        continue;
                    }

                    Storage::PgTuple candidate = rows[idx];
                    for (const auto& assign : assignments) {
                        const auto* colDef = schema->findColumn(assign.columnName);
                        if (!colDef) {
                            console.raiseError(
                                SqlState::UNDEFINED_COLUMN, "UPDATE", tableName, assign.columnName,
                                "column \"" + String(assign.columnName) + "\" of relation \"" + tableName + "\" does not exist"
                            );
                            probe.finish(); result.cost = lastCost;
                            return result;
                        }
                        candidate.set(assign.columnName, assign.rawText, colDef->dataType);
                    }

                    if (!validateCandidateTuple(*schema, candidate, "UPDATE", false)) {
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }

                    // Unicidad contra el estado actual de la tabla (excluye la propia fila)
                    String conflictIdx = "";
                    bool uniqueViolation =
                        findUniqueConflictRowIndex(tableName, candidate, static_cast<int32_t>(idx), conflictIdx) >= 0;

                    // Unicidad contra las filas ya preparadas en este mismo UPDATE (1 clave por índice)
                    if (!uniqueViolation && !stagedUpdates.empty()) {
                        for (const auto& ix : storage.getIndexesOfTable(tableName)) {
                            if (!ix.isUnique && !ix.isPrimary) continue;
                            String kNew = ix.buildCompositeKey(candidate);
                            if (kNew.isEmpty()) continue;
                            for (const auto& st : stagedUpdates) {
                                if (kNew == ix.buildCompositeKey(st.second)) {
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
                        probe.finish(); result.cost = lastCost;
                        return result;
                    }

                    stagedUpdates.emplace_back(idx, std::move(candidate));
                }

                // Fase 2: se aplica todo junto moviendo tuplas en RAM sin copias adicionales
                if (!stagedUpdates.empty()) {
                    std::vector<std::pair<uint32_t, Storage::PgTuple>> undo;
                    undo.reserve(stagedUpdates.size());
                    for (auto& st : stagedUpdates) {
                        undo.emplace_back(st.first, std::move(rows[st.first]));
                        rows[st.first] = std::move(st.second);
                        result.affectedRows++;
                        if (!returningColumns.empty()) {
                            result.returningRows.push_back(projectReturning(rows[st.first], returningColumns));
                        }
                    }
                    storage.rebuildIndexesForTable(tableName);
                    if (!storage.persistTableToSd(tableName)) {
                        for (auto& u : undo) rows[u.first] = std::move(u.second);
                        storage.rebuildIndexesForTable(tableName);
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

        // ---------------- DELETE FROM ... [USING ...] WHERE ... [RETURNING ...] ----------------
        // Elimina filas aplicando las políticas ON DELETE (RESTRICT, CASCADE, SET NULL, SET DEFAULT) en las tablas hijas.
        DmlExecutionResult deleteRows(const String& tableName,
                                      const std::vector<PredicateCondition>& whereFilters,
                                      const std::vector<String>& returningColumns = {},
                                      bool allowUnconditionalDelete = false,
                                      const String& usingAuxTable = "") {
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

                // Determina cuál es el camino de acceso para medir el plan en EXPLAIN
                AccessPathPlanner::chooseBestAccessPath(tableName, whereFilters, lastCost);

                auto& rows = storage.getTableRowsMutable(tableName);
                lastCost.rowsExamined = static_cast<uint16_t>(rows.size());
                const auto& usingRows = storage.getTableRows(usingAuxTable);
                
                std::vector<Storage::PgTuple> keptRows;
                std::vector<Storage::PgTuple> toDelete;

                for (const auto& r : rows) {
                    bool matches = false;
                    if (!usingAuxTable.isEmpty()) {
                        // DELETE ... USING: la fila se elimina si existe alguna fila auxiliar que cumpla el WHERE
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
                }

                                // Verifica acciones referenciales ON DELETE en todas las tablas hijas que apuntan a esta tabla
                std::vector<String> childTables = cat.findDependentTables(tableName);

                // FASE 1: solo validación (RESTRICT / NO ACTION). No modifica ninguna tabla.
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

                // FASE 2: efectos en hijas (CASCADE / SET NULL / SET DEFAULT) con copia para poder revertir
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
                    return result;   // el diagnóstico ya lo emitió la llamada anidada
                }

                // FASE 3: aplicar en la tabla principal (sin copia: se mueve y se restaura si falla la SD)
                if (!toDelete.empty()) {
                    std::vector<Storage::PgTuple> oldRows = std::move(rows);
                    rows = std::move(keptRows);
                    storage.rebuildIndexesForTable(tableName);
                    if (!storage.persistTableToSd(tableName)) {
                        rows = std::move(oldRows);
                        storage.rebuildIndexesForTable(tableName);
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

        // ---------------- MERGE INTO target USING source ON ... WHEN MATCHED / NOT MATCHED ----------------
        // Ejecuta la sentencia estándar MERGE combinando inserciones, actualizaciones o borrados en una sola pasada.
        DmlExecutionResult mergeRows(const String& targetTable,
                                     const std::vector<Storage::PgTuple>& sourceRows,
                                     const String& joinKeyTargetCol,
                                     const String& joinKeySourceCol,
                                     const std::vector<MergeWhenClause>& whenClauses) {
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

        // ---------------- TRUNCATE TABLE (100% Autónomo en Runtime, CERO dependencia de ModuloDDL) ----------------
        // Vacía la tabla en RAM, limpia sus índices y trunca el archivo .tbl verificando dependencias FK en el catálogo.
        DmlExecutionResult truncateTable(const String& tableName,
                                         DbNotRuntime::Standard::ReferentialAction behavior = DbNotRuntime::Standard::ReferentialAction::RESTRICT) {
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
            storage.getTableRowsMutable(tableName).clear();
            storage.rebuildIndexesForTable(tableName);
            storage.persistTableToSd(tableName);

            if (behavior == DbNotRuntime::Standard::ReferentialAction::CASCADE) {
                for (const auto& dep : dependents) {
                    countBefore += static_cast<uint16_t>(storage.getTableRows(dep).size());
                    storage.getTableRowsMutable(dep).clear();
                    storage.rebuildIndexesForTable(dep);
                    storage.persistTableToSd(dep);
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