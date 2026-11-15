#pragma once
#include <Arduino.h>
#include <vector>
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

        bool isPrimaryKey = false;
        bool isNotNull = false;
        bool isUnique = false;

        IdentityGeneration identityMode = IdentityGeneration::NONE;
        int32_t identityStart = 1;
        int32_t identityIncrement = 1;

        bool hasDefault = false;
        String defaultValue = "";
        bool autoNowAdd = false; // Estampa fecha/hora automáticamente en INSERT
        bool autoNow = false;    // Actualiza fecha/hora automáticamente en INSERT y UPDATE

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