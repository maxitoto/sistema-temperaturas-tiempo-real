#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include "PostgresDataTypes.hpp"
#include "PostgresConstraints.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"

namespace DbCommon::Engine {

    struct TablePhysicalSchema {
        String schemaName = "public";
        String tableName = "";
        Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT;
        String tablespace = "pg_default";
        String ownerRole = "postgres";
        String comment = "";
        std::vector<Standard::ColumnConstraintDef> columns;
        std::vector<Standard::TableLevelConstraintDef> tableConstraints;

        const Standard::ColumnConstraintDef* findColumn(const String& colName) const {
            for (const auto& c : columns) {
                if (c.columnName.equalsIgnoreCase(colName)) return &c;
            }
            return nullptr;
        }

        Standard::ColumnConstraintDef* findColumnMutable(const String& colName) {
            for (auto& c : columns) {
                if (c.columnName.equalsIgnoreCase(colName)) return &c;
            }
            return nullptr;
        }

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

        static String normalizeKey(const String& id) {
            String k = id;
            k.trim();
            if (!k.startsWith("\"")) k.toLowerCase();
            return k;
        }

        static String extractKV(const String& csvProps, const String& keyPrefix) {
            int pos = csvProps.indexOf(keyPrefix);
            if (pos == -1) return "";
            int start = pos + keyPrefix.length();
            int end = csvProps.indexOf(',', start);
            if (end == -1) end = csvProps.length();
            return csvProps.substring(start, end);
        }

    public:
        static PhysicalSchemaCatalog& getInstance() {
            static PhysicalSchemaCatalog instance;
            return instance;
        }

        String baseDir() const { return "/db"; }
        String catalogFilePath() const { return "/db/pg_catalog.cat"; }
        String tableDataFilePath(const String& tableName) const { return "/db/" + tableName + ".tbl"; }

        bool hasEnum(const String& name) const { return enums.find(normalizeKey(name)) != enums.end(); }
        void registerEnum(const Standard::EnumTypeDefinition& def) { enums[normalizeKey(def.typeName)] = def; }
        void removeEnum(const String& name) { enums.erase(normalizeKey(name)); }
        const Standard::EnumTypeDefinition* getEnum(const String& name) const {
            auto it = enums.find(normalizeKey(name));
            return (it != enums.end()) ? &(it->second) : nullptr;
        }

        bool hasDomain(const String& name) const { return domains.find(normalizeKey(name)) != domains.end(); }
        void registerDomain(const Standard::DomainTypeDefinition& def) { domains[normalizeKey(def.domainName)] = def; }
        void removeDomain(const String& name) { domains.erase(normalizeKey(name)); }
        const Standard::DomainTypeDefinition* getDomain(const String& name) const {
            auto it = domains.find(normalizeKey(name));
            return (it != domains.end()) ? &(it->second) : nullptr;
        }

        bool hasSequence(const String& name) const { return sequences.find(normalizeKey(name)) != sequences.end(); }
        void registerSequence(const Standard::SequenceDefinition& seq) { sequences[normalizeKey(seq.sequenceName)] = seq; }
        void removeSequence(const String& name) { sequences.erase(normalizeKey(name)); }

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

