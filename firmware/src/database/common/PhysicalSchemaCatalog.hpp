#pragma once
#include <Arduino.h>
#include <SD.h>
#include <map>
#include <vector>
#include <cstring>
#include "PostgresDataTypes.hpp"
#include "PostgresConstraints.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"

namespace DbCommon::Engine {

    struct TablePhysicalSchema {
        // Prefijo fijo de 8 bytes en cada registro: [flags:1][reserved:3][nullMask:4]
        static constexpr uint16_t RECORD_HEADER_SIZE = 8;
        static constexpr size_t   MAX_COLUMNS        = 32;

        String schemaName = "public";
        String tableName = "";
        Standard::TablePersistence persistence = Standard::TablePersistence::PERMANENT;
        String tablespace = "pg_default";
        String ownerRole = "postgres";
        String comment = "";
        std::vector<Standard::ColumnConstraintDef> columns;
        std::vector<Standard::TableLevelConstraintDef> tableConstraints;

        // Metadatos binarios precalculados para acceso O(1) sin heap
        uint16_t recordSize       = RECORD_HEADER_SIZE;
        int8_t   pkColIndex       = -1;
        int8_t   identityColIndex = -1;

        void recomputeBinaryLayout() {
            uint16_t off     = RECORD_HEADER_SIZE;
            pkColIndex       = -1;
            identityColIndex = -1;

            for (size_t i = 0; i < columns.size(); ++i) {
                auto& c = columns[i];
                c.binOffset = off;
                c.binSize   = Standard::computeBinaryColumnSize(c.dataType, c.maxLength);
                off = static_cast<uint16_t>(off + c.binSize);

                if (c.isPrimaryKey && pkColIndex < 0) {
                    pkColIndex = static_cast<int8_t>(i);
                }
                if (c.identityMode != Standard::IdentityGeneration::NONE && identityColIndex < 0) {
                    identityColIndex = static_cast<int8_t>(i);
                }
            }

            // Alinea el tamaño total del slot a múltiplo de 4 bytes
            recordSize = static_cast<uint16_t>((off + 3u) & ~3u);
        }

        int8_t findColumnIndex(const char* colName) const {
            if (!colName || colName[0] == '\0') return -1;
            const char* clean = strchr(colName, '.');
            const char* target = clean ? (clean + 1) : colName;
            for (size_t i = 0; i < columns.size(); ++i) {
                if (strcasecmp(columns[i].columnName.c_str(), target) == 0) {
                    return static_cast<int8_t>(i);
                }
            }
            return -1;
        }

        int8_t findColumnIndex(const String& colName) const {
            return findColumnIndex(colName.c_str());
        }

        const Standard::ColumnConstraintDef* findColumn(const String& colName) const {
            int8_t idx = findColumnIndex(colName.c_str());
            return (idx >= 0) ? &columns[static_cast<size_t>(idx)] : nullptr;
        }

        Standard::ColumnConstraintDef* findColumnMutable(const String& colName) {
            int8_t idx = findColumnIndex(colName.c_str());
            return (idx >= 0) ? &columns[static_cast<size_t>(idx)] : nullptr;
        }

