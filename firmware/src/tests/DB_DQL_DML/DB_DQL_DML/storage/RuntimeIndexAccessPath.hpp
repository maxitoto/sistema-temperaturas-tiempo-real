#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>
#include <algorithm>
#include "RuntimeTypedTuple.hpp"
#include "../../DB_DDL/engine/PhysicalSchemaCatalog.hpp"

namespace DbRuntime::Storage {

    // Entrada compacta de índice que vincula el valor de la clave con su posición exacta (byte offset / rowIndex).
    struct IndexEntry {
        String keyValue;
        uint32_t rowPosition = 0; // Offset de byte en archivo .tbl o posición en buffer RAM

        // Compara entradas numérica o lexicográficamente para búsqueda binaria O(log N).
        bool operator<(const IndexEntry& other) const {
            return keyValue < other.keyValue;
        }
    };

    // Representa la estructura en memoria de un índice activo (PK, UNIQUE o CREATE INDEX).
    struct ActiveIndexStructure {
        String indexName;
        String tableName;
        std::vector<String> keyColumns;
        bool isUnique = false;
        bool isPrimary = false;
        std::vector<IndexEntry> sortedEntries;

        // Compone la clave de búsqueda concatenando las columnas del índice con separador '\x1F'.
        String buildCompositeKey(const PgTuple& tuple) const {
            String k = "";
            for (size_t i = 0; i < keyColumns.size(); ++i) {
                if (i > 0) k += "\x1F";
                const PgCell* c = tuple.findCell(keyColumns[i]);
                if (!c || c->isNull) return ""; // En PostgreSQL los NULL no colisionan en UNIQUE estándar
                k += c->rawText;
            }
            return k;
        }

        // Ejecuta búsqueda binaria O(log N) sobre las entradas ordenadas devolviendo las posiciones coincidentes.
        std::vector<uint32_t> lookupEqual(const String& targetKey) const {
            std::vector<uint32_t> matches;
            IndexEntry probe{targetKey, 0};
            auto it = std::lower_bound(sortedEntries.begin(), sortedEntries.end(), probe);
            while (it != sortedEntries.end() && it->keyValue == targetKey) {
                matches.push_back(it->rowPosition);
                ++it;
            }
            return matches;
        }

        // Inserta una entrada manteniendo el orden para B-Tree en RAM.
        void insertEntry(const String& key, uint32_t pos) {
            if (key.isEmpty()) return;
            IndexEntry entry{key, pos};
            auto it = std::upper_bound(sortedEntries.begin(), sortedEntries.end(), entry);
            sortedEntries.insert(it, entry);
        }
    };

    // Gestor de almacenamiento físico y caminos de acceso indexados sincronizado con PhysicalSchemaCatalog.
    class RuntimeStorageAndIndexManager {
    private:
        // Almacén en RAM de respaldo (y caché de alta velocidad) sincronizado con los archivos .tbl de la SD
        std::map<String, std::vector<PgTuple>> tableRowsCache;
        std::map<String, std::vector<ActiveIndexStructure>> tableIndexes;
        std::map<String, int64_t> sequenceCounters;

        RuntimeStorageAndIndexManager() = default;

        static String norm(const String& s) {
            String k = s;
            k.trim();
            k.toLowerCase();
            return k;
        }

    public:
        static RuntimeStorageAndIndexManager& getInstance() {
            static RuntimeStorageAndIndexManager instance;
            return instance;
        }

        // Sincroniza todas las tablas, PKs, UNIQUEs e índices declarados en database_not_runtime.
        bool synchronizeWithDdlCatalog() {
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            for (const auto& pair : cat.getAllTables()) {
                const auto& schema = pair.second;
                loadTableFromSdIfAvailable(schema.tableName);
                rebuildIndexesForTable(schema.tableName);
            }
            return true;
        }

