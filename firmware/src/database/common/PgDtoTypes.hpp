#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <type_traits>

namespace DbCommon::Dto {

    using PgInt8        = int8_t;
    using PgUInt8       = uint8_t;
    using PgBool        = bool;
    using PgSmallInt    = int16_t;
    using PgPosSmallInt = uint16_t;
    using PgInt         = int32_t;
    using PgAuto        = int32_t;
    using PgPosInt      = uint32_t;
    using PgEpoch       = uint32_t;
    using PgBigInt      = int64_t;
    using PgPosBigInt   = uint64_t;
    using PgFloat       = float;
    using PgDouble      = double;

    template <size_t MaxLen>
    struct PgChar {
        char data[MaxLen]{};

        constexpr PgChar() = default;

        PgChar(const char* s) { asignar(s); }
        PgChar(const String& s) { asignar(s.c_str()); }

        template <size_t OtherLen>
        PgChar(const PgChar<OtherLen>& other) { asignar(other.data); }

        PgChar& operator=(const char* s) {
            asignar(s);
            return *this;
        }

        PgChar& operator=(const String& s) {
            asignar(s.c_str());
            return *this;
        }

        template <size_t OtherLen>
        PgChar& operator=(const PgChar<OtherLen>& other) {
            asignar(other.data);
            return *this;
        }

        void asignar(const char* s) {
            memset(data, 0, MaxLen);
            if (s != nullptr && MaxLen > 0) {
                strncpy(data, s, MaxLen - 1);
            }
        }

        const char* c_str() const { return data; }
        bool vacio() const { return data[0] == '\0'; }

        operator const char*() const { return data; }
        operator String() const { return String(data); }
    };

    template <size_t MaxLen = 128>
    using PgText = PgChar<MaxLen>;

    using PgDateTime = PgChar<20>;
    using PgDate     = PgChar<12>;
    using PgTime     = PgChar<12>;

    struct PgMac {
        uint8_t data[8]{};

        constexpr PgMac() = default;

        explicit PgMac(const uint8_t* raw8) {
            if (raw8 != nullptr) {
                memcpy(data, raw8, 8);
            }
        }

        String toHexString() const {
            char hex[17]{};
            for (size_t i = 0; i < 8; ++i) {
                snprintf(hex + (i * 2), 3, "%02X", data[i]);
            }
            return String(hex);
        }
    };

    template <typename TItem, size_t MaxN = 32>
    struct PgList {
        static constexpr bool ES_PG_LIST = true;
        uint8_t cantidad = 0;
        TItem   items[MaxN]{};

        void limpiar() { cantidad = 0; }

        bool agregar(const TItem& item) {
            if (cantidad >= MaxN) return false;
            items[cantidad++] = item;
            return true;
        }

        size_t bytesActivos() const {
            const size_t n = (cantidad <= MaxN) ? static_cast<size_t>(cantidad) : MaxN;
            return offsetof(PgList, items) + (n * sizeof(TItem));
        }
    };

    template <typename TEnum>
    struct PgEnumTraits {
        static constexpr bool isRegistered = false;
    };

    template <typename TEnum>
    struct PgEnumPair {
        TEnum       value;
        const char* label;
    };

    enum class RolUsuario : uint8_t {
        NINGUNO  = 0,
        NO_ADMIN = 1,
        ADMIN    = 2
    };

    enum class CriticidadEvento : uint8_t {
        NULA  = 0,
        BAJA  = 1,
        MEDIA = 2,
        ALTA  = 3
    };

    enum class TipoEvento : uint8_t {
        INFORMATIVO = 0,
        ADVERTENCIA = 1,
        EXCEPCION   = 2
    };

    enum class TipoRecepcionCorreo : uint8_t {
        RECIBIR_TODOS            = 0,
        RECIBIR_EVENTOS          = 1,
        RECIBIR_EVENTOS_CRITICOS = 2
    };