        bool hasPrimaryKey() const {
            if (pkColIndex >= 0) return true;
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
        String tableDataFilePath(const String& tableName) const { return "/db/" + tableName + ".pgb"; }

        void clearInMemory() {
            enums.clear();
            domains.clear();
            sequences.clear();
            tables.clear();
            indexes.clear();
        }

        bool hasEnum(const String& name) const { return enums.find(normalizeKey(name)) != enums.end(); }
        void registerEnum(const Standard::EnumTypeDefinition& def) { enums[normalizeKey(def.typeName)] = def; }
        void removeEnum(const String& name) { enums.erase(normalizeKey(name)); }
        const Standard::EnumTypeDefinition* getEnum(const String& name) const {
            auto it = enums.find(normalizeKey(name));
            return (it != enums.end()) ? &(it->second) : nullptr;
        }
        const std::map<String, Standard::EnumTypeDefinition>& getAllEnums() const { return enums; }

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
        void registerTable(TablePhysicalSchema schema) {
            schema.columns.shrink_to_fit();
            schema.tableConstraints.shrink_to_fit();
            schema.recomputeBinaryLayout();
            tables[normalizeKey(schema.tableName)] = std::move(schema);
        }
        void removeTable(const String& name) { tables.erase(normalizeKey(name)); }

        const TablePhysicalSchema* getTable(const String& name) const {
            auto it = tables.find(normalizeKey(name));
            return (it != tables.end()) ? &(it->second) : nullptr;
        }

        const TablePhysicalSchema* getTable(const char* name) const {
            if (!name || name[0] == '\0') return nullptr;
            return getTable(String(name));
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

        // Escribe el catálogo en streaming directo a SD (sin concatenar un String gigante en RAM)
        bool flushCatalogToSingleFile() const {
            TarjetaSD& sd = TarjetaSD::getInstance();
            TarjetaSD::ScopedLock lk(sd.obtenerMutex());
            if (!sd.estaInicializada()) return false;
            if (!SD.exists(baseDir().c_str())) {
                SD.mkdir(baseDir().c_str());
            }

            String catPath = catalogFilePath();
            String tmpPath = catPath + ".tmp";
            if (SD.exists(tmpPath.c_str())) SD.remove(tmpPath.c_str());

            File f = SD.open(tmpPath.c_str(), FILE_WRITE);
            if (!f) return false;

            f.print("# PG_CATALOG_V2\n");

            for (const auto& e : enums) {
                f.print("ENUM|");
                f.print(e.second.typeName);
                f.print("|");
                for (size_t i = 0; i < e.second.labels.size(); ++i) {
                    if (i > 0) f.print(",");
                    f.print(e.second.labels[i]);
                }
                f.print("\n");
            }

            for (const auto& d : domains) {
                const auto& dm = d.second;
                f.print("DOMAIN|");
                f.print(dm.domainName);
                f.print("|");
                f.print(Standard::DataTypeUtils::toString(dm.baseType));
                f.print("|LEN=");  f.print(dm.maxLength);
                f.print(",PREC="); f.print(dm.precision);
                f.print(",SCL=");  f.print(dm.scale);
                f.print(",NN=");   f.print(dm.notNull ? 1 : 0);
                f.print(",DEF=");  f.print(dm.hasDefault ? dm.defaultValue : String("__NONE__"));
                f.print("\n");
            }

            for (const auto& t : tables) {
                const auto& tb = t.second;
                f.print("TABLE|");
                f.print(tb.schemaName);
                f.print(".");
                f.print(tb.tableName);
                f.print("|OWNER=");
                f.print(tb.ownerRole);
                f.print("\n");

                for (const auto& c : tb.columns) {
                    f.print("COL|");
                    f.print(tb.tableName);
                    f.print("|");
                    f.print(c.columnName);
                    f.print("|");
                    f.print(Standard::DataTypeUtils::toString(c.dataType));
                    f.print("|PK=");    f.print(c.isPrimaryKey ? 1 : 0);
                    f.print(",NN=");    f.print(c.isNotNull ? 1 : 0);
                    f.print(",UQ=");    f.print(c.isUnique ? 1 : 0);
                    f.print(",ID=");    f.print(static_cast<int>(c.identityMode));
                    f.print(",LEN=");   f.print(c.maxLength);
                    f.print(",PREC=");  f.print(c.precision);
                    f.print(",SCL=");   f.print(c.scale);
                    f.print(",ANADD="); f.print(c.autoNowAdd ? 1 : 0);
                    f.print(",ANOW=");  f.print(c.autoNow ? 1 : 0);
                    f.print(",ARR=");   f.print(c.isArray ? 1 : 0);
                    f.print(",IFLD=");  f.print(static_cast<int>(c.intervalField));
                    f.print(",CTYPE="); f.print(c.customTypeName);
                    f.print(",DEF=");
                    if (!c.hasDefault) f.print("__NONE__");
                    else if (c.defaultValue.isEmpty()) f.print("__EMPTY__");
                    else f.print(c.defaultValue);
                    f.print(",FK=");
                    if (c.isForeignKey) {
                        f.print(c.referencedTable);
                        f.print(".");
                        f.print(c.referencedColumn);
                    } else {
                        f.print("NONE");
                    }
                    f.print("\n");
                }
            }

            for (const auto& idx : indexes) {
                f.print("INDEX|");
                f.print(idx.second.indexName);
                f.print("|");
                f.print(idx.second.tableName);
                f.print("|UQ=");
                f.print(idx.second.isUnique ? 1 : 0);
                f.print("|COLS=");
                for (size_t i = 0; i < idx.second.keyColumns.size(); ++i) {
                    if (i > 0) f.print(",");
                    f.print(idx.second.keyColumns[i]);
                }
                f.print("\n");
            }

            f.flush();
            f.close();

            if (SD.exists(catPath.c_str())) SD.remove(catPath.c_str());
            return SD.rename(tmpPath.c_str(), catPath.c_str());
        }

        // Carga PG_CATALOG_V2 línea por línea; rechaza PG_CATALOG_V1 (viejo formato TSV)
        bool loadCatalogFromSingleFile() {
            TarjetaSD& sd = TarjetaSD::getInstance();
            TarjetaSD::ScopedLock lk(sd.obtenerMutex());
            if (!sd.estaInicializada() || !SD.exists(catalogFilePath().c_str())) {
                return false;
            }

            File f = SD.open(catalogFilePath().c_str(), FILE_READ);
            if (!f) return false;

            char lineBuf[384]{};
            bool versionValida = false;
            bool primeraLinea  = true;

            while (f.available()) {
                size_t len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
                while (len > 0 && (lineBuf[len - 1] == '\r' || lineBuf[len - 1] == ' ' || lineBuf[len - 1] == '\t')) {
                    --len;
                }
                lineBuf[len] = '\0';
                if (len == 0) continue;

                if (primeraLinea) {
                    primeraLinea = false;
                    if (strncmp(lineBuf, "# PG_CATALOG_V2", 15) != 0) {
                        f.close();
                        return false;
                    }
                    versionValida = true;
                    continue;
                }

                if (lineBuf[0] == '#') continue;
                String line(lineBuf);

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
                        registerTable(std::move(tb));
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
            f.close();

            if (!versionValida || tables.empty()) return false;

            for (auto& kv : tables) {
                kv.second.columns.shrink_to_fit();
                kv.second.tableConstraints.shrink_to_fit();
                kv.second.recomputeBinaryLayout();
            }
            for (auto& kv : enums) {
                kv.second.labels.shrink_to_fit();
            }
            for (auto& kv : indexes) {
                kv.second.keyColumns.shrink_to_fit();
            }
            return true;
        }

        // Asegura que /db/<tableName>.pgb exista con una cabecera binaria válida de 32 bytes
        bool ensurePhysicalTableFileExists(const String& tableName) const {
            TarjetaSD& sd = TarjetaSD::getInstance();
            TarjetaSD::ScopedLock lk(sd.obtenerMutex());
            if (!sd.estaInicializada()) return true;
            if (!SD.exists(baseDir().c_str())) {
                SD.mkdir(baseDir().c_str());
            }

            const TablePhysicalSchema* tb = getTable(tableName);
            if (!tb) return false;

            String path = tableDataFilePath(tableName);
            if (SD.exists(path.c_str())) {
                File f = SD.open(path.c_str(), FILE_READ);
                if (f) {
                    Standard::PgBinHeader hdr{};
                    const bool ok = (f.size() >= sizeof(Standard::PgBinHeader)) &&
                                    (f.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) == sizeof(hdr)) &&
                                    hdr.esValida(tb->recordSize);
                    f.close();
                    if (ok) return true;
                }
            }

            Standard::PgBinHeader hdr{};
            int64_t startId = 1;
            if (tb->identityColIndex >= 0) {
                startId = tb->columns[static_cast<size_t>(tb->identityColIndex)].identityStart;
            }
            hdr.inicializar(tb->recordSize, startId);

            File fOut = SD.open(path.c_str(), FILE_WRITE);
            if (!fOut) return false;
            const size_t w = fOut.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));
            fOut.flush();
            fOut.close();
            return w == sizeof(hdr);
        }
    };

} // namespace DbCommon::Engine

namespace DbNotRuntime::Engine {
    using namespace DbCommon::Engine;
}