        // Carga las filas desde el archivo físico /db/<tabla>.tbl si la tarjeta SD está inicializada.
        void loadTableFromSdIfAvailable(const String& tableName) {
            String key = norm(tableName);
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";
            if (sd.existeArchivo(tmpPath.c_str())) {
                if (!sd.existeArchivo(path.c_str())) {
                    SD.rename(tmpPath.c_str(), path.c_str()); // corte entre remove y rename: recupera
                } else {
                    sd.eliminarArchivo(tmpPath.c_str());      // corte durante la escritura: .tmp incompleto
                }
            }
            if (!sd.existeArchivo(path.c_str())) return;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return;

            std::vector<PgTuple> rows;
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.isEmpty()) continue;
                rows.push_back(PgTuple::deserializeTsv(line, schema));
            }
            f.close();
            tableRowsCache[key] = std::move(rows);
        }

        // Vuelca el vector de tuplas de una tabla al archivo físico /db/<tabla>.tbl en una sola operación de I/O.
        bool persistTableToSd(const String& tableName) {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return true; // Modo en memoria para tests

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";
            const auto& rows = getTableRows(tableName);

            String payload = "";
            for (const auto& r : rows) {
                payload += r.serializeTsv();
            }

            // 1. Escribir primero en el archivo temporal .tmp
            if (!sd.escribirArchivo(tmpPath.c_str(), payload.c_str())) {
                Serial.println("[SD] ERROR: no se pudo escribir " + tmpPath);
                return false;
            }

            // 2. Si ya existe el archivo final, eliminarlo de forma segura
            if (sd.existeArchivo(path.c_str())) {
                sd.eliminarArchivo(path.c_str());
            }

            // 3. Renombrar .tmp -> .tbl de forma atómica
            bool ok = SD.rename(tmpPath.c_str(), path.c_str());
            if (!ok) Serial.println("[SD] ERROR: rename " + tmpPath + " -> " + path + " fallo");
            return ok;
        }

        // Reconstruye todos los índices (PK, UNIQUE de columna, UNIQUE compuestos y CREATE INDEX) de una tabla.
        bool rebuildIndexesForTable(const String& tableName) {
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            if (!schema) return false;

            String key = norm(tableName);
            std::vector<ActiveIndexStructure> idxList;

            // 1. Índices implícitos por PRIMARY KEY y UNIQUE a nivel columna
            for (const auto& col : schema->columns) {
                if (col.isPrimaryKey || col.isUnique) {
                    ActiveIndexStructure idx;
                    idx.indexName = schema->tableName + "_" + col.columnName + (col.isPrimaryKey ? "_pkey" : "_key");
                    idx.tableName = schema->tableName;
                    idx.keyColumns = { col.columnName };
                    idx.isUnique  = true;
                    idx.isPrimary = col.isPrimaryKey;
                    idxList.push_back(std::move(idx));
                }
            }

            // 2. Índices implícitos por restricciones UNIQUE / PK compuestas a nivel tabla
            for (const auto& tc : schema->tableConstraints) {
                if (tc.type == DbNotRuntime::Standard::TableLevelConstraintDef::Type::PRIMARY_KEY ||
                    tc.type == DbNotRuntime::Standard::TableLevelConstraintDef::Type::UNIQUE) {
                    ActiveIndexStructure idx;
                    idx.indexName  = tc.constraintName;
                    idx.tableName  = schema->tableName;
                    idx.keyColumns = tc.localColumns;
                    idx.isUnique   = true;
                    idx.isPrimary  = (tc.type == DbNotRuntime::Standard::TableLevelConstraintDef::Type::PRIMARY_KEY);
                    idxList.push_back(std::move(idx));
                }
            }

            // 3. Índices explícitos creados con ModuloDDL::createIndex (Meta.indexes de Django)
            // Inspeccionamos nombres conocidos o registrados en el catálogo
            const auto& rows = getTableRows(tableName);
            for (auto& idx : idxList) {
                for (uint32_t i = 0; i < rows.size(); ++i) {
                    String k = idx.buildCompositeKey(rows[i]);
                    idx.insertEntry(k, i);
                }
            }

            tableIndexes[key] = std::move(idxList);
            return true;
        }

        // Registra también en el motor de caminos de acceso cualquier índice secundario declarado en un modelo Django.
        void ensureSecondaryIndexRegistered(const String& tableName, const String& indexName,
                                            const std::vector<String>& cols, bool isUnique) {
            String key = norm(tableName);
            auto& list = tableIndexes[key];
            for (const auto& existing : list) {
                if (existing.indexName.equalsIgnoreCase(indexName)) return;
            }
            ActiveIndexStructure idx;
            idx.indexName  = indexName;
            idx.tableName  = tableName;
            idx.keyColumns = cols;
            idx.isUnique   = isUnique;
            idx.isPrimary  = false;

            const auto& rows = getTableRows(tableName);
            for (uint32_t i = 0; i < rows.size(); ++i) {
                idx.insertEntry(idx.buildCompositeKey(rows[i]), i);
            }
            list.push_back(std::move(idx));
        }

        // Busca si existe un índice cuyo primer atributo coincida con la columna filtrada.
        const ActiveIndexStructure* findBestIndexForColumn(const String& tableName, const String& colName) const {
            String key = norm(tableName);
            auto it = tableIndexes.find(key);
            if (it == tableIndexes.end()) return nullptr;

            const ActiveIndexStructure* fallbackSecondary = nullptr;
            for (const auto& idx : it->second) {
                if (!idx.keyColumns.empty() && idx.keyColumns[0].equalsIgnoreCase(colName)) {
                    if (idx.isPrimary || idx.isUnique) return &idx; // Prioriza índice único / PK
                    fallbackSecondary = &idx;
                }
            }
            return fallbackSecondary;
        }

        // Genera el siguiente valor de secuencia IDENTITY para una tabla y columna.
        int64_t nextIdentityValue(const String& tableName, const String& colName, int64_t start, int64_t inc) {
            String seqKey = norm(tableName + "." + colName);
            if (sequenceCounters.find(seqKey) == sequenceCounters.end()) {
                int64_t maxFound = start - inc;
                for (const auto& r : getTableRows(tableName)) {
                    int64_t v = r.getBigInt(colName.c_str(), maxFound);
                    if (v > maxFound) maxFound = v;
                }
                sequenceCounters[seqKey] = maxFound + inc;
            } else {
                sequenceCounters[seqKey] += inc;
            }
            return sequenceCounters[seqKey];
        }

        const std::vector<PgTuple>& getTableRows(const String& tableName) const {
            static const std::vector<PgTuple> emptyVec;
            auto it = tableRowsCache.find(norm(tableName));
            return (it != tableRowsCache.end()) ? it->second : emptyVec;
        }

        std::vector<PgTuple>& getTableRowsMutable(const String& tableName) {
            return tableRowsCache[norm(tableName)];
        }

        const std::vector<ActiveIndexStructure>& getIndexesOfTable(const String& tableName) const {
            static const std::vector<ActiveIndexStructure> emptyIdx;
            auto it = tableIndexes.find(norm(tableName));
            return (it != tableIndexes.end()) ? it->second : emptyIdx;
        }
    };

} // namespace DbRuntime::Storage