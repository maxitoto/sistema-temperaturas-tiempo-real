#pragma once
#include <stdint.h>

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

enum class EstadoBus : uint8_t {
    OK = 0,
    LINEA_EN_CORTO_GND,
    SIN_PULSO_PRESENCIA,
    TOPOLOGIA_MODIFICADA,
    FALLO_CONVERSION_GLOBAL,
    LIMITE_SENSORES_EXCEDIDO
};

enum class EventoGestor : uint8_t {
    NINGUNO = 0,
    BUS_INICIALIZADO,
    NUEVO_SENSOR_DETECTADO,
    SENSOR_DESCONECTADO_EN_CALIENTE,
    CONVERSION_COMPLETADA,
    ERROR_BUS_ELECTRICO,
    ALARMA_DETECTADA_EN_BUS
};

enum class EstadoSensor : int16_t {
    OK                        = 0,
    ALARMA_SUPERIOR           = 1,
    ALARMA_INFERIOR           = 2,
    SLOT_VACIO                = -1,
    DIRECCION_ROM_INVALIDA    = -2,
    FAMILIA_NO_COMPATIBLE     = -3,
    SCRATCHPAD_VACIO_CEROS    = -4,
    ERROR_CRC_SCRATCHPAD      = -5,
    CONVERSION_PENDIENTE      = -6,
    TIMEOUT_CONVERSION        = -7,
    FALLO_ESCRITURA_EEPROM    = -8,
    DESCONECTADO              = -127,
    ALIMENTACION_INSUFICIENTE = 127,
    REINICIO_ENERGIA_85C      = 85,
    CORTOCIRCUITO_VDD         = -128,
    CORTOCIRCUITO_GND         = -129,
    CIRCUITO_ABIERTO          = -130
};