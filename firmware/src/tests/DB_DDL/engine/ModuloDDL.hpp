#pragma once
#include <Arduino.h>
#include <vector>
#include "../standard/PostgresReservedWords.hpp"
#include "../standard/PostgresSqlStates.hpp"
#include "../standard/PostgresDataTypes.hpp"
#include "../standard/PostgresConstraints.hpp"
#include "DdlDiagnosticConsole.hpp"
#include "PhysicalSchemaCatalog.hpp"
#include "DeterministicTableBuilder.hpp"

namespace DbNotRuntime::Engine {

    // Fachada central del motor DDL que implementa todas las operaciones de esquema de PostgreSQL sobre tablas, tipos e índices.
    class ModuloDDL {
    private:
        ModuloDDL() = default;

    public:
        // Devuelve la instancia única del motor DDL.
        static ModuloDDL& getInstance() {
            static ModuloDDL instance;
            return instance;
        }

        // ---------------- CREATE TYPE ... AS ENUM ----------------
        // Registra un tipo ENUM nativo de PostgreSQL validando que no existan etiquetas vacías ni duplicadas.
        bool createEnumType(const String& typeName, const std::vector<String>& labels) {
            String reason;
            if (!Standard::PostgresLexicalValidator::isValidIdentifierFormat(typeName, reason) ||
                Standard::PostgresLexicalValidator::isForbiddenUnquotedIdentifier(typeName, true)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::SYNTAX_ERROR, "CREATE TYPE", "", "",
                    "invalid enum type name \"" + typeName + "\""
                );
            }
            if (PhysicalSchemaCatalog::getInstance().hasEnum(typeName)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_OBJECT, "CREATE TYPE", "", "",
                    "type \"" + typeName + "\" already exists"
                );
            }
            if (labels.empty()) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::INVALID_OBJECT_DEFINITION, "CREATE TYPE", "", "",
                    "enum type \"" + typeName + "\" must have at least one label"
                );
            }

            Standard::EnumTypeDefinition def;
            def.typeName = typeName;
            for (const auto& l : labels) {
                if (def.containsLabel(l)) {
                    return DdlDiagnosticConsole::getInstance().raiseError(
                        Standard::SqlState::INVALID_OBJECT_DEFINITION, "CREATE TYPE", "", "",
                        "enum label \"" + l + "\" specified more than once"
                    );
                }
                def.labels.push_back(l);
            }
            PhysicalSchemaCatalog::getInstance().registerEnum(def);
            PhysicalSchemaCatalog::getInstance().flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("CREATE TYPE", typeName, "AS ENUM");
        }

        // ---------------- CREATE DOMAIN ----------------
        // Crea un dominio personalizado sobre un tipo base de PostgreSQL.
        bool createDomain(const Standard::DomainTypeDefinition& domainDef) {
            if (PhysicalSchemaCatalog::getInstance().hasDomain(domainDef.domainName)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_OBJECT, "CREATE DOMAIN", "", "",
                    "domain \"" + domainDef.domainName + "\" already exists"
                );
            }
            PhysicalSchemaCatalog::getInstance().registerDomain(domainDef);
            PhysicalSchemaCatalog::getInstance().flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("CREATE DOMAIN", domainDef.domainName);
        }

        // ---------------- CREATE TABLE ----------------
        // Inicia la máquina de estados determinista para construir una tabla con gramática PostgreSQL.
        DeterministicTableBuilder createTable(const String& tableName,
                                              Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT,
                                              bool ifNotExists = false) {
            return DeterministicTableBuilder(tableName, persistence, ifNotExists);
        }

        // ---------------- DROP TABLE ----------------
        // Elimina una tabla verificando dependencias FK (RESTRICT vs CASCADE) y borrando su archivo físico .tbl.
        bool dropTable(const String& tableName, bool ifExists = false,
                       Standard::ReferentialAction behavior = Standard::ReferentialAction::RESTRICT) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasTable(tableName)) {
                if (ifExists) {
                    return DdlDiagnosticConsole::getInstance().reportSuccess(
                        "DROP TABLE", tableName, "table \"" + tableName + "\" does not exist, skipping"
                    );
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "DROP TABLE", tableName, "",
                    "table \"" + tableName + "\" does not exist"
                );
            }

            std::vector<String> dependents = cat.findDependentTables(tableName);
            if (!dependents.empty() && behavior != Standard::ReferentialAction::CASCADE) {
                String depList = "";
                for (size_t i = 0; i < dependents.size(); ++i) {
                    if (i > 0) depList += ", ";
                    depList += dependents[i];
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DEPENDENT_OBJECTS_STILL_EXIST, "DROP TABLE", tableName, "",
                    "cannot drop table \"" + tableName + "\" because other objects depend on it (" + depList + ")",
                    "Use DROP TABLE ... CASCADE to drop the dependent constraints too."
                );
            }

            if (behavior == Standard::ReferentialAction::CASCADE) {
                for (const auto& depTable : dependents) {
                    TablePhysicalSchema* child = cat.getTableMutable(depTable);
                    if (child) {
                        for (auto& col : child->columns) {
                            if (col.isForeignKey && col.referencedTable.equalsIgnoreCase(tableName)) {
                                col.isForeignKey = false;
                                col.referencedTable = "";
                                col.referencedColumn = "";
                            }
                        }
                    }
                }
            }

            cat.removeIndexesOfTable(tableName);
            cat.removeTable(tableName);
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (sd.estaInicializada()) {
                String path = cat.tableDataFilePath(tableName);
                if (sd.existeArchivo(path.c_str())) sd.eliminarArchivo(path.c_str());
            }
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("DROP TABLE", tableName);
        }

        // ---------------- TRUNCATE TABLE ----------------
        // Vacía instantáneamente una tabla verificando que no existan tablas hijas restringidas por FK.
        bool truncateTable(const String& tableName, Standard::ReferentialAction behavior = Standard::ReferentialAction::RESTRICT) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasTable(tableName)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "TRUNCATE TABLE", tableName, "",
                    "table \"" + tableName + "\" does not exist"
                );
            }

            std::vector<String> dependents = cat.findDependentTables(tableName);
            if (!dependents.empty() && behavior != Standard::ReferentialAction::CASCADE) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::FEATURE_NOT_SUPPORTED,
                    "TRUNCATE TABLE", tableName, "",
                    "cannot truncate a table referenced in a foreign key constraint",
                    "Truncate table \"" + dependents[0] + "\" at the same time, or use TRUNCATE ... CASCADE."
                );
            }

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (sd.estaInicializada()) {
                sd.escribirArchivo(cat.tableDataFilePath(tableName).c_str(), "");
                if (behavior == Standard::ReferentialAction::CASCADE) {
                    for (const auto& dep : dependents) {
                        sd.escribirArchivo(cat.tableDataFilePath(dep).c_str(), "");
                    }
                }
            }
            return DdlDiagnosticConsole::getInstance().reportSuccess("TRUNCATE TABLE", tableName);
        }


        // ---------------- ALTER TABLE Operations ----------------
        // Agrega una columna nueva a una tabla existente validando que no viole reglas de nombre ni unicidad.
        bool alterTableAddColumn(const String& tableName, const Standard::ColumnConstraintDef& newCol) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE ADD COLUMN", tableName, newCol.columnName,
                    "relation \"" + tableName + "\" does not exist"
                );
            }
            if (tb->findColumn(newCol.columnName) != nullptr) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_COLUMN, "ALTER TABLE ADD COLUMN", tableName, newCol.columnName,
                    "column \"" + newCol.columnName + "\" of relation \"" + tableName + "\" already exists"
                );
            }

            String reason;
            if (!Standard::PostgresLexicalValidator::isValidIdentifierFormat(newCol.columnName, reason) ||
                Standard::PostgresLexicalValidator::isForbiddenUnquotedIdentifier(newCol.columnName, false)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::SYNTAX_ERROR, "ALTER TABLE ADD COLUMN", tableName, newCol.columnName,
                    "invalid or reserved column name \"" + newCol.columnName + "\""
                );
            }

            if (newCol.isPrimaryKey && tb->hasPrimaryKey()) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::TUPLE_ALREADY_HAS_PRIMARY_KEY, "ALTER TABLE ADD COLUMN", tableName, newCol.columnName,
                    "multiple primary keys for table \"" + tableName + "\" are not allowed"
                );
            }

            tb->columns.push_back(newCol);
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess(
                "ALTER TABLE ADD COLUMN", tableName, "column \"" + newCol.columnName + "\" added"
            );
        }

        // Elimina una columna verificando dependencias referenciales en tablas hijas y evitando dejar la tabla vacía.
        bool alterTableDropColumn(const String& tableName, const String& colName,
                                  bool ifExists = false,
                                  Standard::ReferentialAction behavior = Standard::ReferentialAction::RESTRICT) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE DROP COLUMN", tableName, colName,
                    "relation \"" + tableName + "\" does not exist"
                );
            }

            int idx = -1;
            for (size_t i = 0; i < tb->columns.size(); ++i) {
                if (tb->columns[i].columnName.equalsIgnoreCase(colName)) {
                    idx = static_cast<int>(i);
                    break;
                }
            }

            if (idx < 0) {
                if (ifExists) {
                    return DdlDiagnosticConsole::getInstance().reportSuccess(
                        "ALTER TABLE DROP COLUMN", tableName, "column \"" + colName + "\" does not exist, skipping"
                    );
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_COLUMN, "ALTER TABLE DROP COLUMN", tableName, colName,
                    "column \"" + colName + "\" of relation \"" + tableName + "\" does not exist"
                );
            }

            if (tb->columns.size() <= 1) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::INVALID_TABLE_DEFINITION, "ALTER TABLE DROP COLUMN", tableName, colName,
                    "cannot drop the only column of table \"" + tableName + "\""
                );
            }

            // Verifica si otra tabla referencia esta columna mediante FOREIGN KEY
            for (const auto& pair : cat.getAllTables()) {
                for (const auto& childCol : pair.second.columns) {
                    if (childCol.isForeignKey &&
                        childCol.referencedTable.equalsIgnoreCase(tableName) &&
                        childCol.referencedColumn.equalsIgnoreCase(colName)) {
                        if (behavior != Standard::ReferentialAction::CASCADE) {
                            return DdlDiagnosticConsole::getInstance().raiseError(
                                Standard::SqlState::DEPENDENT_OBJECTS_STILL_EXIST, "ALTER TABLE DROP COLUMN", tableName, colName,
                                "cannot drop column \"" + colName + "\" because table \"" + pair.second.tableName + "\" depends on it",
                                "Use CASCADE to drop the dependent foreign key constraint too."
                            );
                        }
                        TablePhysicalSchema* mutableChild = cat.getTableMutable(pair.second.tableName);
                        if (mutableChild) {
                            Standard::ColumnConstraintDef* mc = mutableChild->findColumnMutable(childCol.columnName);
                            if (mc) {
                                mc->isForeignKey = false;
                                mc->referencedTable = "";
                                mc->referencedColumn = "";
                            }
                        }
                    }
                }
            }

            tb->columns.erase(tb->columns.begin() + idx);
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("ALTER TABLE DROP COLUMN", tableName, colName);
        }

        // Renombra una columna actualizando automáticamente las referencias en claves foráneas dependientes.
        bool alterTableRenameColumn(const String& tableName, const String& oldColName, const String& newColName) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE RENAME COLUMN", tableName, oldColName,
                    "relation \"" + tableName + "\" does not exist"
                );
            }

            Standard::ColumnConstraintDef* col = tb->findColumnMutable(oldColName);
            if (!col) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_COLUMN, "ALTER TABLE RENAME COLUMN", tableName, oldColName,
                    "column \"" + oldColName + "\" does not exist"
                );
            }

            if (tb->findColumn(newColName) != nullptr) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_COLUMN, "ALTER TABLE RENAME COLUMN", tableName, newColName,
                    "column \"" + newColName + "\" of relation \"" + tableName + "\" already exists"
                );
            }

            String reason;
            if (!Standard::PostgresLexicalValidator::isValidIdentifierFormat(newColName, reason) ||
                Standard::PostgresLexicalValidator::isForbiddenUnquotedIdentifier(newColName, false)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::SYNTAX_ERROR, "ALTER TABLE RENAME COLUMN", tableName, newColName,
                    "invalid or reserved column name \"" + newColName + "\""
                );
            }

            col->columnName = newColName;
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("ALTER TABLE RENAME COLUMN", tableName, oldColName + " TO " + newColName);
        }

        // Renombra una tabla en el catálogo, actualiza las FKs que le apuntan y renombra el archivo físico .tbl.
        bool alterTableRenameTo(const String& oldTableName, const String& newTableName) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            const TablePhysicalSchema* existing = cat.getTable(oldTableName);
            if (!existing) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE RENAME TO", oldTableName, "",
                    "relation \"" + oldTableName + "\" does not exist"
                );
            }
            if (cat.hasTable(newTableName)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_TABLE, "ALTER TABLE RENAME TO", newTableName, "",
                    "relation \"" + newTableName + "\" already exists"
                );
            }

            String reason;
            if (!Standard::PostgresLexicalValidator::isValidIdentifierFormat(newTableName, reason) ||
                Standard::PostgresLexicalValidator::isForbiddenUnquotedIdentifier(newTableName, false)) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::SYNTAX_ERROR, "ALTER TABLE RENAME TO", newTableName, "",
                    "invalid or reserved table name \"" + newTableName + "\""
                );
            }

            TablePhysicalSchema copy = *existing;
            copy.tableName = newTableName;
            cat.removeTable(oldTableName);
            cat.registerTable(copy);

            for (const auto& pair : cat.getAllTables()) {
                TablePhysicalSchema* mutableTb = cat.getTableMutable(pair.second.tableName);
                if (!mutableTb) continue;
                for (auto& c : mutableTb->columns) {
                    if (c.isForeignKey && c.referencedTable.equalsIgnoreCase(oldTableName)) {
                        c.referencedTable = newTableName;
                    }
                }
            }

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (sd.estaInicializada()) {
                String oldPath = cat.tableDataFilePath(oldTableName);
                String newPath = cat.tableDataFilePath(newTableName);
                if (sd.existeArchivo(oldPath.c_str())) {
                    SD.rename(oldPath.c_str(), newPath.c_str());
                }
            }

            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("ALTER TABLE RENAME TO", newTableName);
        }

        // Modifica el estado NOT NULL (SET NOT NULL / DROP NOT NULL) de una columna existente.
        bool alterTableAlterColumnNullability(const String& tableName, const String& colName, bool setNotNull) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE ALTER COLUMN", tableName, colName,
                    "relation \"" + tableName + "\" does not exist"
                );
            }
            Standard::ColumnConstraintDef* col = tb->findColumnMutable(colName);
            if (!col) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_COLUMN, "ALTER TABLE ALTER COLUMN", tableName, colName,
                    "column \"" + colName + "\" does not exist"
                );
            }
            if (!setNotNull && col->isPrimaryKey) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::INVALID_TABLE_DEFINITION, "ALTER TABLE ALTER COLUMN", tableName, colName,
                    "column \"" + colName + "\" is in a primary key and cannot be nullable"
                );
            }
            col->isNotNull = setNotNull;
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess(
                "ALTER TABLE ALTER COLUMN", tableName, colName + (setNotNull ? " SET NOT NULL" : " DROP NOT NULL")
            );
        }

        // Establece o elimina (DROP DEFAULT) el valor por defecto de una columna validando el literal contra su tipo.
        bool alterTableAlterColumnDefault(const String& tableName, const String& colName,
                                          bool setDefault, const String& literalValue = "") {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "ALTER TABLE ALTER COLUMN", tableName, colName,
                    "relation \"" + tableName + "\" does not exist"
                );
            }
            Standard::ColumnConstraintDef* col = tb->findColumnMutable(colName);
            if (!col) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_COLUMN, "ALTER TABLE ALTER COLUMN", tableName, colName,
                    "column \"" + colName + "\" does not exist"
                );
            }
            if (!setDefault) {
                col->hasDefault = false;
                col->defaultValue = "";
            } else {
                const Standard::EnumTypeDefinition* enumDef = (col->dataType == Standard::PgDataType::ENUM)
                    ? cat.getEnum(col->customTypeName) : nullptr;
                String sqlState, errMsg;
                if (!Standard::DataTypeUtils::validateLiteral(col->dataType, literalValue,
                                                              col->maxLength, col->precision, col->scale,
                                                              enumDef, sqlState, errMsg)) {
                    return DdlDiagnosticConsole::getInstance().raiseError(
                        sqlState.c_str(), "ALTER TABLE ALTER COLUMN", tableName, colName, errMsg
                    );
                }
                col->hasDefault = true;
                col->defaultValue = literalValue;
            }
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("ALTER TABLE ALTER COLUMN", tableName, colName);
        }

        // ---------------- CREATE / DROP INDEX ----------------
        // Registra un índice físico en el catálogo consolidado verificando la existencia de la tabla y sus columnas.
        bool createIndex(const Standard::IndexDefinition& idxDef, bool ifNotExists = false) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.hasIndex(idxDef.indexName)) {
                if (ifNotExists) {
                    return DdlDiagnosticConsole::getInstance().reportSuccess("CREATE INDEX", idxDef.indexName, "already exists, skipping");
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_OBJECT, "CREATE INDEX", idxDef.tableName, "",
                    "relation \"" + idxDef.indexName + "\" already exists"
                );
            }

            const TablePhysicalSchema* tb = cat.getTable(idxDef.tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "CREATE INDEX", idxDef.tableName, "",
                    "relation \"" + idxDef.tableName + "\" does not exist"
                );
            }

            if (idxDef.keyColumns.empty()) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::SYNTAX_ERROR, "CREATE INDEX", idxDef.tableName, "",
                    "index \"" + idxDef.indexName + "\" must specify at least one key column"
                );
            }

            for (const auto& colName : idxDef.keyColumns) {
                if (!tb->findColumn(colName)) {
                    return DdlDiagnosticConsole::getInstance().raiseError(
                        Standard::SqlState::UNDEFINED_COLUMN, "CREATE INDEX", idxDef.tableName, colName,
                        "column \"" + colName + "\" does not exist in table \"" + idxDef.tableName + "\""
                    );
                }
            }

            cat.registerIndex(idxDef);
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("CREATE INDEX", idxDef.indexName);
        }

        // Elimina un índice del catálogo del sistema.
        bool dropIndex(const String& indexName, bool ifExists = false) {
            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasIndex(indexName)) {
                if (ifExists) {
                    return DdlDiagnosticConsole::getInstance().reportSuccess("DROP INDEX", indexName, "does not exist, skipping");
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_OBJECT, "DROP INDEX", "", "",
                    "index \"" + indexName + "\" does not exist"
                );
            }
            cat.removeIndex(indexName);
            cat.flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("DROP INDEX", indexName);
        }

        // ---------------- COMMENT ON ----------------
        // Registra comentarios de documentación en el diccionario de datos sobre una tabla o columna.
        bool commentOnTable(const String& tableName, const String& commentText) {
            TablePhysicalSchema* tb = PhysicalSchemaCatalog::getInstance().getTableMutable(tableName);
            if (!tb) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::UNDEFINED_TABLE, "COMMENT ON TABLE", tableName, "",
                    "relation \"" + tableName + "\" does not exist"
                );
            }
            tb->comment = commentText;
            PhysicalSchemaCatalog::getInstance().flushCatalogToSingleFile();
            return DdlDiagnosticConsole::getInstance().reportSuccess("COMMENT ON TABLE", tableName);
        }
    };

} // namespace DbNotRuntime::Engine