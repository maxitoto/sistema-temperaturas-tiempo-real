#pragma once
#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../common/PhysicalSchemaCatalog.hpp"
#include "../common/PostgresDataTypes.hpp"
#include "../common/PostgresConstraints.hpp"
#include "../common/PgDtoTypes.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../../rtos/RelojSistema.hpp"

namespace DbRuntime::Storage {

    using DbCommon::Engine::PhysicalSchemaCatalog;
    using DbCommon::Engine::TablePhysicalSchema;
    using DbCommon::Standard::ColumnConstraintDef;
    using DbCommon::Standard::DataTypeUtils;
    using DbCommon::Standard::PgBinHeader;
    using DbCommon::Standard::PgDataType;

    static constexpr uint8_t SLOT_FLAG_ACTIVE = 0x01;
    static constexpr size_t  MAX_RECORD_SIZE  = 640; // AuditoriaAuth = 560 B; resto <= 380 B

    // ========================================================================
    // 1. VISTA DE SOLO LECTURA SOBRE UN REGISTRO BINARIO (CERO HEAP)
    // ========================================================================
    class PgRowView {
    protected:
        const TablePhysicalSchema* schema = nullptr;
        const uint8_t* data = nullptr;

        bool colValida(int idx) const {
            return schema != nullptr && data != nullptr &&
                   idx >= 0 && static_cast<size_t>(idx) < schema->columns.size();
        }

    public:
        PgRowView() = default;
        PgRowView(const TablePhysicalSchema* sch, const uint8_t* raw)
            : schema(sch), data(raw) {}

        const TablePhysicalSchema* getSchema() const { return schema; }
        const uint8_t* rawBytes() const { return data; }

        bool isActive() const {
            return data != nullptr && (data[0] & SLOT_FLAG_ACTIVE) != 0;
        }

        uint32_t nullMask() const {
            if (!data) return 0xFFFFFFFFu;
            uint32_t m = 0;
            memcpy(&m, data + 4, sizeof(m));
            return m;
        }

        bool isNullIdx(int idx) const {
            if (!colValida(idx) || idx >= 32) return true;
            return (nullMask() & (1u << idx)) != 0;
        }

        bool isNull(const char* colName) const {
            return !schema ? true : isNullIdx(schema->findColumnIndex(colName));
        }

        int16_t getInt16Idx(int idx, int16_t fallback = 0) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            const uint8_t* p = data + col.binOffset;
            if (col.dataType == PgDataType::SMALLINT) {
                int16_t v = 0; memcpy(&v, p, sizeof(v)); return v;
            }
            if (col.dataType == PgDataType::INTEGER) {
                int32_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int16_t>(v);
            }
            if (col.dataType == PgDataType::BIGINT) {
                int64_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int16_t>(v);
            }
            return fallback;
        }

