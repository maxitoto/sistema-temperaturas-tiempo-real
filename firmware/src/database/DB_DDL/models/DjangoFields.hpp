#pragma once
#include <Arduino.h>
#include <vector>
#include <utility>
#include "../../common/PostgresDataTypes.hpp"
#include "../../common/PostgresConstraints.hpp"
#include "../engine/ModuloDDL.hpp"

namespace DbNotRuntime::Models {

    static constexpr Standard::ReferentialAction CASCADE     = Standard::ReferentialAction::CASCADE;
    static constexpr Standard::ReferentialAction PROTECT     = Standard::ReferentialAction::RESTRICT;
    static constexpr Standard::ReferentialAction RESTRICT    = Standard::ReferentialAction::RESTRICT;
    static constexpr Standard::ReferentialAction SET_NULL    = Standard::ReferentialAction::SET_NULL;
    static constexpr Standard::ReferentialAction SET_DEFAULT = Standard::ReferentialAction::SET_DEFAULT;
    static constexpr Standard::ReferentialAction DO_NOTHING  = Standard::ReferentialAction::NO_ACTION;

    class PgEnumChoices {
    public:
        static bool registerChoices(const String& enumTypeName, const std::vector<String>& labels) {
            if (Engine::PhysicalSchemaCatalog::getInstance().hasEnum(enumTypeName)) {
                return true;
            }
            return Engine::ModuloDDL::getInstance().createEnumType(enumTypeName, labels);
        }
    };

    class Field {
    protected:
        Standard::ColumnConstraintDef def;

    public:
        Field(const String& colName, Standard::PgDataType type) {
            def.columnName = colName;
            def.dataType   = type;
            def.isNotNull  = true;
        }

        Field(const Field&) = default;
        Field(Field&&) noexcept = default;
        Field& operator=(const Field&) = default;
        Field& operator=(Field&&) noexcept = default;

        Field&& primary_key(bool val = true) {
            def.isPrimaryKey = val;
            if (val) {
                def.isNotNull = true;
                def.isUnique  = true;
            }
            return std::move(*this);
        }

        Field&& null(bool allowNull) {
            def.isNotNull = !allowNull;
            return std::move(*this);
        }

        Field&& unique(bool val = true) {
            def.isUnique = val;
            return std::move(*this);
        }

        Field&& default_val(const char* literal) {
            def.hasDefault   = true;
            def.defaultValue = literal ? String(literal) : String("");
            return std::move(*this);
        }

        Field&& default_val(const String& literal) {
            def.hasDefault   = true;
            def.defaultValue = literal;
            return std::move(*this);
        }

        Field&& default_val(int numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(numericLiteral);
            return std::move(*this);
        }

        Field&& default_val(long numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(numericLiteral);
            return std::move(*this);
        }

        Field&& default_val(long long numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(static_cast<long>(numericLiteral));
            return std::move(*this);
        }

        Field&& default_val(float floatLiteral, uint8_t decimals = 2) {
            def.hasDefault   = true;
            def.defaultValue = String(floatLiteral, static_cast<unsigned int>(decimals));
            return std::move(*this);
        }

        Field&& default_val(double doubleLiteral, uint8_t decimals = 2) {
            def.hasDefault   = true;
            def.defaultValue = String(doubleLiteral, static_cast<unsigned int>(decimals));
            return std::move(*this);
        }

        Field&& auto_now_add(bool enabled = true) {
            def.autoNowAdd = enabled;
            if (enabled) {
                def.hasDefault = true;
                if (def.dataType == Standard::PgDataType::DATE) def.defaultValue = "CURRENT_DATE";
                else if (def.dataType == Standard::PgDataType::TIME || def.dataType == Standard::PgDataType::TIME_TZ) def.defaultValue = "CURRENT_TIME";
                else def.defaultValue = "CURRENT_TIMESTAMP";
            }
            return std::move(*this);
        }

        Field&& auto_now(bool enabled = true) {
            def.autoNow = enabled;
            if (enabled) {
                def.hasDefault = true;
                if (def.dataType == Standard::PgDataType::DATE) def.defaultValue = "CURRENT_DATE";
                else if (def.dataType == Standard::PgDataType::TIME || def.dataType == Standard::PgDataType::TIME_TZ) def.defaultValue = "CURRENT_TIME";
                else def.defaultValue = "CURRENT_TIMESTAMP";
            }
            return std::move(*this);
        }

        Field&& check(const String& sqlExpr, Standard::CheckEvaluatorFn fn = nullptr) {
            def.hasCheck        = true;
            def.checkExpression = sqlExpr;
            def.checkFn         = fn;
            return std::move(*this);
        }

        Field&& db_comment(const String& comment) {
            def.comment = comment;
            return std::move(*this);
        }

        const Standard::ColumnConstraintDef& compiled() const { return def; }
    };

    // ---------------- 1. Campos Autoincrementales (IDENTITY) ----------------

