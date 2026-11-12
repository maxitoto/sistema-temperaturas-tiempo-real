#pragma once
#include <Arduino.h>
#include <vector>

namespace DbNotRuntime::Standard {

    // Enumera todos los tipos de datos nativos de PostgreSQL definidos en la Sección 2 de informe.md.
    enum class PgDataType : uint8_t {
        SMALLINT = 0,          // int2 (16-bit)
        INTEGER,               // int4 / INT / INTEGER (32-bit)
        BIGINT,                // int8 (64-bit)
        DECIMAL,               // DECIMAL(p,s) / DEC(p,s)
        NUMERIC,               // NUMERIC(p,s)
        DECFLOAT,              // DECFLOAT
        REAL,                  // float4 (32-bit float)
        DOUBLE_PRECISION,      // float8 / DOUBLE PRECISION / FLOAT
        BOOLEAN,               // bool
        CHAR,                  // CHAR(n) / CHARACTER(n)
        NCHAR,                 // NATIONAL CHARACTER(n)
        VARCHAR,               // VARCHAR(n) / CHARACTER VARYING(n)
        TEXT,                  // TEXT (ilimitado)
        CLOB,                  // CLOB / NCLOB
        BIT,                   // BIT(n)
        BINARY,                // BINARY(n)
        VARBINARY,             // VARBINARY(n) / BYTEA
        BLOB,                  // BLOB / LARGE OBJECT
        DATE,                  // DATE (YYYY-MM-DD)
        TIME,                  // TIME [WITHOUT TIME ZONE]
        TIME_TZ,               // TIME WITH TIME ZONE
        TIMESTAMP,             // TIMESTAMP [WITHOUT TIME ZONE]
        TIMESTAMP_TZ,          // TIMESTAMP WITH TIME ZONE
        INTERVAL,              // INTERVAL [YEAR/MONTH/DAY/HOUR/MINUTE/SECOND]
        JSON,                  // JSON
        XML,                   // XML
        URI,                   // URI / DATALINK
        ENUM,                  // CREATE TYPE ... AS ENUM (...)
        DOMAIN_TYPE,           // CREATE DOMAIN ... AS base_type
        ARRAY                  // base_type[]
    };

    // Define los campos de precisión para el tipo INTERVAL según PostgreSQL.
    enum class IntervalField : uint8_t {
        NONE = 0, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, YEAR_TO_MONTH, DAY_TO_SECOND
    };

    // Registra un tipo ENUM personalizado en el catálogo con sus etiquetas ordenadas como en PostgreSQL.
    struct EnumTypeDefinition {
        String typeName = "";
        std::vector<String> labels;

        // Comprueba linealmente si una etiqueta pertenece al conjunto válido del ENUM.
        bool containsLabel(const String& candidate) const {
            for (const auto& l : labels) {
                if (l == candidate) return true;
            }
            return false;
        }
    };

    // Registra un DOMAIN personalizado sobre un tipo base con restricciones NOT NULL, DEFAULT y CHECK.
    struct DomainTypeDefinition {
        String domainName = "";
        PgDataType baseType = PgDataType::TEXT;
        uint16_t maxLength = 0;
        uint8_t precision = 0;
        uint8_t scale = 0;
        bool notNull = false;
        bool hasDefault = false;
        String defaultValue = "";
        String checkExpression = "";
    };

