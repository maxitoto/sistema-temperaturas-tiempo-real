#include "GestorSensores.hpp"
#include <string.h>
#include <stdio.h>
#include <math.h>

GestorSensores::GestorSensores()
    : bus(Pin::PIN_SENSOR_1WIRE) {
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        sensores[i].limpiar(i);
    }
}

bool GestorSensores::esFamiliaValida(uint8_t fam) {
    return fam == DS18B20MODEL || fam == DS18S20MODEL ||
           fam == DS1822MODEL  || fam == DS1825MODEL  ||
           fam == DS28EA00MODEL;
}

int8_t GestorSensores::clampToInt8(float val) {
    float r = roundf(val);
    if (r > 125.0f) return 125;
    if (r < -55.0f) return -55;
    return static_cast<int8_t>(r);
}

int8_t GestorSensores::buscarSlotPorROM(const DeviceAddress rom) const {
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].coincideROM(rom)) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

bool GestorSensores::leerScratchpadNativo(const uint8_t* rom, uint8_t* pad) {
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_READ_SCRATCHPAD);
    for (uint8_t i = 0; i < 9; i++) {
        pad[i] = bus.read();
    }
    return bus.reset() != 0;
}

bool GestorSensores::escribirScratchpadNativo(const uint8_t* rom, const uint8_t* pad, bool copiarAEeprom) {
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_WRITE_SCRATCHPAD);
    bus.write(pad[2]); // TH / User byte 1
    bus.write(pad[3]); // TL / User byte 2
    if (rom[0] != DS18S20MODEL) {
        bus.write(pad[4]); // Registro de configuración (resolución 9..12 bits)
    }
    if (!copiarAEeprom) return true;
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_COPY_SCRATCHPAD, modoParasitoGlobal ? 1 : 0);
    delay(15); // Tiempo de escritura en EEPROM según hoja de datos (10 ms min)
    if (modoParasitoGlobal) bus.depower();
    return true;
}

bool GestorSensores::leerFuenteAlimentacionNativa(const uint8_t* rom) {
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_READ_POWER_SUPPLY);
    uint8_t bit = bus.read_bit(); // 0 = parásita, 1 = VDD externo
    bus.reset();
    return (bit == 0);
}

uint8_t GestorSensores::extraerResolucion(const uint8_t* rom, const uint8_t* pad) {
    if (rom[0] == DS18S20MODEL) return 12;
    switch (pad[4] & 0x60) {
        case 0x00: return 9;
        case 0x20: return 10;
        case 0x40: return 11;
        case 0x60:
        default:   return 12;
    }
}

