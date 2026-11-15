#pragma once
#include <Arduino.h>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "PostgresSqlStates.hpp"
#include "../../rtos/RelojSistema.hpp"

namespace DbCommon::Standard {

    enum class PgDataType : uint8_t {
        SMALLINT = 0,          // int2 (16-bit)
        INTEGER,               // int4 (32-bit)
        BIGINT,                // int8 (64-bit)
        DECIMAL,               // DECIMAL(p,s)
        NUMERIC,               // NUMERIC(p,s)
        DECFLOAT,              // DECFLOAT
        REAL,                  // float4 (32-bit float)
        DOUBLE_PRECISION,      // float8 (64-bit double)
        BOOLEAN,               // bool
        CHAR,                  // CHAR(n)
        NCHAR,                 // NATIONAL CHARACTER(n)
        VARCHAR,               // VARCHAR(n)
        TEXT,                  // TEXT
        CLOB,                  // CLOB
        BIT,                   // BIT(n)
        BINARY,                // BINARY(n)
        VARBINARY,             // VARBINARY(n) / BYTEA
        BLOB,                  // BLOB
        DATE,                  // DATE ("YYYY-MM-DD")
        TIME,                  // TIME ("HH:MM:SS")
        TIME_TZ,               // TIME WITH TIME ZONE ("HH:MM:SS-03")
        TIMESTAMP,             // TIMESTAMP ("YYYY-MM-DD HH:MM:SS")
        TIMESTAMP_TZ,          // TIMESTAMP WITH TIME ZONE ("YYYY-MM-DD HH:MM:SS-03")
        INTERVAL,              // INTERVAL ("10 days", "02:30:00")
        JSON,                  // JSON
        XML,                   // XML
        URI,                   // URI
        ENUM,                  // CREATE TYPE ... AS ENUM
        DOMAIN_TYPE,           // CREATE DOMAIN ...
        ARRAY                  // base_type[]
    };

    enum class IntervalField : uint8_t {
        NONE = 0, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, YEAR_TO_MONTH, DAY_TO_SECOND
    };

    struct EnumTypeDefinition {
        String typeName = "";
        std::vector<String> labels;

        bool containsLabel(const String& candidate) const {
            for (const auto& l : labels) {
                if (l == candidate) return true;
            }
            return false;
        }
    };

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

    // Unión única compartida para almacenar el valor tipado en RAM y compararlo en O(1)
    union PgValueUnion {
        int64_t i64; // Usado por SMALLINT, INTEGER, BIGINT, DATE, TIME, TIME_TZ, TIMESTAMP, TIMESTAMP_TZ, INTERVAL
        double  f64; // Usado por REAL, DOUBLE_PRECISION, DECIMAL, NUMERIC, DECFLOAT
        bool    b;   // Usado por BOOLEAN
        PgValueUnion() : i64(0) {}
    };

    class DataTypeUtils {
    private:
        static bool esBisiesto(int anio) {
            return (anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0);
        }

        static int diasEnMes(int anio, int mes) {
            static const int tabla[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (mes < 1 || mes > 12) return 0;
            if (mes == 2 && esBisiesto(anio)) return 29;
            return tabla[mes - 1];
        }

        static bool leerDigitosFijos(const char* s, size_t offset, size_t cant, int& outVal) {
            int acum = 0;
            for (size_t i = 0; i < cant; ++i) {
                char c = s[offset + i];
                if (!isdigit(static_cast<unsigned char>(c))) return false;
                acum = (acum * 10) + (c - '0');
            }
            outVal = acum;
            return true;
        }

    public:
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
                default:                           return "TEXT";
            }
        }

