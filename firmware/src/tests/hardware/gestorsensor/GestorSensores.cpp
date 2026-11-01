// Archivo: src/tests/hardware/gestorsensor/GestorSensores.cpp
#include "GestorSensores.hpp"
#include <string.h>
#include <stdio.h>

GestorSensores::GestorSensores()
    : bus(Pin::PIN_SENSOR_1WIRE), driver(&bus) {}

void GestorSensores::formatearRomHex(const DeviceAddress rom, char* salida17B) {
    for (uint8_t i = 0; i < 8; i++) {
        snprintf(salida17B + (i * 2), 3, "%02X", rom[i]);
    }
    salida17B[16] = '\0';
}

EstadoBus GestorSensores::verificarEstadoElectricoBus() {
    pinMode(Pin::PIN_SENSOR_1WIRE, INPUT_PULLUP);
    delay(2);
    if (digitalRead(Pin::PIN_SENSOR_1WIRE) == LOW) {
        return EstadoBus::LINEA_EN_CORTO_GND;
    }
    if (bus.reset() == 0) {
        return EstadoBus::SIN_PULSO_PRESENCIA;
    }
    return EstadoBus::OK;
}

uint8_t GestorSensores::escanearYAsignarSensores(uint8_t resolucionGlobal, bool guardarEnEeprom) {
    driver.begin();
    driver.setWaitForConversion(true);
    driver.setCheckForConversion(true);
    driver.setAutoSaveScratchPad(guardarEnEeprom);

    totalDetectadosBus = driver.getDeviceCount();
    totalAsignados = 0;

    for (uint8_t s = 0; s < MAX_SENSORES; s++) {
        slots[s] = DiagnosticoSlot{};
        slots[s].indiceSlot = s;
    }

    uint8_t tope = (totalDetectadosBus > MAX_SENSORES) ? MAX_SENSORES : totalDetectadosBus;

    for (uint8_t i = 0; i < tope; i++) {
        DiagnosticoSlot& slot = slots[i];
        if (!driver.getAddress(slot.rom, i)) {
            slot.estado = EstadoSensor::DESCONECTADO;
            continue;
        }

        formatearRomHex(slot.rom, slot.romHex);

        if (!driver.validAddress(slot.rom) || OneWire::crc8(slot.rom, 7) != slot.rom[7]) {
            slot.estado = EstadoSensor::DIRECCION_ROM_INVALIDA;
            continue;
        }

        if (!driver.validFamily(slot.rom)) {
            slot.estado = EstadoSensor::FAMILIA_NO_COMPATIBLE;
            continue;
        }

        slot.activo = true;
        driver.setResolution(slot.rom, resolucionGlobal, true);
        slot.resolucionBits       = driver.getResolution(slot.rom);
        slot.alimentacionParasita = !driver.readPowerSupply(slot.rom);
        slot.alarmaAltaC          = driver.getHighAlarmTemp(slot.rom);
        slot.alarmaBajaC          = driver.getLowAlarmTemp(slot.rom);
        slot.datoUsuario          = driver.getUserData(slot.rom);
        slot.estado               = EstadoSensor::OK;
        totalAsignados++;
    }

    return totalAsignados;
}

bool GestorSensores::verificarCambiosEnCaliente() {
    bool mismoConteo = driver.verifyDeviceCount();
    totalDetectadosBus = driver.getDeviceCount();
    return !mismoConteo;
}

bool GestorSensores::solicitarConversionGlobal(bool bloqueante) {
    driver.setWaitForConversion(bloqueante);
    DallasTemperature::request_t req = driver.requestTemperatures();
    tsUltimaConversion = req.timestamp;
    return req.result;
}

bool GestorSensores::solicitarConversionPorSlot(uint8_t slot, bool bloqueante) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    driver.setWaitForConversion(bloqueante);
    DallasTemperature::request_t req = driver.requestTemperaturesByAddress(slots[slot].rom);
    tsUltimaConversion = req.timestamp;
    return req.result;
}

bool GestorSensores::estaConversionCompleta() {
    return driver.isConversionComplete();
}

uint16_t GestorSensores::tiempoConversionSegunResolucion(uint8_t bits) const {
    return DallasTemperature::millisToWaitForConversion(bits);
}

