#pragma once
#include <Arduino.h>
#include <vector>
#include <stdint.h>
#include <string.h>
#include "PostgresDataTypes.hpp"

namespace DbCommon::Standard {

    enum class ReferentialAction : uint8_t {
        NO_ACTION = 0,
        RESTRICT,
        CASCADE,
        SET_NULL,
        SET_DEFAULT
    };

    enum class MatchType : uint8_t {
        SIMPLE = 0,
        PARTIAL,
        FULL
    };

    enum class IdentityGeneration : uint8_t {
        NONE = 0,
        ALWAYS,
        BY_DEFAULT,
        STORED_EXPRESSION
    };

    enum class IndexMethod : uint8_t {
        BTREE = 0,
        HASH,
        GIST,
        GIN,
        BRIN
    };

    enum class SortDirection : uint8_t { ASC = 0, DESC = 1 };
    enum class NullsOrder : uint8_t { FIRST = 0, LAST = 1 };

    enum class TablePersistence : uint8_t {
        PERMANENT = 0,
        TEMPORARY,
        UNLOGGED
    };

    inline constexpr const char* PGBIN_MAGIC_STR      = "PGBIN01";
    inline constexpr uint16_t    PGBIN_FORMAT_VERSION = 1;

    // Cabecera física fija de 32 bytes al inicio de cada archivo /db/<Tabla>.pgb
    struct PgBinHeader {
        char     magic[8];       // 0..7   ("PGBIN01\0")
        int64_t  nextIdentityId; // 8..15
        uint32_t totalSlots;     // 16..19
        uint32_t activeRows;     // 20..23
        uint16_t version;        // 24..25
        uint16_t recordSize;     // 26..27
        uint32_t reserved;       // 28..31

        void inicializar(uint16_t tamanoRegistro, int64_t inicioIdentity = 1) {
            memset(this, 0, sizeof(PgBinHeader));
            memcpy(magic, PGBIN_MAGIC_STR, 8);
            nextIdentityId = inicioIdentity;
            totalSlots     = 0;
            activeRows     = 0;
            version        = PGBIN_FORMAT_VERSION;
            recordSize     = tamanoRegistro;
            reserved       = 0;
        }

        bool esValida(uint16_t recordSizeEsperado = 0) const {
            if (memcmp(magic, PGBIN_MAGIC_STR, 7) != 0) return false;
            if (version != PGBIN_FORMAT_VERSION || recordSize < 8) return false;
            if (recordSizeEsperado != 0 && recordSize != recordSizeEsperado) return false;
            return true;
        }
    };
    static_assert(sizeof(PgBinHeader) == 32, "PgBinHeader debe medir exactamente 32 bytes");

    // Calcula el tamaño binario fijo en disco de una columna según su PgDataType
    inline uint16_t computeBinaryColumnSize(PgDataType dt, uint16_t maxLength) {
        switch (dt) {
            case PgDataType::SMALLINT:         return 2;
            case PgDataType::INTEGER:          return 4;
            case PgDataType::BIGINT:           return 8;
            case PgDataType::REAL:             return 4;
            case PgDataType::DOUBLE_PRECISION:
            case PgDataType::DECIMAL:
            case PgDataType::NUMERIC:
            case PgDataType::DECFLOAT:         return 8;
            case PgDataType::BOOLEAN:          return 1;
            case PgDataType::ENUM:             return 1;
            case PgDataType::DATE:
            case PgDataType::TIME:
            case PgDataType::TIME_TZ:          return 12;
            case PgDataType::TIMESTAMP:
            case PgDataType::TIMESTAMP_TZ:     return 20;
            case PgDataType::INTERVAL:         return 24;
            case PgDataType::CHAR:
            case PgDataType::NCHAR:
            case PgDataType::VARCHAR:
            case PgDataType::TEXT:
            case PgDataType::CLOB:
            case PgDataType::BIT:
            case PgDataType::BINARY:
            case PgDataType::VARBINARY:
            case PgDataType::BLOB:
            case PgDataType::JSON:
            case PgDataType::XML:
            case PgDataType::URI:
            case PgDataType::DOMAIN_TYPE:
            case PgDataType::ARRAY:
            default: {
                uint16_t len = (maxLength > 0) ? maxLength : 64;
                if (len > 255) len = 255;
                return static_cast<uint16_t>(len + 1); // Incluye terminador '\0'
            }
        }
    }