        static PgDataType fromString(const String& name) {
            if (name == "SMALLINT")                        return PgDataType::SMALLINT;
            if (name == "INTEGER")                         return PgDataType::INTEGER;
            if (name == "BIGINT")                          return PgDataType::BIGINT;
            if (name == "DECIMAL")                         return PgDataType::DECIMAL;
            if (name == "NUMERIC")                         return PgDataType::NUMERIC;
            if (name == "DECFLOAT")                        return PgDataType::DECFLOAT;
            if (name == "REAL")                            return PgDataType::REAL;
            if (name == "DOUBLE PRECISION")                return PgDataType::DOUBLE_PRECISION;
            if (name == "BOOLEAN")                         return PgDataType::BOOLEAN;
            if (name == "CHARACTER")                       return PgDataType::CHAR;
            if (name == "NATIONAL CHARACTER")              return PgDataType::NCHAR;
            if (name == "CHARACTER VARYING")               return PgDataType::VARCHAR;
            if (name == "TEXT")                            return PgDataType::TEXT;
            if (name == "CLOB")                            return PgDataType::CLOB;
            if (name == "BIT")                             return PgDataType::BIT;
            if (name == "BINARY")                          return PgDataType::BINARY;
            if (name == "VARBINARY")                       return PgDataType::VARBINARY;
            if (name == "BLOB")                            return PgDataType::BLOB;
            if (name == "DATE")                            return PgDataType::DATE;
            if (name == "TIME WITHOUT TIME ZONE")          return PgDataType::TIME;
            if (name == "TIME WITH TIME ZONE")             return PgDataType::TIME_TZ;
            if (name == "TIMESTAMP WITHOUT TIME ZONE")     return PgDataType::TIMESTAMP;
            if (name == "TIMESTAMP WITH TIME ZONE")        return PgDataType::TIMESTAMP_TZ;
            if (name == "INTERVAL")                        return PgDataType::INTERVAL;
            if (name == "JSON")                            return PgDataType::JSON;
            if (name == "XML")                             return PgDataType::XML;
            if (name == "URI")                             return PgDataType::URI;
            if (name == "USER-DEFINED (ENUM)")             return PgDataType::ENUM;
            if (name == "USER-DEFINED (DOMAIN)")           return PgDataType::DOMAIN_TYPE;
            if (name == "ARRAY")                           return PgDataType::ARRAY;
            return PgDataType::TEXT;
        }

        static bool isIntegerType(PgDataType t) {
            return t == PgDataType::SMALLINT || t == PgDataType::INTEGER || t == PgDataType::BIGINT;
        }

        static bool isFloatingOrDecimalType(PgDataType t) {
            return t == PgDataType::REAL || t == PgDataType::DOUBLE_PRECISION ||
                   t == PgDataType::DECIMAL || t == PgDataType::NUMERIC || t == PgDataType::DECFLOAT;
        }

        static bool isTemporalOrEpochComparable(PgDataType t) {
            return t == PgDataType::DATE || t == PgDataType::TIME || t == PgDataType::TIME_TZ ||
                   t == PgDataType::TIMESTAMP || t == PgDataType::TIMESTAMP_TZ || t == PgDataType::INTERVAL;
        }