    class DataTypeUtils {
    public:
        // Convierte el identificador enum interno a su representación canónica SQL de PostgreSQL.
        static const char* toString(PgDataType t) {
            switch (t) {
                case PgDataType::SMALLINT:         return "SMALLINT";
                case PgDataType::INTEGER:          return "INTEGER";
                case PgDataType::BIGINT:           return "BIGINT";
                case PgDataType::DECIMAL:          return "DECIMAL";
                case PgDataType::NUMERIC:          return "NUMERIC";
                case PgDataType::DECFLOAT:         return "DECFLOAT";
                case PgDataType::REAL:             return "REAL";
                case PgDataType::DOUBLE_PRECISION: return "DOUBLE PRECISION";
                case PgDataType::BOOLEAN:          return "BOOLEAN";
                case PgDataType::CHAR:             return "CHARACTER";
                case PgDataType::NCHAR:            return "NATIONAL CHARACTER";
                case PgDataType::VARCHAR:          return "CHARACTER VARYING";
                case PgDataType::TEXT:             return "TEXT";
                case PgDataType::CLOB:             return "CLOB";
                case PgDataType::BIT:              return "BIT";
                case PgDataType::BINARY:           return "BINARY";
                case PgDataType::VARBINARY:        return "VARBINARY";
                case PgDataType::BLOB:             return "BLOB";
                case PgDataType::DATE:             return "DATE";
                case PgDataType::TIME:             return "TIME WITHOUT TIME ZONE";
                case PgDataType::TIME_TZ:          return "TIME WITH TIME ZONE";
                case PgDataType::TIMESTAMP:        return "TIMESTAMP WITHOUT TIME ZONE";
                case PgDataType::TIMESTAMP_TZ:     return "TIMESTAMP WITH TIME ZONE";
                case PgDataType::INTERVAL:         return "INTERVAL";
                case PgDataType::JSON:             return "JSON";
                case PgDataType::XML:              return "XML";
                case PgDataType::URI:              return "URI";
                case PgDataType::ENUM:             return "USER-DEFINED (ENUM)";
                case PgDataType::DOMAIN_TYPE:      return "USER-DEFINED (DOMAIN)";
                case PgDataType::ARRAY:            return "ARRAY";
                default:                           return "UNKNOWN";
            }
        }

        // Verifica si dos tipos de datos son compatibles para una relación FOREIGN KEY en PostgreSQL.
        static bool areForeignKeyCompatible(PgDataType fkType, const String& fkCustomName,
                                            PgDataType pkType, const String& pkCustomName) {
            if (fkType == pkType) {
                if (fkType == PgDataType::ENUM || fkType == PgDataType::DOMAIN_TYPE) {
                    return fkCustomName.equalsIgnoreCase(pkCustomName);
                }
                return true;
            }
            // En PostgreSQL DECIMAL y NUMERIC son equivalentes a nivel binario
            if ((fkType == PgDataType::DECIMAL && pkType == PgDataType::NUMERIC) ||
                (fkType == PgDataType::NUMERIC && pkType == PgDataType::DECIMAL)) {
                return true;
            }
            // En PostgreSQL VARCHAR y TEXT o CHAR pueden referenciarse si son familia carácter
            bool fkChar = (fkType == PgDataType::VARCHAR || fkType == PgDataType::TEXT || fkType == PgDataType::CHAR);
            bool pkChar = (pkType == PgDataType::VARCHAR || pkType == PgDataType::TEXT || pkType == PgDataType::CHAR);
            return fkChar && pkChar;
        }