    enum class EstadoEnvioCorreo : uint8_t {
        PENDIENTE = 0,
        ENVIADO   = 1,
        FALLIDO   = 2
    };

    enum class MedioConexion : uint8_t {
        INTERNO  = 0,
        UART_S   = 1,
        WIFI_STA = 2,
        WIFI_AP  = 3
    };

    enum class ResultadoAuditoria : uint8_t {
        EXITO               = 0,
        DENEGADO_ROL        = 1,
        CREDENCIAL_INVALIDA = 2,
        USUARIO_BLOQUEADO   = 3,
        INTRUSION_TERMINAL  = 4,
        ERROR_VALIDACION    = 5
    };

    enum class FuenteTiempo : uint8_t {
        SIN_SINCRONIZAR = 0,
        NTP             = 1,
        RTC             = 2,
        CLIENTE         = 3
    };

    enum class EstadoSensor : uint8_t {
        OK                        = 0,
        ALARMA_SUPERIOR           = 1,
        ALARMA_INFERIOR           = 2,
        SLOT_VACIO                = 3,
        DIRECCION_ROM_INVALIDA    = 4,
        FAMILIA_NO_COMPATIBLE     = 5,
        SCRATCHPAD_VACIO_CEROS    = 6,
        ERROR_CRC_SCRATCHPAD      = 7,
        CONVERSION_PENDIENTE      = 8,
        TIMEOUT_CONVERSION        = 9,
        FALLO_ESCRITURA_EEPROM    = 10,
        DESCONECTADO              = 11,
        ALIMENTACION_INSUFICIENTE = 12,
        REINICIO_ENERGIA_85C      = 13,
        CORTOCIRCUITO_VDD         = 14,
        CORTOCIRCUITO_GND         = 15,
        CIRCUITO_ABIERTO          = 16
    };

    enum class EstadoBus : uint8_t {
        OK                       = 0,
        LINEA_EN_CORTO_GND       = 1,
        SIN_PULSO_PRESENCIA      = 2,
        TOPOLOGIA_MODIFICADA     = 3,
        FALLO_CONVERSION_GLOBAL  = 4,
        LIMITE_SENSORES_EXCEDIDO = 5
    };

#define PG_REGISTER_ENUM(EnumType, SqlTypeName, DefaultVal, ...)                        \
    template <>                                                                         \
    struct PgEnumTraits<EnumType> {                                                     \
        static constexpr bool isRegistered = true;                                      \
        static constexpr const char* typeName() { return SqlTypeName; }                 \
        static constexpr PgEnumPair<EnumType> entries[] = { __VA_ARGS__ };              \
        static constexpr size_t count = sizeof(entries) / sizeof(entries[0]);           \
        static std::vector<String> labels() {                                           \
            std::vector<String> out;                                                    \
            out.reserve(count);                                                         \
            for (size_t i = 0; i < count; ++i) {                                        \
                out.push_back(String(entries[i].label));                                \
            }                                                                           \
            return out;                                                                 \
        }                                                                               \
        static const char* toString(EnumType v) {                                       \
            for (size_t i = 0; i < count; ++i) {                                        \
                if (entries[i].value == v) return entries[i].label;                     \
            }                                                                           \
            return entries[0].label;                                                    \
        }                                                                               \
        static EnumType fromString(const String& s, EnumType fallback = DefaultVal) {   \
            for (size_t i = 0; i < count; ++i) {                                        \
                if (s.equalsIgnoreCase(entries[i].label)) return entries[i].value;      \
            }                                                                           \
            return fallback;                                                            \
        }                                                                               \
    };

    PG_REGISTER_ENUM(
        RolUsuario,
        "enum_rol_usuario",
        RolUsuario::NINGUNO,
        { RolUsuario::ADMIN,    "ADMIN" },
        { RolUsuario::NO_ADMIN, "NO_ADMIN" },
        { RolUsuario::NINGUNO,  "NINGUNO" }
    )