        bool hasIndex(const String& name) const { return indexes.find(normalizeKey(name)) != indexes.end(); }
        void registerIndex(const Standard::IndexDefinition& idx) { indexes[normalizeKey(idx.indexName)] = idx; }
        void removeIndex(const String& name) { indexes.erase(normalizeKey(name)); }
        const std::map<String, Standard::IndexDefinition>& getAllIndexes() const { return indexes; }

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
            for (const auto& d : domains) {
                const auto& dm = d.second;
                out += "DOMAIN|" + dm.domainName + "|" + Standard::DataTypeUtils::toString(dm.baseType) + "|"
                    + "LEN=" + String(dm.maxLength) + ","
                    + "PREC=" + String(dm.precision) + ","
                    + "SCL=" + String(dm.scale) + ","
                    + "NN=" + String(dm.notNull ? 1 : 0) + ","
                    + "DEF=" + (dm.hasDefault ? dm.defaultValue : String("__NONE__")) + "\n";
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
                        + "ID=" + String(static_cast<int>(c.identityMode)) + ","
                        + "LEN=" + String(c.maxLength) + ","
                        + "PREC=" + String(c.precision) + ","
                        + "SCL=" + String(c.scale) + ","
                        + "ANADD=" + String(c.autoNowAdd ? 1 : 0) + ","
                        + "ANOW=" + String(c.autoNow ? 1 : 0) + ","
                        + "ARR=" + String(c.isArray ? 1 : 0) + ","
                        + "IFLD=" + String(static_cast<int>(c.intervalField)) + ","
                        + "CTYPE=" + c.customTypeName + ","
                        + "DEF=" + (c.hasDefault ? (c.defaultValue.isEmpty() ? String("__EMPTY__") : c.defaultValue) : String("__NONE__")) + ","
                        + "FK=" + (c.isForeignKey ? (c.referencedTable + "." + c.referencedColumn) : "NONE")
                        + "\n";
                }
            }
            for (const auto& idx : indexes) {
                String cols = "";
                for (size_t i = 0; i < idx.second.keyColumns.size(); ++i) {
                    if (i > 0) cols += ",";
                    cols += idx.second.keyColumns[i];
                }
                out += "INDEX|" + idx.second.indexName + "|" + idx.second.tableName
                    + "|UQ=" + String(idx.second.isUnique ? 1 : 0) + "|COLS=" + cols + "\n";
            }
            return sd.escribirArchivo(catalogFilePath().c_str(), out.c_str());
        }

        bool loadCatalogFromSingleFile() {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada() || !sd.existeArchivo(catalogFilePath().c_str())) {
                return false;
            }
            String content = "";
            if (!sd.leerArchivo(catalogFilePath().c_str(), content) || content.isEmpty()) {
                return false;
            }

            int start = 0;
            int len = content.length();
            while (start < len) {
                int nl = content.indexOf('\n', start);
                if (nl == -1) nl = len;
                String line = content.substring(start, nl);
                line.trim();
                start = nl + 1;

                if (line.isEmpty() || line.startsWith("#")) continue;

                if (line.startsWith("ENUM|")) {
                    int p1 = line.indexOf('|', 5);
                    if (p1 == -1) continue;
                    Standard::EnumTypeDefinition def;
                    def.typeName = line.substring(5, p1);
                    String csv = line.substring(p1 + 1);
                    int s = 0;
                    while (s < static_cast<int>(csv.length())) {
                        int c = csv.indexOf(',', s);
                        if (c == -1) c = csv.length();
                        def.labels.push_back(csv.substring(s, c));
                        s = c + 1;
                    }
                    registerEnum(def);
                } else if (line.startsWith("DOMAIN|")) {
                    int p1 = line.indexOf('|', 7);
                    int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
                    if (p2 == -1) continue;
                    Standard::DomainTypeDefinition dm;
                    dm.domainName = line.substring(7, p1);
                    dm.baseType   = Standard::DataTypeUtils::fromString(line.substring(p1 + 1, p2));
                    String props  = line.substring(p2 + 1);
                    dm.maxLength  = static_cast<uint16_t>(extractKV(props, "LEN=").toInt());
                    dm.precision  = static_cast<uint8_t>(extractKV(props, "PREC=").toInt());
                    dm.scale      = static_cast<uint8_t>(extractKV(props, "SCL=").toInt());
                    dm.notNull    = (extractKV(props, "NN=") == "1");
                    String defVal = extractKV(props, "DEF=");
                    if (!defVal.isEmpty() && defVal != "__NONE__") {
                        dm.hasDefault   = true;
                        dm.defaultValue = defVal;
                    }
                    registerDomain(dm);
                } else if (line.startsWith("TABLE|")) {
                    int p1 = line.indexOf('|', 6);
                    if (p1 == -1) continue;
                    String full = line.substring(6, p1);
                    int dot = full.indexOf('.');
                    TablePhysicalSchema tb;
                    tb.schemaName = (dot != -1) ? full.substring(0, dot) : "public";
                    tb.tableName  = (dot != -1) ? full.substring(dot + 1) : full;
                    if (!hasTable(tb.tableName)) {
                        registerTable(tb);
                    }
                } else if (line.startsWith("COL|")) {
                    int p1 = line.indexOf('|', 4);
                    int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
                    int p3 = (p2 != -1) ? line.indexOf('|', p2 + 1) : -1;
                    if (p3 == -1) continue;

                    String tName = line.substring(4, p1);
                    String cName = line.substring(p1 + 1, p2);
                    String tStr  = line.substring(p2 + 1, p3);
                    String props = line.substring(p3 + 1);

                    TablePhysicalSchema* tb = getTableMutable(tName);
                    if (!tb || tb->findColumn(cName) != nullptr) continue;

                    Standard::ColumnConstraintDef col;
                    col.columnName     = cName;
                    col.dataType       = Standard::DataTypeUtils::fromString(tStr);
                    col.isPrimaryKey   = (extractKV(props, "PK=") == "1");
                    col.isNotNull      = (extractKV(props, "NN=") == "1");
                    col.isUnique       = (extractKV(props, "UQ=") == "1");
                    col.identityMode   = static_cast<Standard::IdentityGeneration>(extractKV(props, "ID=").toInt());
                    col.maxLength      = static_cast<uint16_t>(extractKV(props, "LEN=").toInt());
                    col.precision      = static_cast<uint8_t>(extractKV(props, "PREC=").toInt());
                    col.scale          = static_cast<uint8_t>(extractKV(props, "SCL=").toInt());
                    col.autoNowAdd     = (extractKV(props, "ANADD=") == "1");
                    col.autoNow        = (extractKV(props, "ANOW=") == "1");
                    col.isArray        = (extractKV(props, "ARR=") == "1");
                    col.intervalField  = static_cast<Standard::IntervalField>(extractKV(props, "IFLD=").toInt());
                    col.customTypeName = extractKV(props, "CTYPE=");

                    String defVal = extractKV(props, "DEF=");
                    if (defVal != "__NONE__") {
                        col.hasDefault   = true;
                        col.defaultValue = (defVal == "__EMPTY__") ? "" : defVal;
                    }

                    String fkVal = extractKV(props, "FK=");
                    if (!fkVal.isEmpty() && fkVal != "NONE") {
                        int d = fkVal.indexOf('.');
                        if (d != -1) {
                            col.isForeignKey     = true;
                            col.referencedTable  = fkVal.substring(0, d);
                            col.referencedColumn = fkVal.substring(d + 1);
                            col.fkConstraintName = tName + "_" + cName + "_fkey";
                        }
                    }
                    tb->columns.push_back(std::move(col));
                } else if (line.startsWith("INDEX|")) {
                    int p1 = line.indexOf('|', 6);
                    int p2 = (p1 != -1) ? line.indexOf('|', p1 + 1) : -1;
                    if (p2 == -1) continue;

                    Standard::IndexDefinition idx;
                    idx.indexName = line.substring(6, p1);
                    idx.tableName = line.substring(p1 + 1, p2);
                    String rest   = line.substring(p2 + 1);
                    idx.isUnique  = (rest.indexOf("UQ=1") != -1);

                    int colsPos = rest.indexOf("COLS=");
                    if (colsPos != -1) {
                        String csvCols = rest.substring(colsPos + 5);
                        int s = 0;
                        while (s < static_cast<int>(csvCols.length())) {
                            int c = csvCols.indexOf(',', s);
                            if (c == -1) c = csvCols.length();
                            idx.keyColumns.push_back(csvCols.substring(s, c));
                            s = c + 1;
                        }
                    }
                    registerIndex(idx);
                }
            }
            return !tables.empty();
        }

        bool ensurePhysicalTableFileExists(const String& tableName) const {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return true;
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

} // namespace DbCommon::Engine

namespace DbNotRuntime::Engine {
    using namespace DbCommon::Engine;
}