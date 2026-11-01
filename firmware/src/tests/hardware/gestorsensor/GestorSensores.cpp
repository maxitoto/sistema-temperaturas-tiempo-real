#include "GestorSensores.hpp"
#include <string.h>
#include <stdio.h>

GestorSensores::GestorSensores()
    : bus(Pin::PIN_SENSOR_1WIRE), driver(&bus) {
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        sensores[i].limpiar(i);
    }
}

int8_t GestorSensores::buscarSlotPorROM(const DeviceAddress rom) const {
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].coincideROM(rom)) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

EstadoBus GestorSensores::verificarEstadoElectricoBus() {
    pinMode(Pin::PIN_SENSOR_1WIRE, INPUT_PULLUP);
    delay(2);
    if (digitalRead(Pin::PIN_SENSOR_1WIRE) == LOW) {
        estadoBus    = EstadoBus::LINEA_EN_CORTO_GND;
        ultimoEvento = EventoGestor::ERROR_BUS_ELECTRICO;
        return estadoBus;
    }
    if (bus.reset() == 0) {
        estadoBus    = EstadoBus::SIN_PULSO_PRESENCIA;
        ultimoEvento = EventoGestor::ERROR_BUS_ELECTRICO;
        return estadoBus;
    }
    estadoBus = EstadoBus::OK;
    return estadoBus;
}

uint8_t GestorSensores::escanearYAsignarSensores(uint8_t resolucionGlobal, bool guardarEnEeprom) {
    driver.begin();
    driver.setWaitForConversion(true);
    driver.setCheckForConversion(false);
    driver.setAutoSaveScratchPad(guardarEnEeprom);

    totalDetectadosBus = driver.getDeviceCount();
    if (totalDetectadosBus > MAX_SENSORES) {
        estadoBus = EstadoBus::LIMITE_SENSORES_EXCEDIDO;
    } else {
        estadoBus = EstadoBus::OK;
    }

    Sensor anteriores[MAX_SENSORES];
    for (uint8_t s = 0; s < MAX_SENSORES; s++) {
        anteriores[s] = sensores[s];
        sensores[s].limpiar(s);
    }

    totalAsignados = 0;
    uint8_t tope = (totalDetectadosBus > MAX_SENSORES) ? MAX_SENSORES : totalDetectadosBus;

    for (uint8_t i = 0; i < tope; i++) {
        Sensor& s = sensores[i];
        DeviceAddress romDetectada = {0};

        if (!driver.getAddress(romDetectada, i)) {
            s.setEstado(EstadoSensor::DESCONECTADO);
            continue;
        }

        if (!driver.validAddress(romDetectada) || OneWire::crc8(romDetectada, 7) != romDetectada[7]) {
            s.setEstado(EstadoSensor::DIRECCION_ROM_INVALIDA);
            continue;
        }

        if (!driver.validFamily(romDetectada)) {
            s.setEstado(EstadoSensor::FAMILIA_NO_COMPATIBLE);
            continue;
        }

        bool yaExistia = false;
        for (uint8_t k = 0; k < MAX_SENSORES; k++) {
            if (anteriores[k].coincideROM(romDetectada)) {
                s = anteriores[k];
                s.setIndiceSlot(i);
                yaExistia = true;
                break;
            }
        }

        if (!yaExistia) {
            s.inicializarNuevo(i, romDetectada);
            driver.setHighAlarmTemp(romDetectada, Sensor::UMBRAL_MAX_DEFAULT);
            driver.setLowAlarmTemp(romDetectada, Sensor::UMBRAL_MIN_DEFAULT);
            driver.setOffsetByDeviceAddress(romDetectada, Sensor::OFFSET_DEFAULT);
            driver.setResolution(romDetectada, resolucionGlobal, false);
            if (guardarEnEeprom) {
                driver.saveScratchPad(romDetectada);
            }
            ultimoEvento = EventoGestor::NUEVO_SENSOR_DETECTADO;
        }

        s.setResolucionBits(driver.getResolution(romDetectada));
        s.setAlimentacionParasita(!driver.readPowerSupply(romDetectada));
        s.setDatoUsuario(driver.getUserData(romDetectada));
        s.setEstado(EstadoSensor::OK);
        totalAsignados++;
    }

    if (totalAsignados > 0) {
        driver.requestTemperatures();
    }

    if (ultimoEvento == EventoGestor::NINGUNO) {
        ultimoEvento = EventoGestor::BUS_INICIALIZADO;
    }

    return totalAsignados;
}