    class SmallAutoField : public Field {
    public:
        explicit SmallAutoField(const String& colName) : Field(colName, Standard::PgDataType::SMALLINT) {
            def.isPrimaryKey      = true;
            def.isNotNull         = true;
            def.isUnique          = true;
            def.identityMode      = Standard::IdentityGeneration::BY_DEFAULT;
            def.identityStart     = 1;
            def.identityIncrement = 1;
        }
    };

    class AutoField : public Field {
    public:
        explicit AutoField(const String& colName) : Field(colName, Standard::PgDataType::INTEGER) {
            def.isPrimaryKey      = true;
            def.isNotNull         = true;
            def.isUnique          = true;
            def.identityMode      = Standard::IdentityGeneration::BY_DEFAULT;
            def.identityStart     = 1;
            def.identityIncrement = 1;
        }
    };

    class BigAutoField : public Field {
    public:
        explicit BigAutoField(const String& colName) : Field(colName, Standard::PgDataType::BIGINT) {
            def.isPrimaryKey      = true;
            def.isNotNull         = true;
            def.isUnique          = true;
            def.identityMode      = Standard::IdentityGeneration::BY_DEFAULT;
            def.identityStart     = 1;
            def.identityIncrement = 1;
        }
    };

    // ---------------- 2. Campos Numéricos Enteros y de Punto Flotante ----------------

    class SmallIntegerField : public Field {
    public:
        explicit SmallIntegerField(const String& colName) : Field(colName, Standard::PgDataType::SMALLINT) {}
    };

    class PositiveSmallIntegerField : public Field {
    public:
        explicit PositiveSmallIntegerField(const String& colName) : Field(colName, Standard::PgDataType::SMALLINT) {
            check(colName + " >= 0", +[](const String& v) -> bool { return v.toInt() >= 0; });
        }
    };

    class IntegerField : public Field {
    public:
        explicit IntegerField(const String& colName) : Field(colName, Standard::PgDataType::INTEGER) {}
    };

    class PositiveIntegerField : public Field {
    public:
        explicit PositiveIntegerField(const String& colName) : Field(colName, Standard::PgDataType::INTEGER) {
            check(colName + " >= 0", +[](const String& v) -> bool { return v.toInt() >= 0; });
        }
    };

    class BigIntegerField : public Field {
    public:
        explicit BigIntegerField(const String& colName) : Field(colName, Standard::PgDataType::BIGINT) {}
    };

    class PositiveBigIntegerField : public Field {
    public:
        explicit PositiveBigIntegerField(const String& colName) : Field(colName, Standard::PgDataType::BIGINT) {
            check(colName + " >= 0", +[](const String& v) -> bool { return atoll(v.c_str()) >= 0; });
        }
    };

    class FloatField : public Field {
    public:
        explicit FloatField(const String& colName, bool doublePrecision = false)
            : Field(colName, doublePrecision ? Standard::PgDataType::DOUBLE_PRECISION : Standard::PgDataType::REAL) {}
    };

    class DoublePrecisionField : public Field {
    public:
        explicit DoublePrecisionField(const String& colName)
            : Field(colName, Standard::PgDataType::DOUBLE_PRECISION) {}
    };

    class DecimalField : public Field {
    public:
        DecimalField(const String& colName, uint8_t maxDigits, uint8_t decimalPlaces, bool useDecimalType = false)
            : Field(colName, useDecimalType ? Standard::PgDataType::DECIMAL : Standard::PgDataType::NUMERIC) {
            def.precision = maxDigits;
            def.scale     = decimalPlaces;
        }
    };

    class DecFloatField : public Field {
    public:
        explicit DecFloatField(const String& colName) : Field(colName, Standard::PgDataType::DECFLOAT) {}
    };

    // ---------------- 3. Campos Booleanos, Caracteres y Binarios ----------------

    class BooleanField : public Field {
    public:
        explicit BooleanField(const String& colName) : Field(colName, Standard::PgDataType::BOOLEAN) {}
    };

    class CharField : public Field {
    public:
        CharField(const String& colName, uint16_t maxLength)
            : Field(colName, Standard::PgDataType::VARCHAR) {
            def.maxLength = maxLength;
        }
    };

    class FixedCharField : public Field {
    public:
        FixedCharField(const String& colName, uint16_t exactLength)
            : Field(colName, Standard::PgDataType::CHAR) {
            def.maxLength = exactLength;
        }
    };

    class NationalCharField : public Field {
    public:
        NationalCharField(const String& colName, uint16_t maxLength)
            : Field(colName, Standard::PgDataType::NCHAR) {
            def.maxLength = maxLength;
        }
    };

    class TextField : public Field {
    public:
        explicit TextField(const String& colName) : Field(colName, Standard::PgDataType::TEXT) {}
    };

    class ClobField : public Field {
    public:
        explicit ClobField(const String& colName) : Field(colName, Standard::PgDataType::CLOB) {}
    };

