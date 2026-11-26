#pragma once
#include <Arduino.h>
#include <vector>
#include <cstring>
#include <strings.h>
#include "../common/PhysicalSchemaCatalog.hpp"
#include "../storage/PgBinaryStorage.hpp"

namespace DbSql {

    using DbCommon::Engine::PhysicalSchemaCatalog;
    using DbCommon::Standard::DataTypeUtils;
    using DbCommon::Standard::PgBinHeader;
    using DbCommon::Standard::PgDataType;
    using DbRuntime::Storage::PgBinaryStorage;

    struct VirtualColDef {
        const char* name;
        bool        isNumeric;
    };

    class PgVirtualCatalog {
    public:
        enum class Kind : uint8_t {
            NONE = 0,
            PG_TYPE,
            PG_ENUM,
            PG_TABLES,
            PG_INDEXES,
            PG_CLASS,
            IS_TABLES,
            IS_COLUMNS,
            IS_TABLE_CONSTRAINTS,
            IS_KEY_COLUMN_USAGE
        };

        struct BaseTypeInfo {
            uint32_t    oid;
            const char* name;
            int16_t     len;
            const char* category;
        };

        static constexpr BaseTypeInfo BASE_TYPES[] = {
            { 16,   "bool",        1,  "B" },
            { 20,   "int8",        8,  "N" },
            { 21,   "int2",        2,  "N" },
            { 23,   "int4",        4,  "N" },
            { 25,   "text",       -1,  "S" },
            { 114,  "json",       -1,  "U" },
            { 700,  "float4",      4,  "N" },
            { 701,  "float8",      8,  "N" },
            { 1042, "bpchar",     -1,  "S" },
            { 1043, "varchar",    -1,  "S" },
            { 1082, "date",        4,  "D" },
            { 1083, "time",        8,  "D" },
            { 1114, "timestamp",   8,  "D" },
            { 1184, "timestamptz", 8,  "D" },
            { 1186, "interval",   16,  "T" },
            { 1700, "numeric",    -1,  "N" }
        };

        static const char* udtNameForColumn(const DbCommon::Standard::ColumnConstraintDef& c) {
            if (c.dataType == PgDataType::ENUM && !c.customTypeName.isEmpty()) {
                return c.customTypeName.c_str();
            }
            switch (c.dataType) {
                case PgDataType::SMALLINT:         return "int2";
                case PgDataType::INTEGER:          return "int4";
                case PgDataType::BIGINT:           return "int8";
                case PgDataType::REAL:             return "float4";
                case PgDataType::DOUBLE_PRECISION: return "float8";
                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:         return "numeric";
                case PgDataType::BOOLEAN:          return "bool";
                case PgDataType::CHAR:
                case PgDataType::NCHAR:            return "bpchar";
                case PgDataType::VARCHAR:          return "varchar";
                case PgDataType::DATE:             return "date";
                case PgDataType::TIME:
                case PgDataType::TIME_TZ:          return "time";
                case PgDataType::TIMESTAMP:        return "timestamp";
                case PgDataType::TIMESTAMP_TZ:     return "timestamptz";
                case PgDataType::INTERVAL:         return "interval";
                case PgDataType::JSON:             return "json";
                default:                           return "text";
            }
        }

        static Kind resolve(const String& rawName) {
            String k = rawName;
            k.trim();
            k.toLowerCase();
            if (k.startsWith("pg_catalog.")) k = k.substring(11);

            if (k == "pg_type")                             return Kind::PG_TYPE;
            if (k == "pg_enum")                             return Kind::PG_ENUM;
            if (k == "pg_tables")                           return Kind::PG_TABLES;
            if (k == "pg_indexes")                          return Kind::PG_INDEXES;
            if (k == "pg_class")                            return Kind::PG_CLASS;
            if (k == "information_schema.tables")           return Kind::IS_TABLES;
            if (k == "information_schema.columns")          return Kind::IS_COLUMNS;
            if (k == "information_schema.table_constraints")return Kind::IS_TABLE_CONSTRAINTS;
            if (k == "information_schema.key_column_usage") return Kind::IS_KEY_COLUMN_USAGE;
            return Kind::NONE;
        }