float GestorSensores::convertirScratchpadACelsius(const uint8_t* rom, const uint8_t* pad, int32_t& outRaw) {
    int16_t raw16 = static_cast<int16_t>((static_cast<uint16_t>(pad[1]) << 8) | pad[0]);

    if (rom[0] == DS18S20MODEL) {
        // DS18S20 usa resolución extendida con COUNT_REMAIN (pad[6]) y COUNT_PER_C (pad[7])
        int16_t trunc = raw16 >> 1;
        float tempC = static_cast<float>(trunc) - 0.25f;
        if (pad[7] != 0) {
            tempC += static_cast<float>(pad[7] - pad[6]) / static_cast<float>(pad[7]);
        }
        outRaw = static_cast<int32_t>(tempC * 128.0f);
        return tempC;
    }

    // Limpiar bits indefinidos según resolución configurada (9, 10, 11 o 12 bits)
    switch (pad[4] & 0x60) {
        case 0x00: raw16 &= ~0x0007; break; // 9 bits
        case 0x20: raw16 &= ~0x0003; break; // 10 bits
        case 0x40: raw16 &= ~0x0001; break; // 11 bits
        default:   break;                   // 12 bits
    }

    outRaw = static_cast<int32_t>(raw16) * 8; // En unidades de 1/128 °C (16 LSB/°C * 8 = 128)
    return static_cast<float>(raw16) * 0.0625f;
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
    if (resolucionGlobal < 9 || resolucionGlobal > 12) resolucionGlobal = 12;

    DeviceAddress romsEncontradas[MAX_SENSORES + 2];
    uint8_t detectados = 0;

    bus.reset_search();
    DeviceAddress addr;
    while (bus.search(addr)) {
        if (detectados < (MAX_SENSORES + 2)) {
            memcpy(romsEncontradas[detectados], addr, sizeof(DeviceAddress));
        }
        detectados++;
    }

    totalDetectadosBus = detectados;
    estadoBus = (totalDetectadosBus > MAX_SENSORES)
                ? EstadoBus::LIMITE_SENSORES_EXCEDIDO
                : EstadoBus::OK;

    Sensor anteriores[MAX_SENSORES];
    for (uint8_t s = 0; s < MAX_SENSORES; s++) {
        anteriores[s] = sensores[s];
        sensores[s].limpiar(s);
    }

    totalAsignados = 0;
    modoParasitoGlobal = false;
    uint8_t tope = (totalDetectadosBus > MAX_SENSORES) ? MAX_SENSORES : totalDetectadosBus;

    for (uint8_t i = 0; i < tope; i++) {
        Sensor& s = sensores[i];
        const uint8_t* romDetectada = romsEncontradas[i];

        if (OneWire::crc8(romDetectada, 7) != romDetectada[7]) {
            s.setEstado(EstadoSensor::DIRECCION_ROM_INVALIDA);
            continue;
        }

        if (!esFamiliaValida(romDetectada[0])) {
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
            ultimoEvento = EventoGestor::NUEVO_SENSOR_DETECTADO;
        }

        bool parasita = leerFuenteAlimentacionNativa(romDetectada);
        s.setAlimentacionParasita(parasita);
        if (parasita) modoParasitoGlobal = true;

        uint8_t* pad = s.obtenerScratchpad();
        if (leerScratchpadNativo(romDetectada, pad) && OneWire::crc8(pad, 8) == pad[8]) {
            uint8_t cfgByte = ((resolucionGlobal - 9) << 5) | 0x1F;
            if (!yaExistia && pad[4] != cfgByte) {
                pad[2] = static_cast<uint8_t>(clampToInt8(s.obtenerUmbralSuperior()));
                pad[3] = static_cast<uint8_t>(clampToInt8(s.obtenerUmbralInferior()));
                pad[4] = cfgByte;
                escribirScratchpadNativo(romDetectada, pad, guardarEnEeprom);
            }
            s.setResolucionBits(extraerResolucion(romDetectada, pad));
            int16_t usr = static_cast<int16_t>((static_cast<uint16_t>(pad[2]) << 8) | pad[3]);
            s.setDatoUsuario(usr);
        } else {
            s.setResolucionBits(resolucionGlobal);
        }

        s.setEstado(EstadoSensor::OK);
        totalAsignados++;
    }

    if (totalAsignados > 0) {
        solicitarConversionGlobal(true);
    }

    if (ultimoEvento == EventoGestor::NINGUNO) {
        ultimoEvento = EventoGestor::BUS_INICIALIZADO;
    }

    return totalAsignados;
}

bool GestorSensores::verificarCambiosEnCaliente() {
    DeviceAddress romsActuales[MAX_SENSORES + 2];
    uint8_t nuevoTotal = 0;

    bus.reset_search();
    DeviceAddress addr;
    while (bus.search(addr)) {
        if (nuevoTotal < (MAX_SENSORES + 2)) {
            memcpy(romsActuales[nuevoTotal], addr, sizeof(DeviceAddress));
        }
        nuevoTotal++;
    }

    if (nuevoTotal != totalDetectadosBus) {
        estadoBus = EstadoBus::TOPOLOGIA_MODIFICADA;
        ultimoEvento = (nuevoTotal < totalDetectadosBus)
            ? EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE
            : EventoGestor::NUEVO_SENSOR_DETECTADO;
        return true;
    }

    for (uint8_t i = 0; i < nuevoTotal && i < MAX_SENSORES; ++i) {
        bool existe = false;
        for (uint8_t k = 0; k < MAX_SENSORES; ++k) {
            if (sensores[k].estaActivo() && sensores[k].coincideROM(romsActuales[i])) {
                existe = true;
                break;
            }
        }
        if (!existe) {
            estadoBus = EstadoBus::TOPOLOGIA_MODIFICADA;
            ultimoEvento = EventoGestor::NUEVO_SENSOR_DETECTADO;
            return true;
        }
    }
    return false;
}

