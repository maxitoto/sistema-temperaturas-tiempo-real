#pragma once
#include <Arduino.h>
#include "../standard/PostgresReservedWords.hpp"
#include "../standard/PostgresSqlStates.hpp"
#include "../standard/PostgresDataTypes.hpp"
#include "../standard/PostgresConstraints.hpp"
#include "DdlDiagnosticConsole.hpp"
#include "PhysicalSchemaCatalog.hpp"

namespace DbNotRuntime::Engine {

    // Constructor determinista de columnas y tablas que prohíbe combinaciones ilegales en PostgreSQL.
    class DeterministicTableBuilder {
    private:
        TablePhysicalSchema draft;
        int currentColIdx = -1;
        bool hasAborted = false;
        bool ifNotExistsFlag = false;

        // Aborta la construcción de la tabla y emite el diagnóstico exacto de PostgreSQL.
        DeterministicTableBuilder& abortWith(const char* sqlState, const String& col, const String& msg, const String& hint = "") {
            if (!hasAborted) {
                hasAborted = true;
                DdlDiagnosticConsole::getInstance().raiseError(
                    sqlState, "CREATE TABLE", draft.tableName, col, msg, hint
                );
            }
            return *this;
        }

        // Verifica que exista una columna activa antes de aplicar un modificador de restricción.
        bool requireActiveColumn(const char* modifierName) {
            if (hasAborted) return false;
            if (currentColIdx < 0 || currentColIdx >= static_cast<int>(draft.columns.size())) {
                abortWith(Standard::SqlState::SYNTAX_ERROR, "",
                          String("modifier ") + modifierName + " cannot be called before declaring a column",
                          "Declare a column with column(...) before applying column constraints.");
                return false;
            }
            return true;
        }

        // Valida que el nombre de una tabla o columna respete las reglas léxicas y palabras reservadas de PostgreSQL.
        bool validateIdentifier(const String& id, const char* objectKind) {
            String reason;
            if (!Standard::PostgresLexicalValidator::isValidIdentifierFormat(id, reason)) {
                const char* state = (id.length() > 63) ? Standard::SqlState::NAME_TOO_LONG : Standard::SqlState::INVALID_NAME;
                abortWith(state, id, String("invalid ") + objectKind + " name \"" + id + "\": " + reason);
                return false;
            }
            if (Standard::PostgresLexicalValidator::isForbiddenUnquotedIdentifier(id, false)) {
                abortWith(Standard::SqlState::SYNTAX_ERROR, id,
                          String("syntax error at or near \"") + id + "\" (" + id + " is a PostgreSQL RESERVED word)",
                          "Quote the identifier as \"\\\"" + id + "\\\"\" or choose a non-reserved name.");
                return false;
            }
            return true;
        }

    public:
        // Inicializa el constructor para una tabla nueva validando inmediatamente su identificador.
        explicit DeterministicTableBuilder(const String& tableName,
                                           Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT,
                                           bool ifNotExists = false)
            : ifNotExistsFlag(ifNotExists) {
            draft.tableName = tableName;
            draft.persistence = persistence;
            validateIdentifier(tableName, "table");
        }

