#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include "../standard/PostgresDataTypes.hpp"
#include "../standard/PostgresConstraints.hpp"
#include "../hardware/driver/tarjetasd/TarjetaSD.hpp"

namespace DbNotRuntime::Engine {

    // Representa la definición física completa de una tabla en el catálogo de PostgreSQL.
    struct TablePhysicalSchema {
        String schemaName = "public";
        String tableName = "";
        Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT;
        String tablespace = "pg_default";
        String ownerRole = "postgres";
        String comment = "";
        std::vector<Standard::ColumnConstraintDef> columns;
        std::vector<Standard::TableLevelConstraintDef> tableConstraints;

        // Busca una columna por nombre de forma case-insensitive según la normalización de PostgreSQL.
        const Standard::ColumnConstraintDef* findColumn(const String& colName) const {
            for (const auto& c : columns) {
                if (c.columnName.equalsIgnoreCase(colName)) return &c;
            }
            return nullptr;
        }

        // Busca una columna mutable por nombre para operaciones ALTER TABLE.
        Standard::ColumnConstraintDef* findColumnMutable(const String& colName) {
            for (auto& c : columns) {
                if (c.columnName.equalsIgnoreCase(colName)) return &c;
            }
            return nullptr;
        }

        // Verifica si la tabla ya posee una clave primaria declarada a nivel columna o tabla.
        bool hasPrimaryKey() const {
            for (const auto& c : columns) {
                if (c.isPrimaryKey) return true;
            }
            for (const auto& tc : tableConstraints) {
                if (tc.type == Standard::TableLevelConstraintDef::Type::PRIMARY_KEY) return true;
            }
            return false;
        }
    };

    class PhysicalSchemaCatalog {
    private:
        std::map<String, Standard::EnumTypeDefinition> enums;
        std::map<String, Standard::DomainTypeDefinition> domains;
        std::map<String, Standard::SequenceDefinition> sequences;
        std::map<String, TablePhysicalSchema> tables;
        std::map<String, Standard::IndexDefinition> indexes;

        PhysicalSchemaCatalog() = default;

        // Normaliza un identificador no entrecomillado a minúsculas como hace PostgreSQL internamente.
        static String normalizeKey(const String& id) {
            String k = id;
            k.trim();
            if (!k.startsWith("\"")) k.toLowerCase();
            return k;
        }

    public:
        // Obtiene la instancia única del diccionario de datos (information_schema).
        static PhysicalSchemaCatalog& getInstance() {
            static PhysicalSchemaCatalog instance;
            return instance;
        }

        // Rutas físicas consolidadas: un único archivo de catálogo y archivos .tbl de datos (sin .meta ni .idx sueltos).
        String baseDir() const { return "/db"; }
        String catalogFilePath() const { return "/db/pg_catalog.cat"; }
        String tableDataFilePath(const String& tableName) const { return "/db/" + tableName + ".tbl"; }

        // ---------------- Gestión de Tipos ENUM ----------------
        bool hasEnum(const String& name) const { return enums.find(normalizeKey(name)) != enums.end(); }
        void registerEnum(const Standard::EnumTypeDefinition& def) { enums[normalizeKey(def.typeName)] = def; }
        void removeEnum(const String& name) { enums.erase(normalizeKey(name)); }
        const Standard::EnumTypeDefinition* getEnum(const String& name) const {
            auto it = enums.find(normalizeKey(name));
            return (it != enums.end()) ? &(it->second) : nullptr;
        }

        // ---------------- Gestión de Tipos DOMAIN ----------------
        bool hasDomain(const String& name) const { return domains.find(normalizeKey(name)) != domains.end(); }
        void registerDomain(const Standard::DomainTypeDefinition& def) { domains[normalizeKey(def.domainName)] = def; }
        void removeDomain(const String& name) { domains.erase(normalizeKey(name)); }
        const Standard::DomainTypeDefinition* getDomain(const String& name) const {
            auto it = domains.find(normalizeKey(name));
            return (it != domains.end()) ? &(it->second) : nullptr;
        }

        // ---------------- Gestión de Secuencias ----------------
        bool hasSequence(const String& name) const { return sequences.find(normalizeKey(name)) != sequences.end(); }
        void registerSequence(const Standard::SequenceDefinition& seq) { sequences[normalizeKey(seq.sequenceName)] = seq; }
        void removeSequence(const String& name) { sequences.erase(normalizeKey(name)); }

