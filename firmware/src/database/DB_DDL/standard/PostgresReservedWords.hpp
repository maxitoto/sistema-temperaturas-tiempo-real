#pragma once
#include <Arduino.h>
#include <cstring>

namespace DbNotRuntime::Standard {

    // Clasifica el nivel de restricción gramatical de una palabra clave según el parser de PostgreSQL.
    enum class KeywordCategory : uint8_t {
        UNRESERVED = 0,
        NON_RESERVED,
        NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE,
        RESERVED,
        RESERVED_CAN_BE_FUNC_OR_TYPE
    };

    // Almacena en Flash (PROGMEM-friendly) el par léxico de palabra clave y su categoría PostgreSQL.
    struct KeywordEntry {
        const char* word;
        KeywordCategory category;
    };

    // Tabla ordenada alfabéticamente de palabras clave DDL/Esquema Físico extraídas de informe.md.
    static constexpr KeywordEntry POSTGRES_KEYWORDS[] = {
        {"ACCESS", KeywordCategory::NON_RESERVED},
        {"ACTION", KeywordCategory::NON_RESERVED},
        {"ADMIN", KeywordCategory::NON_RESERVED},
        {"AFTER", KeywordCategory::NON_RESERVED},
        {"AGGREGATE", KeywordCategory::NON_RESERVED},
        {"ALL", KeywordCategory::RESERVED},
        {"ALTER", KeywordCategory::NON_RESERVED},
        {"ALWAYS", KeywordCategory::NON_RESERVED},
        {"ANY", KeywordCategory::RESERVED},
        {"ARRAY", KeywordCategory::RESERVED},
        {"AS", KeywordCategory::RESERVED},
        {"ASC", KeywordCategory::RESERVED},
        {"ATTACH", KeywordCategory::NON_RESERVED},
        {"ATTRIBUTE", KeywordCategory::NON_RESERVED},
        {"ATTRIBUTES", KeywordCategory::NON_RESERVED},
        {"AUTHORIZATION", KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE},
        {"BEFORE", KeywordCategory::NON_RESERVED},
        {"BEGIN", KeywordCategory::NON_RESERVED},
        {"BIGINT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"BINARY", KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE},
        {"BIT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"BLOB", KeywordCategory::RESERVED},
        {"BOOLEAN", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"BY", KeywordCategory::NON_RESERVED},
        {"CACHE", KeywordCategory::NON_RESERVED},
        {"CALLED", KeywordCategory::NON_RESERVED},
        {"CASCADE", KeywordCategory::NON_RESERVED},
        {"CASCADED", KeywordCategory::NON_RESERVED},
        {"CASE", KeywordCategory::RESERVED},
        {"CAST", KeywordCategory::RESERVED},
        {"CATALOG", KeywordCategory::NON_RESERVED},
        {"CHAR", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"CHARACTER", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"CHARACTER_SET_CATALOG", KeywordCategory::NON_RESERVED},
        {"CHARACTER_SET_NAME", KeywordCategory::NON_RESERVED},
        {"CHARACTER_SET_SCHEMA", KeywordCategory::NON_RESERVED},
        {"CHECK", KeywordCategory::RESERVED},
        {"CLOB", KeywordCategory::RESERVED},
        {"COLLATE", KeywordCategory::RESERVED},
        {"COLLATION", KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE},
        {"COLLATION_CATALOG", KeywordCategory::NON_RESERVED},
        {"COLLATION_NAME", KeywordCategory::NON_RESERVED},
        {"COLLATION_SCHEMA", KeywordCategory::NON_RESERVED},
        {"COLUMN", KeywordCategory::RESERVED},
        {"COLUMNS", KeywordCategory::NON_RESERVED},
        {"COLUMN_NAME", KeywordCategory::NON_RESERVED},
        {"COMMAND_FUNCTION", KeywordCategory::NON_RESERVED},
        {"COMMAND_FUNCTION_CODE", KeywordCategory::NON_RESERVED},
        {"COMMENT", KeywordCategory::NON_RESERVED},
        {"COMPRESSION", KeywordCategory::NON_RESERVED},
        {"CONCURRENTLY", KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE},
        {"CONDITION_NUMBER", KeywordCategory::NON_RESERVED},
        {"CONNECT", KeywordCategory::RESERVED},
        {"CONSTRAINT", KeywordCategory::RESERVED},
        {"CONSTRAINTS", KeywordCategory::NON_RESERVED},
        {"CONSTRAINT_CATALOG", KeywordCategory::NON_RESERVED},
        {"CONSTRAINT_NAME", KeywordCategory::NON_RESERVED},
        {"CONSTRAINT_SCHEMA", KeywordCategory::NON_RESERVED},
        {"CONVERSION", KeywordCategory::NON_RESERVED},
        {"COST", KeywordCategory::NON_RESERVED},
        {"CREATE", KeywordCategory::RESERVED},
        {"CURRENT_DATE", KeywordCategory::RESERVED},
        {"CURRENT_TIME", KeywordCategory::RESERVED},
        {"CURRENT_TIMESTAMP", KeywordCategory::RESERVED},
        {"CURRENT_USER", KeywordCategory::RESERVED},
        {"CURSOR_NAME", KeywordCategory::NON_RESERVED},
        {"CYCLE", KeywordCategory::NON_RESERVED},
        {"DATA", KeywordCategory::NON_RESERVED},
        {"DATABASE", KeywordCategory::NON_RESERVED},
        {"DATALINK", KeywordCategory::RESERVED},
        {"DATE", KeywordCategory::RESERVED},
        {"DAY", KeywordCategory::NON_RESERVED},
        {"DEC", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"DECFLOAT", KeywordCategory::RESERVED},
        {"DECIMAL", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"DECLARE", KeywordCategory::NON_RESERVED},
        {"DEFAULT", KeywordCategory::RESERVED},
        {"DEFERRABLE", KeywordCategory::RESERVED},
        {"DEFERRED", KeywordCategory::NON_RESERVED},
        {"DEFINER", KeywordCategory::NON_RESERVED},
        {"DELETE", KeywordCategory::NON_RESERVED},
        {"DESC", KeywordCategory::RESERVED},
        {"DETACH", KeywordCategory::NON_RESERVED},
        {"DISABLE", KeywordCategory::NON_RESERVED},
        {"DOMAIN", KeywordCategory::NON_RESERVED},
        {"DOUBLE", KeywordCategory::NON_RESERVED},
        {"DROP", KeywordCategory::NON_RESERVED},
        {"DYNAMIC_FUNCTION", KeywordCategory::NON_RESERVED},
        {"DYNAMIC_FUNCTION_CODE", KeywordCategory::NON_RESERVED},
        {"EACH", KeywordCategory::NON_RESERVED},
        {"ENABLE", KeywordCategory::NON_RESERVED},
        {"ENCODING", KeywordCategory::NON_RESERVED},
        {"ENCRYPTED", KeywordCategory::NON_RESERVED},
        {"END", KeywordCategory::RESERVED},
        {"ENFORCED", KeywordCategory::NON_RESERVED},
        {"ENUM", KeywordCategory::NON_RESERVED},
        {"EVENT", KeywordCategory::NON_RESERVED},
        {"EXECUTE", KeywordCategory::NON_RESERVED},
        {"EXISTS", KeywordCategory::NON_RESERVED},
        {"EXTENSION", KeywordCategory::NON_RESERVED},
        {"FIRST", KeywordCategory::NON_RESERVED},
        {"FLOAT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"FOR", KeywordCategory::RESERVED},
        {"FOREIGN", KeywordCategory::RESERVED},
        {"FULL", KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE},
        {"FUNCTION", KeywordCategory::NON_RESERVED},
        {"FUNCTIONS", KeywordCategory::NON_RESERVED},
        {"GENERATED", KeywordCategory::NON_RESERVED},
        {"GLOBAL", KeywordCategory::NON_RESERVED},
        {"GRANT", KeywordCategory::RESERVED},
        {"GRANTED", KeywordCategory::NON_RESERVED},
        {"GROUP", KeywordCategory::RESERVED},
        {"HOUR", KeywordCategory::NON_RESERVED},
        {"IDENTITY", KeywordCategory::NON_RESERVED},
        {"IF", KeywordCategory::NON_RESERVED},
        {"IMMEDIATE", KeywordCategory::NON_RESERVED},
        {"IMMUTABLE", KeywordCategory::NON_RESERVED},
        {"IN", KeywordCategory::RESERVED},
        {"INCLUDE", KeywordCategory::NON_RESERVED},
        {"INCREMENT", KeywordCategory::NON_RESERVED},
        {"INDEX", KeywordCategory::NON_RESERVED},
        {"INDEXES", KeywordCategory::NON_RESERVED},
        {"INITIALLY", KeywordCategory::RESERVED},
        {"INOUT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"INPUT", KeywordCategory::NON_RESERVED},
        {"INSERT", KeywordCategory::NON_RESERVED},
        {"INSTEAD", KeywordCategory::NON_RESERVED},
        {"INT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"INTEGER", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"INTERVAL", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"INVOKER", KeywordCategory::NON_RESERVED},
        {"JSON", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"KEY", KeywordCategory::NON_RESERVED},
        {"KEY_MEMBER", KeywordCategory::NON_RESERVED},
        {"KEY_TYPE", KeywordCategory::NON_RESERVED},
        {"LANGUAGE", KeywordCategory::NON_RESERVED},
        {"LARGE", KeywordCategory::NON_RESERVED},
        {"LAST", KeywordCategory::NON_RESERVED},
        {"LOCAL", KeywordCategory::NON_RESERVED},
        {"LOCALTIME", KeywordCategory::RESERVED},
        {"LOCALTIMESTAMP", KeywordCategory::RESERVED},
        {"MAPPING", KeywordCategory::NON_RESERVED},
        {"MATCH", KeywordCategory::NON_RESERVED},
        {"MATERIALIZED", KeywordCategory::NON_RESERVED},
        {"MAXVALUE", KeywordCategory::NON_RESERVED},
        {"MESSAGE_LENGTH", KeywordCategory::NON_RESERVED},
        {"MESSAGE_OCTET_LENGTH", KeywordCategory::NON_RESERVED},
        {"MESSAGE_TEXT", KeywordCategory::NON_RESERVED},
        {"METHOD", KeywordCategory::NON_RESERVED},
        {"MINUTE", KeywordCategory::NON_RESERVED},
        {"MINVALUE", KeywordCategory::NON_RESERVED},
        {"MONTH", KeywordCategory::NON_RESERVED},
        {"MULTISET", KeywordCategory::RESERVED},
        {"NATIONAL", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"NCHAR", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"NCLOB", KeywordCategory::RESERVED},
        {"NEW", KeywordCategory::NON_RESERVED},
        {"NO", KeywordCategory::NON_RESERVED},
        {"NONE", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"NOT", KeywordCategory::RESERVED},
        {"NULL", KeywordCategory::RESERVED},
        {"NULLABLE", KeywordCategory::NON_RESERVED},
        {"NULLS", KeywordCategory::NON_RESERVED},
        {"NUMERIC", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"OBJECT", KeywordCategory::NON_RESERVED},
        {"OF", KeywordCategory::NON_RESERVED},
        {"OLD", KeywordCategory::NON_RESERVED},
        {"ON", KeywordCategory::RESERVED},
        {"ONLY", KeywordCategory::RESERVED},
        {"OPERATOR", KeywordCategory::NON_RESERVED},
        {"OPTION", KeywordCategory::NON_RESERVED},
        {"OR", KeywordCategory::RESERVED},
        {"ORDINALITY", KeywordCategory::NON_RESERVED},
        {"OUT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"OUTPUT", KeywordCategory::NON_RESERVED},
        {"OVERRIDING", KeywordCategory::NON_RESERVED},
        {"OWNED", KeywordCategory::NON_RESERVED},
        {"OWNER", KeywordCategory::NON_RESERVED},
        {"PARALLEL", KeywordCategory::NON_RESERVED},
        {"PARAMETER_MODE", KeywordCategory::NON_RESERVED},
        {"PARAMETER_NAME", KeywordCategory::NON_RESERVED},
        {"PARAMETER_ORDINAL_POSITION", KeywordCategory::NON_RESERVED},
        {"PARAMETER_SPECIFIC_CATALOG", KeywordCategory::NON_RESERVED},
        {"PARAMETER_SPECIFIC_NAME", KeywordCategory::NON_RESERVED},
        {"PARAMETER_SPECIFIC_SCHEMA", KeywordCategory::NON_RESERVED},
        {"PARTIAL", KeywordCategory::NON_RESERVED},
        {"PARTITION", KeywordCategory::NON_RESERVED},
        {"PASSWORD", KeywordCategory::NON_RESERVED},
        {"POLICY", KeywordCategory::NON_RESERVED},
        {"PRECISION", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"PRIMARY", KeywordCategory::RESERVED},
        {"PRIVILEGES", KeywordCategory::NON_RESERVED},
        {"PROCEDURAL", KeywordCategory::NON_RESERVED},
        {"PROCEDURE", KeywordCategory::NON_RESERVED},
        {"PROCEDURES", KeywordCategory::NON_RESERVED},
        {"PUBLIC", KeywordCategory::NON_RESERVED},
        {"PUBLICATION", KeywordCategory::NON_RESERVED},
        {"RANGE", KeywordCategory::NON_RESERVED},
        {"REAL", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"RECURSIVE", KeywordCategory::NON_RESERVED},
        {"REFERENCES", KeywordCategory::RESERVED},
        {"REFERENCING", KeywordCategory::NON_RESERVED},
        {"REFRESH", KeywordCategory::NON_RESERVED},
        {"RENAME", KeywordCategory::NON_RESERVED},
        {"REPLACE", KeywordCategory::NON_RESERVED},
        {"REPLICA", KeywordCategory::NON_RESERVED},
        {"RESTART", KeywordCategory::NON_RESERVED},
        {"RESTRICT", KeywordCategory::NON_RESERVED},
        {"RETURN", KeywordCategory::NON_RESERVED},
        {"RETURNED_CARDINALITY", KeywordCategory::NON_RESERVED},
        {"RETURNED_LENGTH", KeywordCategory::NON_RESERVED},
        {"RETURNED_OCTET_LENGTH", KeywordCategory::NON_RESERVED},
        {"RETURNED_SQLSTATE", KeywordCategory::NON_RESERVED},
        {"RETURNS", KeywordCategory::NON_RESERVED},
        {"REVOKE", KeywordCategory::NON_RESERVED},
        {"ROLE", KeywordCategory::NON_RESERVED},
        {"ROUTINE", KeywordCategory::NON_RESERVED},
        {"ROUTINES", KeywordCategory::NON_RESERVED},
        {"ROUTINE_CATALOG", KeywordCategory::NON_RESERVED},
        {"ROUTINE_NAME", KeywordCategory::NON_RESERVED},
        {"ROUTINE_SCHEMA", KeywordCategory::NON_RESERVED},
        {"ROW", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"ROWS", KeywordCategory::NON_RESERVED},
        {"ROW_COUNT", KeywordCategory::NON_RESERVED},
        {"RULE", KeywordCategory::NON_RESERVED},
        {"SCHEMA", KeywordCategory::NON_RESERVED},
        {"SCHEMAS", KeywordCategory::NON_RESERVED},
        {"SCOPE_CATALOG", KeywordCategory::NON_RESERVED},
        {"SCOPE_NAME", KeywordCategory::NON_RESERVED},
        {"SCOPE_SCHEMA", KeywordCategory::NON_RESERVED},
        {"SECOND", KeywordCategory::NON_RESERVED},
        {"SECURITY", KeywordCategory::NON_RESERVED},
        {"SELECT", KeywordCategory::RESERVED},
        {"SEQUENCE", KeywordCategory::NON_RESERVED},
        {"SEQUENCES", KeywordCategory::NON_RESERVED},
        {"SERVER", KeywordCategory::NON_RESERVED},
        {"SESSION_USER", KeywordCategory::RESERVED},
        {"SET", KeywordCategory::NON_RESERVED},
        {"SETOF", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"SIMPLE", KeywordCategory::NON_RESERVED},
        {"SMALLINT", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"SOME", KeywordCategory::RESERVED},
        {"SQLCODE", KeywordCategory::NON_RESERVED},
        {"SQLERROR", KeywordCategory::NON_RESERVED},
        {"SQLEXCEPTION", KeywordCategory::RESERVED},
        {"SQLSTATE", KeywordCategory::RESERVED},
        {"SQLWARNING", KeywordCategory::RESERVED},
        {"STABLE", KeywordCategory::NON_RESERVED},
        {"START", KeywordCategory::NON_RESERVED},
        {"STATEMENT", KeywordCategory::NON_RESERVED},
        {"STATISTICS", KeywordCategory::NON_RESERVED},
        {"STORAGE", KeywordCategory::NON_RESERVED},
        {"STORED", KeywordCategory::NON_RESERVED},
        {"STRICT", KeywordCategory::NON_RESERVED},
        {"SUBSCRIPTION", KeywordCategory::NON_RESERVED},
        {"SYSTEM", KeywordCategory::NON_RESERVED},
        {"SYSTEM_USER", KeywordCategory::RESERVED},
        {"TABLE", KeywordCategory::RESERVED},
        {"TABLESPACE", KeywordCategory::NON_RESERVED},
        {"TABLE_NAME", KeywordCategory::NON_RESERVED},
        {"TEMP", KeywordCategory::NON_RESERVED},
        {"TEMPORARY", KeywordCategory::NON_RESERVED},
        {"TEXT", KeywordCategory::NON_RESERVED},
        {"TIME", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"TIMESTAMP", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"TIMEZONE_HOUR", KeywordCategory::RESERVED},
        {"TIMEZONE_MINUTE", KeywordCategory::RESERVED},
        {"TO", KeywordCategory::RESERVED},
        {"TOP_LEVEL_COUNT", KeywordCategory::NON_RESERVED},
        {"TRANSACTION_ACTIVE", KeywordCategory::NON_RESERVED},
        {"TRANSACTIONS_COMMITTED", KeywordCategory::NON_RESERVED},
        {"TRANSACTIONS_ROLLED_BACK", KeywordCategory::NON_RESERVED},
        {"TRANSFORM", KeywordCategory::NON_RESERVED},
        {"TRIGGER", KeywordCategory::NON_RESERVED},
        {"TRIGGER_CATALOG", KeywordCategory::NON_RESERVED},
        {"TRIGGER_NAME", KeywordCategory::NON_RESERVED},
        {"TRIGGER_SCHEMA", KeywordCategory::NON_RESERVED},
        {"TRUNCATE", KeywordCategory::NON_RESERVED},
        {"TYPE", KeywordCategory::NON_RESERVED},
        {"TYPES", KeywordCategory::NON_RESERVED},
        {"UNENCRYPTED", KeywordCategory::NON_RESERVED},
        {"UNIQUE", KeywordCategory::RESERVED},
        {"UNLOGGED", KeywordCategory::NON_RESERVED},
        {"UPDATE", KeywordCategory::NON_RESERVED},
        {"URI", KeywordCategory::NON_RESERVED},
        {"USAGE", KeywordCategory::NON_RESERVED},
        {"USER", KeywordCategory::RESERVED},
        {"USER_DEFINED_TYPE_CATALOG", KeywordCategory::NON_RESERVED},
        {"USER_DEFINED_TYPE_CODE", KeywordCategory::NON_RESERVED},
        {"USER_DEFINED_TYPE_NAME", KeywordCategory::NON_RESERVED},
        {"USER_DEFINED_TYPE_SCHEMA", KeywordCategory::NON_RESERVED},
        {"USING", KeywordCategory::RESERVED},
        {"VALID", KeywordCategory::NON_RESERVED},
        {"VALIDATE", KeywordCategory::NON_RESERVED},
        {"VALUE", KeywordCategory::NON_RESERVED},
        {"VALUES", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"VARBINARY", KeywordCategory::RESERVED},
        {"VARCHAR", KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE},
        {"VARIADIC", KeywordCategory::RESERVED},
        {"VARYING", KeywordCategory::NON_RESERVED},
        {"VIEW", KeywordCategory::NON_RESERVED},
        {"VIEWS", KeywordCategory::NON_RESERVED},
        {"VIRTUAL", KeywordCategory::NON_RESERVED},
        {"VOLATILE", KeywordCategory::NON_RESERVED},
        {"WHEN", KeywordCategory::RESERVED},
        {"WHERE", KeywordCategory::RESERVED},
        {"WITH", KeywordCategory::RESERVED},
        {"WITHOUT", KeywordCategory::NON_RESERVED},
        {"WRAPPER", KeywordCategory::NON_RESERVED},
        {"XML", KeywordCategory::NON_RESERVED},
        {"YEAR", KeywordCategory::NON_RESERVED},
        {"ZONE", KeywordCategory::NON_RESERVED}
    };

    class PostgresLexicalValidator {
    public:
        // Ejecuta búsqueda binaria O(log N) sobre la tabla estática convirtiendo el identificador a mayúsculas.
        static KeywordCategory classifyKeyword(const String& identifier) {
            String upper = identifier;
            upper.trim();
            upper.toUpperCase();

            int low = 0;
            int high = static_cast<int>(sizeof(POSTGRES_KEYWORDS) / sizeof(KeywordEntry)) - 1;

            while (low <= high) {
                int mid = low + (high - low) / 2;
                int cmp = strcmp(upper.c_str(), POSTGRES_KEYWORDS[mid].word);
                if (cmp == 0) return POSTGRES_KEYWORDS[mid].category;
                if (cmp < 0) high = mid - 1;
                else low = mid + 1;
            }
            return KeywordCategory::UNRESERVED;
        }

        // Verifica si un identificador está entre comillas dobles ("IDENT") permitiendo saltar la restricción RESERVED.
        static bool isQuotedIdentifier(const String& id) {
            return id.length() >= 3 && id.startsWith("\"") && id.endsWith("\"");
        }

        // Valida formato ASCII de identificadores PostgreSQL (máximo 63 bytes NAMEDATALEN, inicio alfabético o '_').
        static bool isValidIdentifierFormat(const String& id, String& outReason) {
            if (id.isEmpty()) {
                outReason = "zero-length delimited identifier";
                return false;
            }
            if (id.length() > 63) {
                outReason = "identifier exceeds PostgreSQL NAMEDATALEN (63 bytes)";
                return false;
            }
            if (isQuotedIdentifier(id)) {
                return true;
            }
            char first = id[0];
            if (!isalpha(first) && first != '_') {
                outReason = "unquoted identifier must start with a letter or underscore";
                return false;
            }
            for (size_t i = 1; i < id.length(); ++i) {
                char c = id[i];
                if (!isalnum(c) && c != '_' && c != '$') {
                    outReason = String("invalid character '") + c + "' in identifier";
                    return false;
                }
            }
            return true;
        }

        // Determina si el identificador colisiona con una palabra RESERVED sin estar entrecomillado.
        static bool isForbiddenUnquotedIdentifier(const String& id, bool isTypeOrFuncContext = false) {
            if (isQuotedIdentifier(id)) return false;
            KeywordCategory cat = classifyKeyword(id);
            if (cat == KeywordCategory::RESERVED) return true;
            if (!isTypeOrFuncContext && cat == KeywordCategory::RESERVED_CAN_BE_FUNC_OR_TYPE) return true;
            if (isTypeOrFuncContext && cat == KeywordCategory::NON_RESERVED_CANNOT_BE_FUNC_OR_TYPE) return true;
            return false;
        }
    };

} // namespace DbNotRuntime::Standard