        // Agrega una columna tipada verificando que no esté duplicada y que los parámetros de longitud/precisión sean coherentes.
        DeterministicTableBuilder& column(const String& colName,
                                          Standard::PgDataType dataType,
                                          uint16_t maxLength = 0,
                                          uint8_t precision = 0,
                                          uint8_t scale = 0,
                                          const String& customTypeName = "") {
            if (hasAborted) return *this;
            if (!validateIdentifier(colName, "column")) return *this;

            if (draft.findColumn(colName) != nullptr) {
                return abortWith(Standard::SqlState::DUPLICATE_COLUMN, colName,
                                 "column \"" + colName + "\" specified more than once");
            }

            // Validación determinista de modificadores de tipo según PostgreSQL
            bool acceptsMaxLength = (dataType == Standard::PgDataType::VARCHAR ||
                                     dataType == Standard::PgDataType::CHAR ||
                                     dataType == Standard::PgDataType::NCHAR ||
                                     dataType == Standard::PgDataType::BIT ||
                                     dataType == Standard::PgDataType::BINARY ||
                                     dataType == Standard::PgDataType::VARBINARY);

            if (!acceptsMaxLength && maxLength > 0) {
                return abortWith(Standard::SqlState::SYNTAX_ERROR, colName,
                                 String("type modifier length (") + maxLength + ") is not allowed for type "
                                 + Standard::DataTypeUtils::toString(dataType));
            }

            bool acceptsPrecision = (dataType == Standard::PgDataType::DECIMAL ||
                                     dataType == Standard::PgDataType::NUMERIC);
            if (!acceptsPrecision && (precision > 0 || scale > 0)) {
                return abortWith(Standard::SqlState::SYNTAX_ERROR, colName,
                                 String("precision/scale modifier is not allowed for type ")
                                 + Standard::DataTypeUtils::toString(dataType));
            }
            if (acceptsPrecision && scale > precision) {
                return abortWith(Standard::SqlState::INVALID_PARAMETER_VALUE, colName,
                                 "NUMERIC scale " + String(scale) + " must be between 0 and precision " + String(precision));
            }

            if (dataType == Standard::PgDataType::ENUM) {
                if (!PhysicalSchemaCatalog::getInstance().hasEnum(customTypeName)) {
                    return abortWith(Standard::SqlState::UNDEFINED_OBJECT, colName,
                                     "type \"" + customTypeName + "\" does not exist",
                                     "Execute CREATE TYPE " + customTypeName + " AS ENUM (...) before creating the table.");
                }
            }

            if (dataType == Standard::PgDataType::DOMAIN_TYPE) {
                if (!PhysicalSchemaCatalog::getInstance().hasDomain(customTypeName)) {
                    return abortWith(Standard::SqlState::UNDEFINED_OBJECT, colName,
                                     "domain \"" + customTypeName + "\" does not exist");
                }
            }

            Standard::ColumnConstraintDef col;
            col.columnName     = colName;
            col.dataType       = dataType;
            col.maxLength      = maxLength;
            col.precision      = precision;
            col.scale          = scale;
            col.customTypeName = customTypeName;

            draft.columns.push_back(col);
            currentColIdx = static_cast<int>(draft.columns.size()) - 1;
            return *this;
        }

        // Marca la columna actual como PRIMARY KEY (implica NOT NULL + UNIQUE) y prohíbe múltiples PKs en la tabla.
        DeterministicTableBuilder& primaryKey() {
            if (!requireActiveColumn("PRIMARY KEY")) return *this;
            if (draft.hasPrimaryKey()) {
                return abortWith(Standard::SqlState::TUPLE_ALREADY_HAS_PRIMARY_KEY,
                                 draft.columns[currentColIdx].columnName,
                                 "multiple primary keys for table \"" + draft.tableName + "\" are not allowed");
            }
            auto& col = draft.columns[currentColIdx];
            col.isPrimaryKey = true;
            col.isNotNull    = true;
            col.isUnique     = true;
            return *this;
        }