bool GestorSensores::verificarCambiosEnCaliente() {
    bool mismoConteo = driver.verifyDeviceCount();
    uint8_t nuevoTotal = driver.getDeviceCount();
    if (!mismoConteo || nuevoTotal != totalDetectadosBus) {
        estadoBus = EstadoBus::TOPOLOGIA_MODIFICADA;
        ultimoEvento = (nuevoTotal < totalDetectadosBus)
            ? EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE
            : EventoGestor::NUEVO_SENSOR_DETECTADO;
        totalDetectadosBus = nuevoTotal;
        return true;
    }
    return false;
}

bool GestorSensores::solicitarConversionGlobal(bool bloqueante) {
    driver.setWaitForConversion(bloqueante);
    DallasTemperature::request_t req = driver.requestTemperatures();
    tsUltimaConversion = req.timestamp;
    if (!req.result) {
        estadoBus = EstadoBus::FALLO_CONVERSION_GLOBAL;
        return false;
    }
    ultimoEvento = EventoGestor::CONVERSION_COMPLETADA;
    return true;
}

bool GestorSensores::solicitarConversionPorSlot(uint8_t slot, bool bloqueante) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    driver.setWaitForConversion(bloqueante);
    DallasTemperature::request_t req = driver.requestTemperaturesByAddress(sensores[slot].obtenerROM());
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
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) {
        return EstadoSensor::SLOT_VACIO;
    }

    Sensor& s = sensores[slot];
    uint8_t* pad = s.obtenerScratchpad();

    bool padLeido = false;
    for (uint8_t intento = 0; intento < REINTENTOS_LECTURA; intento++) {
        if (driver.readScratchPad(s.obtenerROM(), pad)) {
            padLeido = true;
            break;
        }
    }

    if (!padLeido) {
        s.setTempRaw(DEVICE_DISCONNECTED_RAW);
        s.setTempCelsius(DEVICE_DISCONNECTED_C);
        s.setEstado(EstadoSensor::DESCONECTADO);
        return s.obtenerEstado();
    }

    bool todoCeros = true;
    for (uint8_t b = 0; b < 9; b++) {
        if (pad[b] != 0) { todoCeros = false; break; }
    }
    if (todoCeros) {
        s.setEstado(EstadoSensor::SCRATCHPAD_VACIO_CEROS);
        return s.obtenerEstado();
    }

    if (OneWire::crc8(pad, 8) != pad[8]) {
        s.setEstado(EstadoSensor::ERROR_CRC_SCRATCHPAD);
        return s.obtenerEstado();
    }

    int32_t raw = driver.getTemp(s.obtenerROM(), REINTENTOS_LECTURA);
    EstadoSensor estadoEvaluado = traducirCodigoRaw(raw, pad);

    if (estadoEvaluado == EstadoSensor::REINICIO_ENERGIA_85C) {
        driver.requestTemperaturesByAddress(s.obtenerROM());
        delay(tiempoConversionSegunResolucion(s.obtenerResolucionBits()));
        driver.readScratchPad(s.obtenerROM(), pad);
        raw = driver.getTemp(s.obtenerROM(), REINTENTOS_LECTURA);
        estadoEvaluado = traducirCodigoRaw(raw, pad);
    }

    s.setTempRaw(raw);

    if (estadoEvaluado != EstadoSensor::OK) {
        s.setTempCelsius(static_cast<float>(static_cast<int16_t>(estadoEvaluado)));
        s.setEstado(estadoEvaluado);
        return s.obtenerEstado();
    }

    float tempFinal = driver.getTempC(s.obtenerROM(), REINTENTOS_LECTURA);
    s.setTempCelsius(tempFinal);

    bool alarmaHw = driver.hasAlarm(s.obtenerROM());
    s.setEnAlarma(alarmaHw);

    if (tempFinal >= static_cast<float>(s.obtenerUmbralSuperior())) {
        s.setEstado(EstadoSensor::ALARMA_SUPERIOR);
        s.setEnAlarma(true);
    } else if (tempFinal <= static_cast<float>(s.obtenerUmbralInferior())) {
        s.setEstado(EstadoSensor::ALARMA_INFERIOR);
        s.setEnAlarma(true);
    } else {
        s.setEstado(EstadoSensor::OK);
    }

    return s.obtenerEstado();
}

void GestorSensores::actualizarTodosLosSlots() {
    solicitarConversionGlobal(true);
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].estaActivo()) {
            actualizarLecturaSlot(i);
        }
    }
}

bool GestorSensores::configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    if (!driver.setResolution(s.obtenerROM(), bits, false)) {
        s.setEstado(EstadoSensor::FALLO_ESCRITURA_EEPROM);
        return false;
    }
    s.setResolucionBits(driver.getResolution(s.obtenerROM()));
    return guardarEeprom ? driver.saveScratchPad(s.obtenerROM()) : true;
}

bool GestorSensores::configurarOffsetPorROM(const uint8_t* rom, float offsetC) {
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot >= 0) {
        sensores[slot].setOffset(offsetC);
    }
    return driver.setOffsetByDeviceAddress(rom, offsetC);
}

