#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>
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
    };

    struct ActiveIndexStructure {
        String indexName;
        String tableName;
        std::vector<String> keyColumns;
        bool isUnique = false;
        bool isPrimary = false;

        std::vector<uint32_t> lookupEqual(const String&) const { return {}; }
        std::vector<uint32_t> lookupFirstColumnPrefix(const String&) const { return {}; }
    };

    class RuntimeStorageAndIndexManager {
    public:
        static constexpr size_t MAX_SELECT_ROWS_RAM = 32;

    private:
        static constexpr size_t LINE_BUF_CAP = 1024;
        static constexpr size_t IO_CHUNK_CAP = 512;

        std::map<String, int64_t> sequenceCounters;
        uint8_t batchDepth = 0;
        std::map<String, String> pendingBatchAppends;

        // Buffers compartidos bajo lockEngine(): reemplazan los 3 static lineBuf[640] previos
        char    lineBuf[LINE_BUF_CAP]{};
        uint8_t ioChunk[IO_CHUNK_CAP]{};

        RuntimeStorageAndIndexManager() = default;

        static String norm(const String& s) {
            String k = s;
            k.trim();
            k.toLowerCase();
            return k;
        }

        size_t readSingleLineAtOpenFile(File& f, uint32_t byteOffset) {
            if (!f.seek(byteOffset)) return 0;
            int n = f.read(reinterpret_cast<uint8_t*>(lineBuf), LINE_BUF_CAP - 1);
            if (n <= 0) return 0;

            const char* nl = static_cast<const char*>(memchr(lineBuf, '\n', static_cast<size_t>(n)));
            size_t len = nl ? static_cast<size_t>(nl - lineBuf) : static_cast<size_t>(n);
            while (len > 0 && (lineBuf[len - 1] == '\r' || lineBuf[len - 1] == ' ' || lineBuf[len - 1] == '\t')) {
                --len;
            }
            lineBuf[len] = '\0';
            return len;
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
            if (!sd.estaInicializada()) return false;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            String path = cat.tableDataFilePath(tableName);
            if (!SD.exists(path.c_str())) return false;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return false;

            const size_t len = readSingleLineAtOpenFile(f, byteOffset);
            f.close();
            if (len == 0) return false;

            PgTuple::deserializeTsvInto(lineBuf, schema, outTuple);
            return true;
        }

        // Lee una lista de offsets puntuales abriendo el archivo 1 sola vez y reutilizando 1 PgTuple
        template <typename CallbackFn>
        void forEachRowAtOffsets(const String& tableName, const uint32_t* offsets, size_t count, CallbackFn fn) {
            if (!offsets || count == 0) return;

            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            String path = cat.tableDataFilePath(tableName);
            if (!SD.exists(path.c_str())) return;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return;

            PgTuple row;
            if (schema) row.reserve(schema->columns.size());

            for (size_t i = 0; i < count; ++i) {
                if (readSingleLineAtOpenFile(f, offsets[i]) == 0) continue;
                PgTuple::deserializeTsvInto(lineBuf, schema, row);
                if (!fn(row)) break;
            }
            f.close();
        }

        // Escaneo secuencial en bloques de 512 B con 1 sola PgTuple reutilizada en RAM
        template <typename CallbackFn>
        void forEachRowStreaming(const String& tableName, CallbackFn fn) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            String path = cat.tableDataFilePath(tableName);
            if (!SD.exists(path.c_str())) return;

            File f = SD.open(path.c_str(), FILE_READ);
            if (!f) return;

            PgTuple row;
            if (schema) row.reserve(schema->columns.size());

            uint32_t currentFilePos  = 0;
            uint32_t lineStartOffset = 0;
            size_t   lineLen         = 0;
            bool     skippingLong    = false;
            bool     stopRequested   = false;
            uint16_t iterCount       = 0;

            auto dispatchLine = [&]() -> bool {
                while (lineLen > 0 && (lineBuf[lineLen - 1] == '\r' || lineBuf[lineLen - 1] == ' ' || lineBuf[lineLen - 1] == '\t')) {
                    --lineLen;
                }
                lineBuf[lineLen] = '\0';
                if (lineLen == 0) return true;

                PgTuple::deserializeTsvInto(lineBuf, schema, row);
                if ((++iterCount & 63u) == 0) {
                    vTaskDelay(1);
                }
                return fn(row, lineStartOffset);
            };

            while (!stopRequested) {
                int bytesRead = f.read(ioChunk, IO_CHUNK_CAP);
                if (bytesRead <= 0) break;

                size_t pos = 0;
                const size_t n = static_cast<size_t>(bytesRead);
                while (pos < n && !stopRequested) {
                    const uint8_t* nl = static_cast<const uint8_t*>(memchr(ioChunk + pos, '\n', n - pos));
                    const size_t segEnd = nl ? static_cast<size_t>(nl - ioChunk) : n;
                    const size_t segLen = segEnd - pos;

                    if (lineLen == 0 && !skippingLong) {
                        lineStartOffset = currentFilePos + static_cast<uint32_t>(pos);
                    }

                    if (!skippingLong && segLen > 0) {
                        const size_t capLibre = (LINE_BUF_CAP - 1) - lineLen;
                        if (segLen <= capLibre) {
                            memcpy(lineBuf + lineLen, ioChunk + pos, segLen);
                            lineLen += segLen;
                        } else {
                            if (capLibre > 0) {
                                memcpy(lineBuf + lineLen, ioChunk + pos, capLibre);
                                lineLen += capLibre;
                            }
                            skippingLong = true;
                        }
                    }

                    if (nl != nullptr) {
                        if (!dispatchLine()) {
                            stopRequested = true;
                        }
                        lineLen      = 0;
                        skippingLong = false;
                        pos          = segEnd + 1;
                    } else {
                        pos = n;
                    }
                }
                currentFilePos += static_cast<uint32_t>(n);
            }

            if (!stopRequested && lineLen > 0) {
                dispatchLine();
            }

            f.close();
        }

        // Reescribe una tabla línea por línea con lectura en bloques de 512 B y 1 sola PgTuple en RAM
        template <typename TransformFn>
        bool rewriteTableStreaming(const String& tableName, TransformFn fn) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return false;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";

            if (sd.existeArchivo(tmpPath.c_str())) {
                sd.eliminarArchivo(tmpPath.c_str());
            }

            File fOut = SD.open(tmpPath.c_str(), FILE_WRITE);
            if (!fOut) return false;

            bool writeOk = true;
            if (SD.exists(path.c_str())) {
                File fIn = SD.open(path.c_str(), FILE_READ);
                if (fIn) {
                    PgTuple row;
                    if (schema) row.reserve(schema->columns.size());

                    size_t   lineLen      = 0;
                    bool     skippingLong = false;
                    uint16_t iterCount    = 0;

                    auto processLine = [&]() {
                        while (lineLen > 0 && (lineBuf[lineLen - 1] == '\r' || lineBuf[lineLen - 1] == ' ' || lineBuf[lineLen - 1] == '\t')) {
                            --lineLen;
                        }
                        lineBuf[lineLen] = '\0';
                        if (lineLen == 0) return;

                        PgTuple::deserializeTsvInto(lineBuf, schema, row);
                        if (fn(row)) {
                            String rowTsv = row.serializeTsv();
                            size_t expected = rowTsv.length();
                            if (expected > 0 && fOut.write(reinterpret_cast<const uint8_t*>(rowTsv.c_str()), expected) != expected) {
                                writeOk = false;
                            }
                        }
                        if ((++iterCount & 63u) == 0) {
                            vTaskDelay(1);
                        }
                    };

                    while (writeOk) {
                        int bytesRead = fIn.read(ioChunk, IO_CHUNK_CAP);
                        if (bytesRead <= 0) break;

                        size_t pos = 0;
                        const size_t n = static_cast<size_t>(bytesRead);
                        while (pos < n && writeOk) {
                            const uint8_t* nl = static_cast<const uint8_t*>(memchr(ioChunk + pos, '\n', n - pos));
                            const size_t segEnd = nl ? static_cast<size_t>(nl - ioChunk) : n;
                            const size_t segLen = segEnd - pos;

                            if (!skippingLong && segLen > 0) {
                                const size_t capLibre = (LINE_BUF_CAP - 1) - lineLen;
                                if (segLen <= capLibre) {
                                    memcpy(lineBuf + lineLen, ioChunk + pos, segLen);
                                    lineLen += segLen;
                                } else {
                                    if (capLibre > 0) {
                                        memcpy(lineBuf + lineLen, ioChunk + pos, capLibre);
                                        lineLen += capLibre;
                                    }
                                    skippingLong = true;
                                }
                            }

                            if (nl != nullptr) {
                                processLine();
                                lineLen      = 0;
                                skippingLong = false;
                                pos          = segEnd + 1;
                            } else {
                                pos = n;
                            }
                        }
                    }

                    if (writeOk && lineLen > 0) {
                        processLine();
                    }
                    fIn.close();
                }
            }

            fOut.flush();
            fOut.close();

            if (!writeOk) {
                sd.eliminarArchivo(tmpPath.c_str());
                return false;
            }

            if (sd.existeArchivo(path.c_str())) {
                sd.eliminarArchivo(path.c_str());
            }
            return SD.rename(tmpPath.c_str(), path.c_str());
        }

        bool appendRowAndIndex(const String& tableName, const PgTuple& newRow) {
            auto lk = lockEngine();
            String key = norm(tableName);
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return false;

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            String path = cat.tableDataFilePath(tableName);
            String lineTsv = newRow.serializeTsv();

            if (batchDepth > 0) {
                pendingBatchAppends[key] += lineTsv;
                return true;
            }
            return sd.anexarArchivo(path.c_str(), lineTsv.c_str());
        }

        bool rebuildIndexesForTable(const String& tableName) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            auto& cat = DbNotRuntime::Engine::PhysicalSchemaCatalog::getInstance();
            const auto* schema = cat.getTable(tableName);
            if (!schema) return false;

            bool hasIdentity = false;
            for (const auto& col : schema->columns) {
                if (col.identityMode != DbNotRuntime::Standard::IdentityGeneration::NONE) {
                    hasIdentity = true;
                    break;
                }
            }
            if (!hasIdentity) return true;

            forEachRowStreaming(tableName, [&](const PgTuple& row, uint32_t) -> bool {
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
            return true;
        }

        const ActiveIndexStructure* findBestIndexForColumn(const String&, const String&) const {
            return nullptr;
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

        bool replaceTableRows(const String& tableName, const std::vector<PgTuple>& newRows) {
            auto lk = lockEngine();
            flushPendingBatchForTable(tableName);

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return false;

            auto& cat = DbCommon::Engine::PhysicalSchemaCatalog::getInstance();
            String path = cat.tableDataFilePath(tableName);
            String tmpPath = path + ".tmp";

            if (sd.existeArchivo(tmpPath.c_str())) {
                sd.eliminarArchivo(tmpPath.c_str());
            }

            File fTmp = SD.open(tmpPath.c_str(), FILE_WRITE);
            if (!fTmp) return false;

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
            return SD.rename(tmpPath.c_str(), path.c_str());
        }

        const std::vector<ActiveIndexStructure>& getIndexesOfTable(const String&) const {
            static const std::vector<ActiveIndexStructure> emptyIdx;
            return emptyIdx;
        }
    };

} // namespace DbRuntime::Storage