        // Configura GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY exigiendo estrictamente un tipo entero (SMALLINT, INTEGER, BIGINT).
        DeterministicTableBuilder& generatedAsIdentity(Standard::IdentityGeneration mode = Standard::IdentityGeneration::ALWAYS,
                                                       int64_t start = 1, int64_t increment = 1) {
            if (!requireActiveColumn("GENERATED AS IDENTITY")) return *this;
            auto& col = draft.columns[currentColIdx];
            if (col.dataType != Standard::PgDataType::SMALLINT &&
                col.dataType != Standard::PgDataType::INTEGER &&
                col.dataType != Standard::PgDataType::BIGINT) {
                return abortWith(Standard::SqlState::INVALID_OBJECT_DEFINITION, col.columnName,
                                 "identity column type must be smallint, integer, or bigint");
            }
            if (col.hasDefault) {
                return abortWith(Standard::SqlState::SYNTAX_ERROR, col.columnName,
                                 "both default and identity specified for column \"" + col.columnName + "\"");
            }
            if (increment == 0) {
                return abortWith(Standard::SqlState::INVALID_PARAMETER_VALUE, col.columnName,
                                 "INCREMENT must not be zero");
            }
            col.identityMode      = mode;
            col.identityStart     = start;
            col.identityIncrement = increment;
            col.isNotNull         = true;
            return *this;
        }

        // Marca la columna actual como NOT NULL.
        DeterministicTableBuilder& notNull() {
            if (!requireActiveColumn("NOT NULL")) return *this;
            auto& col = draft.columns[currentColIdx];
            if (col.isForeignKey && col.onDelete == Standard::ReferentialAction::SET_NULL) {
                return abortWith(Standard::SqlState::INVALID_FOREIGN_KEY, col.columnName,
                                 "column \"" + col.columnName + "\" cannot be NOT NULL while having ON DELETE SET NULL");
            }
            col.isNotNull = true;
            return *this;
        }

        // Marca la columna actual como UNIQUE.
        DeterministicTableBuilder& unique() {
            if (!requireActiveColumn("UNIQUE")) return *this;
            draft.columns[currentColIdx].isUnique = true;
            return *this;
        }

        // Asigna un valor DEFAULT validando determinísticamente que el literal coincida con el tipo PostgreSQL de la columna.
        DeterministicTableBuilder& defaultValue(const String& literalValue) {
            if (!requireActiveColumn("DEFAULT")) return *this;
            auto& col = draft.columns[currentColIdx];

            if (col.identityMode != Standard::IdentityGeneration::NONE) {
                return abortWith(Standard::SqlState::SYNTAX_ERROR, col.columnName,
                                 "both default and identity specified for column \"" + col.columnName + "\"");
            }
            if (literalValue.equalsIgnoreCase("NULL") && col.isNotNull) {
                return abortWith(Standard::SqlState::NOT_NULL_VIOLATION, col.columnName,
                                 "null value in column \"" + col.columnName + "\" violates not-null constraint");
            }

            const Standard::EnumTypeDefinition* enumDef = nullptr;
            if (col.dataType == Standard::PgDataType::ENUM) {
                enumDef = PhysicalSchemaCatalog::getInstance().getEnum(col.customTypeName);
            }

            String sqlState, errMsg;
            if (!Standard::DataTypeUtils::validateLiteral(col.dataType, literalValue,
                                                          col.maxLength, col.precision, col.scale,
                                                          enumDef, sqlState, errMsg)) {
                return abortWith(sqlState.c_str(), col.columnName,
                                 "invalid DEFAULT expression for column \"" + col.columnName + "\": " + errMsg);
            }

            if (col.hasCheck && col.checkFn && !col.checkFn(literalValue)) {
                return abortWith(Standard::SqlState::CHECK_VIOLATION, col.columnName,
                                 "DEFAULT value \"" + literalValue + "\" violates check constraint \"" + col.checkExpression + "\"");
            }

            col.hasDefault   = true;
            col.defaultValue = literalValue;
            return *this;
        }

