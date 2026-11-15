#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "RuntimeTypedTuple.hpp"
#include "../../common/PhysicalSchemaCatalog.hpp"

namespace DbRuntime::Storage {

    struct IndexEntry {
        char key[32];
        uint32_t rowPosition;

        IndexEntry() : rowPosition(0) { key[0] = '\0'; }
        IndexEntry(const char* k, uint32_t pos) : rowPosition(pos) {
            if (k) {
                strncpy(key, k, sizeof(key) - 1);
                key[sizeof(key) - 1] = '\0';
            } else {
                key[0] = '\0';
            }
        }

        bool operator<(const IndexEntry& other) const {
            return strcmp(key, other.key) < 0;
        }
    };

    struct ActiveIndexStructure {
        String indexName;
        String tableName;
        std::vector<String> keyColumns;
        bool isUnique = false;
        bool isPrimary = false;
        std::vector<IndexEntry> sortedEntries;

        static void formatCanonicalComponent(const String& rawVal, char* outBuf, size_t cap) {
            if (rawVal.isEmpty()) {
                outBuf[0] = '\0';
                return;
            }
            bool isInt = true;
            size_t start = (rawVal[0] == '-' || rawVal[0] == '+') ? 1 : 0;
            if (start >= rawVal.length()) isInt = false;
            for (size_t i = start; i < rawVal.length() && isInt; ++i) {
                if (!isdigit(rawVal[i])) isInt = false;
            }

            if (isInt && rawVal.length() <= 11) {
                long long v = atoll(rawVal.c_str());
                if (v >= 0) {
                    snprintf(outBuf, cap, "P%010lld", v);
                } else {
                    long long inv = 9999999999LL + v;
                    if (inv < 0) inv = 0;
                    snprintf(outBuf, cap, "N%010lld", inv);
                }
            } else {
                strncpy(outBuf, rawVal.c_str(), cap - 1);
                outBuf[cap - 1] = '\0';
            }
        }

        bool buildCompositeKey(const PgTuple& tuple, char* outKey, size_t cap) const {
            outKey[0] = '\0';
            size_t used = 0;
            for (size_t i = 0; i < keyColumns.size(); ++i) {
                const PgCell* c = tuple.findCell(keyColumns[i]);
                if (!c || c->isNull) return false;

                char comp[24];
                formatCanonicalComponent(c->rawText, comp, sizeof(comp));

                if (i > 0 && used + 1 < cap) {
                    outKey[used++] = '\x1F';
                    outKey[used] = '\0';
                }
                size_t rem = (used < cap) ? (cap - used - 1) : 0;
                if (rem > 0) {
                    strncat(outKey, comp, rem);
                    used = strlen(outKey);
                }
            }
            return outKey[0] != '\0';
        }

        std::vector<uint32_t> lookupEqual(const String& rawTargetKey) const {
            char target[32];
            if (rawTargetKey.indexOf('\x1F') != -1) {
                strncpy(target, rawTargetKey.c_str(), sizeof(target) - 1);
                target[sizeof(target) - 1] = '\0';
            } else {
                formatCanonicalComponent(rawTargetKey, target, sizeof(target));
            }

            std::vector<uint32_t> matches;
            IndexEntry probe(target, 0);
            auto it = std::lower_bound(sortedEntries.begin(), sortedEntries.end(), probe);
            while (it != sortedEntries.end() && strcmp(it->key, target) == 0) {
                matches.push_back(it->rowPosition);
                ++it;
            }
            return matches;
        }

        std::vector<uint32_t> lookupFirstColumnPrefix(const String& firstColValue) const {
            if (keyColumns.size() <= 1) {
                return lookupEqual(firstColValue);
            }
            char canonFirst[24];
            formatCanonicalComponent(firstColValue, canonFirst, sizeof(canonFirst));

            char prefix[28];
            snprintf(prefix, sizeof(prefix), "%s\x1F", canonFirst);
            size_t prefixLen = strlen(prefix);

            std::vector<uint32_t> matches;
            IndexEntry probe(prefix, 0);
            auto it = std::lower_bound(sortedEntries.begin(), sortedEntries.end(), probe);
            while (it != sortedEntries.end() && strncmp(it->key, prefix, prefixLen) == 0) {
                matches.push_back(it->rowPosition);
                ++it;
            }
            return matches;
        }