bool GestorSensores::configurarOffsetSlot(uint8_t slot, float offsetC) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    sensores[slot].setOffset(offsetC);
    return driver.setOffsetByDeviceAddress(sensores[slot].obtenerROM(), offsetC);
}

bool GestorSensores::configurarAlarmasSlot(uint8_t slot, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    driver.setHighAlarmTemp(s.obtenerROM(), thAltoC);
    driver.setLowAlarmTemp(s.obtenerROM(), tlBajoC);
    driver.setResolution(s.obtenerROM(), s.obtenerResolucionBits(), false);
    s.setUmbrales(driver.getHighAlarmTemp(s.obtenerROM()), driver.getLowAlarmTemp(s.obtenerROM()));
    return guardarEeprom ? driver.saveScratchPad(s.obtenerROM()) : true;
}

bool GestorSensores::configurarAlarmasPorROM(const uint8_t* rom, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom) {
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot >= 0) {
        return configurarAlarmasSlot(static_cast<uint8_t>(slot), thAltoC, tlBajoC, guardarEeprom);
    }
    driver.setHighAlarmTemp(rom, thAltoC);
    driver.setLowAlarmTemp(rom, tlBajoC);
    return guardarEeprom ? driver.saveScratchPad(rom) : true;
}

bool GestorSensores::guardarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    return driver.saveScratchPad(sensores[slot].obtenerROM());
}

bool GestorSensores::restaurarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    if (!driver.recallScratchPad(s.obtenerROM())) return false;
    s.setUmbrales(driver.getHighAlarmTemp(s.obtenerROM()), driver.getLowAlarmTemp(s.obtenerROM()));
    s.setResolucionBits(driver.getResolution(s.obtenerROM()));
    return true;
}

bool GestorSensores::escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    driver.setUserData(s.obtenerROM(), idCustom);
    s.setDatoUsuario(driver.getUserData(s.obtenerROM()));
    return s.obtenerDatoUsuario() == idCustom;
}

uint8_t GestorSensores::procesarBusquedaAlarmasHardware() {
    uint8_t contador = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (!sensores[i].estaActivo()) continue;
        bool alarma = driver.hasAlarm(sensores[i].obtenerROM());
        sensores[i].setEnAlarma(alarma);
        if (alarma) {
            contador++;
            ultimoEvento = EventoGestor::ALARMA_DETECTADA_EN_BUS;
        }
    }
    return contador;
}

Sensor& GestorSensores::obtenerSensor(uint8_t slot) {
    return sensores[(slot < MAX_SENSORES) ? slot : 0];
}

const Sensor& GestorSensores::obtenerSensor(uint8_t slot) const {
    return sensores[(slot < MAX_SENSORES) ? slot : 0];
}

Sensor* GestorSensores::buscarSensorPorMAC(const char* macHex) {
    if (!macHex) return nullptr;
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].estaActivo() && strncmp(sensores[i].obtenerMAC(), macHex, 16) == 0) {
            return &sensores[i];
        }
    }
    return nullptr;
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

const char* GestorSensores::nombreEstadoSensor(EstadoSensor estado) {
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

const char* GestorSensores::nombreEstadoBus(EstadoBus estado) {
    switch (estado) {
        case EstadoBus::OK:                       return "BUS_OK";
        case EstadoBus::LINEA_EN_CORTO_GND:       return "CORTO_GND";
        case EstadoBus::SIN_PULSO_PRESENCIA:      return "SIN_PRESENCIA";
        case EstadoBus::TOPOLOGIA_MODIFICADA:     return "TOPOLOGIA_CAMBIO";
        case EstadoBus::FALLO_CONVERSION_GLOBAL:  return "ERR_CONVERSION";
        case EstadoBus::LIMITE_SENSORES_EXCEDIDO: return "MAX_6_EXCEDIDO";
        default:                                  return "BUS_DESC";
    }
}

const char* GestorSensores::nombreEventoGestor(EventoGestor evento) {
    switch (evento) {
        case EventoGestor::NINGUNO:                        return "NINGUNO";
        case EventoGestor::BUS_INICIALIZADO:               return "BUS_INICIALIZADO";
        case EventoGestor::NUEVO_SENSOR_DETECTADO:         return "NUEVO_SENSOR";
        case EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE:return "HOT_UNPLUG";
        case EventoGestor::CONVERSION_COMPLETADA:          return "CONV_OK";
        case EventoGestor::ERROR_BUS_ELECTRICO:            return "ERR_ELECTRICO";
        case EventoGestor::ALARMA_DETECTADA_EN_BUS:        return "ALARMA_ACTIVA";
        default:                                           return "EVT_DESC";
    }
}