        // Valida y convierte "YYYY-MM-DD" a epoch en segundos (00:00:00 UTC)
        static bool parseIsoDate(const String& str, int64_t& outEpochSec, String& outSqlState, String& outErr) {
            if (str.equalsIgnoreCase("CURRENT_DATE") || str.equalsIgnoreCase("NOW()")) {
                outEpochSec = static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual() / 86400UL) * 86400LL;
                return true;
            }
            if (str.length() != 10 || str[4] != '-' || str[7] != '-') {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type date: \"" + str + "\" (expected YYYY-MM-DD)";
                return false;
            }
            int y = 0, m = 0, d = 0;
            if (!leerDigitosFijos(str.c_str(), 0, 4, y) ||
                !leerDigitosFijos(str.c_str(), 5, 2, m) ||
                !leerDigitosFijos(str.c_str(), 8, 2, d)) {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type date: \"" + str + "\"";
                return false;
            }
            int maxDias = diasEnMes(y, m);
            if (y < 1970 || y > 2100 || m < 1 || m > 12 || d < 1 || d > maxDias) {
                outSqlState = SqlState::DATETIME_FIELD_OVERFLOW;
                outErr = "date/time field value out of range: \"" + str + "\"";
                return false;
            }
            struct tm tmVal{};
            tmVal.tm_year = y - 1900;
            tmVal.tm_mon  = m - 1;
            tmVal.tm_mday = d;
            outEpochSec   = static_cast<int64_t>(mktime(&tmVal));
            return true;
        }

        // Valida y convierte "HH:MM:SS" (y zona horaria opcional en TIME_TZ) a segundos desde medianoche
        static bool parseIsoTime(const String& str, bool allowTz, int64_t& outSecOfDay, String& outSqlState, String& outErr) {
            if (str.equalsIgnoreCase("CURRENT_TIME") || str.equalsIgnoreCase("NOW()")) {
                outSecOfDay = static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual() % 86400UL);
                return true;
            }
            if (str.length() < 8 || str[2] != ':' || str[5] != ':') {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type time: \"" + str + "\" (expected HH:MM:SS)";
                return false;
            }
            int hh = 0, mm = 0, ss = 0;
            if (!leerDigitosFijos(str.c_str(), 0, 2, hh) ||
                !leerDigitosFijos(str.c_str(), 3, 2, mm) ||
                !leerDigitosFijos(str.c_str(), 6, 2, ss)) {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type time: \"" + str + "\"";
                return false;
            }
            if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
                outSqlState = SqlState::DATETIME_FIELD_OVERFLOW;
                outErr = "time field value out of range: \"" + str + "\"";
                return false;
            }
            if (str.length() > 8) {
                if (!allowTz) {
                    outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                    outErr = "unexpected time zone in TIME WITHOUT TIME ZONE: \"" + str + "\"";
                    return false;
                }
                char sign = str[8];
                if (sign != '+' && sign != '-' && sign != 'Z') {
                    outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                    outErr = "invalid timezone offset in TIME WITH TIME ZONE: \"" + str + "\"";
                    return false;
                }
            }
            outSecOfDay = static_cast<int64_t>(hh * 3600 + mm * 60 + ss);
            return true;
        }

        // Valida y convierte "YYYY-MM-DD HH:MM:SS" (o con 'T' y zona horaria) a Unix timestamp en segundos
        static bool parseIsoTimestamp(const String& str, bool allowTz, int64_t& outEpochSec, String& outSqlState, String& outErr) {
            if (str.equalsIgnoreCase("CURRENT_TIMESTAMP") || str.equalsIgnoreCase("NOW()")) {
                outEpochSec = static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
                return true;
            }
            if (str.length() < 19 || (str[10] != ' ' && str[10] != 'T')) {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type timestamp: \"" + str + "\" (expected YYYY-MM-DD HH:MM:SS)";
                return false;
            }
            String datePart = str.substring(0, 10);
            String timePart = str.substring(11);
            int64_t dEpoch = 0, tSec = 0;
            if (!parseIsoDate(datePart, dEpoch, outSqlState, outErr)) return false;

            String pureTime = timePart.substring(0, 8);
            if (!parseIsoTime(pureTime, false, tSec, outSqlState, outErr)) return false;

            if (timePart.length() > 8) {
                if (!allowTz) {
                    outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                    outErr = "unexpected timezone offset in TIMESTAMP WITHOUT TIME ZONE: \"" + str + "\"";
                    return false;
                }
                char sign = timePart[8];
                if (sign != '+' && sign != '-' && sign != 'Z') {
                    outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                    outErr = "invalid timezone offset in timestamp: \"" + str + "\"";
                    return false;
                }
            }
            outEpochSec = dEpoch + tSec;
            return true;
        }

        static bool parseIntervalSeconds(const String& str, int64_t& outSeconds, String& outSqlState, String& outErr) {
            String t = str;
            t.trim();
            if (t.isEmpty()) {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "empty interval literal";
                return false;
            }
            if (t.length() == 8 && t[2] == ':' && t[5] == ':') {
                return parseIsoTime(t, false, outSeconds, outSqlState, outErr);
            }
            char* endPtr = nullptr;
            long val = strtol(t.c_str(), &endPtr, 10);
            if (endPtr == t.c_str()) {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "invalid input syntax for type interval: \"" + str + "\"";
                return false;
            }
            String unit = String(endPtr);
            unit.trim();
            unit.toLowerCase();
            if (unit.startsWith("sec") || unit == "s" || unit.isEmpty()) outSeconds = val;
            else if (unit.startsWith("min") || unit == "m") outSeconds = val * 60LL;
            else if (unit.startsWith("hour") || unit == "h") outSeconds = val * 3600LL;
            else if (unit.startsWith("day") || unit == "d") outSeconds = val * 86400LL;
            else if (unit.startsWith("mon")) outSeconds = val * 2592000LL;
            else if (unit.startsWith("year") || unit == "y") outSeconds = val * 31536000LL;
            else {
                outSqlState = SqlState::INVALID_DATETIME_FORMAT;
                outErr = "unrecognized interval unit in: \"" + str + "\"";
                return false;
            }
            return true;
        }

        static String resolveTemporalKeywords(PgDataType type, const String& rawLiteral) {
            if (rawLiteral.equalsIgnoreCase("CURRENT_DATE")) {
                return RTOS::RelojSistema::fechaActualPg().toString();
            }
            if (rawLiteral.equalsIgnoreCase("CURRENT_TIME")) {
                if (type == PgDataType::TIME_TZ) {
                    String dtTz = RTOS::RelojSistema::fechaHoraTzActualPg().toString();
                    return (dtTz.length() >= 22) ? dtTz.substring(11) : String("00:00:00-03");
                }
                String dt = RTOS::RelojSistema::fechaHoraActualPg().toString();
                return (dt.length() >= 19) ? dt.substring(11, 19) : String("00:00:00");
            }
            if (rawLiteral.equalsIgnoreCase("CURRENT_TIMESTAMP") || rawLiteral.equalsIgnoreCase("NOW()")) {
                if (type == PgDataType::DATE) return RTOS::RelojSistema::fechaActualPg().toString();
                if (type == PgDataType::TIME) {
                    String dt = RTOS::RelojSistema::fechaHoraActualPg().toString();
                    return (dt.length() >= 19) ? dt.substring(11, 19) : String("00:00:00");
                }
                if (type == PgDataType::TIME_TZ) {
                    String dtTz = RTOS::RelojSistema::fechaHoraTzActualPg().toString();
                    return (dtTz.length() >= 22) ? dtTz.substring(11) : String("00:00:00-03");
                }
                if (type == PgDataType::TIMESTAMP_TZ) return RTOS::RelojSistema::fechaHoraTzActualPg().toString();
                return RTOS::RelojSistema::fechaHoraActualPg().toString();
            }
            return rawLiteral;
        }

        static bool areForeignKeyCompatible(PgDataType fkType, const String& fkCustomName,
                                            PgDataType pkType, const String& pkCustomName) {
            if (fkType == pkType) {
                if (fkType == PgDataType::ENUM || fkType == PgDataType::DOMAIN_TYPE) {
                    return fkCustomName.equalsIgnoreCase(pkCustomName);
                }
                return true;
            }
            if ((fkType == PgDataType::DECIMAL && pkType == PgDataType::NUMERIC) ||
                (fkType == PgDataType::NUMERIC && pkType == PgDataType::DECIMAL)) {
                return true;
            }
            bool fkChar = (fkType == PgDataType::VARCHAR || fkType == PgDataType::TEXT ||
                           fkType == PgDataType::CHAR || fkType == PgDataType::NCHAR);
            bool pkChar = (pkType == PgDataType::VARCHAR || pkType == PgDataType::TEXT ||
                           pkType == PgDataType::CHAR || pkType == PgDataType::NCHAR);
            return fkChar && pkChar;
        }

        // Valida que un literal cumpla estrictamente la gramática de cualquiera de los 30 tipos de PostgreSQL
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
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid input syntax for type integer: \"" + literal + "\"";
                        return false;
                    }
                    for (size_t i = start; i < literal.length(); ++i) {
                        if (!isdigit(static_cast<unsigned char>(literal[i]))) {
                            outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                            outErrorMsg = "invalid input syntax for type integer: \"" + literal + "\"";
                            return false;
                        }
                    }
                    long long val = atoll(literal.c_str());
                    if (type == PgDataType::SMALLINT && (val < -32768LL || val > 32767LL)) {
                        outSqlState = SqlState::NUMERIC_VALUE_OUT_OF_RANGE;
                        outErrorMsg = "smallint out of range";
                        return false;
                    }
                    if (type == PgDataType::INTEGER && (val < -2147483648LL || val > 2147483647LL)) {
                        outSqlState = SqlState::NUMERIC_VALUE_OUT_OF_RANGE;
                        outErrorMsg = "integer out of range";
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
                    int intDigits = 0, fracDigits = 0;
                    if (literal.length() <= static_cast<size_t>(start)) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid input syntax for type numeric: \"" + literal + "\"";
                        return false;
                    }
                    for (size_t i = start; i < literal.length(); ++i) {
                        if (literal[i] == '.') {
                            if (hasDot) {
                                outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                                outErrorMsg = "multiple decimal points in numeric literal";
                                return false;
                            }
                            hasDot = true;
                        } else if (isdigit(static_cast<unsigned char>(literal[i]))) {
                            if (hasDot) fracDigits++;
                            else intDigits++;
                        } else {
                            outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                            outErrorMsg = "invalid input syntax for type numeric: \"" + literal + "\"";
                            return false;
                        }
                    }
                    if ((type == PgDataType::DECIMAL || type == PgDataType::NUMERIC) && precision > 0) {
                        if ((intDigits + fracDigits) > precision || fracDigits > scale) {
                            outSqlState = SqlState::NUMERIC_VALUE_OUT_OF_RANGE;
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
                    outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                    outErrorMsg = "invalid input syntax for type boolean: \"" + literal + "\"";
                    return false;
                }

                case PgDataType::CHAR:
                case PgDataType::NCHAR:
                case PgDataType::VARCHAR: {
                    if (maxLength > 0 && literal.length() > maxLength) {
                        outSqlState = SqlState::STRING_DATA_RIGHT_TRUNCATION;
                        outErrorMsg = "value too long for type character varying(" + String(maxLength) + ")";
                        return false;
                    }
                    return true;
                }

                case PgDataType::BIT: {
                    if (maxLength > 0 && literal.length() > maxLength) {
                        outSqlState = SqlState::STRING_DATA_RIGHT_TRUNCATION;
                        outErrorMsg = "bit string too long for type bit(" + String(maxLength) + ")";
                        return false;
                    }
                    for (size_t i = 0; i < literal.length(); ++i) {
                        if (literal[i] != '0' && literal[i] != '1') {
                            outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                            outErrorMsg = "\"" + literal + "\" is not a valid bit string";
                            return false;
                        }
                    }
                    return true;
                }

                case PgDataType::BINARY:
                case PgDataType::VARBINARY:
                case PgDataType::BLOB: {
                    size_t start = (literal.startsWith("\\x") || literal.startsWith("0x")) ? 2 : 0;
                    size_t hexLen = literal.length() - start;
                    if (hexLen % 2 != 0) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid hexadecimal data: odd number of digits";
                        return false;
                    }
                    for (size_t i = start; i < literal.length(); ++i) {
                        if (!isxdigit(static_cast<unsigned char>(literal[i]))) {
                            outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                            outErrorMsg = "invalid hexadecimal digit in binary literal";
                            return false;
                        }
                    }
                    if (maxLength > 0 && (hexLen / 2) > maxLength) {
                        outSqlState = SqlState::STRING_DATA_RIGHT_TRUNCATION;
                        outErrorMsg = "binary value exceeds maximum length of " + String(maxLength) + " bytes";
                        return false;
                    }
                    return true;
                }

                case PgDataType::DATE: {
                    int64_t dummy = 0;
                    return parseIsoDate(literal, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::TIME: {
                    int64_t dummy = 0;
                    return parseIsoTime(literal, false, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::TIME_TZ: {
                    int64_t dummy = 0;
                    return parseIsoTime(literal, true, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::TIMESTAMP: {
                    int64_t dummy = 0;
                    return parseIsoTimestamp(literal, false, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::TIMESTAMP_TZ: {
                    int64_t dummy = 0;
                    return parseIsoTimestamp(literal, true, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::INTERVAL: {
                    int64_t dummy = 0;
                    return parseIntervalSeconds(literal, dummy, outSqlState, outErrorMsg);
                }

                case PgDataType::ENUM: {
                    if (!enumDef || !enumDef->containsLabel(literal)) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid input value for enum " + (enumDef ? enumDef->typeName : "UNKNOWN") + ": \"" + literal + "\"";
                        return false;
                    }
                    return true;
                }

                case PgDataType::JSON: {
                    String t = literal;
                    t.trim();
                    if (!(t.startsWith("{") && t.endsWith("}")) && !(t.startsWith("[") && t.endsWith("]"))) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid input syntax for type json";
                        return false;
                    }
                    return true;
                }

                case PgDataType::XML: {
                    String t = literal;
                    t.trim();
                    if (!t.startsWith("<") || !t.endsWith(">")) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid XML content: must start with '<' and end with '>'";
                        return false;
                    }
                    return true;
                }

                case PgDataType::URI: {
                    if (literal.indexOf("://") <= 0) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "invalid URI syntax: missing scheme:// in \"" + literal + "\"";
                        return false;
                    }
                    return true;
                }

                case PgDataType::ARRAY: {
                    String t = literal;
                    t.trim();
                    if (!(t.startsWith("{") && t.endsWith("}")) && !(t.startsWith("[") && t.endsWith("]"))) {
                        outSqlState = SqlState::INVALID_TEXT_REPRESENTATION;
                        outErrorMsg = "malformed array literal: \"" + literal + "\" (expected {...})";
                        return false;
                    }
                    return true;
                }

                default:
                    return true;
            }
        }

        // ÚNICO punto de sincronización: convierte el literal a la unión nativa (Usado por PgCell::setValue)
        static void syncLiteralToUnion(PgDataType dt, const String& literal, String& outNormalizedText, PgValueUnion& outUnion, bool& outIsNull) {
            if (literal.equalsIgnoreCase("NULL")) {
                outIsNull         = true;
                outNormalizedText = "NULL";
                outUnion.i64      = 0;
                return;
            }
            outIsNull         = false;
            outNormalizedText = resolveTemporalKeywords(dt, literal);
            String dummyState, dummyErr;

            switch (dt) {
                case PgDataType::SMALLINT:
                case PgDataType::INTEGER:
                case PgDataType::BIGINT:
                    outUnion.i64 = atoll(outNormalizedText.c_str());
                    break;

                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:
                case PgDataType::REAL:
                case PgDataType::DOUBLE_PRECISION:
                    outUnion.f64 = strtod(outNormalizedText.c_str(), nullptr);
                    break;

                case PgDataType::BOOLEAN: {
                    String up = outNormalizedText;
                    up.toUpperCase();
                    outUnion.b        = (up == "TRUE" || up == "1" || up == "T");
                    outNormalizedText = outUnion.b ? "TRUE" : "FALSE";
                    break;
                }

                case PgDataType::DATE:
                    if (!parseIsoDate(outNormalizedText, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                case PgDataType::TIME:
                    if (!parseIsoTime(outNormalizedText, false, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                case PgDataType::TIME_TZ:
                    if (!parseIsoTime(outNormalizedText, true, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                case PgDataType::TIMESTAMP:
                    if (!parseIsoTimestamp(outNormalizedText, false, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                case PgDataType::TIMESTAMP_TZ:
                    if (!parseIsoTimestamp(outNormalizedText, true, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                case PgDataType::INTERVAL:
                    if (!parseIntervalSeconds(outNormalizedText, outUnion.i64, dummyState, dummyErr)) outUnion.i64 = 0;
                    break;

                default:
                    outUnion.i64 = 0;
                    break;
            }
        }

        // ÚNICO comparador tipado para WHERE, ORDER BY, MIN/MAX y Ventanas en ModuloDQL
        static int compareUnionWithLiteral(PgDataType dt, const PgValueUnion& cellUnion, const String& cellRawText, const String& rhsLiteral) {
            if (isIntegerType(dt)) {
                int64_t a = cellUnion.i64;
                int64_t b = atoll(rhsLiteral.c_str());
                return (a < b) ? -1 : ((a > b) ? 1 : 0);
            }
            if (isFloatingOrDecimalType(dt)) {
                double a = cellUnion.f64;
                double b = strtod(rhsLiteral.c_str(), nullptr);
                if (fabs(a - b) < 0.00001) return 0;
                return (a < b) ? -1 : 1;
            }
            if (dt == PgDataType::BOOLEAN) {
                bool a = cellUnion.b;
                String up = rhsLiteral;
                up.toUpperCase();
                bool b = (up == "TRUE" || up == "1" || up == "T");
                return (a == b) ? 0 : (a ? 1 : -1);
            }
            if (isTemporalOrEpochComparable(dt)) {
                String normRhs;
                PgValueUnion rhsUnion;
                bool rhsNull = false;
                syncLiteralToUnion(dt, rhsLiteral, normRhs, rhsUnion, rhsNull);
                if (cellUnion.i64 != 0 && rhsUnion.i64 != 0) {
                    return (cellUnion.i64 < rhsUnion.i64) ? -1 : ((cellUnion.i64 > rhsUnion.i64) ? 1 : 0);
                }
                return strcmp(cellRawText.c_str(), normRhs.c_str());
            }
            return strcmp(cellRawText.c_str(), rhsLiteral.c_str());
        }
    };

} // namespace DbCommon::Standard

namespace DbNotRuntime::Standard {
    using namespace DbCommon::Standard;
}