        // ---------------- Gestión de Tablas ----------------
        bool hasTable(const String& name) const { return tables.find(normalizeKey(name)) != tables.end(); }
        void registerTable(const TablePhysicalSchema& schema) { tables[normalizeKey(schema.tableName)] = schema; }
        void removeTable(const String& name) { tables.erase(normalizeKey(name)); }

        const TablePhysicalSchema* getTable(const String& name) const {
            auto it = tables.find(normalizeKey(name));
            return (it != tables.end()) ? &(it->second) : nullptr;
        }

        TablePhysicalSchema* getTableMutable(const String& name) {
            auto it = tables.find(normalizeKey(name));
            return (it != tables.end()) ? &(it->second) : nullptr;
        }

        const std::map<String, TablePhysicalSchema>& getAllTables() const { return tables; }

        // Busca qué tablas hijas tienen una FOREIGN KEY apuntando a la tabla padre indicada.
        std::vector<String> findDependentTables(const String& parentTableName) const {
            std::vector<String> dependents;
            String normParent = normalizeKey(parentTableName);
            for (const auto& pair : tables) {
                if (pair.first == normParent) continue;
                for (const auto& col : pair.second.columns) {
                    if (col.isForeignKey && normalizeKey(col.referencedTable) == normParent) {
                        dependents.push_back(pair.second.tableName);
                        break;
                    }
                }
            }
            return dependents;
        }

        // ---------------- Gestión de Índices ----------------
        bool hasIndex(const String& name) const { return indexes.find(normalizeKey(name)) != indexes.end(); }
        void registerIndex(const Standard::IndexDefinition& idx) { indexes[normalizeKey(idx.indexName)] = idx; }
        void removeIndex(const String& name) { indexes.erase(normalizeKey(name)); }

        // Elimina todos los índices asociados a una tabla cuando se ejecuta DROP TABLE.
        void removeIndexesOfTable(const String& tableName) {
            String normTable = normalizeKey(tableName);
            for (auto it = indexes.begin(); it != indexes.end(); ) {
                if (normalizeKey(it->second.tableName) == normTable) {
                    it = indexes.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Persiste todo el diccionario de datos en un único archivo compacto (/db/pg_catalog.cat) evitando crear archivos basura.
        bool flushCatalogToSingleFile() const {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return false;
            if (!sd.existeArchivo(baseDir().c_str())) {
                sd.crearDirectorio(baseDir().c_str());
            }

            String out = "# PG_CATALOG_V1\n";
            for (const auto& e : enums) {
                out += "ENUM|" + e.second.typeName + "|";
                for (size_t i = 0; i < e.second.labels.size(); ++i) {
                    if (i > 0) out += ",";
                    out += e.second.labels[i];
                }
                out += "\n";
            }
            for (const auto& t : tables) {
                const auto& tb = t.second;
                out += "TABLE|" + tb.schemaName + "." + tb.tableName + "|OWNER=" + tb.ownerRole + "\n";
                for (const auto& c : tb.columns) {
                    out += "COL|" + tb.tableName + "|" + c.columnName + "|"
                        + Standard::DataTypeUtils::toString(c.dataType) + "|"
                        + "PK=" + String(c.isPrimaryKey ? 1 : 0) + ","
                        + "NN=" + String(c.isNotNull ? 1 : 0) + ","
                        + "UQ=" + String(c.isUnique ? 1 : 0) + ","
                        + "LEN=" + String(c.maxLength) + ","
                        + "DEF=" + c.defaultValue + ","
                        + "FK=" + (c.isForeignKey ? (c.referencedTable + "." + c.referencedColumn) : "NONE")
                        + "\n";
                }
            }
            for (const auto& idx : indexes) {
                out += "INDEX|" + idx.second.indexName + "|" + idx.second.tableName + "|UQ=" + String(idx.second.isUnique ? 1 : 0) + "\n";
            }
            return sd.escribirArchivo(catalogFilePath().c_str(), out.c_str());
        }

        // Asegura que el archivo de datos .tbl exista en la MicroSD sin crear archivos auxiliares innecesarios.
        bool ensurePhysicalTableFileExists(const String& tableName) const {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return true; // Permite validación en memoria si SD no está montada en test
            if (!sd.existeArchivo(baseDir().c_str())) {
                sd.crearDirectorio(baseDir().c_str());
            }
            String path = tableDataFilePath(tableName);
            if (!sd.existeArchivo(path.c_str())) {
                return sd.escribirArchivo(path.c_str(), "");
            }
            return true;
        }
    };

} // namespace DbNotRuntime::Engine