        static bool isVirtualTable(const String& rawName) {
            return resolve(rawName) != Kind::NONE;
        }

        static const VirtualColDef* getColumns(Kind kind, size_t& countOut) {
            static constexpr VirtualColDef C_PG_TYPE[] = {
                {"oid", true}, {"typname", false}, {"typnamespace", true},
                {"typtype", false}, {"typlen", true}, {"typcategory", false}
            };
            static constexpr VirtualColDef C_PG_ENUM[] = {
                {"oid", true}, {"enumtypid", true}, {"enumsortorder", true}, {"enumlabel", false}
            };
            static constexpr VirtualColDef C_PG_TABLES[] = {
                {"schemaname", false}, {"tablename", false}, {"tableowner", false}, {"hasindexes", false}
            };
            static constexpr VirtualColDef C_PG_INDEXES[] = {
                {"schemaname", false}, {"tablename", false}, {"indexname", false}, {"indexdef", false}
            };
            static constexpr VirtualColDef C_PG_CLASS[] = {
                {"oid", true}, {"relname", false}, {"relkind", false}, {"reltuples", true}
            };
            static constexpr VirtualColDef C_IS_TABLES[] = {
                {"table_catalog", false}, {"table_schema", false}, {"table_name", false}, {"table_type", false}
            };
            static constexpr VirtualColDef C_IS_COLUMNS[] = {
                {"table_schema", false}, {"table_name", false}, {"column_name", false},
                {"ordinal_position", true}, {"column_default", false}, {"is_nullable", false},
                {"data_type", false}, {"character_maximum_length", true}, {"udt_name", false}
            };
            static constexpr VirtualColDef C_IS_CONSTRAINTS[] = {
                {"constraint_name", false}, {"table_schema", false}, {"table_name", false}, {"constraint_type", false}
            };
            static constexpr VirtualColDef C_IS_KCU[] = {
                {"constraint_name", false}, {"table_schema", false}, {"table_name", false},
                {"column_name", false}, {"ordinal_position", true},
                {"referenced_table", false}, {"referenced_column", false}
            };

            switch (kind) {
                case Kind::PG_TYPE:             countOut = 6; return C_PG_TYPE;
                case Kind::PG_ENUM:             countOut = 4; return C_PG_ENUM;
                case Kind::PG_TABLES:           countOut = 4; return C_PG_TABLES;
                case Kind::PG_INDEXES:          countOut = 4; return C_PG_INDEXES;
                case Kind::PG_CLASS:            countOut = 4; return C_PG_CLASS;
                case Kind::IS_TABLES:           countOut = 4; return C_IS_TABLES;
                case Kind::IS_COLUMNS:          countOut = 9; return C_IS_COLUMNS;
                case Kind::IS_TABLE_CONSTRAINTS:countOut = 4; return C_IS_CONSTRAINTS;
                case Kind::IS_KEY_COLUMN_USAGE: countOut = 7; return C_IS_KCU;
                default:                        countOut = 0; return nullptr;
            }
        }