        int32_t getInt32Idx(int idx, int32_t fallback = 0) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            const uint8_t* p = data + col.binOffset;
            if (col.dataType == PgDataType::INTEGER) {
                int32_t v = 0; memcpy(&v, p, sizeof(v)); return v;
            }
            if (col.dataType == PgDataType::SMALLINT) {
                int16_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int32_t>(v);
            }
            if (col.dataType == PgDataType::BIGINT) {
                int64_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int32_t>(v);
            }
            return fallback;
        }

        uint32_t getUInt32Idx(int idx, uint32_t fallback = 0) const {
            return static_cast<uint32_t>(getInt64Idx(idx, static_cast<int64_t>(fallback)));
        }

        int64_t getInt64Idx(int idx, int64_t fallback = 0) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            const uint8_t* p = data + col.binOffset;
            if (col.dataType == PgDataType::BIGINT) {
                int64_t v = 0; memcpy(&v, p, sizeof(v)); return v;
            }
            if (col.dataType == PgDataType::INTEGER) {
                int32_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int64_t>(v);
            }
            if (col.dataType == PgDataType::SMALLINT) {
                int16_t v = 0; memcpy(&v, p, sizeof(v)); return static_cast<int64_t>(v);
            }
            return fallback;
        }

        uint64_t getUInt64Idx(int idx, uint64_t fallback = 0) const {
            return static_cast<uint64_t>(getInt64Idx(idx, static_cast<int64_t>(fallback)));
        }

        float getFloatIdx(int idx, float fallback = 0.0f) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            const uint8_t* p = data + col.binOffset;
            if (col.dataType == PgDataType::REAL) {
                float v = 0.0f; memcpy(&v, p, sizeof(v)); return v;
            }
            if (DataTypeUtils::isFloatingOrDecimalType(col.dataType)) {
                double v = 0.0; memcpy(&v, p, sizeof(v)); return static_cast<float>(v);
            }
            return fallback;
        }

        double getDoubleIdx(int idx, double fallback = 0.0) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            const uint8_t* p = data + col.binOffset;
            if (col.dataType == PgDataType::REAL) {
                float v = 0.0f; memcpy(&v, p, sizeof(v)); return static_cast<double>(v);
            }
            if (DataTypeUtils::isFloatingOrDecimalType(col.dataType)) {
                double v = 0.0; memcpy(&v, p, sizeof(v)); return v;
            }
            return fallback;
        }

        bool getBoolIdx(int idx, bool fallback = false) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            return data[col.binOffset] != 0;
        }

        uint8_t getEnumU8Idx(int idx, uint8_t fallback = 0) const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            return data[col.binOffset];
        }

        const char* getCStrIdx(int idx, const char* fallback = "") const {
            if (!colValida(idx) || isNullIdx(idx)) return fallback;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            return reinterpret_cast<const char*>(data + col.binOffset);
        }

        // Accesores por nombre de columna (cero heap)
        int16_t     getInt16(const char* col, int16_t def = 0) const     { return schema ? getInt16Idx(schema->findColumnIndex(col), def) : def; }
        int32_t     getInt32(const char* col, int32_t def = 0) const     { return schema ? getInt32Idx(schema->findColumnIndex(col), def) : def; }
        uint32_t    getUInt32(const char* col, uint32_t def = 0) const   { return schema ? getUInt32Idx(schema->findColumnIndex(col), def) : def; }
        int64_t     getInt64(const char* col, int64_t def = 0) const     { return schema ? getInt64Idx(schema->findColumnIndex(col), def) : def; }
        uint64_t    getUInt64(const char* col, uint64_t def = 0) const   { return schema ? getUInt64Idx(schema->findColumnIndex(col), def) : def; }
        float       getFloat(const char* col, float def = 0.0f) const    { return schema ? getFloatIdx(schema->findColumnIndex(col), def) : def; }
        double      getDouble(const char* col, double def = 0.0) const   { return schema ? getDoubleIdx(schema->findColumnIndex(col), def) : def; }
        bool        getBool(const char* col, bool def = false) const     { return schema ? getBoolIdx(schema->findColumnIndex(col), def) : def; }
        uint8_t     getEnumU8(const char* col, uint8_t def = 0) const    { return schema ? getEnumU8Idx(schema->findColumnIndex(col), def) : def; }
        const char* getCStr(const char* col, const char* def = "") const { return schema ? getCStrIdx(schema->findColumnIndex(col), def) : def; }

        template <typename TEnum>
        TEnum getEnum(const char* col, TEnum def = static_cast<TEnum>(0)) const {
            return static_cast<TEnum>(getEnumU8(col, static_cast<uint8_t>(def)));
        }

        template <size_t N>
        void copyToPgChar(const char* col, PgChar<N>& dest) const {
            dest.asignar(getCStr(col, ""));
        }

        // Formatea cualquier celda a texto en un buffer de stack (usado por SELECT en Terminal SQL)
        void formatCellToBuf(int idx, char* outBuf, size_t outCap) const {
            if (!outBuf || outCap == 0) return;
            if (!colValida(idx) || isNullIdx(idx)) {
                strncpy(outBuf, "NULL", outCap - 1);
                outBuf[outCap - 1] = '\0';
                return;
            }
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            switch (col.dataType) {
                case PgDataType::SMALLINT:
                    snprintf(outBuf, outCap, "%d", static_cast<int>(getInt16Idx(idx)));
                    break;
                case PgDataType::INTEGER:
                    snprintf(outBuf, outCap, "%ld", static_cast<long>(getInt32Idx(idx)));
                    break;
                case PgDataType::BIGINT:
                    snprintf(outBuf, outCap, "%lld", static_cast<long long>(getInt64Idx(idx)));
                    break;
                case PgDataType::REAL:
                    snprintf(outBuf, outCap, "%.2f", static_cast<double>(getFloatIdx(idx)));
                    break;
                case PgDataType::DOUBLE_PRECISION:
                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:
                    snprintf(outBuf, outCap, "%.4f", getDoubleIdx(idx));
                    break;
                case PgDataType::BOOLEAN:
                    strncpy(outBuf, getBoolIdx(idx) ? "TRUE" : "FALSE", outCap - 1);
                    outBuf[outCap - 1] = '\0';
                    break;
                case PgDataType::ENUM: {
                    const uint8_t val = getEnumU8Idx(idx);
                    const auto* eDef = PhysicalSchemaCatalog::getInstance().getEnum(col.customTypeName);
                    if (eDef && val < eDef->labels.size()) {
                        strncpy(outBuf, eDef->labels[val].c_str(), outCap - 1);
                        outBuf[outCap - 1] = '\0';
                    } else {
                        snprintf(outBuf, outCap, "%u", static_cast<unsigned>(val));
                    }
                    break;
                }
                default:
                    strncpy(outBuf, getCStrIdx(idx, ""), outCap - 1);
                    outBuf[outCap - 1] = '\0';
                    break;
            }
        }

        // Comparación tipada sin heap contra un literal de texto SQL (para WHERE en PgSqlEngine)
        int compareCellWithLiteral(int idx, const char* rhsLiteral) const {
            if (!colValida(idx) || !rhsLiteral) return -1;
            const auto& col = schema->columns[static_cast<size_t>(idx)];

            if (DataTypeUtils::isIntegerType(col.dataType)) {
                const int64_t a = getInt64Idx(idx);
                const int64_t b = atoll(rhsLiteral);
                return (a < b) ? -1 : ((a > b) ? 1 : 0);
            }
            if (DataTypeUtils::isFloatingOrDecimalType(col.dataType)) {
                const double a = getDoubleIdx(idx);
                const double b = strtod(rhsLiteral, nullptr);
                if (fabs(a - b) < 0.00001) return 0;
                return (a < b) ? -1 : 1;
            }
            if (col.dataType == PgDataType::BOOLEAN) {
                const bool a = getBoolIdx(idx);
                const bool b = (strcasecmp(rhsLiteral, "TRUE") == 0 || strcmp(rhsLiteral, "1") == 0 || strcasecmp(rhsLiteral, "T") == 0);
                return (a == b) ? 0 : (a ? 1 : -1);
            }
            if (col.dataType == PgDataType::ENUM) {
                char labelBuf[40]{};
                formatCellToBuf(idx, labelBuf, sizeof(labelBuf));
                return strcasecmp(labelBuf, rhsLiteral);
            }
            return strcmp(getCStrIdx(idx, ""), rhsLiteral);
        }
    };

    // ========================================================================
    // 2. BUFFER MUTABLE DE REGISTRO BINARIO EN STACK (640 B, CERO HEAP)
    // ========================================================================
    class PgRowBuffer {
    private:
        const TablePhysicalSchema* schema = nullptr;
        uint8_t  buf[MAX_RECORD_SIZE]{};
        uint32_t providedMask = 0;

        bool colValida(int idx) const {
            return schema != nullptr && idx >= 0 && static_cast<size_t>(idx) < schema->columns.size();
        }

        void markNullBit(int idx, bool isNullVal) {
            if (idx < 0 || idx >= 32) return;
            uint32_t mask = 0;
            memcpy(&mask, buf + 4, sizeof(mask));
            if (isNullVal) mask |= (1u << idx);
            else           mask &= ~(1u << idx);
            memcpy(buf + 4, &mask, sizeof(mask));
        }

        void markProvided(int idx) {
            if (idx >= 0 && idx < 32) providedMask |= (1u << idx);
        }

    public:
        PgRowBuffer() = default;

        explicit PgRowBuffer(const TablePhysicalSchema* sch) {
            resetForSchema(sch);
        }

        void resetForSchema(const TablePhysicalSchema* sch) {
            schema       = sch;
            providedMask = 0;
            const size_t n = (sch && sch->recordSize <= MAX_RECORD_SIZE) ? sch->recordSize : MAX_RECORD_SIZE;
            memset(buf, 0, n);
            buf[0] = SLOT_FLAG_ACTIVE;
        }

        void loadRaw(const TablePhysicalSchema* sch, const uint8_t* rawBytes) {
            schema       = sch;
            providedMask = 0;
            if (!sch || !rawBytes || sch->recordSize > MAX_RECORD_SIZE) return;
            memcpy(buf, rawBytes, sch->recordSize);
        }

        const TablePhysicalSchema* getSchema() const { return schema; }
        uint8_t*       rawMutable() { return buf; }
        const uint8_t* rawBytes() const { return buf; }
        uint16_t       recordSize() const { return schema ? schema->recordSize : 0; }
        bool           wasProvidedIdx(int idx) const { return (idx >= 0 && idx < 32) && ((providedMask & (1u << idx)) != 0); }

        PgRowView view() const { return PgRowView(schema, buf); }

        void setActive(bool active) {
            if (active) buf[0] |= SLOT_FLAG_ACTIVE;
            else        buf[0] &= static_cast<uint8_t>(~SLOT_FLAG_ACTIVE);
        }

        void setNullIdx(int idx) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            memset(buf + col.binOffset, 0, col.binSize);
            markNullBit(idx, true);
            markProvided(idx);
        }

        void setInt16Idx(int idx, int16_t v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            if (col.dataType == PgDataType::SMALLINT)     memcpy(buf + col.binOffset, &v, sizeof(v));
            else if (col.dataType == PgDataType::INTEGER) { int32_t w = v; memcpy(buf + col.binOffset, &w, sizeof(w)); }
            else if (col.dataType == PgDataType::BIGINT)  { int64_t w = v; memcpy(buf + col.binOffset, &w, sizeof(w)); }
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setInt32Idx(int idx, int32_t v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            if (col.dataType == PgDataType::INTEGER)       memcpy(buf + col.binOffset, &v, sizeof(v));
            else if (col.dataType == PgDataType::SMALLINT) { int16_t w = static_cast<int16_t>(v); memcpy(buf + col.binOffset, &w, sizeof(w)); }
            else if (col.dataType == PgDataType::BIGINT)   { int64_t w = v; memcpy(buf + col.binOffset, &w, sizeof(w)); }
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setUInt32Idx(int idx, uint32_t v) {
            setInt64Idx(idx, static_cast<int64_t>(v));
        }

        void setInt64Idx(int idx, int64_t v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            if (col.dataType == PgDataType::BIGINT)        memcpy(buf + col.binOffset, &v, sizeof(v));
            else if (col.dataType == PgDataType::INTEGER)  { int32_t w = static_cast<int32_t>(v); memcpy(buf + col.binOffset, &w, sizeof(w)); }
            else if (col.dataType == PgDataType::SMALLINT) { int16_t w = static_cast<int16_t>(v); memcpy(buf + col.binOffset, &w, sizeof(w)); }
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setFloatIdx(int idx, float v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            if (col.dataType == PgDataType::REAL) memcpy(buf + col.binOffset, &v, sizeof(v));
            else { double d = static_cast<double>(v); memcpy(buf + col.binOffset, &d, sizeof(d)); }
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setDoubleIdx(int idx, double v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            if (col.dataType == PgDataType::REAL) { float f = static_cast<float>(v); memcpy(buf + col.binOffset, &f, sizeof(f)); }
            else memcpy(buf + col.binOffset, &v, sizeof(v));
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setBoolIdx(int idx, bool v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            buf[col.binOffset] = v ? 1 : 0;
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setEnumU8Idx(int idx, uint8_t v) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            buf[col.binOffset] = v;
            markNullBit(idx, false);
            markProvided(idx);
        }

        void setTextIdx(int idx, const char* s) {
            if (!colValida(idx)) return;
            const auto& col = schema->columns[static_cast<size_t>(idx)];
            memset(buf + col.binOffset, 0, col.binSize);
            if (s != nullptr && col.binSize > 1) {
                strncpy(reinterpret_cast<char*>(buf + col.binOffset), s, col.binSize - 1);
            }
            markNullBit(idx, false);
            markProvided(idx);
        }

        // Mutadores por nombre de columna
        void setNull(const char* col)                { if (schema) setNullIdx(schema->findColumnIndex(col)); }
        void setInt16(const char* col, int16_t v)    { if (schema) setInt16Idx(schema->findColumnIndex(col), v); }
        void setInt32(const char* col, int32_t v)    { if (schema) setInt32Idx(schema->findColumnIndex(col), v); }
        void setUInt32(const char* col, uint32_t v)  { if (schema) setUInt32Idx(schema->findColumnIndex(col), v); }
        void setInt64(const char* col, int64_t v)    { if (schema) setInt64Idx(schema->findColumnIndex(col), v); }
        void setUInt64(const char* col, uint64_t v)  { if (schema) setInt64Idx(schema->findColumnIndex(col), static_cast<int64_t>(v)); }
        void setFloat(const char* col, float v)      { if (schema) setFloatIdx(schema->findColumnIndex(col), v); }
        void setDouble(const char* col, double v)    { if (schema) setDoubleIdx(schema->findColumnIndex(col), v); }
        void setBool(const char* col, bool v)        { if (schema) setBoolIdx(schema->findColumnIndex(col), v); }
        void setEnumU8(const char* col, uint8_t v)   { if (schema) setEnumU8Idx(schema->findColumnIndex(col), v); }
        void setText(const char* col, const char* s) { if (schema) setTextIdx(schema->findColumnIndex(col), s); }

        template <typename TEnum>
        void setEnum(const char* col, TEnum v) {
            setEnumU8(col, static_cast<uint8_t>(v));
        }

        // Parsea un literal SQL y lo escribe en la columna con su tipo binario exacto
        bool setFromLiteralIdx(int idx, const char* literal, String* outSqlState = nullptr, String* outErr = nullptr) {
            if (!colValida(idx)) return false;
            const auto& col = schema->columns[static_cast<size_t>(idx)];

            if (literal == nullptr || strcasecmp(literal, "NULL") == 0) {
                if (col.isNotNull) {
                    if (outSqlState) *outSqlState = DbCommon::Standard::SqlState::NOT_NULL_VIOLATION;
                    if (outErr) *outErr = "null value in column \"" + col.columnName + "\" violates not-null constraint";
                    return false;
                }
                setNullIdx(idx);
                return true;
            }

            if (col.dataType == PgDataType::ENUM) {
                const auto* eDef = PhysicalSchemaCatalog::getInstance().getEnum(col.customTypeName);
                if (eDef) {
                    for (size_t k = 0; k < eDef->labels.size(); ++k) {
                        if (eDef->labels[k].equalsIgnoreCase(literal)) {
                            setEnumU8Idx(idx, static_cast<uint8_t>(k));
                            return true;
                        }
                    }
                }
                if (outSqlState) *outSqlState = DbCommon::Standard::SqlState::INVALID_TEXT_REPRESENTATION;
                if (outErr) *outErr = "invalid input value for enum " + col.customTypeName + ": \"" + String(literal) + "\"";
                return false;
            }

            // Resuelve palabras temporales CURRENT_TIMESTAMP / CURRENT_DATE / CURRENT_TIME / NOW()
            if (DataTypeUtils::isTemporalOrEpochComparable(col.dataType) &&
                (strcasecmp(literal, "CURRENT_TIMESTAMP") == 0 ||
                 strcasecmp(literal, "NOW()") == 0 ||
                 strcasecmp(literal, "CURRENT_DATE") == 0 ||
                 strcasecmp(literal, "CURRENT_TIME") == 0)) {
                String resolved = DataTypeUtils::resolveTemporalKeywords(col.dataType, String(literal));
                setTextIdx(idx, resolved.c_str());
                return true;
            }

            switch (col.dataType) {
                case PgDataType::SMALLINT:
                    setInt16Idx(idx, static_cast<int16_t>(atoi(literal)));
                    return true;
                case PgDataType::INTEGER:
                    setInt32Idx(idx, static_cast<int32_t>(atol(literal)));
                    return true;
                case PgDataType::BIGINT:
                    setInt64Idx(idx, static_cast<int64_t>(atoll(literal)));
                    return true;
                case PgDataType::REAL:
                    setFloatIdx(idx, strtof(literal, nullptr));
                    return true;
                case PgDataType::DOUBLE_PRECISION:
                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:
                    setDoubleIdx(idx, strtod(literal, nullptr));
                    return true;
                case PgDataType::BOOLEAN: {
                    const bool isT = (strcasecmp(literal, "TRUE") == 0 || strcmp(literal, "1") == 0 || strcasecmp(literal, "T") == 0);
                    const bool isF = (strcasecmp(literal, "FALSE") == 0 || strcmp(literal, "0") == 0 || strcasecmp(literal, "F") == 0);
                    if (!isT && !isF) {
                        if (outSqlState) *outSqlState = DbCommon::Standard::SqlState::INVALID_TEXT_REPRESENTATION;
                        if (outErr) *outErr = "invalid input syntax for type boolean: \"" + String(literal) + "\"";
                        return false;
                    }
                    setBoolIdx(idx, isT);
                    return true;
                }
                default:
                    setTextIdx(idx, literal);
                    return true;
            }
        }

        bool setFromLiteral(const char* col, const char* literal, String* outSqlState = nullptr, String* outErr = nullptr) {
            return schema ? setFromLiteralIdx(schema->findColumnIndex(col), literal, outSqlState, outErr) : false;
        }

        // Aplica DEFAULT, CURRENT_TIMESTAMP (auto_now / auto_now_add) y NULLs del esquema
        void applySchemaDefaults(bool isInsert) {
            if (!schema) return;
            for (size_t i = 0; i < schema->columns.size(); ++i) {
                const int idx = static_cast<int>(i);
                const auto& col = schema->columns[i];
                if (col.identityMode != DbCommon::Standard::IdentityGeneration::NONE) {
                    continue; // Lo asigna PgBinaryStorage::appendRecord desde PgBinHeader
                }
                if (wasProvidedIdx(idx)) continue;

                if (!isInsert && col.autoNow) {
                    const char* kw = (col.dataType == PgDataType::DATE) ? "CURRENT_DATE"
                                   : (col.dataType == PgDataType::TIME || col.dataType == PgDataType::TIME_TZ) ? "CURRENT_TIME"
                                   : "CURRENT_TIMESTAMP";
                    setFromLiteralIdx(idx, kw);
                    continue;
                }

                if (isInsert) {
                    if (col.hasDefault) {
                        setFromLiteralIdx(idx, col.defaultValue.c_str());
                    } else if (col.autoNowAdd || col.autoNow) {
                        const char* kw = (col.dataType == PgDataType::DATE) ? "CURRENT_DATE"
                                       : (col.dataType == PgDataType::TIME || col.dataType == PgDataType::TIME_TZ) ? "CURRENT_TIME"
                                       : "CURRENT_TIMESTAMP";
                        setFromLiteralIdx(idx, kw);
                    } else if (!col.isNotNull) {
                        setNullIdx(idx);
                    }
                }
            }
        }
    };

    // ========================================================================
    // 3. MOTOR DE ALMACENAMIENTO BINARIO FIJO SOBRE MICROSD (CERO HEAP)
    // ========================================================================
    class PgBinaryStorage {
    private:
        static constexpr size_t IO_BUF_CAP    = 1024; // Buffer de lectura por bloques en RAM estática
        static constexpr size_t BATCH_BUF_CAP = 512;  // Buffer fijo para TransaccionBatch (cero heap)

        uint8_t     ioBuf[IO_BUF_CAP]{};
        uint8_t     batchDepth = 0;
        char        batchTable[32]{};
        PgBinHeader batchHeader{};
        uint32_t    batchStartSlot = 0;
        size_t      batchBytes     = 0;
        uint8_t     batchBuf[BATCH_BUF_CAP]{};

        PgBinaryStorage() = default;

        static void buildPath(const char* tableName, char* outPath, size_t outCap) {
            snprintf(outPath, outCap, "/db/%s.pgb", tableName ? tableName : "");
        }

        static bool ensureFileLocked(const TablePhysicalSchema* schema, char* outPath, size_t outCap) {
            if (!schema) return false;
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) return false;
            buildPath(schema->tableName.c_str(), outPath, outCap);
            if (SD.exists(outPath)) return true;
            return PhysicalSchemaCatalog::getInstance().ensurePhysicalTableFileExists(schema->tableName);
        }

        static bool readHeader(File& f, PgBinHeader& hdr, uint16_t expectedRecSize) {
            if (!f.seek(0)) return false;
            if (f.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr)) return false;
            return hdr.esValida(expectedRecSize);
        }

        static bool writeHeader(File& f, const PgBinHeader& hdr) {
            if (!f.seek(0)) return false;
            return f.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr)) == sizeof(hdr);
        }

        bool flushBatchLocked() {
            if (batchBytes == 0 || batchTable[0] == '\0') return true;

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) {
                batchBytes = 0;
                batchTable[0] = '\0';
                return false;
            }

            char path[64]{};
            buildPath(batchTable, path, sizeof(path));
            File f = SD.open(path, "r+");
            if (!f) {
                batchBytes = 0;
                batchTable[0] = '\0';
                return false;
            }

            const uint32_t writePos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                      (batchStartSlot * static_cast<uint32_t>(batchHeader.recordSize));
            bool ok = f.seek(writePos) &&
                      (f.write(batchBuf, batchBytes) == batchBytes) &&
                      writeHeader(f, batchHeader);
            f.flush();
            f.close();

            batchBytes    = 0;
            batchTable[0] = '\0';
            return ok;
        }

    public:
        static PgBinaryStorage& getInstance() {
            static PgBinaryStorage instance;
            return instance;
        }

        static TarjetaSD::ScopedLock lockEngine() {
            return TarjetaSD::ScopedLock(TarjetaSD::getInstance().obtenerMutex());
        }

        // --- Soporte de TransaccionBatch (cero heap) ---
        void beginBatch() {
            xSemaphoreTakeRecursive(TarjetaSD::getInstance().obtenerMutex(), portMAX_DELAY);
            batchDepth++;
        }

        bool commitBatch() {
            bool ok = true;
            if (batchDepth > 0) {
                batchDepth--;
                if (batchDepth == 0) {
                    ok = flushBatchLocked();
                }
            }
            xSemaphoreGiveRecursive(TarjetaSD::getInstance().obtenerMutex());
            return ok;
        }

        // --- Sincronización de arranque con /db/pg_catalog.cat (V2) ---
        bool bindToPhysicalCatalog() {
            auto lk = lockEngine();
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.getAllTables().empty()) {
                if (!cat.loadCatalogFromSingleFile()) {
                    return false;
                }
            }
            for (const auto& kv : cat.getAllTables()) {
                cat.ensurePhysicalTableFileExists(kv.second.tableName);
            }
            return !cat.getAllTables().empty();
        }

        // --- Metadatos O(1): lee solo 32 bytes de cabecera ---
        bool getTableHeader(const char* tableName, PgBinHeader& outHdr) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return false;

            File f = SD.open(path, FILE_READ);
            if (!f) return false;
            const bool ok = readHeader(f, outHdr, schema->recordSize);
            f.close();
            return ok;
        }

        // --- INSERT O(1): asigna IDENTITY, aplica defaults y anexa al final ---
        bool appendRecord(const char* tableName, PgRowBuffer& row, int64_t* outAssignedId = nullptr) {
            auto lk = lockEngine();
            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema || schema->recordSize > MAX_RECORD_SIZE) return false;

            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return false;

            row.setActive(true);
            row.applySchemaDefaults(true);

            // Modo batch: acumula en batchBuf[512] y escribe todo en una sola ráfaga SD
            if (batchDepth > 0 && schema->recordSize <= BATCH_BUF_CAP) {
                if (batchBytes > 0 &&
                    (strcasecmp(batchTable, schema->tableName.c_str()) != 0 ||
                     (batchBytes + schema->recordSize) > BATCH_BUF_CAP)) {
                    if (!flushBatchLocked()) return false;
                }

                if (batchBytes == 0) {
                    File fHdr = SD.open(path, FILE_READ);
                    if (!fHdr) return false;
                    const bool hdrOk = readHeader(fHdr, batchHeader, schema->recordSize);
                    fHdr.close();
                    if (!hdrOk) return false;

                    strncpy(batchTable, schema->tableName.c_str(), sizeof(batchTable) - 1);
                    batchTable[sizeof(batchTable) - 1] = '\0';
                    batchStartSlot = batchHeader.totalSlots;
                }

                if (schema->identityColIndex >= 0) {
                    const int idIdx = schema->identityColIndex;
                    const auto& idCol = schema->columns[static_cast<size_t>(idIdx)];
                    int64_t val = row.view().getInt64Idx(idIdx, 0);
                    if (row.view().isNullIdx(idIdx) || val <= 0) {
                        val = batchHeader.nextIdentityId;
                        batchHeader.nextIdentityId += idCol.identityIncrement;
                        row.setInt64Idx(idIdx, val);
                    } else if (val >= batchHeader.nextIdentityId) {
                        batchHeader.nextIdentityId = val + idCol.identityIncrement;
                    }
                    if (outAssignedId) *outAssignedId = val;
                }

                memcpy(batchBuf + batchBytes, row.rawBytes(), schema->recordSize);
                batchBytes += schema->recordSize;
                batchHeader.totalSlots++;
                batchHeader.activeRows++;
                return true;
            }

            // Modo inmediato: una sola apertura "r+", escribe registro + actualiza cabecera 32 B
            if (batchBytes > 0) flushBatchLocked();

            File f = SD.open(path, "r+");
            if (!f) return false;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize)) {
                f.close();
                return false;
            }

            if (schema->identityColIndex >= 0) {
                const int idIdx = schema->identityColIndex;
                const auto& idCol = schema->columns[static_cast<size_t>(idIdx)];
                int64_t val = row.view().getInt64Idx(idIdx, 0);
                if (row.view().isNullIdx(idIdx) || val <= 0) {
                    val = hdr.nextIdentityId;
                    hdr.nextIdentityId += idCol.identityIncrement;
                    row.setInt64Idx(idIdx, val);
                } else if (val >= hdr.nextIdentityId) {
                    hdr.nextIdentityId = val + idCol.identityIncrement;
                }
                if (outAssignedId) *outAssignedId = val;
            }

            const uint32_t writePos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                      (hdr.totalSlots * static_cast<uint32_t>(hdr.recordSize));
            hdr.totalSlots++;
            hdr.activeRows++;

            const bool ok = f.seek(writePos) &&
                            (f.write(row.rawBytes(), hdr.recordSize) == hdr.recordSize) &&
                            writeHeader(f, hdr);
            f.flush();
            f.close();
            return ok;
        }

        // --- Lectura puntual O(1) por índice de slot ---
        bool readSlot(const char* tableName, uint32_t slotIndex, PgRowBuffer& outRow) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return false;

            File f = SD.open(path, FILE_READ);
            if (!f) return false;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || slotIndex >= hdr.totalSlots) {
                f.close();
                return false;
            }

            const uint32_t pos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                 (slotIndex * static_cast<uint32_t>(hdr.recordSize));
            outRow.resetForSchema(schema);
            const bool ok = f.seek(pos) &&
                            (f.read(outRow.rawMutable(), hdr.recordSize) == hdr.recordSize);
            f.close();
            return ok && outRow.view().isActive();
        }

        // --- Escaneo hacia adelante (slot 0 -> N-1) en bloques de 1 KB, 0 heap ---
        template <typename CallbackFn>
        void scanForward(const char* tableName, CallbackFn fn) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return;

            File f = SD.open(path, FILE_READ);
            if (!f) return;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || hdr.totalSlots == 0) {
                f.close();
                return;
            }

            const uint16_t recSize       = hdr.recordSize;
            const uint32_t slotsPerChunk = (IO_BUF_CAP / recSize) > 0 ? static_cast<uint32_t>(IO_BUF_CAP / recSize) : 1u;

            uint32_t slotActual = 0;
            uint16_t yieldCtr   = 0;
            bool stop           = false;

            while (slotActual < hdr.totalSlots && !stop) {
                const uint32_t rem = hdr.totalSlots - slotActual;
                const uint32_t nSlots = (rem > slotsPerChunk) ? slotsPerChunk : rem;
                const size_t bytesReq = static_cast<size_t>(nSlots) * recSize;

                const int got = f.read(ioBuf, bytesReq);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (uint32_t i = 0; i < gotSlots; ++i) {
                    const uint8_t* ptr = ioBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        PgRowView view(schema, ptr);
                        if (!fn(view, slotActual + i)) {
                            stop = true;
                            break;
                        }
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                slotActual += gotSlots;
            }
            f.close();
        }

        // Escaneo hacia adelante usando un buffer externo propio (seguro para JOIN anidados)
        template <typename CallbackFn>
        void scanForwardWithBuf(const char* tableName, uint8_t* extBuf, size_t extCap, CallbackFn fn) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return;
            if (!extBuf || extCap < schema->recordSize) return;

            File f = SD.open(path, FILE_READ);
            if (!f) return;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || hdr.totalSlots == 0) {
                f.close();
                return;
            }

            const uint16_t recSize       = hdr.recordSize;
            const uint32_t slotsPerChunk = (extCap / recSize) > 0 ? static_cast<uint32_t>(extCap / recSize) : 1u;

            uint32_t slotActual = 0;
            uint16_t yieldCtr   = 0;
            bool stop           = false;

            while (slotActual < hdr.totalSlots && !stop) {
                const uint32_t rem    = hdr.totalSlots - slotActual;
                const uint32_t nSlots = (rem > slotsPerChunk) ? slotsPerChunk : rem;
                const size_t bytesReq = static_cast<size_t>(nSlots) * recSize;

                const int got = f.read(extBuf, bytesReq);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (uint32_t i = 0; i < gotSlots; ++i) {
                    const uint8_t* ptr = extBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        PgRowView view(schema, ptr);
                        if (!fn(view, slotActual + i)) {
                            stop = true;
                            break;
                        }
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                slotActual += gotSlots;
            }
            f.close();
        }

        // --- Escaneo inverso (slot N-1 -> 0) en bloques de 1 KB: paginación DESC en < 5 ms ---
        template <typename CallbackFn>
        void scanBackward(const char* tableName, CallbackFn fn) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return;

            File f = SD.open(path, FILE_READ);
            if (!f) return;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || hdr.totalSlots == 0) {
                f.close();
                return;
            }

            const uint16_t recSize       = hdr.recordSize;
            const uint32_t slotsPerChunk = (IO_BUF_CAP / recSize) > 0 ? static_cast<uint32_t>(IO_BUF_CAP / recSize) : 1u;

            uint32_t remaining = hdr.totalSlots;
            uint16_t yieldCtr  = 0;
            bool stop          = false;

            while (remaining > 0 && !stop) {
                const uint32_t nSlots    = (remaining > slotsPerChunk) ? slotsPerChunk : remaining;
                const uint32_t startSlot = remaining - nSlots;
                const uint32_t filePos   = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                           (startSlot * static_cast<uint32_t>(recSize));
                const size_t bytesReq    = static_cast<size_t>(nSlots) * recSize;

                if (!f.seek(filePos)) break;
                const int got = f.read(ioBuf, bytesReq);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (int32_t i = static_cast<int32_t>(gotSlots) - 1; i >= 0; --i) {
                    const uint8_t* ptr = ioBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        PgRowView view(schema, ptr);
                        if (!fn(view, startSlot + static_cast<uint32_t>(i))) {
                            stop = true;
                            break;
                        }
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                remaining = startSlot;
            }
            f.close();
        }

        // --- UPDATE in-place puntual O(1) ---
        bool updateSlotInPlace(const char* tableName, uint32_t slotIndex, PgRowBuffer& updatedRow) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return false;

            File f = SD.open(path, "r+");
            if (!f) return false;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || slotIndex >= hdr.totalSlots) {
                f.close();
                return false;
            }

            updatedRow.setActive(true);
            updatedRow.applySchemaDefaults(false);

            const uint32_t pos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                 (slotIndex * static_cast<uint32_t>(hdr.recordSize));
            const bool ok = f.seek(pos) &&
                            (f.write(updatedRow.rawBytes(), hdr.recordSize) == hdr.recordSize);
            f.flush();
            f.close();
            return ok;
        }

        // --- UPDATE condicional in-place (sin reescribir archivos .tmp) ---
        template <typename PredicateFn, typename MutateFn>
        uint16_t updateWhere(const char* tableName, PredicateFn pred, MutateFn mut, PgRowBuffer* lastUpdatedOut = nullptr) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return 0;

            File f = SD.open(path, "r+");
            if (!f) return 0;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || hdr.totalSlots == 0) {
                f.close();
                return 0;
            }

            const uint16_t recSize       = hdr.recordSize;
            const uint32_t slotsPerChunk = (IO_BUF_CAP / recSize) > 0 ? static_cast<uint32_t>(IO_BUF_CAP / recSize) : 1u;
            uint16_t affected            = 0;
            uint16_t yieldCtr            = 0;
            uint32_t slotActual          = 0;

            PgRowBuffer workRow;

            while (slotActual < hdr.totalSlots) {
                const uint32_t rem      = hdr.totalSlots - slotActual;
                const uint32_t nSlots   = (rem > slotsPerChunk) ? slotsPerChunk : rem;
                const uint32_t chunkPos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                          (slotActual * static_cast<uint32_t>(recSize));

                if (!f.seek(chunkPos)) break;
                const int got = f.read(ioBuf, static_cast<size_t>(nSlots) * recSize);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (uint32_t i = 0; i < gotSlots; ++i) {
                    const uint8_t* ptr = ioBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        PgRowView view(schema, ptr);
                        if (pred(view)) {
                            workRow.loadRaw(schema, ptr);
                            mut(workRow);
                            workRow.setActive(true);
                            workRow.applySchemaDefaults(false);

                            const uint32_t writePos = chunkPos + (i * static_cast<uint32_t>(recSize));
                            if (f.seek(writePos) && f.write(workRow.rawBytes(), recSize) == recSize) {
                                affected++;
                                if (lastUpdatedOut) *lastUpdatedOut = workRow;
                            }
                        }
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                slotActual += gotSlots;
            }

            f.flush();
            f.close();
            return affected;
        }

        // --- DELETE condicional in-place (tombstone en byte 0, < 5 ms) ---
        template <typename PredicateFn>
        uint16_t deleteWhere(const char* tableName, PredicateFn pred, bool compactarSiConviene = false) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return 0;

            File f = SD.open(path, "r+");
            if (!f) return 0;

            PgBinHeader hdr{};
            if (!readHeader(f, hdr, schema->recordSize) || hdr.totalSlots == 0) {
                f.close();
                return 0;
            }

            const uint16_t recSize       = hdr.recordSize;
            const uint32_t slotsPerChunk = (IO_BUF_CAP / recSize) > 0 ? static_cast<uint32_t>(IO_BUF_CAP / recSize) : 1u;
            uint16_t deleted             = 0;
            uint16_t yieldCtr            = 0;
            uint32_t slotActual          = 0;
            const uint8_t deadFlag       = 0x00;

            while (slotActual < hdr.totalSlots) {
                const uint32_t rem      = hdr.totalSlots - slotActual;
                const uint32_t nSlots   = (rem > slotsPerChunk) ? slotsPerChunk : rem;
                const uint32_t chunkPos = static_cast<uint32_t>(sizeof(PgBinHeader)) +
                                          (slotActual * static_cast<uint32_t>(recSize));

                if (!f.seek(chunkPos)) break;
                const int got = f.read(ioBuf, static_cast<size_t>(nSlots) * recSize);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (uint32_t i = 0; i < gotSlots; ++i) {
                    const uint8_t* ptr = ioBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        PgRowView view(schema, ptr);
                        if (pred(view)) {
                            const uint32_t flagPos = chunkPos + (i * static_cast<uint32_t>(recSize));
                            if (f.seek(flagPos) && f.write(&deadFlag, 1) == 1) {
                                deleted++;
                                if (hdr.activeRows > 0) hdr.activeRows--;
                            }
                        }
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                slotActual += gotSlots;
            }

            if (deleted > 0) {
                writeHeader(f, hdr);
                f.flush();
            }
            f.close();

            if (compactarSiConviene && deleted > 0 && hdr.totalSlots >= 32 && (hdr.activeRows * 2 < hdr.totalSlots)) {
                compactTable(tableName);
            }
            return deleted;
        }

        // --- TRUNCATE: reinicia la tabla a 0 filas en O(1) ---
        bool truncateTable(const char* tableName) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) return false;

            char path[64]{};
            buildPath(schema->tableName.c_str(), path, sizeof(path));

            PgBinHeader hdr{};
            int64_t startId = 1;
            if (schema->identityColIndex >= 0) {
                startId = schema->columns[static_cast<size_t>(schema->identityColIndex)].identityStart;
            }
            hdr.inicializar(schema->recordSize, startId);

            File f = SD.open(path, FILE_WRITE);
            if (!f) return false;
            const bool ok = (f.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr)) == sizeof(hdr));
            f.flush();
            f.close();
            return ok;
        }

        // --- VACUUM: compacta físicamente eliminando slots borrados (tombstones) ---
        bool compactTable(const char* tableName) {
            auto lk = lockEngine();
            flushBatchLocked();

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            char path[64]{};
            if (!ensureFileLocked(schema, path, sizeof(path))) return false;

            char tmpPath[72]{};
            snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
            if (SD.exists(tmpPath)) SD.remove(tmpPath);

            File fIn = SD.open(path, FILE_READ);
            if (!fIn) return false;

            PgBinHeader oldHdr{};
            if (!readHeader(fIn, oldHdr, schema->recordSize)) {
                fIn.close();
                return false;
            }

            File fOut = SD.open(tmpPath, FILE_WRITE);
            if (!fOut) {
                fIn.close();
                return false;
            }

            PgBinHeader newHdr = oldHdr;
            newHdr.totalSlots  = 0;
            newHdr.activeRows  = 0;
            fOut.write(reinterpret_cast<const uint8_t*>(&newHdr), sizeof(newHdr));

            const uint16_t recSize       = oldHdr.recordSize;
            const uint32_t slotsPerChunk = (IO_BUF_CAP / recSize) > 0 ? static_cast<uint32_t>(IO_BUF_CAP / recSize) : 1u;
            uint32_t slotActual          = 0;
            uint16_t yieldCtr            = 0;
            bool writeOk                 = true;

            while (slotActual < oldHdr.totalSlots && writeOk) {
                const uint32_t rem    = oldHdr.totalSlots - slotActual;
                const uint32_t nSlots = (rem > slotsPerChunk) ? slotsPerChunk : rem;
                const int got = fIn.read(ioBuf, static_cast<size_t>(nSlots) * recSize);
                if (got < static_cast<int>(recSize)) break;

                const uint32_t gotSlots = static_cast<uint32_t>(got / recSize);
                for (uint32_t i = 0; i < gotSlots; ++i) {
                    const uint8_t* ptr = ioBuf + (static_cast<size_t>(i) * recSize);
                    if ((ptr[0] & SLOT_FLAG_ACTIVE) != 0) {
                        if (fOut.write(ptr, recSize) != recSize) {
                            writeOk = false;
                            break;
                        }
                        newHdr.totalSlots++;
                        newHdr.activeRows++;
                    }
                    if ((++yieldCtr & 127u) == 0) vTaskDelay(1);
                }
                slotActual += gotSlots;
            }

            fIn.close();
            if (writeOk) {
                writeOk = writeHeader(fOut, newHdr);
            }
            fOut.flush();
            fOut.close();

            if (!writeOk) {
                SD.remove(tmpPath);
                return false;
            }

            SD.remove(path);
            return SD.rename(tmpPath, path);
        }

        // --- Migración binaria en streaming para ALTER TABLE ADD / DROP COLUMN ---
        bool migrateTableSchema(const TablePhysicalSchema& oldSchema, const TablePhysicalSchema& newSchema) {
            auto lk = lockEngine();
            flushBatchLocked();

            if (oldSchema.recordSize > MAX_RECORD_SIZE || newSchema.recordSize > MAX_RECORD_SIZE) {
                return false;
            }

            char path[64]{};
            char tmpPath[72]{};
            buildPath(oldSchema.tableName.c_str(), path, sizeof(path));
            snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

            if (!SD.exists(path)) {
                return PhysicalSchemaCatalog::getInstance().ensurePhysicalTableFileExists(newSchema.tableName);
            }
            if (SD.exists(tmpPath)) SD.remove(tmpPath);

            File fIn = SD.open(path, FILE_READ);
            if (!fIn) return false;

            PgBinHeader oldHdr{};
            if (!readHeader(fIn, oldHdr, oldSchema.recordSize)) {
                fIn.close();
                return false;
            }

            File fOut = SD.open(tmpPath, FILE_WRITE);
            if (!fOut) {
                fIn.close();
                return false;
            }

            PgBinHeader newHdr{};
            newHdr.inicializar(newSchema.recordSize, oldHdr.nextIdentityId);
            fOut.write(reinterpret_cast<const uint8_t*>(&newHdr), sizeof(newHdr));

            PgRowBuffer newRow(&newSchema);
            bool ok = true;

            for (uint32_t s = 0; s < oldHdr.totalSlots && ok; ++s) {
                if (fIn.read(ioBuf, oldSchema.recordSize) != oldSchema.recordSize) break;
                if ((ioBuf[0] & SLOT_FLAG_ACTIVE) == 0) continue;

                PgRowView oldView(&oldSchema, ioBuf);
                newRow.resetForSchema(&newSchema);

                for (size_t c = 0; c < newSchema.columns.size(); ++c) {
                    const auto& dstCol = newSchema.columns[c];
                    const int8_t srcIdx = oldSchema.findColumnIndex(dstCol.columnName.c_str());
                    if (srcIdx >= 0) {
                        if (oldView.isNullIdx(srcIdx)) {
                            newRow.setNullIdx(static_cast<int>(c));
                        } else {
                            const auto& srcCol = oldSchema.columns[static_cast<size_t>(srcIdx)];
                            const size_t copyLen = (srcCol.binSize < dstCol.binSize) ? srcCol.binSize : dstCol.binSize;
                            memcpy(newRow.rawMutable() + dstCol.binOffset, ioBuf + srcCol.binOffset, copyLen);
                        }
                    } else if (dstCol.hasDefault) {
                        newRow.setFromLiteralIdx(static_cast<int>(c), dstCol.defaultValue.c_str());
                    } else if (!dstCol.isNotNull) {
                        newRow.setNullIdx(static_cast<int>(c));
                    }
                }

                if (fOut.write(newRow.rawBytes(), newSchema.recordSize) != newSchema.recordSize) {
                    ok = false;
                    break;
                }
                newHdr.totalSlots++;
                newHdr.activeRows++;
                if ((s & 63u) == 0) vTaskDelay(1);
            }

            fIn.close();
            if (ok) ok = writeHeader(fOut, newHdr);
            fOut.flush();
            fOut.close();

            if (!ok) {
                SD.remove(tmpPath);
                return false;
            }
            SD.remove(path);
            return SD.rename(tmpPath, path);
        }
    };

    // Alias de compatibilidad para código que solo pedía el lock de SD (ej. RPSistema.hpp)
    using RuntimeStorageAndIndexManager = PgBinaryStorage;

} // namespace DbRuntime::Storage