EstadoSensor GestorSensores::traducirCodigoRaw(int32_t raw, const uint8_t* pad) const {
    switch (raw) {
        case DEVICE_DISCONNECTED_RAW:       return EstadoSensor::DESCONECTADO;
        case DEVICE_INSUFFICIENT_POWER_RAW: return EstadoSensor::ALIMENTACION_INSUFICIENTE;
        case DEVICE_POWER_ON_RESET_RAW:     return EstadoSensor::REINICIO_ENERGIA_85C;
        case DEVICE_FAULT_OPEN_RAW:         return EstadoSensor::CIRCUITO_ABIERTO;
        case DEVICE_FAULT_SHORTGND_RAW:     return EstadoSensor::CORTOCIRCUITO_GND;
        case DEVICE_FAULT_SHORTVDD_RAW:     return EstadoSensor::CORTOCIRCUITO_VDD;
        default: break;
    }

    // Verificación explícita de glitch 85.00 °C (0x0550 en los primeros 2 bytes y 0x0C en byte 6)
    if (pad != nullptr) {
        int16_t raw16 = static_cast<int16_t>((static_cast<uint16_t>(pad[1]) << 8) | pad[0]);
        if (raw16 == 0x0550 && pad[6] == 0x0C) {
            return EstadoSensor::REINICIO_ENERGIA_85C;
        }
        if (raw16 == 0x07FF) {
            return EstadoSensor::ALIMENTACION_INSUFICIENTE;
        }
    }

    return EstadoSensor::OK;
}

EstadoSensor GestorSensores::actualizarLecturaSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) {
        return EstadoSensor::SLOT_VACIO;
    }

    DiagnosticoSlot& s = slots[slot];

    // 1. Leer y validar el Scratchpad de 9 bytes con reintentos
    bool padLeido = false;
    for (uint8_t intento = 0; intento < REINTENTOS_LECTURA; intento++) {
        if (driver.readScratchPad(s.rom, s.ultimoScratchpad)) {
            padLeido = true;
            break;
        }
    }

    if (!padLeido) {
        s.tempRaw     = DEVICE_DISCONNECTED_RAW;
        s.tempCelsius = DEVICE_DISCONNECTED_C;
        s.estado      = EstadoSensor::DESCONECTADO;
        return s.estado;
    }

    // 2. Comprobar que no sean todos ceros
    bool todoCeros = true;
    for (uint8_t b = 0; b < 9; b++) {
        if (s.ultimoScratchpad[b] != 0) { todoCeros = false; break; }
    }
    if (todoCeros) {
        s.estado = EstadoSensor::SCRATCHPAD_VACIO_CEROS;
        return s.estado;
    }

    // 3. Verificar CRC8 del Scratchpad
    if (OneWire::crc8(s.ultimoScratchpad, 8) != s.ultimoScratchpad[8]) {
        s.estado = EstadoSensor::ERROR_CRC_SCRATCHPAD;
        return s.estado;
    }

    // 4. Obtener valor RAW de 32 bits desde DallasTemperature
    s.tempRaw = driver.getTemp(s.rom, REINTENTOS_LECTURA);
    EstadoSensor estadoEvaluado = traducirCodigoRaw(s.tempRaw, s.ultimoScratchpad);
    if (estadoEvaluado != EstadoSensor::OK) {
        s.tempCelsius = static_cast<float>(static_cast<int16_t>(estadoEvaluado));
        s.estado      = estadoEvaluado;
        return s.estado;
    }

    s.tempCelsius = DallasTemperature::rawToCelsius(s.tempRaw);

    // 5. Evaluar alarmas de umbral en el chip (TH / TL)
    s.alarmaAltaC = static_cast<int8_t>(s.ultimoScratchpad[2]);
    s.alarmaBajaC = static_cast<int8_t>(s.ultimoScratchpad[3]);
    s.enAlarma    = driver.hasAlarm(s.rom);

    if (s.tempCelsius >= static_cast<float>(s.alarmaAltaC)) {
        s.estado = EstadoSensor::ALARMA_SUPERIOR;
    } else if (s.tempCelsius <= static_cast<float>(s.alarmaBajaC)) {
        s.estado = EstadoSensor::ALARMA_INFERIOR;
    } else {
        s.estado = EstadoSensor::OK;
    }

    return s.estado;
}

void GestorSensores::actualizarTodosLosSlots() {
    solicitarConversionGlobal(true);
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (slots[i].activo) {
            actualizarLecturaSlot(i);
        }
    }
}

bool GestorSensores::configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    if (!driver.setResolution(slots[slot].rom, bits)) {
        slots[slot].estado = EstadoSensor::FALLO_ESCRITURA_EEPROM;
        return false;
    }
    slots[slot].resolucionBits = driver.getResolution(slots[slot].rom);
    return guardarEeprom ? driver.saveScratchPad(slots[slot].rom) : true;
}