    using CheckEvaluatorFn = bool (*)(const String&);

    struct ColumnConstraintDef {
        String columnName = "";
        PgDataType dataType = PgDataType::TEXT;
        String customTypeName = "";
        bool isArray = false;

        uint16_t maxLength = 0;
        uint8_t precision = 0;
        uint8_t scale = 0;
        IntervalField intervalField = IntervalField::NONE;

        // Layout binario fijo dentro del registro .pgb (calculado automáticamente)
        uint16_t binOffset = 0;
        uint16_t binSize   = 0;

        bool isPrimaryKey = false;
        bool isNotNull = false;
        bool isUnique = false;

        IdentityGeneration identityMode = IdentityGeneration::NONE;
        int32_t identityStart = 1;
        int32_t identityIncrement = 1;

        bool hasDefault = false;
        String defaultValue = "";
        bool autoNowAdd = false;
        bool autoNow = false;

        String collation = "default";
        String comment = "";

        bool hasCheck = false;
        String checkConstraintName = "";
        String checkExpression = "";
        CheckEvaluatorFn checkFn = nullptr;

        bool isForeignKey = false;
        String fkConstraintName = "";
        String referencedTable = "";
        String referencedColumn = "";
        ReferentialAction onDelete = ReferentialAction::NO_ACTION;
        ReferentialAction onUpdate = ReferentialAction::NO_ACTION;
        MatchType matchType = MatchType::SIMPLE;
        bool deferrable = false;
        bool initiallyDeferred = false;

        ColumnConstraintDef() = default;
        ColumnConstraintDef(const ColumnConstraintDef&) = default;
        ColumnConstraintDef(ColumnConstraintDef&&) noexcept = default;
        ColumnConstraintDef& operator=(const ColumnConstraintDef&) = default;
        ColumnConstraintDef& operator=(ColumnConstraintDef&&) noexcept = default;
    };

    struct TableLevelConstraintDef {
        String constraintName = "";
        enum class Type : uint8_t { PRIMARY_KEY, UNIQUE, CHECK, FOREIGN_KEY } type;
        std::vector<String> localColumns;
        String referencedTable = "";
        std::vector<String> referencedColumns;
        ReferentialAction onDelete = ReferentialAction::NO_ACTION;
        ReferentialAction onUpdate = ReferentialAction::NO_ACTION;
        String checkExpression = "";
        bool deferrable = false;
        bool initiallyDeferred = false;
    };

    struct SequenceDefinition {
        String sequenceName = "";
        PgDataType dataType = PgDataType::BIGINT;
        int64_t startValue = 1;
        int64_t incrementBy = 1;
        int64_t minValue = 1;
        int64_t maxValue = 2147483647LL;
        bool cycle = false;
        uint16_t cache = 1;
        String ownedByTable = "";
        String ownedByColumn = "";
    };

    struct IndexDefinition {
        String indexName = "";
        String tableName = "";
        bool isUnique = false;
        IndexMethod method = IndexMethod::BTREE;
        std::vector<String> keyColumns;
        std::vector<String> includeColumns;
        SortDirection direction = SortDirection::ASC;
        NullsOrder nullsOrder = NullsOrder::LAST;
        String wherePredicate = "";
        String tablespace = "pg_default";
    };

} // namespace DbCommon::Standard

namespace DbNotRuntime::Standard {
    using namespace DbCommon::Standard;
}