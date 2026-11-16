#pragma once
#include <stdint.h>
#include "../../../database/common/PgDtoTypes.hpp"

// FUENTE DE VERDAD: EstadoSensor y EstadoBus viven en PgDtoTypes.hpp (los usa la BD).
// Este archivo solo conserva lo que es propio del bus 1-Wire y no se persiste.

// Dirección ROM de 64 bits (8 bytes) estándar 1-Wire
using DeviceAddress = uint8_t[8];

// Códigos de familia 1-Wire (Maxim / Dallas)
static constexpr uint8_t DS18S20MODEL  = 0x10;
static constexpr uint8_t DS18B20MODEL  = 0x28;
static constexpr uint8_t DS1822MODEL   = 0x22;
static constexpr uint8_t DS1825MODEL   = 0x3B;
static constexpr uint8_t DS28EA00MODEL = 0x42;

// Constantes de diagnóstico y fallos eléctricos (en °C y en 1/128 °C raw)
static constexpr float   DEVICE_DISCONNECTED_C       = -127.0f;
static constexpr float   DEVICE_INSUFFICIENT_POWER_C =  127.9375f;
static constexpr float   DEVICE_POWER_ON_RESET_C     =   85.0f;
static constexpr float   DEVICE_FAULT_SHORTVDD_C     = -128.0f;
static constexpr float   DEVICE_FAULT_SHORTGND_C     = -129.0f;
static constexpr float   DEVICE_FAULT_OPEN_C         = -130.0f;

static constexpr int32_t DEVICE_DISCONNECTED_RAW       = -16256;
static constexpr int32_t DEVICE_INSUFFICIENT_POWER_RAW =  16376;
static constexpr int32_t DEVICE_POWER_ON_RESET_RAW     =  10880;
static constexpr int32_t DEVICE_FAULT_SHORTVDD_RAW     = -16384;
static constexpr int32_t DEVICE_FAULT_SHORTGND_RAW     = -16512;
static constexpr int32_t DEVICE_FAULT_OPEN_RAW         = -16640;

// Solo runtime (no se persiste): su información se refleja en AuditoriaSistema.detalle / estado_bus
enum class EventoGestor : uint8_t {
    NINGUNO = 0,
    BUS_INICIALIZADO,
    NUEVO_SENSOR_DETECTADO,
    SENSOR_DESCONECTADO_EN_CALIENTE,
    CONVERSION_COMPLETADA,
    ERROR_BUS_ELECTRICO,
    ALARMA_DETECTADA_EN_BUS
};

// true solo si la temperatura medida es real (el resto son fallos y su "temperatura" es un código)
inline bool estadoTieneLecturaValida(EstadoSensor e) {
    return e == EstadoSensor::OK
        || e == EstadoSensor::ALARMA_SUPERIOR
        || e == EstadoSensor::ALARMA_INFERIOR;
}

// Reemplaza el viejo static_cast<int16_t>(estado): conserva los códigos centinela
// que antes vivían en los valores numéricos del enum.
inline float temperaturaCentinelaDeEstado(EstadoSensor e) {
    switch (e) {
        case EstadoSensor::SLOT_VACIO:                return -1.0f;
        case EstadoSensor::DIRECCION_ROM_INVALIDA:    return -2.0f;
        case EstadoSensor::FAMILIA_NO_COMPATIBLE:     return -3.0f;
        case EstadoSensor::SCRATCHPAD_VACIO_CEROS:    return -4.0f;
        case EstadoSensor::ERROR_CRC_SCRATCHPAD:      return -5.0f;
        case EstadoSensor::CONVERSION_PENDIENTE:      return -6.0f;
        case EstadoSensor::TIMEOUT_CONVERSION:        return -7.0f;
        case EstadoSensor::FALLO_ESCRITURA_EEPROM:    return -8.0f;
        case EstadoSensor::DESCONECTADO:              return DEVICE_DISCONNECTED_C;
        case EstadoSensor::ALIMENTACION_INSUFICIENTE: return DEVICE_INSUFFICIENT_POWER_C;
        case EstadoSensor::REINICIO_ENERGIA_85C:      return DEVICE_POWER_ON_RESET_C;
        case EstadoSensor::CORTOCIRCUITO_VDD:         return DEVICE_FAULT_SHORTVDD_C;
        case EstadoSensor::CORTOCIRCUITO_GND:         return DEVICE_FAULT_SHORTGND_C;
        case EstadoSensor::CIRCUITO_ABIERTO:          return DEVICE_FAULT_OPEN_C;
        default:                                      return DEVICE_DISCONNECTED_C;
    }
}