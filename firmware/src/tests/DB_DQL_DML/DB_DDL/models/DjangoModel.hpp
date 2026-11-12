#pragma once
#include <Arduino.h>
#include <vector>
#include "DjangoFields.hpp"
#include "../engine/ModuloDDL.hpp"

namespace DbNotRuntime::Models {

    // Equivale a models.Index(fields=[...], name="...") dentro de class Meta en Django.
    struct Index {
        String name;
        std::vector<String> fields;
        bool unique = false;
        Standard::IndexMethod method = Standard::IndexMethod::BTREE;

        // Inicializa la definición declarativa de un índice para Meta.indexes.
        Index(const String& idxName, const std::vector<String>& cols, bool isUnique = false)
            : name(idxName), fields(cols), unique(isUnique) {}
    };

    // Equivale a models.UniqueConstraint(fields=[...], name="...") dentro de class Meta en Django.
    struct UniqueConstraint {
        String name;
        std::vector<String> fields;

        // Inicializa una restricción UNIQUE compuesta a nivel tabla.
        UniqueConstraint(const String& constraintName, const std::vector<String>& cols)
            : name(constraintName), fields(cols) {}
    };

    // Equivale a la subclase class Meta de un modelo Django (db_table, tablespace, constraints, indexes).
    struct MetaOptions {
        String db_table = "";
        String db_tablespace = "pg_default";
        Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT;
        std::vector<UniqueConstraint> unique_constraints;
        std::vector<Index> indexes;
    };

    // Clase base declarativa estilo django.db.models.Model que traduce los campos y Meta al motor DDL PostgreSQL.
    class Model {
    public:
        virtual ~Model() = default;

        // Devuelve las opciones Meta del modelo (nombre de tabla físico, índices y restricciones compuestas).
        virtual MetaOptions meta() const = 0;

        // Devuelve la lista ordenada de campos declarados en el modelo.
        virtual std::vector<Field> fields() const = 0;

        // Ejecuta la migración DDL (equivalente a manage.py migrate) pasando por el validador determinista de PostgreSQL.
        bool migrate(bool ifNotExists = true) const {
            MetaOptions m = meta();
            std::vector<Field> fList = fields();

            Engine::DeterministicTableBuilder builder =
                Engine::ModuloDDL::getInstance().createTable(m.db_table, m.persistence, ifNotExists);

            builder.tablespace(m.db_tablespace);

            for (const auto& f : fList) {
                const auto& c = f.compiled();
                builder.column(c.columnName, c.dataType, c.maxLength, c.precision, c.scale, c.customTypeName);

                if (c.isPrimaryKey) {
                    builder.primaryKey();
                }
                if (c.identityMode != Standard::IdentityGeneration::NONE) {
                    builder.generatedAsIdentity(c.identityMode, c.identityStart, c.identityIncrement);
                }
                if (c.isNotNull && !c.isPrimaryKey) {
                    builder.notNull();
                }
                if (c.isUnique && !c.isPrimaryKey) {
                    builder.unique();
                }
                if (c.hasCheck) {
                    builder.check(c.checkExpression, c.checkFn, c.checkConstraintName);
                }
                if (c.hasDefault) {
                    builder.defaultValue(c.defaultValue);
                }
                if (c.isForeignKey) {
                    builder.references(c.referencedTable, c.referencedColumn, c.onDelete, c.onUpdate, c.matchType);
                }
            }

            for (const auto& uq : m.unique_constraints) {
                builder.tableUniqueConstraint(uq.name, uq.fields, false);
            }

            if (!builder.execute()) {
                return false;
            }

            // Crea los índices secundarios declarados en Meta.indexes
            for (const auto& idx : m.indexes) {
                Standard::IndexDefinition idxDef;
                idxDef.indexName  = idx.name;
                idxDef.tableName  = m.db_table;
                idxDef.isUnique   = idx.unique;
                idxDef.method     = idx.method;
                idxDef.keyColumns = idx.fields;
                idxDef.tablespace = m.db_tablespace;
                if (!Engine::ModuloDDL::getInstance().createIndex(idxDef, ifNotExists)) {
                    return false;
                }
            }
            return true;
        }
    };

} // namespace DbNotRuntime::Models