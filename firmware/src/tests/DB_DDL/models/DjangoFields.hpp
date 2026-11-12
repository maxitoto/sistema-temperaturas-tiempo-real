#pragma once
#include <Arduino.h>
#include <vector>
#include <utility>
#include "../standard/PostgresDataTypes.hpp"
#include "../standard/PostgresConstraints.hpp"
#include "../engine/ModuloDDL.hpp"

namespace DbNotRuntime::Models {

    // Mapea las constantes de integridad referencial estilo Django (models::CASCADE, models::PROTECT, etc.) a PostgreSQL.
    static constexpr Standard::ReferentialAction CASCADE     = Standard::ReferentialAction::CASCADE;
    static constexpr Standard::ReferentialAction PROTECT     = Standard::ReferentialAction::RESTRICT;
    static constexpr Standard::ReferentialAction RESTRICT    = Standard::ReferentialAction::RESTRICT;
    static constexpr Standard::ReferentialAction SET_NULL    = Standard::ReferentialAction::SET_NULL;
    static constexpr Standard::ReferentialAction SET_DEFAULT = Standard::ReferentialAction::SET_DEFAULT;
    static constexpr Standard::ReferentialAction DO_NOTHING  = Standard::ReferentialAction::NO_ACTION;

    // Permite declarar enumeraciones nativas de PostgreSQL al estilo TextChoices / IntegerChoices de Django.
    class PgEnumChoices {
    public:
        // Registra inmediatamente el tipo ENUM en el catálogo PostgreSQL a partir de su nombre y lista de etiquetas.
        static bool registerChoices(const String& enumTypeName, const std::vector<String>& labels) {
            if (Engine::PhysicalSchemaCatalog::getInstance().hasEnum(enumTypeName)) {
                return true;
            }
            return Engine::ModuloDDL::getInstance().createEnumType(enumTypeName, labels);
        }
    };

    // Clase fluido-declarativa que representa un Field de Django y se mueve sin copias pesadas en la pila.
    class Field {
    protected:
        Standard::ColumnConstraintDef def;

    public:
        // Inicializa un campo base indicando el nombre de columna y su tipo nativo en PostgreSQL.
        Field(const String& colName, Standard::PgDataType type) {
            def.columnName = colName;
            def.dataType   = type;
            def.isNotNull  = true; // En Django todos los campos son null=False (NOT NULL) por defecto
        }

        Field(const Field&) = default;
        Field(Field&&) noexcept = default;
        Field& operator=(const Field&) = default;
        Field& operator=(Field&&) noexcept = default;

        // Equivale a primary_key=True en Django moviendo la instancia temporal para no duplicar memoria en pila.
        Field&& primary_key(bool val = true) {
            def.isPrimaryKey = val;
            if (val) {
                def.isNotNull = true;
                def.isUnique  = true;
            }
            return std::move(*this);
        }

        // Equivale a null=True/False en Django (invierte la bandera física NOT NULL de PostgreSQL).
        Field&& null(bool allowNull) {
            def.isNotNull = !allowNull;
            return std::move(*this);
        }

        // Equivale a unique=True en Django.
        Field&& unique(bool val = true) {
            def.isUnique = val;
            return std::move(*this);
        }

        // Equivale a default="..." con literal C-string evitando conversión ambigua con el literal entero 0.
        Field&& default_val(const char* literal) {
            def.hasDefault   = true;
            def.defaultValue = literal ? String(literal) : String("");
            return std::move(*this);
        }

        // Equivale a default=... con objeto String de Arduino.
        Field&& default_val(const String& literal) {
            def.hasDefault   = true;
            def.defaultValue = literal;
            return std::move(*this);
        }

        // Sobrecarga exacta para literales int estándar (ej: default_val(0), default_val(8443)).
        Field&& default_val(int numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(numericLiteral);
            return std::move(*this);
        }

        // Sobrecarga exacta para enteros de 32 bits en Xtensa (long / int32_t).
        Field&& default_val(long numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(numericLiteral);
            return std::move(*this);
        }

        // Sobrecarga exacta para enteros de 64 bits (long long / int64_t).
        Field&& default_val(long long numericLiteral) {
            def.hasDefault   = true;
            def.defaultValue = String(static_cast<long>(numericLiteral));
            return std::move(*this);
        }

        // Sobrecarga exacta para literales float (ej: default_val(0.0f), default_val(1013.25f)).
        Field&& default_val(float floatLiteral, uint8_t decimals = 2) {
            def.hasDefault   = true;
            def.defaultValue = String(floatLiteral, static_cast<unsigned int>(decimals));
            return std::move(*this);
        }

        // Sobrecarga exacta para literales double (ej: default_val(0.0)).
        Field&& default_val(double doubleLiteral, uint8_t decimals = 2) {
            def.hasDefault   = true;
            def.defaultValue = String(doubleLiteral, static_cast<unsigned int>(decimals));
            return std::move(*this);
        }