        void insertEntry(const char* keyStr, uint32_t pos) {
            if (!keyStr || keyStr[0] == '\0') return;
            if (sortedEntries.size() >= 512) {
                sortedEntries.erase(sortedEntries.begin());
            }
            IndexEntry entry(keyStr, pos);
            auto it = std::upper_bound(sortedEntries.begin(), sortedEntries.end(), entry);
            sortedEntries.insert(it, entry);
        }
    };

    class RuntimeStorageAndIndexManager {
    public:
        static constexpr size_t MAX_SELECT_ROWS_RAM = 32;

    private:
        std::map<String, std::vector<PgTuple>> fallbackRamRows;
        std::map<String, std::vector<ActiveIndexStructure>> tableIndexes;
        std::map<String, int64_t> sequenceCounters;

        uint8_t batchDepth = 0;
        std::map<String, String> pendingBatchAppends;

        RuntimeStorageAndIndexManager() = default;

        static String norm(const String& s) {
            String k = s;
            k.trim();
            k.toLowerCase();
            return k;
        }

        void recoverInterruptedTmpFile(const String& tableName) {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return;
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";
            if (sd.existeArchivo(tmpPath.c_str())) {
                if (!sd.existeArchivo(path.c_str())) {
                    SD.rename(tmpPath.c_str(), path.c_str());
                } else {
                    sd.eliminarArchivo(tmpPath.c_str());
                }
            }
        }

    public:
        static RuntimeStorageAndIndexManager& getInstance() {
            static RuntimeStorageAndIndexManager instance;
            return instance;
        }

        static TarjetaSD::ScopedLock lockEngine() {
            return TarjetaSD::ScopedLock(TarjetaSD::getInstance().obtenerMutex());
        }

        void beginBatch() {
            xSemaphoreTakeRecursive(TarjetaSD::getInstance().obtenerMutex(), portMAX_DELAY);
            batchDepth++;
        }

        bool commitBatch() {
            bool ok = true;
            if (batchDepth > 0) {
                batchDepth--;
                if (batchDepth == 0) {
                    TarjetaSD& sd = TarjetaSD::getInstance();
                    auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
                    if (sd.estaInicializada()) {
                        for (const auto& kv : pendingBatchAppends) {
                            if (kv.second.isEmpty()) continue;
                            String path = cat.tableDataFilePath(kv.first);
                            if (!sd.anexarArchivo(path.c_str(), kv.second.c_str())) {
                                ok = false;
                            }
                        }
                    }
                    pendingBatchAppends.clear();
                }
            }
            xSemaphoreGiveRecursive(TarjetaSD::getInstance().obtenerMutex());
            return ok;
        }

        void flushPendingBatchForTable(const String& tableName) {
            auto lk = lockEngine();
            String key = norm(tableName);
            auto it = pendingBatchAppends.find(key);
            if (it == pendingBatchAppends.end() || it->second.isEmpty()) return;

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (sd.estaInicializada()) {
                auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
                String path = cat.tableDataFilePath(tableName);
                sd.anexarArchivo(path.c_str(), it->second.c_str());
            }
            it->second = "";
        }

        bool synchronizeWithDdlCatalog() {
            auto lk = lockEngine();
            auto& cat = DbCommon::Engine::PhysicalSchemaCatalog::getInstance();
            if (cat.getAllTables().empty()) {
                cat.loadCatalogFromSingleFile();
            }
            for (const auto& pair : cat.getAllTables()) {
                const auto& schema = pair.second;
                recoverInterruptedTmpFile(schema.tableName);
                rebuildIndexesForTable(schema.tableName);
            }
            return true;
        }

        bool readRowAtOffset(const String& tableName, uint32_t byteOffset, PgTuple& outTuple) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);

            if (!sd.estaInicializada()) {
                const auto& rows = fallbackRamRows[norm(tableName)];
                if (byteOffset >= rows.size()) return false;
                outTuple = rows[byteOffset];
                return true;
            }

            String path = cat.tableDataFilePath(tableName);
            if (!SD.exists(path.c_str())) return false;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return false;

            if (!f.seek(byteOffset)) {
                f.close();
                return false;
            }

