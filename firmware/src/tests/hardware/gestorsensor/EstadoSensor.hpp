// Archivo: src/tests/hardware/gestorsensor/EstadosSensor.hpp
#pragma once
#include <stdint.h>
#include <DallasTemperature.h>

enum class EstadoBus : uint8_t {
    OK = 0,
    LINEA_EN_CORTO_GND,       // El GPIO permanece en LOW pese al pull-up
    SIN_PULSO_PRESENCIA,      // bus.reset() no detecta ningún esclavo 1-Wire
    TOPOLOGIA_MODIFICADA      // verifyDeviceCount() detectó desconexión/conexión en caliente
};

enum class EstadoSensor : int16_t {
    // --- Operación normal y alarmas ---
    OK                        = 0,
    ALARMA_SUPERIOR           = 1,    // TH superado en hardware
    ALARMA_INFERIOR           = 2,    // TL superado en hardware

    // --- Diagnósticos de protocolo y configuración ---
    SLOT_VACIO                = -1,   // Ranura (0..5) sin sensor asignado
    DIRECCION_ROM_INVALIDA    = -2,   // Falló validAddress() o CRC8 de ROM
    FAMILIA_NO_COMPATIBLE     = -3,   // No pertenece a familias DS18x20 reconocidas
    SCRATCHPAD_VACIO_CEROS    = -4,   // Los 9 bytes del scratchpad son 0x00
    ERROR_CRC_SCRATCHPAD      = -5,   // El byte 8 no coincide con OneWire::crc8()
    CONVERSION_PENDIENTE      = -6,   // El sensor sigue convirtiendo
    TIMEOUT_CONVERSION        = -7,   // Superó millisToWaitForConversion() (750 ms máx.)
    FALLO_ESCRITURA_EEPROM    = -8,   // Falló setResolution() o saveScratchPad()

    // --- Códigos de error nativos de DallasTemperature 4.0.5+ ---
    DESCONECTADO              = DEVICE_DISCONNECTED_C,       // -127 °C (RAW: -7040)
    ALIMENTACION_INSUFICIENTE = DEVICE_INSUFFICIENT_POWER_C, // -250 °C (RAW: -32000 / 127.94 °C)
    REINICIO_ENERGIA_85C      = DEVICE_POWER_ON_RESET_C,     // -251 °C (RAW: -32128 / 85 °C + 0x0C)
    CORTOCIRCUITO_VDD         = DEVICE_FAULT_SHORTVDD_C,     // -252 °C (RAW: -32256)
    CORTOCIRCUITO_GND         = DEVICE_FAULT_SHORTGND_C,     // -253 °C (RAW: -32384)
    CIRCUITO_ABIERTO          = DEVICE_FAULT_OPEN_C          // -254 °C (RAW: -32512)
};