#pragma once
#include <Arduino.h>
#include <vector>
#include <cstring>
#include "../../common/PostgresDataTypes.hpp"
#include "../../common/PhysicalSchemaCatalog.hpp"
#include "../../common/PgDtoTypes.hpp"

namespace DbRuntime::Storage {

    using DbCommon::Standard::PgDataType;
    using DbCommon::Standard::PgValueUnion;
    using DbCommon::Standard::DataTypeUtils;
    using DbCommon::Dto::PgText;

    struct PgCell {
        char columnName[32] = {0};
        PgDataType type     = PgDataType::TEXT;
        bool isNull         = false;
        PgValueUnion num{};
        String rawText      = "";

        PgCell() = default;

        PgCell(const String& col, const String& val, PgDataType dt = PgDataType::TEXT) : type(dt) {
            setColumnName(col);
            setValue(val, dt);
        }

        PgCell(const char* col, const char* val)
            : PgCell(String(col ? col : ""), val ? String(val) : String(""), PgDataType::TEXT) {}
        PgCell(const char* col, const String& val)
            : PgCell(String(col ? col : ""), val, PgDataType::TEXT) {}
        PgCell(const char* col, int32_t val)
            : PgCell(String(col ? col : ""), String(val), PgDataType::INTEGER) {}
        PgCell(const char* col, uint32_t val)
            : PgCell(String(col ? col : ""), String(val), PgDataType::BIGINT) {}
        PgCell(const char* col, int64_t val) {
            char buf[24]{};
            snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(val));
            setColumnName(String(col ? col : ""));
            setValue(String(buf), PgDataType::BIGINT);
        }
        PgCell(const char* col, uint64_t val) {
            char buf[24]{};
            snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(val));
            setColumnName(String(col ? col : ""));
            setValue(String(buf), PgDataType::BIGINT);
        }
        PgCell(const char* col, int16_t val)
            : PgCell(String(col ? col : ""), String(val), PgDataType::SMALLINT) {}
        PgCell(const char* col, uint16_t val)
            : PgCell(String(col ? col : ""), String(val), PgDataType::SMALLINT) {}
        PgCell(const char* col, bool val)
            : PgCell(String(col ? col : ""), val ? String("TRUE") : String("FALSE"), PgDataType::BOOLEAN) {}
        PgCell(const char* col, float val, uint8_t dec = 2)
            : PgCell(String(col ? col : ""), String(val, static_cast<unsigned int>(dec)), PgDataType::REAL) {}

        template <typename TEnum, typename = std::enable_if_t<std::is_enum_v<TEnum>>>
        PgCell(const char* col, TEnum val)
            : PgCell(String(col ? col : ""), String(DbCommon::Dto::PgEnumTraits<TEnum>::toString(val)), PgDataType::ENUM) {}

        void setColumnName(const String& col) {
            strncpy(columnName, col.c_str(), sizeof(columnName) - 1);
            columnName[sizeof(columnName) - 1] = '\0';
        }

        void setValue(const String& literal, PgDataType dt = PgDataType::TEXT) {
            type = dt;
            DataTypeUtils::syncLiteralToUnion(dt, literal, rawText, num, isNull);
        }

        int64_t asInt64() const {
            if (isNull) return 0;
            if (DataTypeUtils::isIntegerType(type) || DataTypeUtils::isTemporalOrEpochComparable(type)) {
                return num.i64;
            }
            return atoll(rawText.c_str());
        }

        double asDouble() const {
            if (isNull) return 0.0;
            if (DataTypeUtils::isFloatingOrDecimalType(type)) {
                return num.f64;
            }
            return strtod(rawText.c_str(), nullptr);
        }

        float asFloat() const {
            return static_cast<float>(asDouble());
        }

        bool asBool() const {
            if (isNull) return false;
            if (type == PgDataType::BOOLEAN) return num.b;
            return rawText.equalsIgnoreCase("TRUE") || rawText == "1" || rawText.equalsIgnoreCase("T");
        }
    };

    class PgTuple {
    private:
        std::vector<PgCell> cells;

    public:
        PgTuple() = default;

        void reserve(size_t n) { cells.reserve(n); }

        const PgCell* findCell(const String& colName) const {
            const char* target = colName.c_str();
            for (const auto& c : cells) {
                if (strcasecmp(c.columnName, target) == 0) return &c;
            }
            const char* targetDot = strchr(target, '.');
            for (const auto& c : cells) {
                const char* cellDot = strchr(c.columnName, '.');
                if (cellDot && strcasecmp(cellDot + 1, target) == 0) return &c;
                if (targetDot && strcasecmp(c.columnName, targetDot + 1) == 0) return &c;
            }
            return nullptr;
        }

        PgCell* findCellMutable(const String& colName) {
            const char* target = colName.c_str();
            for (auto& c : cells) {
                if (strcasecmp(c.columnName, target) == 0) return &c;
            }
            return nullptr;
        }

        void set(const String& colName, const String& value, PgDataType dt = PgDataType::TEXT) {
            PgCell* existing = findCellMutable(colName);
            if (existing) {
                existing->setValue(value, dt);
                return;
            }
            cells.emplace_back(colName, value, dt);
        }

        void removeColumn(const String& colName) {
            const char* target = colName.c_str();
            for (auto it = cells.begin(); it != cells.end(); ++it) {
                if (strcasecmp(it->columnName, target) == 0) {
                    cells.erase(it);
                    return;
                }
            }
        }

        String getText(const char* colName, const char* fallback = "") const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->rawText : String(fallback);
        }

        const char* getRawText(const char* colName, const char* defaultVal = "") const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->rawText.c_str() : defaultVal;
        }

        int16_t getSmallInt(const char* colName, int16_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? static_cast<int16_t>(c->asInt64()) : fallback;
        }

        int32_t getInt(const char* colName, int32_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? static_cast<int32_t>(c->asInt64()) : fallback;
        }

        int64_t getBigInt(const char* colName, int64_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asInt64() : fallback;
        }

        float getFloat(const char* colName, float fallback = 0.0f) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asFloat() : fallback;
        }

        double getDouble(const char* colName, double fallback = 0.0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asDouble() : fallback;
        }

        bool getBool(const char* colName, bool fallback = false) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asBool() : fallback;
        }

        RTOS::PgDate getDate(const char* colName, const char* fallback = "1970-01-01") const {
            const PgCell* c = findCell(colName);
            return RTOS::PgDate((c && !c->isNull) ? c->rawText.c_str() : fallback);
        }

        RTOS::PgDateTime getDateTime(const char* colName, const char* fallback = "1970-01-01 00:00:00") const {
            const PgCell* c = findCell(colName);
            return RTOS::PgDateTime((c && !c->isNull) ? c->rawText.c_str() : fallback);
        }

        int64_t getEpochSeconds(const char* colName, int64_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asInt64() : fallback;
        }

        bool isNull(const char* colName) const {
            const PgCell* c = findCell(colName);
            return (!c || c->isNull);
        }

        const std::vector<PgCell>& allCells() const { return cells; }
        std::vector<PgCell>& allCellsMutable() { return cells; }
        bool empty() const { return cells.empty(); }
        size_t size() const { return cells.size(); }

        template <typename TDto, typename MapperFn>
        TDto mapToStruct(MapperFn mapper) const {
            return mapper(*this);
        }

        String serializeTsv() const {
            String line = "";
            line.reserve(cells.size() * 24);
            for (size_t i = 0; i < cells.size(); ++i) {
                if (i > 0) line += '\t';
                line += cells[i].columnName;
                line += '=';
                line += cells[i].rawText;
            }
            line += '\n';
            return line;
        }

        static PgTuple deserializeTsvFromCStr(const char* rawLine, const DbCommon::Engine::TablePhysicalSchema* schema = nullptr) {
            PgTuple tuple;
            if (!rawLine) return tuple;

            while (*rawLine == ' ' || *rawLine == '\t' || *rawLine == '\r' || *rawLine == '\n') {
                ++rawLine;
            }
            if (*rawLine == '\0') return tuple;

            if (schema) tuple.reserve(schema->columns.size());

            const char* p = rawLine;
            while (*p != '\0' && *p != '\r' && *p != '\n') {
                const char* tab = strchr(p, '\t');
                const char* endToken = tab ? tab : (p + strlen(p));
                while (endToken > p && (*(endToken - 1) == '\r' || *(endToken - 1) == '\n')) {
                    --endToken;
                }

                const char* eq = static_cast<const char*>(memchr(p, '=', static_cast<size_t>(endToken - p)));
                if (eq != nullptr) {
                    char colBuf[32];
                    size_t colLen = static_cast<size_t>(eq - p);
                    if (colLen >= sizeof(colBuf)) colLen = sizeof(colBuf) - 1;
                    memcpy(colBuf, p, colLen);
                    colBuf[colLen] = '\0';

                    const char* valStart = eq + 1;
                    size_t valLen = static_cast<size_t>(endToken - valStart);
                    String valStr = "";
                    if (valLen > 0) {
                        valStr.reserve(valLen);
                        valStr.concat(valStart, static_cast<unsigned int>(valLen));
                    }

                    PgDataType dt = PgDataType::TEXT;
                    if (schema) {
                        const auto* colDef = schema->findColumn(colBuf);
                        if (colDef) dt = colDef->dataType;
                    }
                    tuple.set(colBuf, valStr, dt);
                }

                if (!tab) break;
                p = tab + 1;
            }
            return tuple;
        }

        static PgTuple deserializeTsv(const String& line, const DbCommon::Engine::TablePhysicalSchema* schema = nullptr) {
            return deserializeTsvFromCStr(line.c_str(), schema);
        }

        struct CellView {
            const PgCell* cell;
            const char* c_str(const char* def = "") const {
                return (cell && !cell->isNull) ? cell->rawText.c_str() : def;
            }
            operator const char*() const { return c_str(""); }
            operator String() const      { return String(c_str("")); }
            operator int64_t() const     { return (cell && !cell->isNull) ? cell->asInt64() : 0; }
            operator int32_t() const     { return static_cast<int32_t>((cell && !cell->isNull) ? cell->asInt64() : 0); }
            operator int16_t() const     { return static_cast<int16_t>((cell && !cell->isNull) ? cell->asInt64() : 0); }
            operator uint32_t() const    { return static_cast<uint32_t>((cell && !cell->isNull) ? cell->asInt64() : 0); }
            operator uint16_t() const    { return static_cast<uint16_t>((cell && !cell->isNull) ? cell->asInt64() : 0); }
            operator uint8_t() const     { return static_cast<uint8_t>((cell && !cell->isNull) ? cell->asInt64() : 0); }
            operator bool() const        { return (cell && !cell->isNull) ? cell->asBool() : false; }
            operator float() const       { return (cell && !cell->isNull) ? cell->asFloat() : 0.0f; }
            operator double() const      { return (cell && !cell->isNull) ? cell->asDouble() : 0.0; }
            template <typename TEnum, typename = std::enable_if_t<std::is_enum_v<TEnum>>>
            operator TEnum() const {
                return DbCommon::Dto::PgEnumTraits<TEnum>::fromString(c_str(""), static_cast<TEnum>(0));
            }

            template <size_t N>
            operator PgText<N>() const { return PgText<N>(c_str("")); }

            template <size_t N>
            void operator>>(char (&dest)[N]) const {
                strncpy(dest, c_str(""), N - 1);
                dest[N - 1] = '\0';
            }

            template <typename T>
            void operator>>(T& dest) const {
                dest = static_cast<T>(*this);
            }
        };

        CellView operator[](const char* colName) const {
            return CellView{ findCell(colName) };
        }
    };

} // namespace DbRuntime::Storage