    class BitField : public Field {
    public:
        BitField(const String& colName, uint16_t bitLength = 1)
            : Field(colName, Standard::PgDataType::BIT) {
            def.maxLength = bitLength;
        }
    };

    class BinaryField : public Field {
    public:
        BinaryField(const String& colName, uint16_t maxBytes = 0, bool fixedLength = false)
            : Field(colName, fixedLength ? Standard::PgDataType::BINARY : Standard::PgDataType::VARBINARY) {
            def.maxLength = maxBytes;
        }
    };

    class BlobField : public Field {
    public:
        explicit BlobField(const String& colName) : Field(colName, Standard::PgDataType::BLOB) {}
    };

    // ---------------- 4. Campos de Fecha, Hora e Intervalo (ISO-8601 real) ----------------

    class DateField : public Field {
    public:
        explicit DateField(const String& colName, bool autoNow = false, bool autoNowAdd = true)
            : Field(colName, Standard::PgDataType::DATE) {
            def.autoNow      = autoNow;
            def.autoNowAdd   = autoNowAdd;
            def.hasDefault   = (autoNow || autoNowAdd);
            def.defaultValue = (autoNow || autoNowAdd) ? "CURRENT_DATE" : "";
        }
    };

    class TimeField : public Field {
    public:
        explicit TimeField(const String& colName, bool useTz = false, bool autoNow = false, bool autoNowAdd = true)
            : Field(colName, useTz ? Standard::PgDataType::TIME_TZ : Standard::PgDataType::TIME) {
            def.autoNow      = autoNow;
            def.autoNowAdd   = autoNowAdd;
            def.hasDefault   = (autoNow || autoNowAdd);
            def.defaultValue = (autoNow || autoNowAdd) ? "CURRENT_TIME" : "";
        }
    };

    class DateTimeField : public Field {
    public:
        explicit DateTimeField(const String& colName, bool useTz = false, bool autoNow = false, bool autoNowAdd = true)
            : Field(colName, useTz ? Standard::PgDataType::TIMESTAMP_TZ : Standard::PgDataType::TIMESTAMP) {
            def.autoNow      = autoNow;
            def.autoNowAdd   = autoNowAdd;
            def.hasDefault   = (autoNow || autoNowAdd);
            def.defaultValue = (autoNow || autoNowAdd) ? "CURRENT_TIMESTAMP" : "";
        }
    };

    class DurationField : public Field {
    public:
        explicit DurationField(const String& colName, Standard::IntervalField precisionField = Standard::IntervalField::SECOND)
            : Field(colName, Standard::PgDataType::INTERVAL) {
            def.intervalField = precisionField;
        }
    };

    // ---------------- 5. Campos Estructurados, Personalizados y Relacionales ----------------

    class JSONField : public Field {
    public:
        explicit JSONField(const String& colName) : Field(colName, Standard::PgDataType::JSON) {}
    };

    class XMLField : public Field {
    public:
        explicit XMLField(const String& colName) : Field(colName, Standard::PgDataType::XML) {}
    };

    class URLField : public Field {
    public:
        explicit URLField(const String& colName) : Field(colName, Standard::PgDataType::URI) {}
    };

    class EnumField : public Field {
    public:
        EnumField(const String& colName, const String& registeredEnumTypeName)
            : Field(colName, Standard::PgDataType::ENUM) {
            def.customTypeName = registeredEnumTypeName;
        }
    };

    class DomainField : public Field {
    public:
        DomainField(const String& colName, const String& registeredDomainName)
            : Field(colName, Standard::PgDataType::DOMAIN_TYPE) {
            def.customTypeName = registeredDomainName;
        }
    };

    class ArrayField : public Field {
    public:
        explicit ArrayField(const String& colName) : Field(colName, Standard::PgDataType::ARRAY) {
            def.isArray = true;
        }
    };

    class ForeignKey : public Field {
    public:
        ForeignKey(const String& colName,
                   const String& toTable,
                   const String& toColumn,
                   Standard::ReferentialAction onDeleteAction = CASCADE,
                   Standard::PgDataType fkDataType = Standard::PgDataType::INTEGER)
            : Field(colName, fkDataType) {
            def.isForeignKey     = true;
            def.referencedTable  = toTable;
            def.referencedColumn = toColumn;
            def.onDelete         = onDeleteAction;
            def.onUpdate         = Standard::ReferentialAction::CASCADE;
        }
    };

    class OneToOneField : public ForeignKey {
    public:
        OneToOneField(const String& colName,
                      const String& toTable,
                      const String& toColumn,
                      Standard::ReferentialAction onDeleteAction = CASCADE,
                      Standard::PgDataType fkDataType = Standard::PgDataType::INTEGER)
            : ForeignKey(colName, toTable, toColumn, onDeleteAction, fkDataType) {
            def.isUnique = true;
        }
    };

} // namespace DbNotRuntime::Models