        // Declara una restricción CHECK sobre la columna actual y verifica el DEFAULT si ya fue asignado.
        DeterministicTableBuilder& check(const String& sqlExpression, Standard::CheckEvaluatorFn evaluator = nullptr, const String& constraintName = "") {
            if (!requireActiveColumn("CHECK")) return *this;
            auto& col = draft.columns[currentColIdx];
            if (sqlExpression.isEmpty()) {
                return abortWith(Standard::SqlState::SYNTAX_ERROR, col.columnName, "CHECK expression cannot be empty");
            }
            if (col.hasDefault && evaluator && !evaluator(col.defaultValue)) {
                return abortWith(Standard::SqlState::CHECK_VIOLATION, col.columnName,
                                 "DEFAULT value \"" + col.defaultValue + "\" violates check constraint \"" + sqlExpression + "\"");
            }
            col.hasCheck            = true;
            col.checkExpression     = sqlExpression;
            col.checkFn             = evaluator;
            col.checkConstraintName = constraintName.isEmpty() ? (draft.tableName + "_" + col.columnName + "_check") : constraintName;
            return *this;
        }

        // Declara una clave foránea REFERENCES validando existencia de tabla padre, unicidad en destino, compatibilidad de tipos y coherencia de acciones.
        DeterministicTableBuilder& references(const String& targetTable,
                                              const String& targetColumn,
                                              Standard::ReferentialAction onDelete = Standard::ReferentialAction::NO_ACTION,
                                              Standard::ReferentialAction onUpdate = Standard::ReferentialAction::NO_ACTION,
                                              Standard::MatchType matchType = Standard::MatchType::SIMPLE) {
            if (!requireActiveColumn("REFERENCES")) return *this;
            auto& col = draft.columns[currentColIdx];

            // Permite autorreferencia o referencia a tabla previamente registrada en el catálogo
            const TablePhysicalSchema* parentTable = nullptr;
            if (targetTable.equalsIgnoreCase(draft.tableName)) {
                parentTable = &draft;
            } else {
                parentTable = PhysicalSchemaCatalog::getInstance().getTable(targetTable);
            }

            if (!parentTable) {
                return abortWith(Standard::SqlState::UNDEFINED_TABLE, col.columnName,
                                 "referenced relation \"" + targetTable + "\" does not exist");
            }

            const Standard::ColumnConstraintDef* parentCol = parentTable->findColumn(targetColumn);
            if (!parentCol) {
                return abortWith(Standard::SqlState::UNDEFINED_COLUMN, col.columnName,
                                 "column \"" + targetColumn + "\" referenced in foreign key constraint does not exist in table \"" + targetTable + "\"");
            }

            // En PostgreSQL una columna referenciada por FK DEBE tener restricción PRIMARY KEY o UNIQUE (SQLSTATE 42830)
            if (!parentCol->isPrimaryKey && !parentCol->isUnique) {
                return abortWith(Standard::SqlState::INVALID_FOREIGN_KEY, col.columnName,
                                 "there is no unique constraint matching given keys for referenced table \"" + targetTable + "\"",
                                 "Add PRIMARY KEY or UNIQUE to \"" + targetTable + "." + targetColumn + "\".");
            }

            // Compatibilidad estricta de tipos de datos entre FK y PK (SQLSTATE 42804)
            if (!Standard::DataTypeUtils::areForeignKeyCompatible(col.dataType, col.customTypeName,
                                                                  parentCol->dataType, parentCol->customTypeName)) {
                return abortWith(Standard::SqlState::DATATYPE_MISMATCH, col.columnName,
                                 "foreign key constraint \"" + draft.tableName + "_" + col.columnName + "_fkey\" cannot be implemented: "
                                 "Key columns \"" + col.columnName + "\" and \"" + targetColumn + "\" are of incompatible types: "
                                 + Standard::DataTypeUtils::toString(col.dataType) + " and " + Standard::DataTypeUtils::toString(parentCol->dataType));
            }

            if (onDelete == Standard::ReferentialAction::SET_NULL && col.isNotNull) {
                return abortWith(Standard::SqlState::INVALID_FOREIGN_KEY, col.columnName,
                                 "column \"" + col.columnName + "\" is NOT NULL and cannot use ON DELETE SET NULL");
            }

            if (onDelete == Standard::ReferentialAction::SET_DEFAULT && !col.hasDefault) {
                return abortWith(Standard::SqlState::INVALID_FOREIGN_KEY, col.columnName,
                                 "column \"" + col.columnName + "\" has no DEFAULT value and cannot use ON DELETE SET DEFAULT");
            }

            col.isForeignKey     = true;
            col.fkConstraintName = draft.tableName + "_" + col.columnName + "_fkey";
            col.referencedTable  = targetTable;
            col.referencedColumn = targetColumn;
            col.onDelete         = onDelete;
            col.onUpdate         = onUpdate;
            col.matchType        = matchType;
            return *this;
        }

