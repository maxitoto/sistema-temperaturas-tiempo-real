#pragma once

namespace DbCommon::Standard {

    struct SqlState {
        static constexpr const char* SUCCESSFUL_COMPLETION             = "00000";
        static constexpr const char* WARNING                           = "01000";
        static constexpr const char* WARNING_IMPLICIT_ZERO_BIT_PADDING = "01008";

        static constexpr const char* FEATURE_NOT_SUPPORTED             = "0A000";

        static constexpr const char* DATA_EXCEPTION                    = "22000";
        static constexpr const char* STRING_DATA_RIGHT_TRUNCATION      = "22001";
        static constexpr const char* NUMERIC_VALUE_OUT_OF_RANGE        = "22003";
        static constexpr const char* INVALID_DATETIME_FORMAT           = "22007";
        static constexpr const char* DATETIME_FIELD_OVERFLOW           = "22008";
        static constexpr const char* INVALID_TEXT_REPRESENTATION       = "22P02";
        static constexpr const char* INVALID_PARAMETER_VALUE           = "22023";

        static constexpr const char* INTEGRITY_CONSTRAINT_VIOLATION    = "23000";
        static constexpr const char* NOT_NULL_VIOLATION                = "23502";
        static constexpr const char* FOREIGN_KEY_VIOLATION             = "23503";
        static constexpr const char* UNIQUE_VIOLATION                  = "23505";
        static constexpr const char* CHECK_VIOLATION                   = "23514";

        static constexpr const char* DEPENDENT_OBJECTS_STILL_EXIST     = "2BP01";

        static constexpr const char* SYNTAX_ERROR_OR_ACCESS_VIOLATION  = "42000";
        static constexpr const char* SYNTAX_ERROR                      = "42601";
        static constexpr const char* INVALID_NAME                      = "42602";
        static constexpr const char* INVALID_COLUMN_DEFINITION         = "42611";
        static constexpr const char* NAME_TOO_LONG                     = "42622";
        static constexpr const char* DUPLICATE_COLUMN                  = "42701";
        static constexpr const char* AMBIGUOUS_COLUMN                  = "42702";
        static constexpr const char* UNDEFINED_COLUMN                  = "42703";
        static constexpr const char* UNDEFINED_OBJECT                  = "42704";
        static constexpr const char* DUPLICATE_OBJECT                  = "42710";
        static constexpr const char* DUPLICATE_ALIAS                   = "42712";
        static constexpr const char* TUPLE_ALREADY_HAS_PRIMARY_KEY     = "42P16";
        static constexpr const char* UNDEFINED_TABLE                   = "42P01";
        static constexpr const char* DUPLICATE_TABLE                   = "42P07";
        static constexpr const char* INVALID_TABLE_DEFINITION          = "42P16";
        static constexpr const char* INVALID_OBJECT_DEFINITION         = "42P17";
        static constexpr const char* DATATYPE_MISMATCH                 = "42804";
        static constexpr const char* WRONG_OBJECT_TYPE                 = "42809";
        static constexpr const char* INVALID_FOREIGN_KEY               = "42830";

        static constexpr const char* IO_ERROR                          = "58030";
    };

} // namespace DbCommon::Standard

namespace DbNotRuntime::Standard {
    using namespace DbCommon::Standard;
}