bool GestorSensores::configurarOffsetPorROM(const uint8_t* rom, float offsetC) {
    if (rom == nullptr) {
        return false;
    }

    return driver.setOffsetByDeviceAddress(rom, offsetC);
}

bool GestorSensores::configurarOffsetSlot(
    uint8_t slot,
    float offsetC
) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) {
        return false;
    }

    return driver.setOffsetByDeviceAddress(
        slots[slot].rom,
        offsetC
    );
}

bool GestorSensores::configurarAlarmasSlot(uint8_t slot, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    driver.setHighAlarmTemp(slots[slot].rom, thAltoC);
    driver.setLowAlarmTemp(slots[slot].rom, tlBajoC);
    slots[slot].alarmaAltaC = driver.getHighAlarmTemp(slots[slot].rom);
    slots[slot].alarmaBajaC = driver.getLowAlarmTemp(slots[slot].rom);
    return guardarEeprom ? driver.saveScratchPad(slots[slot].rom) : true;
}

bool GestorSensores::guardarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    return driver.saveScratchPad(slots[slot].rom);
}

bool GestorSensores::restaurarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    return driver.recallScratchPad(slots[slot].rom);
}

bool GestorSensores::escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom) {
    if (slot >= MAX_SENSORES || !slots[slot].activo) return false;
    driver.setUserData(slots[slot].rom, idCustom);
    slots[slot].datoUsuario = driver.getUserData(slots[slot].rom);
    return slots[slot].datoUsuario == idCustom;
}

uint8_t GestorSensores::procesarBusquedaAlarmasHardware() {
    uint8_t contador = 0;

    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (!slots[i].activo) {
            continue;
        }

        slots[i].enAlarma = driver.hasAlarm(slots[i].rom);

        if (slots[i].enAlarma) {
            contador++;
        }
    }

    return contador;
}

Sensor& GestorSensores::obtenerSensor(uint8_t slot) {
    return sensores[(slot < MAX_SENSORES) ? slot : 0];
}

const DiagnosticoSlot& GestorSensores::obtenerDiagnostico(uint8_t slot) const {
    return slots[(slot < MAX_SENSORES) ? slot : 0];
}



bool GestorSensores::hayModoParasitoGlobal() {
    return driver.isParasitePowerMode();
}

const char* GestorSensores::nombreFamilia(uint8_t codigoFamilia) {
    switch (codigoFamilia) {
        case DS18S20MODEL:  return "DS18S20";
        case DS18B20MODEL:  return "DS18B20";
        case DS1822MODEL:   return "DS1822";
        case DS1825MODEL:   return "DS1825";
        case DS28EA00MODEL: return "DS28EA00";
        default:            return "DESCONOCIDO";
    }
}

const char* GestorSensores::nombreEstado(EstadoSensor estado) {
    switch (estado) {
        case EstadoSensor::OK:                        return "OK";
        case EstadoSensor::ALARMA_SUPERIOR:           return "ALARMA_ALTA";
        case EstadoSensor::ALARMA_INFERIOR:           return "ALARMA_BAJA";
        case EstadoSensor::SLOT_VACIO:                return "VACIO";
        case EstadoSensor::DIRECCION_ROM_INVALIDA:    return "ROM_CRC_ERR";
        case EstadoSensor::FAMILIA_NO_COMPATIBLE:     return "FAMILIA_ERR";
        case EstadoSensor::SCRATCHPAD_VACIO_CEROS:    return "PAD_CEROS";
        case EstadoSensor::ERROR_CRC_SCRATCHPAD:      return "PAD_CRC_ERR";
        case EstadoSensor::CONVERSION_PENDIENTE:      return "CONV_PEND";
        case EstadoSensor::TIMEOUT_CONVERSION:        return "CONV_TIMEOUT";
        case EstadoSensor::FALLO_ESCRITURA_EEPROM:    return "EEPROM_ERR";
        case EstadoSensor::DESCONECTADO:              return "DESCONECTADO";
        case EstadoSensor::ALIMENTACION_INSUFICIENTE: return "PWR_INSUF_127C";
        case EstadoSensor::REINICIO_ENERGIA_85C:      return "POR_GLITCH_85C";
        case EstadoSensor::CORTOCIRCUITO_VDD:         return "CORTO_VDD";
        case EstadoSensor::CORTOCIRCUITO_GND:         return "CORTO_GND";
        case EstadoSensor::CIRCUITO_ABIERTO:          return "CIRC_ABIERTO";
        default:                                      return "ERROR_DESC";
    }
}