        // Valida que un literal en texto cumpla con la gramática y rango del tipo PostgreSQL asignado.
        static bool validateLiteral(PgDataType type, const String& literal,
                                    uint16_t maxLength, uint8_t precision, uint8_t scale,
                                    const EnumTypeDefinition* enumDef,
                                    String& outSqlState, String& outErrorMsg) {
            if (literal.equalsIgnoreCase("NULL")) return true;

            switch (type) {
                case PgDataType::SMALLINT:
                case PgDataType::INTEGER:
                case PgDataType::BIGINT: {
                    int start = (literal.startsWith("-") || literal.startsWith("+")) ? 1 : 0;
                    if (literal.length() <= static_cast<size_t>(start)) {
                        outSqlState = "22P02";
                        outErrorMsg = "invalid input syntax for type integer: \"" + literal + "\"";
                        return false;
                    }
                    for (size_t i = start; i < literal.length(); ++i) {
                        if (!isdigit(literal[i])) {
                            outSqlState = "22P02";
                            outErrorMsg = "invalid input syntax for type integer: \"" + literal + "\"";
                            return false;
                        }
                    }
                    long val = literal.toInt();
                    if (type == PgDataType::SMALLINT && (val < -32768 || val > 32767)) {
                        outSqlState = "22003";
                        outErrorMsg = "smallint out of range";
                        return false;
                    }
                    return true;
                }

                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:
                case PgDataType::REAL:
                case PgDataType::DOUBLE_PRECISION: {
                    int start = (literal.startsWith("-") || literal.startsWith("+")) ? 1 : 0;
                    bool hasDot = false;
                    int intDigits = 0;
                    int fracDigits = 0;
                    if (literal.length() <= static_cast<size_t>(start)) {
                        outSqlState = "22P02";
                        outErrorMsg = "invalid input syntax for type numeric: \"" + literal + "\"";
                        return false;
                    }
                    for (size_t i = start; i < literal.length(); ++i) {
                        if (literal[i] == '.') {
                            if (hasDot) {
                                outSqlState = "22P02";
                                outErrorMsg = "multiple decimal points in numeric literal";
                                return false;
                            }
                            hasDot = true;
                        } else if (isdigit(literal[i])) {
                            if (hasDot) fracDigits++;
                            else intDigits++;
                        } else {
                            outSqlState = "22P02";
                            outErrorMsg = "invalid input syntax for type numeric: \"" + literal + "\"";
                            return false;
                        }
                    }
                    if ((type == PgDataType::DECIMAL || type == PgDataType::NUMERIC) && precision > 0) {
                        if ((intDigits + fracDigits) > precision || fracDigits > scale) {
                            outSqlState = "22003";
                            outErrorMsg = "numeric field overflow (precision " + String(precision) + ", scale " + String(scale) + ")";
                            return false;
                        }
                    }
                    return true;
                }

                case PgDataType::BOOLEAN: {
                    String up = literal;
                    up.toUpperCase();
                    if (up == "TRUE" || up == "FALSE" || up == "1" || up == "0" || up == "T" || up == "F") {
                        return true;
                    }
                    outSqlState = "22P02";
                    outErrorMsg = "invalid input syntax for type boolean: \"" + literal + "\"";
                    return false;
                }

                case PgDataType::CHAR:
                case PgDataType::NCHAR:
                case PgDataType::VARCHAR:
                case PgDataType::BIT:
                case PgDataType::BINARY:
                case PgDataType::VARBINARY: {
                    if (maxLength > 0 && literal.length() > maxLength) {
                        outSqlState = "22001";
                        outErrorMsg = "value too long for type character varying(" + String(maxLength) + ")";
                        return false;
                    }
                    if (type == PgDataType::BIT) {
                        for (size_t i = 0; i < literal.length(); ++i) {
                            if (literal[i] != '0' && literal[i] != '1') {
                                outSqlState = "22P02";
                                outErrorMsg = "\"" + literal + "\" is not a valid bit string";
                                return false;
                            }
                        }
                    }
                    return true;
                }

                case PgDataType::ENUM: {
                    if (!enumDef || !enumDef->containsLabel(literal)) {
                        outSqlState = "22P02";
                        outErrorMsg = "invalid input value for enum " + (enumDef ? enumDef->typeName : "UNKNOWN") + ": \"" + literal + "\"";
                        return false;
                    }
                    return true;
                }

                case PgDataType::JSON: {
                    String t = literal;
                    t.trim();
                    if (!(t.startsWith("{") && t.endsWith("}")) && !(t.startsWith("[") && t.endsWith("]"))) {
                        outSqlState = "22P02";
                        outErrorMsg = "invalid input syntax for type json";
                        return false;
                    }
                    return true;
                }

                default:
                    return true;
            }
        }
    };

} // namespace DbNotRuntime::Standard