bool GestorSensores::solicitarConversionGlobal(bool bloqueante) {
    if (!bus.reset()) {
        estadoBus = EstadoBus::FALLO_CONVERSION_GLOBAL;
        return false;
    }
    bus.skip(); // Broadcast a todas las sondas del bus
    bus.write(CMD_CONVERT_T, modoParasitoGlobal ? 1 : 0);
    tsUltimaConversion = millis();

    if (bloqueante) {
        uint8_t maxBits = 9;
        for (uint8_t i = 0; i < MAX_SENSORES; i++) {
            if (sensores[i].estaActivo() && sensores[i].obtenerResolucionBits() > maxBits) {
                maxBits = sensores[i].obtenerResolucionBits();
            }
        }
        delay(tiempoConversionSegunResolucion(maxBits));
        if (modoParasitoGlobal) bus.depower();
    }

    ultimoEvento = EventoGestor::CONVERSION_COMPLETADA;
    return true;
}

bool GestorSensores::solicitarConversionPorSlot(uint8_t slot, bool bloqueante) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    if (!bus.reset()) return false;
    bus.select(sensores[slot].obtenerROM());
    bus.write(CMD_CONVERT_T, modoParasitoGlobal ? 1 : 0);
    tsUltimaConversion = millis();

    if (bloqueante) {
        delay(tiempoConversionSegunResolucion(sensores[slot].obtenerResolucionBits()));
        if (modoParasitoGlobal) bus.depower();
    }
    return true;
}

bool GestorSensores::estaConversionCompleta() {
    if (modoParasitoGlobal) {
        return (millis() - tsUltimaConversion) >= 750;
    }
    return bus.read_bit() == 1;
}

uint16_t GestorSensores::tiempoConversionSegunResolucion(uint8_t bits) const {
    switch (bits) {
        case 9:  return 94;
        case 10: return 188;
        case 11: return 375;
        case 12:
        default: return 750;
    }
}

EstadoSensor GestorSensores::evaluarFalloScratchpad(const uint8_t* pad) const {
    if (pad == nullptr) return EstadoSensor::DESCONECTADO;

    // 1. Todo 0xFF → línea abierta o sensor desconectado durante la lectura
    bool todoFF = true;
    bool todoCeros = true;
    for (uint8_t b = 0; b < 9; b++) {
        if (pad[b] != 0xFF) todoFF = false;
        if (pad[b] != 0x00) todoCeros = false;
    }
    if (todoFF)    return EstadoSensor::DESCONECTADO;
    if (todoCeros) return EstadoSensor::SCRATCHPAD_VACIO_CEROS;

    // 2. Verificar CRC8 del scratchpad
    if (OneWire::crc8(pad, 8) != pad[8]) {
        return EstadoSensor::ERROR_CRC_SCRATCHPAD;
    }

    // 3. Códigos especiales en el registro de temperatura (bytes 0 y 1)
    int16_t raw16 = static_cast<int16_t>((static_cast<uint16_t>(pad[1]) << 8) | pad[0]);
    if (raw16 == 0x0550 && pad[6] == 0x0C) {
        return EstadoSensor::REINICIO_ENERGIA_85C;
    }
    if (raw16 == 0x07FF) {
        return EstadoSensor::ALIMENTACION_INSUFICIENTE;
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
    EstadoSensor estadoEvaluado = EstadoSensor::DESCONECTADO;

    for (uint8_t intento = 0; intento < REINTENTOS_LECTURA; intento++) {
        if (leerScratchpadNativo(s.obtenerROM(), pad)) {
            padLeido = true;
            estadoEvaluado = evaluarFalloScratchpad(pad);
            if (estadoEvaluado == EstadoSensor::OK ||
                estadoEvaluado == EstadoSensor::REINICIO_ENERGIA_85C) {
                break;
            }
        }
    }

    if (!padLeido) {
        s.setTempRaw(DEVICE_DISCONNECTED_RAW);
        s.setTempCelsius(DEVICE_DISCONNECTED_C);
        s.setEstado(EstadoSensor::DESCONECTADO);
        return s.obtenerEstado();
    }

    // Si el sensor reporta 85.0 °C por Power-On-Reset (glitch de energía), forzamos reconversión
    if (estadoEvaluado == EstadoSensor::REINICIO_ENERGIA_85C) {
        solicitarConversionPorSlot(slot, true);
        if (leerScratchpadNativo(s.obtenerROM(), pad)) {
            estadoEvaluado = evaluarFalloScratchpad(pad);
        }
    }

    if (estadoEvaluado != EstadoSensor::OK) {
        s.setTempCelsius(static_cast<float>(static_cast<int16_t>(estadoEvaluado)));
        s.setEstado(estadoEvaluado);
        return s.obtenerEstado();
    }

    int32_t raw = 0;
    float tempCruda = convertirScratchpadACelsius(s.obtenerROM(), pad, raw);
    float tempFinal = tempCruda + s.obtenerOffset();

    s.setTempRaw(raw);
    s.setTempCelsius(tempFinal);

    // Evaluación de umbrales en punto flotante real (float)
    if (tempFinal >= s.obtenerUmbralSuperior()) {
        s.setEstado(EstadoSensor::ALARMA_SUPERIOR);
        s.setEnAlarma(true);
    } else if (tempFinal <= s.obtenerUmbralInferior()) {
        s.setEstado(EstadoSensor::ALARMA_INFERIOR);
        s.setEnAlarma(true);
    } else {
        s.setEstado(EstadoSensor::OK);
        s.setEnAlarma(false);
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
    if (bits < 9 || bits > 12) return false;

    Sensor& s = sensores[slot];
    uint8_t* pad = s.obtenerScratchpad();
    if (!leerScratchpadNativo(s.obtenerROM(), pad)) return false;

    pad[4] = ((bits - 9) << 5) | 0x1F;
    if (!escribirScratchpadNativo(s.obtenerROM(), pad, guardarEeprom)) {
        s.setEstado(EstadoSensor::FALLO_ESCRITURA_EEPROM);
        return false;
    }
    s.setResolucionBits(bits);
    return true;
}

bool GestorSensores::configurarOffsetPorROM(const uint8_t* rom, float offsetC) {
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot < 0) return false;
    sensores[slot].setOffset(offsetC);
    return true;
}

bool GestorSensores::configurarOffsetSlot(uint8_t slot, float offsetC) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    sensores[slot].setOffset(offsetC);
    return true;
}

