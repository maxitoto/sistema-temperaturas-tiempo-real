#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <type_traits>

namespace DbCommon::Dto {

    template <size_t N>
    struct PgText {
        char data[N]{};

        PgText() = default;
        PgText(const char* str) { asignar(str); }
        PgText(const String& str) { asignar(str.c_str()); }

        PgText& operator=(const char* str) { asignar(str); return *this; }
        PgText& operator=(const String& str) { asignar(str.c_str()); return *this; }

        void asignar(const char* str) {
            memset(data, 0, N);
            if (!str) return;
            strncpy(data, str, N - 1);
            data[N - 1] = '\0';
        }

        operator const char*() const { return data; }
        operator char*()             { return data; }
        const char* c_str() const    { return data; }
        bool empty() const           { return data[0] == '\0'; }
        char operator[](size_t i) const { return data[i]; }
        char& operator[](size_t i)      { return data[i]; }
    };

    template <size_t N>
    struct PgBytes {
        uint8_t data[N]{};

        PgBytes() = default;
        PgBytes(const uint8_t* src) {
            if (src) memcpy(data, src, N);
        }

        operator const uint8_t*() const { return data; }
        operator uint8_t*()             { return data; }
        uint8_t operator[](size_t i) const { return data[i]; }
        uint8_t& operator[](size_t i)      { return data[i]; }
    };

    template <typename TItem, size_t MaxN = 32>
    struct PgList {
        uint8_t cantidad = 0;
        TItem   items[MaxN]{};

        void limpiar() { cantidad = 0; }

        bool agregar(const TItem& item) {
            if (cantidad >= MaxN) return false;
            items[cantidad++] = item;
            return true;
        }
    };

    // --- Descriptor de Enums C++ <-> PostgreSQL (1 sola fuente de verdad) ---
    struct PgEnumMapping {
        uint8_t     value;
        const char* label;
    };

    template <typename TEnum>
    struct PgEnumTraits {
        static constexpr bool isRegistered = false;
        static const char* typeName() { return ""; }
        static const char* toString(TEnum) { return ""; }
        static TEnum fromString(const char*, TEnum fallback = static_cast<TEnum>(0)) { return fallback; }
        static std::vector<String> labels() { return {}; }
    };

    #define PG_REGISTER_ENUM(EnumType, SqlTypeName, ...)                                          \
        template <>                                                                               \
        struct PgEnumTraits<EnumType> {                                                           \
            static constexpr bool isRegistered = true;                                            \
            static constexpr const char* typeName() { return SqlTypeName; }                       \
            static constexpr PgEnumMapping table[] = { __VA_ARGS__ };                             \
            static constexpr size_t count = sizeof(table) / sizeof(PgEnumMapping);                \
            static const char* toString(EnumType v) {                                             \
                for (size_t i = 0; i < count; ++i) {                                              \
                    if (table[i].value == static_cast<uint8_t>(v)) return table[i].label;         \
                }                                                                                 \
                return (count > 0) ? table[0].label : "";                                         \
            }                                                                                     \
            static EnumType fromString(const char* str, EnumType fallback = static_cast<EnumType>(0)) { \
                if (!str) return fallback;                                                        \
                for (size_t i = 0; i < count; ++i) {                                              \
                    if (strcasecmp(table[i].label, str) == 0) return static_cast<EnumType>(table[i].value); \
                }                                                                                 \
                return fallback;                                                                  \
            }                                                                                     \
            static std::vector<String> labels() {                                                 \
                std::vector<String> out;                                                          \
                out.reserve(count);                                                               \
                for (size_t i = 0; i < count; ++i) out.push_back(String(table[i].label));         \
                return out;                                                                       \
            }                                                                                     \
        };

    // =========================================================================
    // ENUMS COMPARTIDOS EN TODO EL SISTEMA (DDL + DQL/DML + REPOS + RPC)
    // =========================================================================

    enum class RolUsuario : uint8_t {
        NINGUNO  = 0,
        NO_ADMIN = 1,
        ADMIN    = 2
    };
    PG_REGISTER_ENUM(RolUsuario, "enum_rol_usuario",
        { static_cast<uint8_t>(RolUsuario::ADMIN),    "ADMIN"    },
        { static_cast<uint8_t>(RolUsuario::NO_ADMIN), "NO_ADMIN" },
        { static_cast<uint8_t>(RolUsuario::NINGUNO),  "NINGUNO"  }
    )

    enum class CriticidadEvento : uint8_t {
        BAJA    = 0,
        MEDIA   = 1,
        CRITICA = 2
    };
    PG_REGISTER_ENUM(CriticidadEvento, "criticidad_evento_enum",
        { static_cast<uint8_t>(CriticidadEvento::BAJA),    "BAJA"    },
        { static_cast<uint8_t>(CriticidadEvento::MEDIA),   "MEDIA"   },
        { static_cast<uint8_t>(CriticidadEvento::CRITICA), "CRITICA" }
    )

    // =========================================================================
    // TABLA 1:1 ENTRE DDL (DjangoFields.hpp) Y STRUCTS DE REPOSITORIO / RPC
    // =========================================================================
    using PgSmallAuto    = int16_t;         // SmallAutoField
    using PgAuto         = int32_t;         // AutoField
    using PgBigAuto      = int64_t;         // BigAutoField

    using PgSmallInt     = int16_t;         // SmallIntegerField
    using PgPosSmallInt  = uint16_t;        // PositiveSmallIntegerField
    using PgInt          = int32_t;         // IntegerField
    using PgPosInt       = uint32_t;        // PositiveIntegerField
    using PgBigInt       = int64_t;         // BigIntegerField
    using PgPosBigInt    = uint64_t;        // PositiveBigIntegerField

    using PgFloat        = float;           // FloatField
    using PgDouble       = double;          // DoublePrecisionField / DecimalField

    using PgBool         = bool;            // BooleanField (1 byte en C++ Xtensa)
    static_assert(sizeof(PgBool) == 1, "PgBool debe medir 1 byte");

    template <size_t N>
    using PgChar         = PgText<N>;       // CharField("col", N) / TextField("col", N)

    using PgDateTime     = PgText<20>;      // DateTimeField ("YYYY-MM-DD HH:MM:SS")
    using PgDate         = PgText<12>;      // DateField ("YYYY-MM-DD")
    using PgTime         = PgText<12>;      // TimeField ("HH:MM:SS")
    using PgEpoch        = uint32_t;        // Segundos Unix / millis 32-bit
    using PgMac          = PgBytes<8>;      // Dirección ROM / MAC 8 bytes

} // namespace DbCommon::Dto

using namespace DbCommon::Dto;