    PG_REGISTER_ENUM(
        CriticidadEvento,
        "criticidad_evento_enum",
        CriticidadEvento::NULA,
        { CriticidadEvento::NULA,  "NULA" },
        { CriticidadEvento::BAJA,  "BAJA" },
        { CriticidadEvento::MEDIA, "MEDIA" },
        { CriticidadEvento::ALTA,  "ALTA" }
    )

    PG_REGISTER_ENUM(
        TipoEvento,
        "tipo_evento_enum",
        TipoEvento::INFORMATIVO,
        { TipoEvento::INFORMATIVO, "INFORMATIVO" },
        { TipoEvento::ADVERTENCIA, "ADVERTENCIA" },
        { TipoEvento::EXCEPCION,   "EXCEPCION" }
    )

    PG_REGISTER_ENUM(
        TipoRecepcionCorreo,
        "tipo_recepcion_correo_enum",
        TipoRecepcionCorreo::RECIBIR_TODOS,
        { TipoRecepcionCorreo::RECIBIR_TODOS,            "RECIBIR_TODOS" },
        { TipoRecepcionCorreo::RECIBIR_EVENTOS,          "RECIBIR_EVENTOS" },
        { TipoRecepcionCorreo::RECIBIR_EVENTOS_CRITICOS, "RECIBIR_EVENTOS_CRITICOS" }
    )

    PG_REGISTER_ENUM(
        EstadoEnvioCorreo,
        "estado_envio_correo_enum",
        EstadoEnvioCorreo::PENDIENTE,
        { EstadoEnvioCorreo::PENDIENTE, "PENDIENTE" },
        { EstadoEnvioCorreo::ENVIADO,   "ENVIADO" },
        { EstadoEnvioCorreo::FALLIDO,   "FALLIDO" }
    )

    PG_REGISTER_ENUM(
        MedioConexion,
        "medio_conexion_enum",
        MedioConexion::INTERNO,
        { MedioConexion::INTERNO,  "INTERNO" },
        { MedioConexion::UART_S,   "UART_S" },
        { MedioConexion::WIFI_STA, "WIFI_STA" },
        { MedioConexion::WIFI_AP,  "WIFI_AP" }
    )

    PG_REGISTER_ENUM(
        ResultadoAuditoria,
        "resultado_auditoria_enum",
        ResultadoAuditoria::EXITO,
        { ResultadoAuditoria::EXITO,               "EXITO" },
        { ResultadoAuditoria::DENEGADO_ROL,        "DENEGADO_ROL" },
        { ResultadoAuditoria::CREDENCIAL_INVALIDA, "CREDENCIAL_INVALIDA" },
        { ResultadoAuditoria::USUARIO_BLOQUEADO,   "USUARIO_BLOQUEADO" },
        { ResultadoAuditoria::INTRUSION_TERMINAL,  "INTRUSION_TERMINAL" },
        { ResultadoAuditoria::ERROR_VALIDACION,    "ERROR_VALIDACION" }
    )

    PG_REGISTER_ENUM(
        FuenteTiempo,
        "fuente_tiempo_enum",
        FuenteTiempo::SIN_SINCRONIZAR,
        { FuenteTiempo::SIN_SINCRONIZAR, "SIN_SINCRONIZAR" },
        { FuenteTiempo::NTP,             "NTP" },
        { FuenteTiempo::RTC,             "RTC" },
        { FuenteTiempo::CLIENTE,         "CLIENTE" }
    )

