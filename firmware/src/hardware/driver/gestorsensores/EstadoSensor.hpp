#pragma once
#include <stdint.h>
#include <math.h>
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

// Límites físicos reales del silicio DS18B20 según datasheet
static constexpr float DS18B20_TEMP_MIN_FISICA_C = -55.0f;
static constexpr float DS18B20_TEMP_MAX_FISICA_C = 125.0f;

// Códigos centinela estrictamente fuera de [-55, +125] °C (alineados con convención DallasTemperature)
static constexpr float   DEVICE_DISCONNECTED_C       = -127.0f;
static constexpr float   DEVICE_INSUFFICIENT_POWER_C = -250.0f;
static constexpr float   DEVICE_POWER_ON_RESET_C     = -251.0f;
static constexpr float   DEVICE_FAULT_SHORTVDD_C     = -252.0f;
static constexpr float   DEVICE_FAULT_SHORTGND_C     = -253.0f;
static constexpr float   DEVICE_FAULT_OPEN_C         = -254.0f;

static constexpr int32_t DEVICE_DISCONNECTED_RAW       = -7040;
static constexpr int32_t DEVICE_INSUFFICIENT_POWER_RAW = -32000;
static constexpr int32_t DEVICE_POWER_ON_RESET_RAW     = -32128;
static constexpr int32_t DEVICE_FAULT_SHORTVDD_RAW     = -32256;
static constexpr int32_t DEVICE_FAULT_SHORTGND_RAW     = -32384;
static constexpr int32_t DEVICE_FAULT_OPEN_RAW         = -32512;

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

// true solo si la temperatura medida es real (el resto son fallos y su "temperatura" es un código centinela)
inline bool estadoTieneLecturaValida(EstadoSensor e) {
    return e == EstadoSensor::OK
        || e == EstadoSensor::ALARMA_SUPERIOR
        || e == EstadoSensor::ALARMA_INFERIOR;
}

// Valida que una lectura cruda esté dentro del rango físico real del DS18B20 (-55 °C a +125 °C)
inline bool esTemperaturaCrudaDS18B20Valida(float tempC) {
    return isfinite(tempC)
        && tempC >= DS18B20_TEMP_MIN_FISICA_C
        && tempC <= DS18B20_TEMP_MAX_FISICA_C;
}

// Todos los códigos de fallo quedan por debajo de -126 °C para jamás confundirse con temperaturas reales
inline float temperaturaCentinelaDeEstado(EstadoSensor e) {
    switch (e) {
        case EstadoSensor::DESCONECTADO:              return DEVICE_DISCONNECTED_C;
        case EstadoSensor::ALIMENTACION_INSUFICIENTE: return DEVICE_INSUFFICIENT_POWER_C;
        case EstadoSensor::REINICIO_ENERGIA_85C:      return DEVICE_POWER_ON_RESET_C;
        case EstadoSensor::CORTOCIRCUITO_VDD:         return DEVICE_FAULT_SHORTVDD_C;
        case EstadoSensor::CORTOCIRCUITO_GND:         return DEVICE_FAULT_SHORTGND_C;
        case EstadoSensor::CIRCUITO_ABIERTO:          return DEVICE_FAULT_OPEN_C;
        case EstadoSensor::SLOT_VACIO:                return -131.0f;
        case EstadoSensor::DIRECCION_ROM_INVALIDA:    return -132.0f;
        case EstadoSensor::FAMILIA_NO_COMPATIBLE:     return -133.0f;
        case EstadoSensor::SCRATCHPAD_VACIO_CEROS:    return -134.0f;
        case EstadoSensor::ERROR_CRC_SCRATCHPAD:      return -135.0f;
        case EstadoSensor::CONVERSION_PENDIENTE:      return -136.0f;
        case EstadoSensor::TIMEOUT_CONVERSION:        return -137.0f;
        case EstadoSensor::FALLO_ESCRITURA_EEPROM:    return -138.0f;
        default:                                      return DEVICE_DISCONNECTED_C;
    }
}