bool GestorSensores::configurarAlarmasSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];

    // Guardamos los umbrales exactos en float en RAM
    s.setUmbrales(thAltoC, tlBajoC);

    // Opcionalmente escribimos los registros TH/TL del scratchpad si se pide persistir en EEPROM del sensor
    if (guardarEeprom) {
        uint8_t* pad = s.obtenerScratchpad();
        if (!leerScratchpadNativo(s.obtenerROM(), pad)) return false;
        pad[2] = static_cast<uint8_t>(clampToInt8(thAltoC));
        pad[3] = static_cast<uint8_t>(clampToInt8(tlBajoC));
        return escribirScratchpadNativo(s.obtenerROM(), pad, true);
    }
    return true;
}

bool GestorSensores::configurarAlarmasPorROM(const uint8_t* rom, float thAltoC, float tlBajoC, bool guardarEeprom) {
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot >= 0) {
        return configurarAlarmasSlot(static_cast<uint8_t>(slot), thAltoC, tlBajoC, guardarEeprom);
    }
    return false;
}

bool GestorSensores::guardarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    return escribirScratchpadNativo(s.obtenerROM(), s.obtenerScratchpad(), true);
}

bool GestorSensores::restaurarEepromSlot(uint8_t slot) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    if (!bus.reset()) return false;
    bus.select(s.obtenerROM());
    bus.write(CMD_RECALL_EEPROM);
    delay(10);

    uint8_t* pad = s.obtenerScratchpad();
    if (!leerScratchpadNativo(s.obtenerROM(), pad)) return false;
    s.setResolucionBits(extraerResolucion(s.obtenerROM(), pad));
    return true;
}

bool GestorSensores::escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom) {
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    uint8_t* pad = s.obtenerScratchpad();
    if (!leerScratchpadNativo(s.obtenerROM(), pad)) return false;

    pad[2] = static_cast<uint8_t>((idCustom >> 8) & 0xFF);
    pad[3] = static_cast<uint8_t>(idCustom & 0xFF);
    if (!escribirScratchpadNativo(s.obtenerROM(), pad, false)) return false;

    s.setDatoUsuario(idCustom);
    return true;
}

uint8_t GestorSensores::procesarBusquedaAlarmasHardware() {
    uint8_t contador = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (!sensores[i].estaActivo()) continue;
        float t = sensores[i].obtenerTempCelsius();
        bool alarma = (t >= sensores[i].obtenerUmbralSuperior() ||
                       t <= sensores[i].obtenerUmbralInferior());
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