    PG_REGISTER_ENUM(
        EstadoSensor,
        "estado_sensor_enum",
        EstadoSensor::OK,
        { EstadoSensor::OK,                        "OK" },
        { EstadoSensor::ALARMA_SUPERIOR,           "ALARMA_SUPERIOR" },
        { EstadoSensor::ALARMA_INFERIOR,           "ALARMA_INFERIOR" },
        { EstadoSensor::SLOT_VACIO,                "SLOT_VACIO" },
        { EstadoSensor::DIRECCION_ROM_INVALIDA,    "DIRECCION_ROM_INVALIDA" },
        { EstadoSensor::FAMILIA_NO_COMPATIBLE,     "FAMILIA_NO_COMPATIBLE" },
        { EstadoSensor::SCRATCHPAD_VACIO_CEROS,    "SCRATCHPAD_VACIO_CEROS" },
        { EstadoSensor::ERROR_CRC_SCRATCHPAD,      "ERROR_CRC_SCRATCHPAD" },
        { EstadoSensor::CONVERSION_PENDIENTE,      "CONVERSION_PENDIENTE" },
        { EstadoSensor::TIMEOUT_CONVERSION,        "TIMEOUT_CONVERSION" },
        { EstadoSensor::FALLO_ESCRITURA_EEPROM,    "FALLO_ESCRITURA_EEPROM" },
        { EstadoSensor::DESCONECTADO,              "DESCONECTADO" },
        { EstadoSensor::ALIMENTACION_INSUFICIENTE, "ALIMENTACION_INSUFICIENTE" },
        { EstadoSensor::REINICIO_ENERGIA_85C,      "REINICIO_ENERGIA_85C" },
        { EstadoSensor::CORTOCIRCUITO_VDD,         "CORTOCIRCUITO_VDD" },
        { EstadoSensor::CORTOCIRCUITO_GND,         "CORTOCIRCUITO_GND" },
        { EstadoSensor::CIRCUITO_ABIERTO,          "CIRCUITO_ABIERTO" }
    )

    PG_REGISTER_ENUM(
        EstadoBus,
        "estado_bus_enum",
        EstadoBus::OK,
        { EstadoBus::OK,                       "OK" },
        { EstadoBus::LINEA_EN_CORTO_GND,       "LINEA_EN_CORTO_GND" },
        { EstadoBus::SIN_PULSO_PRESENCIA,      "SIN_PULSO_PRESENCIA" },
        { EstadoBus::TOPOLOGIA_MODIFICADA,     "TOPOLOGIA_MODIFICADA" },
        { EstadoBus::FALLO_CONVERSION_GLOBAL,  "FALLO_CONVERSION_GLOBAL" },
        { EstadoBus::LIMITE_SENSORES_EXCEDIDO, "LIMITE_SENSORES_EXCEDIDO" }
    )

}

using DbCommon::Dto::PgInt8;
using DbCommon::Dto::PgUInt8;
using DbCommon::Dto::PgBool;
using DbCommon::Dto::PgSmallInt;
using DbCommon::Dto::PgPosSmallInt;
using DbCommon::Dto::PgInt;
using DbCommon::Dto::PgAuto;
using DbCommon::Dto::PgPosInt;
using DbCommon::Dto::PgEpoch;
using DbCommon::Dto::PgBigInt;
using DbCommon::Dto::PgPosBigInt;
using DbCommon::Dto::PgFloat;
using DbCommon::Dto::PgDouble;
using DbCommon::Dto::PgChar;
using DbCommon::Dto::PgText;
using DbCommon::Dto::PgDateTime;
using DbCommon::Dto::PgDate;
using DbCommon::Dto::PgTime;
using DbCommon::Dto::PgMac;
using DbCommon::Dto::PgList;
using DbCommon::Dto::PgEnumTraits;

using DbCommon::Dto::RolUsuario;
using DbCommon::Dto::CriticidadEvento;
using DbCommon::Dto::TipoEvento;
using DbCommon::Dto::TipoRecepcionCorreo;
using DbCommon::Dto::EstadoEnvioCorreo;
using DbCommon::Dto::MedioConexion;
using DbCommon::Dto::ResultadoAuditoria;
using DbCommon::Dto::FuenteTiempo;
using DbCommon::Dto::EstadoSensor;
using DbCommon::Dto::EstadoBus;