        // Agrega una restricción UNIQUE compuesta o PRIMARY KEY compuesta a nivel tabla.
        DeterministicTableBuilder& tableUniqueConstraint(const String& constraintName, const std::vector<String>& columnNames, bool isPk = false) {
            if (hasAborted) return *this;
            if (isPk && draft.hasPrimaryKey()) {
                return abortWith(Standard::SqlState::TUPLE_ALREADY_HAS_PRIMARY_KEY, "",
                                 "multiple primary keys for table \"" + draft.tableName + "\" are not allowed");
            }
            for (const auto& cName : columnNames) {
                if (!draft.findColumn(cName)) {
                    return abortWith(Standard::SqlState::UNDEFINED_COLUMN, cName,
                                     "column \"" + cName + "\" named in key does not exist");
                }
            }
            Standard::TableLevelConstraintDef tc;
            tc.constraintName = constraintName;
            tc.type = isPk ? Standard::TableLevelConstraintDef::Type::PRIMARY_KEY : Standard::TableLevelConstraintDef::Type::UNIQUE;
            tc.localColumns = columnNames;
            draft.tableConstraints.push_back(tc);
            return *this;
        }

        // Asigna el tablespace o propietario a la tabla en construcción.
        DeterministicTableBuilder& tablespace(const String& tsName) {
            if (!hasAborted) draft.tablespace = tsName;
            return *this;
        }

        // Ejecuta la validación final de cierre y registra el esquema en el catálogo y en el archivo consolidado.
        bool execute() {
            if (hasAborted) return false;

            PhysicalSchemaCatalog& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.hasTable(draft.tableName)) {
                if (ifNotExistsFlag) {
                    return DdlDiagnosticConsole::getInstance().reportSuccess(
                        "CREATE TABLE", draft.tableName, "relation \"" + draft.tableName + "\" already exists, skipping"
                    );
                }
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::DUPLICATE_TABLE, "CREATE TABLE", draft.tableName, "",
                    "relation \"" + draft.tableName + "\" already exists"
                );
            }

            if (draft.columns.empty()) {
                return DdlDiagnosticConsole::getInstance().raiseError(
                    Standard::SqlState::INVALID_TABLE_DEFINITION, "CREATE TABLE", draft.tableName, "",
                    "table \"" + draft.tableName + "\" must have at least one column"
                );
            }

            // Creación automática de secuencias vinculadas para columnas IDENTITY como hace PostgreSQL
            for (const auto& col : draft.columns) {
                if (col.identityMode != Standard::IdentityGeneration::NONE) {
                    Standard::SequenceDefinition seq;
                    seq.sequenceName  = draft.tableName + "_" + col.columnName + "_seq";
                    seq.dataType      = col.dataType;
                    seq.startValue    = col.identityStart;
                    seq.incrementBy   = col.identityIncrement;
                    seq.ownedByTable  = draft.tableName;
                    seq.ownedByColumn = col.columnName;
                    cat.registerSequence(seq);
                }
            }

            cat.registerTable(draft);
            cat.ensurePhysicalTableFileExists(draft.tableName);
            cat.flushCatalogToSingleFile();

            return DdlDiagnosticConsole::getInstance().reportSuccess(
                "CREATE TABLE", draft.tableName, String(draft.columns.size()) + " columns registered"
            );
        }
    };

} // namespace DbNotRuntime::Engine