        template <typename CallbackFn>
        static void scanRows(Kind kind, CallbackFn fn) {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            String vals[10];
            bool   nulls[10];

            auto reset = [&](size_t n) {
                for (size_t i = 0; i < n; ++i) { vals[i] = ""; nulls[i] = false; }
            };

            switch (kind) {
                case Kind::PG_TYPE: {
                    for (const auto& bt : BASE_TYPES) {
                        reset(6);
                        vals[0] = String(bt.oid);
                        vals[1] = bt.name;
                        vals[2] = "11";
                        vals[3] = "b";
                        vals[4] = String(bt.len);
                        vals[5] = bt.category;
                        if (!fn(vals, nulls, 6)) return;
                    }
                    uint32_t idx = 0;
                    for (const auto& kv : cat.getAllEnums()) {
                        reset(6);
                        vals[0] = String(16384u + idx);
                        vals[1] = kv.second.typeName;
                        vals[2] = "2200";
                        vals[3] = "e";
                        vals[4] = "4";
                        vals[5] = "E";
                        if (!fn(vals, nulls, 6)) return;
                        idx++;
                    }
                    break;
                }

                case Kind::PG_ENUM: {
                    uint32_t enumIdx = 0;
                    for (const auto& kv : cat.getAllEnums()) {
                        const uint32_t typeOid = 16384u + enumIdx;
                        for (size_t j = 0; j < kv.second.labels.size(); ++j) {
                            reset(4);
                            vals[0] = String(20000u + (enumIdx * 64u) + static_cast<uint32_t>(j));
                            vals[1] = String(typeOid);
                            vals[2] = String(static_cast<unsigned long>(j + 1));
                            vals[3] = kv.second.labels[j];
                            if (!fn(vals, nulls, 4)) return;
                        }
                        enumIdx++;
                    }
                    break;
                }

                case Kind::PG_TABLES: {
                    for (const auto& kv : cat.getAllTables()) {
                        bool hasIdx = false;
                        for (const auto& ix : cat.getAllIndexes()) {
                            if (ix.second.tableName.equalsIgnoreCase(kv.second.tableName)) {
                                hasIdx = true;
                                break;
                            }
                        }
                        reset(4);
                        vals[0] = kv.second.schemaName;
                        vals[1] = kv.second.tableName;
                        vals[2] = kv.second.ownerRole;
                        vals[3] = hasIdx ? "TRUE" : "FALSE";
                        if (!fn(vals, nulls, 4)) return;
                    }
                    break;
                }

                case Kind::PG_INDEXES: {
                    for (const auto& ix : cat.getAllIndexes()) {
                        reset(4);
                        vals[0] = "public";
                        vals[1] = ix.second.tableName;
                        vals[2] = ix.second.indexName;
                        String def = ix.second.isUnique ? "CREATE UNIQUE INDEX " : "CREATE INDEX ";
                        def += ix.second.indexName + " ON " + ix.second.tableName + " (";
                        for (size_t i = 0; i < ix.second.keyColumns.size(); ++i) {
                            if (i > 0) def += ", ";
                            def += ix.second.keyColumns[i];
                        }
                        def += ")";
                        vals[3] = def;
                        if (!fn(vals, nulls, 4)) return;
                    }
                    break;
                }

                case Kind::PG_CLASS: {
                    auto& st = PgBinaryStorage::getInstance();
                    uint32_t idx = 0;
                    for (const auto& kv : cat.getAllTables()) {
                        PgBinHeader hdr{};
                        uint32_t rows = 0;
                        if (st.getTableHeader(kv.second.tableName.c_str(), hdr)) rows = hdr.activeRows;
                        reset(4);
                        vals[0] = String(16000u + idx++);
                        vals[1] = kv.second.tableName;
                        vals[2] = "r";
                        vals[3] = String(rows);
                        if (!fn(vals, nulls, 4)) return;
                    }
                    for (const auto& ix : cat.getAllIndexes()) {
                        reset(4);
                        vals[0] = String(16000u + idx++);
                        vals[1] = ix.second.indexName;
                        vals[2] = "i";
                        vals[3] = "0";
                        if (!fn(vals, nulls, 4)) return;
                    }
                    break;
                }

                case Kind::IS_TABLES: {
                    for (const auto& kv : cat.getAllTables()) {
                        reset(4);
                        vals[0] = "esp32_pg";
                        vals[1] = kv.second.schemaName;
                        vals[2] = kv.second.tableName;
                        vals[3] = "BASE TABLE";
                        if (!fn(vals, nulls, 4)) return;
                    }
                    break;
                }

                case Kind::IS_COLUMNS: {
                    for (const auto& kv : cat.getAllTables()) {
                        const auto& tb = kv.second;
                        for (size_t i = 0; i < tb.columns.size(); ++i) {
                            const auto& c = tb.columns[i];
                            reset(9);
                            vals[0] = tb.schemaName;
                            vals[1] = tb.tableName;
                            vals[2] = c.columnName;
                            vals[3] = String(static_cast<unsigned long>(i + 1));
                            if (c.hasDefault) vals[4] = c.defaultValue;
                            else nulls[4] = true;
                            vals[5] = c.isNotNull ? "NO" : "YES";
                            vals[6] = (c.dataType == PgDataType::ENUM)
                                ? String("USER-DEFINED")
                                : String(DataTypeUtils::toString(c.dataType));
                            if (c.maxLength > 0) vals[7] = String(c.maxLength);
                            else nulls[7] = true;
                            vals[8] = udtNameForColumn(c);
                            if (!fn(vals, nulls, 9)) return;
                        }
                    }
                    break;
                }

                case Kind::IS_TABLE_CONSTRAINTS: {
                    for (const auto& kv : cat.getAllTables()) {
                        const auto& tb = kv.second;
                        for (const auto& c : tb.columns) {
                            if (c.isPrimaryKey) {
                                reset(4);
                                vals[0] = tb.tableName + "_pkey";
                                vals[1] = tb.schemaName;
                                vals[2] = tb.tableName;
                                vals[3] = "PRIMARY KEY";
                                if (!fn(vals, nulls, 4)) return;
                            } else if (c.isUnique) {
                                reset(4);
                                vals[0] = tb.tableName + "_" + c.columnName + "_key";
                                vals[1] = tb.schemaName;
                                vals[2] = tb.tableName;
                                vals[3] = "UNIQUE";
                                if (!fn(vals, nulls, 4)) return;
                            }
                            if (c.isForeignKey) {
                                reset(4);
                                vals[0] = c.fkConstraintName.isEmpty()
                                    ? (tb.tableName + "_" + c.columnName + "_fkey")
                                    : c.fkConstraintName;
                                vals[1] = tb.schemaName;
                                vals[2] = tb.tableName;
                                vals[3] = "FOREIGN KEY";
                                if (!fn(vals, nulls, 4)) return;
                            }
                        }
                    }
                    break;
                }

                case Kind::IS_KEY_COLUMN_USAGE: {
                    for (const auto& kv : cat.getAllTables()) {
                        const auto& tb = kv.second;
                        for (size_t i = 0; i < tb.columns.size(); ++i) {
                            const auto& c = tb.columns[i];
                            if (c.isPrimaryKey || c.isUnique) {
                                reset(7);
                                vals[0] = c.isPrimaryKey
                                    ? (tb.tableName + "_pkey")
                                    : (tb.tableName + "_" + c.columnName + "_key");
                                vals[1] = tb.schemaName;
                                vals[2] = tb.tableName;
                                vals[3] = c.columnName;
                                vals[4] = String(static_cast<unsigned long>(i + 1));
                                nulls[5] = true;
                                nulls[6] = true;
                                if (!fn(vals, nulls, 7)) return;
                            }
                            if (c.isForeignKey) {
                                reset(7);
                                vals[0] = c.fkConstraintName.isEmpty()
                                    ? (tb.tableName + "_" + c.columnName + "_fkey")
                                    : c.fkConstraintName;
                                vals[1] = tb.schemaName;
                                vals[2] = tb.tableName;
                                vals[3] = c.columnName;
                                vals[4] = String(static_cast<unsigned long>(i + 1));
                                vals[5] = c.referencedTable;
                                vals[6] = c.referencedColumn;
                                if (!fn(vals, nulls, 7)) return;
                            }
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    };

} // namespace DbSql