            char lineBuf[384];
            size_t len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
            f.close();

            while (len > 0 && (lineBuf[len - 1] == '\r' || lineBuf[len - 1] == ' ' || lineBuf[len - 1] == '\t')) {
                --len;
            }
            lineBuf[len] = '\0';
            if (len == 0) return false;

            outTuple = PgTuple::deserializeTsvFromCStr(lineBuf, schema);
            return true;
        }

        template <typename CallbackFn>
        void forEachRowStreaming(const String& tableName, CallbackFn fn) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);

            if (!sd.estaInicializada()) {
                const auto& rows = fallbackRamRows[norm(tableName)];
                for (uint32_t i = 0; i < rows.size(); ++i) {
                    if (!fn(rows[i], i)) break;
                }
                return;
            }

            String path = cat.tableDataFilePath(tableName);
            if (!SD.exists(path.c_str())) return;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return;

            char lineBuf[384];
            while (f.available()) {
                uint32_t offset = static_cast<uint32_t>(f.position());
                size_t len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
                while (len > 0 && (lineBuf[len - 1] == '\r' || lineBuf[len - 1] == ' ' || lineBuf[len - 1] == '\t')) {
                    --len;
                }
                lineBuf[len] = '\0';
                if (len == 0) continue;

                PgTuple row = PgTuple::deserializeTsvFromCStr(lineBuf, schema);
                if (!fn(row, offset)) break;
            }
            f.close();
        }

        bool appendRowAndIndex(const String& tableName, const PgTuple& newRow) {
            auto lk = lockEngine();
            String key = norm(tableName);
            TarjetaSD& sd = TarjetaSD::getInstance();
            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();

            if (!sd.estaInicializada()) {
                auto& rows = fallbackRamRows[key];
                if (rows.size() >= MAX_SELECT_ROWS_RAM) rows.erase(rows.begin());
                uint32_t pos = static_cast<uint32_t>(rows.size());
                rows.push_back(newRow);
                for (auto& idx : tableIndexes[key]) {
                    char k[32];
                    if (idx.buildCompositeKey(newRow, k, sizeof(k))) idx.insertEntry(k, pos);
                }
                return true;
            }

            String path = cat.tableDataFilePath(tableName);
            uint32_t baseSize = 0;
            if (SD.exists(path.c_str())) {
                File f = SD.open(path.c_str(), FILE_READ);
                if (f) {
                    baseSize = static_cast<uint32_t>(f.size());
                    f.close();
                }
            }

            uint32_t newByteOffset = baseSize + static_cast<uint32_t>(pendingBatchAppends[key].length());
            String lineTsv = newRow.serializeTsv();

            if (batchDepth > 0) {
                pendingBatchAppends[key] += lineTsv;
            } else {
                if (!sd.anexarArchivo(path.c_str(), lineTsv.c_str())) {
                    return false;
                }
            }

            for (auto& idx : tableIndexes[key]) {
                char k[32];
                if (idx.buildCompositeKey(newRow, k, sizeof(k))) {
                    idx.insertEntry(k, newByteOffset);
                }
            }
            return true;
        }

        bool rebuildIndexesForTable(const String& tableName) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            if (!schema) return false;

            String key = norm(tableName);
            std::vector<ActiveIndexStructure> idxList;

            for (const auto& col : schema->columns) {
                if (col.isPrimaryKey || col.isUnique) {
                    ActiveIndexStructure idx;
                    idx.indexName  = schema->tableName + "_" + col.columnName + (col.isPrimaryKey ? "_pkey" : "_key");
                    idx.tableName  = schema->tableName;
                    idx.keyColumns = { col.columnName };
                    idx.isUnique   = true;
                    idx.isPrimary  = col.isPrimaryKey;
                    idxList.push_back(std::move(idx));
                }
            }

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

            for (const auto& idxPair : cat.getAllIndexes()) {
                const auto& idxDef = idxPair.second;
                if (norm(idxDef.tableName) == key) {
                    ActiveIndexStructure idx;
                    idx.indexName  = idxDef.indexName;
                    idx.tableName  = schema->tableName;
                    idx.keyColumns = idxDef.keyColumns;
                    idx.isUnique   = idxDef.isUnique;
                    idx.isPrimary  = false;
                    idxList.push_back(std::move(idx));
                }
            }

            forEachRowStreaming(tableName, [&](const PgTuple& row, uint32_t offset) -> bool {
                for (auto& idx : idxList) {
                    char k[32];
                    if (idx.buildCompositeKey(row, k, sizeof(k))) {
                        idx.insertEntry(k, offset);
                    }
                }
                for (const auto& col : schema->columns) {
                    if (col.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE) {
                        String seqKey = norm(tableName + "." + col.columnName);
                        int64_t val = row.getBigInt(col.columnName.c_str(), 0);
                        if (val >= sequenceCounters[seqKey]) {
                            sequenceCounters[seqKey] = val + col.identityIncrement;
                        }
                    }
                }
                return true;
            });

            tableIndexes[key] = std::move(idxList);
            return true;
        }

        const ActiveIndexStructure* findBestIndexForColumn(const String& tableName, const String& colName) const {
            String key = norm(tableName);
            auto it = tableIndexes.find(key);
            if (it == tableIndexes.end()) return nullptr;

            const ActiveIndexStructure* fallbackSecondary = nullptr;
            for (const auto& idx : it->second) {
                if (!idx.keyColumns.empty() && idx.keyColumns[0].equalsIgnoreCase(colName)) {
                    if (idx.isPrimary || idx.isUnique) return &idx;
                    fallbackSecondary = &idx;
                }
            }
            return fallbackSecondary;
        }

        int64_t nextIdentityValue(const String& tableName, const String& colName, int64_t start, int64_t inc) {
            String seqKey = norm(tableName + "." + colName);
            auto it = sequenceCounters.find(seqKey);
            if (it == sequenceCounters.end() || it->second < start) {
                sequenceCounters[seqKey] = start + inc;
                return start;
            }
            int64_t current = it->second;
            it->second += inc;
            return current;
        }

        std::vector<PgTuple> getTableRows(const String& tableName) {
            std::vector<PgTuple> out;
            forEachRowStreaming(tableName, [&](const PgTuple& r, uint32_t) -> bool {
                if (out.size() >= MAX_SELECT_ROWS_RAM) out.erase(out.begin());
                out.push_back(r);
                return true;
            });
            return out;
        }

        std::vector<PgTuple>& getTableRowsMutable(const String& tableName) {
            return fallbackRamRows[norm(tableName)];
        }

        bool replaceTableRows(const String& tableName, const std::vector<PgTuple>& newRows) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            String key = norm(tableName);

            if (!sd.estaInicializada()) {
                fallbackRamRows[key] = newRows;
                return rebuildIndexesForTable(tableName);
            }

            auto& cat = DbCommon::Engine::PhysicalSchemaCatalog::getInstance();
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";

            if (sd.existeArchivo(tmpPath.c_str())) {
                sd.eliminarArchivo(tmpPath.c_str());
            }

            File fTmp = SD.open(tmpPath.c_str(), FILE_WRITE);
            if (!fTmp) {
                return false;
            }

            for (const auto& r : newRows) {
                String rowTsv = r.serializeTsv();
                const size_t expected = rowTsv.length();
                if (expected > 0 && fTmp.write(reinterpret_cast<const uint8_t*>(rowTsv.c_str()), expected) != expected) {
                    fTmp.close();
                    sd.eliminarArchivo(tmpPath.c_str());
                    return false;
                }
            }
            fTmp.flush();
            fTmp.close();

            if (sd.existeArchivo(path.c_str())) {
                sd.eliminarArchivo(path.c_str());
            }
            if (!SD.rename(tmpPath.c_str(), path.c_str())) {
                return false;
            }
            return rebuildIndexesForTable(tableName);
        }

        bool persistTableToSd(const String& tableName) {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return true;
            return replaceTableRows(tableName, fallbackRamRows[norm(tableName)]);
        }

        const std::vector<ActiveIndexStructure>& getIndexesOfTable(const String& tableName) const {
            static const std::vector<ActiveIndexStructure> emptyIdx;
            auto it = tableIndexes.find(norm(tableName));
            return (it != tableIndexes.end()) ? it->second : emptyIdx;
        }
    };

} // namespace DbRuntime::Storage