        // Agrega una restricción CHECK(expr) al campo actual al estilo CheckConstraint de Django.
        Field&& check(const String& sqlExpr, Standard::CheckEvaluatorFn fn = nullptr) {
            def.hasCheck        = true;
            def.checkExpression = sqlExpr;
            def.checkFn         = fn;
            return std::move(*this);
        }

        // Equivale a db_comment="..." en Django 4.2+ para documentar la columna en el catálogo PostgreSQL.
        Field&& db_comment(const String& comment) {
            def.comment = comment;
            return std::move(*this);
        }

        // Devuelve la estructura física de restricción de columna compilada.
        const Standard::ColumnConstraintDef& compiled() const { return def; }
    };

    // ---------------- Campos Numéricos y Autoincrementales (Django -> PostgreSQL) ----------------

    // Crea una clave primaria INTEGER GENERATED BY DEFAULT AS IDENTITY como AutoField en Django.
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

    // Crea una clave primaria BIGINT GENERATED BY DEFAULT AS IDENTITY como BigAutoField en Django.
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

    // Crea una columna SMALLINT (16 bits) en PostgreSQL.
    class SmallIntegerField : public Field {
    public:
        explicit SmallIntegerField(const String& colName) : Field(colName, Standard::PgDataType::SMALLINT) {}
    };

    // Crea una columna INTEGER (32 bits) en PostgreSQL.
    class IntegerField : public Field {
    public:
        explicit IntegerField(const String& colName) : Field(colName, Standard::PgDataType::INTEGER) {}
    };

    // Crea una columna BIGINT (64 bits) en PostgreSQL.
    class BigIntegerField : public Field {
    public:
        explicit BigIntegerField(const String& colName) : Field(colName, Standard::PgDataType::BIGINT) {}
    };

    // Crea una columna INTEGER con restricción automática CHECK (col >= 0) como PositiveIntegerField en Django.
    class PositiveIntegerField : public Field {
    public:
        explicit PositiveIntegerField(const String& colName) : Field(colName, Standard::PgDataType::INTEGER) {
            check(colName + " >= 0", +[](const String& v) -> bool { return v.toInt() >= 0; });
        }
    };

    // Crea una columna REAL / DOUBLE PRECISION en PostgreSQL.
    class FloatField : public Field {
    public:
        explicit FloatField(const String& colName, bool doublePrecision = false)
            : Field(colName, doublePrecision ? Standard::PgDataType::DOUBLE_PRECISION : Standard::PgDataType::REAL) {}
    };

    // Crea una columna NUMERIC(max_digits, decimal_places) exacta en PostgreSQL.
    class DecimalField : public Field {
    public:
        DecimalField(const String& colName, uint8_t maxDigits, uint8_t decimalPlaces)
            : Field(colName, Standard::PgDataType::NUMERIC) {
            def.precision = maxDigits;
            def.scale     = decimalPlaces;
        }
    };

    // ---------------- Campos de Texto, Booleanos, Tiempo y Estructurados ----------------

    // Crea una columna BOOLEAN en PostgreSQL.
    class BooleanField : public Field {
    public:
        explicit BooleanField(const String& colName) : Field(colName, Standard::PgDataType::BOOLEAN) {}
    };

    // Crea una columna CHARACTER VARYING(max_length) en PostgreSQL exigiendo max_length obligatorio como en Django.
    class CharField : public Field {
    public:
        CharField(const String& colName, uint16_t maxLength)
            : Field(colName, Standard::PgDataType::VARCHAR) {
            def.maxLength = maxLength;
        }
    };

    // Crea una columna TEXT ilimitada en PostgreSQL.
    class TextField : public Field {
    public:
        explicit TextField(const String& colName) : Field(colName, Standard::PgDataType::TEXT) {}
    };

    // Crea una columna TIMESTAMP WITHOUT/WITH TIME ZONE en PostgreSQL.
    class DateTimeField : public Field {
    public:
        explicit DateTimeField(const String& colName, bool useTz = false)
            : Field(colName, useTz ? Standard::PgDataType::TIMESTAMP_TZ : Standard::PgDataType::TIMESTAMP) {}
    };

    // Crea una columna JSON nativa de PostgreSQL.
    class JSONField : public Field {
    public:
        explicit JSONField(const String& colName) : Field(colName, Standard::PgDataType::JSON) {}
    };

    // Crea una columna tipada con un ENUM registrado en PostgreSQL.
    class EnumField : public Field {
    public:
        EnumField(const String& colName, const String& registeredEnumTypeName)
            : Field(colName, Standard::PgDataType::ENUM) {
            def.customTypeName = registeredEnumTypeName;
        }
    };

    // ---------------- Relaciones Relacionales (ForeignKey / OneToOneField) ----------------

    // Crea una clave foránea tipada que infiere o verifica el tipo de la PK padre en PostgreSQL.
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

} // namespace DbNotRuntime::Models