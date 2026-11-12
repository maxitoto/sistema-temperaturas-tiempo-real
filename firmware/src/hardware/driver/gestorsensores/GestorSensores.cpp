#include "GestorSensores.hpp"
#include "SensorInicializador.hpp"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>

GestorSensores::GestorSensores()
    : bus(Pin::PIN_SENSOR_1WIRE) {
    mutexSensores = xSemaphoreCreateRecursiveMutex();
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

int8_t GestorSensores::buscarSlotPorMACInterno(const char* macHex) const {
    if (!macHex || macHex[0] == '\0') return -1;
    size_t len = strlen(macHex);
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (!sensores[i].estaActivo()) continue;
        // Permite buscar tanto por MAC completa (16 chars) como por sufijo corto (mac3 o mac2)
        if (len == 3 && strncasecmp(sensores[i].obtenerMAC3(), macHex, 3) == 0) {
            return static_cast<int8_t>(i);
        }
        if (len == 2 && strncasecmp(sensores[i].obtenerMAC2(), macHex, 2) == 0) {
            return static_cast<int8_t>(i);
        }
        if (strncasecmp(sensores[i].obtenerMAC(), macHex, 16) == 0) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t GestorSensores::buscarSlotPorIdInterno(int32_t idSensorDB) const {
    if (idSensorDB < 0) return -1;
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].estaActivo() && sensores[i].obtenerIdSensorDB() == idSensorDB) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

void GestorSensores::reevaluarAlarmaSlotInterno(Sensor& s) {
    float t = s.obtenerTempCelsius();
    if (t >= s.obtenerUmbralSuperior()) {
        s.setEstado(EstadoSensor::ALARMA_SUPERIOR);
        s.setEnAlarma(true);
    } else if (t <= s.obtenerUmbralInferior()) {
        s.setEstado(EstadoSensor::ALARMA_INFERIOR);
        s.setEnAlarma(true);
    } else if (s.obtenerEstado() == EstadoSensor::ALARMA_SUPERIOR ||
               s.obtenerEstado() == EstadoSensor::ALARMA_INFERIOR ||
               s.obtenerEstado() == EstadoSensor::OK) {
        s.setEstado(EstadoSensor::OK);
        s.setEnAlarma(false);
    }
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
    bus.write(pad[2]);
    bus.write(pad[3]);
    if (rom[0] != DS18S20MODEL) {
        bus.write(pad[4]);
    }
    if (!copiarAEeprom) return true;
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_COPY_SCRATCHPAD, modoParasitoGlobal ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(15));
    if (modoParasitoGlobal) bus.depower();
    return true;
}

bool GestorSensores::leerFuenteAlimentacionNativa(const uint8_t* rom) {
    if (!bus.reset()) return false;
    bus.select(rom);
    bus.write(CMD_READ_POWER_SUPPLY);
    uint8_t bit = bus.read_bit();
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
        int16_t trunc = raw16 >> 1;
        float tempC = static_cast<float>(trunc) - 0.25f;
        if (pad[7] != 0) {
            tempC += static_cast<float>(pad[7] - pad[6]) / static_cast<float>(pad[7]);
        }
        outRaw = static_cast<int32_t>(tempC * 128.0f);
        return tempC;
    }

    switch (pad[4] & 0x60) {
        case 0x00: raw16 &= ~0x0007; break;
        case 0x20: raw16 &= ~0x0003; break;
        case 0x40: raw16 &= ~0x0001; break;
        default:   break;
    }

    outRaw = static_cast<int32_t>(raw16) * 8;
    return static_cast<float>(raw16) * 0.0625f;
}

EstadoBus GestorSensores::verificarEstadoElectricoBus() {
    ScopedSensorLock lk(mutexSensores);
    pinMode(Pin::PIN_SENSOR_1WIRE, INPUT_PULLUP);
    vTaskDelay(pdMS_TO_TICKS(2));
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
    ScopedSensorLock lk(mutexSensores);
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

        // Hidrata a demanda desde la caché lazy o desde la tabla Sensor en la MicroSD
        SensorInicializador::hidratarSensorBajoDemanda(s);

        uint8_t* pad = s.obtenerScratchpad();
        if (leerScratchpadNativo(romDetectada, pad) && OneWire::crc8(pad, 8) == pad[8]) {
            uint8_t resDeseada = s.estaHidratadoDB() ? s.obtenerResolucionBits() : resolucionGlobal;
            uint8_t cfgByte = ((resDeseada - 9) << 5) | 0x1F;
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
    ScopedSensorLock lk(mutexSensores);
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
    uint16_t esperaMs = 0;
    bool parasito = false;

    {
        ScopedSensorLock lk(mutexSensores);
        if (!bus.reset()) {
            estadoBus = EstadoBus::FALLO_CONVERSION_GLOBAL;
            return false;
        }
        bus.skip();
        bus.write(CMD_CONVERT_T, modoParasitoGlobal ? 1 : 0);
        tsUltimaConversion = millis();
        parasito = modoParasitoGlobal;

        if (bloqueante) {
            uint8_t maxBits = 9;
            for (uint8_t i = 0; i < MAX_SENSORES; i++) {
                if (sensores[i].estaActivo() && sensores[i].obtenerResolucionBits() > maxBits) {
                    maxBits = sensores[i].obtenerResolucionBits();
                }
            }
            esperaMs = tiempoConversionSegunResolucion(maxBits);
        }
    }

    // Espera de conversión (ej. 750ms) FUERA del mutex para que OLED y RPC lean snapshots sin bloqueo
    if (bloqueante && esperaMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(esperaMs));
        ScopedSensorLock lk(mutexSensores);
        if (parasito) bus.depower();
    }

    ScopedSensorLock lk(mutexSensores);
    ultimoEvento = EventoGestor::CONVERSION_COMPLETADA;
    return true;
}

bool GestorSensores::solicitarConversionPorSlot(uint8_t slot, bool bloqueante) {
    uint16_t esperaMs = 0;
    bool parasito = false;

    {
        ScopedSensorLock lk(mutexSensores);
        if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
        if (!bus.reset()) return false;
        bus.select(sensores[slot].obtenerROM());
        bus.write(CMD_CONVERT_T, modoParasitoGlobal ? 1 : 0);
        tsUltimaConversion = millis();
        parasito = modoParasitoGlobal;
        if (bloqueante) {
            esperaMs = tiempoConversionSegunResolucion(sensores[slot].obtenerResolucionBits());
        }
    }

    if (bloqueante && esperaMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(esperaMs));
        ScopedSensorLock lk(mutexSensores);
        if (parasito) bus.depower();
    }
    return true;
}

bool GestorSensores::estaConversionCompleta() {
    ScopedSensorLock lk(mutexSensores);
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

    bool todoFF = true;
    bool todoCeros = true;
    for (uint8_t b = 0; b < 9; b++) {
        if (pad[b] != 0xFF) todoFF = false;
        if (pad[b] != 0x00) todoCeros = false;
    }
    if (todoFF)    return EstadoSensor::DESCONECTADO;
    if (todoCeros) return EstadoSensor::SCRATCHPAD_VACIO_CEROS;

    if (OneWire::crc8(pad, 8) != pad[8]) {
        return EstadoSensor::ERROR_CRC_SCRATCHPAD;
    }

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
    ScopedSensorLock lk(mutexSensores);
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

    s.setTimestampLectura(millis());

    if (!padLeido) {
        s.setTempRaw(DEVICE_DISCONNECTED_RAW);
        s.setTempCruda(DEVICE_DISCONNECTED_C);
        s.setTempCelsius(DEVICE_DISCONNECTED_C);
        s.setEstado(EstadoSensor::DESCONECTADO);
        s.setEnAlarma(true);
        return s.obtenerEstado();
    }

    if (estadoEvaluado == EstadoSensor::REINICIO_ENERGIA_85C) {
        solicitarConversionPorSlot(slot, true);
        if (leerScratchpadNativo(s.obtenerROM(), pad)) {
            estadoEvaluado = evaluarFalloScratchpad(pad);
        }
    }

    if (estadoEvaluado != EstadoSensor::OK) {
        float codigoComoTemp = static_cast<float>(static_cast<int16_t>(estadoEvaluado));
        s.setTempCruda(codigoComoTemp);
        s.setTempCelsius(codigoComoTemp);
        s.setEstado(estadoEvaluado);
        s.setEnAlarma(true);
        return s.obtenerEstado();
    }

    int32_t raw = 0;
    float tempCruda = convertirScratchpadACelsius(s.obtenerROM(), pad, raw);
    float tempCorregida = s.aplicarCalibracion(tempCruda);

    s.setTempRaw(raw);
    s.setTempCruda(tempCruda);
    s.setTempCelsius(tempCorregida);

    reevaluarAlarmaSlotInterno(s);
    return s.obtenerEstado();
}

void GestorSensores::actualizarTodosLosSlots() {
    solicitarConversionGlobal(true);
    ScopedSensorLock lk(mutexSensores);
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (sensores[i].estaActivo()) {
            actualizarLecturaSlot(i);
        }
    }
}

// --- CONSULTAS THREAD-SAFE ---

bool GestorSensores::obtenerSensorPorMAC(const char* macHex, SnapshotSensor& outSnapshot) const {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorMACInterno(macHex);
    if (slot < 0) return false;
    outSnapshot = sensores[slot].crearSnapshot();
    return true;
}

bool GestorSensores::obtenerSensorPorROM(const uint8_t rom8[8], SnapshotSensor& outSnapshot) const {
    ScopedSensorLock lk(mutexSensores);
    if (!rom8) return false;
    int8_t slot = buscarSlotPorROM(rom8);
    if (slot < 0) return false;
    outSnapshot = sensores[slot].crearSnapshot();
    return true;
}

bool GestorSensores::obtenerSensorPorId(int32_t idSensorDB, SnapshotSensor& outSnapshot) const {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorIdInterno(idSensorDB);
    if (slot < 0) return false;
    outSnapshot = sensores[slot].crearSnapshot();
    return true;
}

bool GestorSensores::obtenerSensorPorSlot(uint8_t slot, SnapshotSensor& outSnapshot) const {
    ScopedSensorLock lk(mutexSensores);
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    outSnapshot = sensores[slot].crearSnapshot();
    return true;
}

uint8_t GestorSensores::obtenerSensoresConectados(SnapshotSensor outLista[MAX_SENSORES]) const {
    ScopedSensorLock lk(mutexSensores);
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; ++i) {
        if (sensores[i].estaActivo()) {
            outLista[count++] = sensores[i].crearSnapshot();
        }
    }
    return count;
}

uint8_t GestorSensores::obtenerMacsConectadas(char outMacs[MAX_SENSORES][17]) const {
    ScopedSensorLock lk(mutexSensores);
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; ++i) {
        if (sensores[i].estaActivo()) {
            strncpy(outMacs[count], sensores[i].obtenerMAC(), 17);
            outMacs[count][16] = '\0';
            count++;
        }
    }
    return count;
}

ResumenSistemaSensores GestorSensores::obtenerEstadoSistema() const {
    ScopedSensorLock lk(mutexSensores);
    ResumenSistemaSensores res{};
    res.estadoBus                   = estadoBus;
    res.ultimoEvento                = ultimoEvento;
    res.totalConectados             = totalAsignados;
    res.totalDetectadosBus          = totalDetectadosBus;
    res.modoParasitoActivo          = modoParasitoGlobal;
    res.timestampUltimaConversionMs = tsUltimaConversion;
    res.totalEnAlarma               = 0;

    for (uint8_t i = 0; i < MAX_SENSORES; ++i) {
        if (sensores[i].estaActivo() && sensores[i].obtenerEstado() != EstadoSensor::OK) {
            res.totalEnAlarma++;
        }
    }
    return res;
}

void GestorSensores::obtenerEstadosParaAlarma(EstadoSensor outEstados[MAX_SENSORES], EstadoBus& outEstadoBus) const {
    ScopedSensorLock lk(mutexSensores);
    outEstadoBus = estadoBus;
    for (uint8_t i = 0; i < MAX_SENSORES; ++i) {
        outEstados[i] = sensores[i].estaActivo() ? sensores[i].obtenerEstado() : EstadoSensor::SLOT_VACIO;
    }
}

uint8_t GestorSensores::extraerEventosCambioEstado(EventoCambioSensor outEventos[MAX_SENSORES]) {
    ScopedSensorLock lk(mutexSensores);
    uint8_t cant = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; ++i) {
        if (sensores[i].estaActivo()) {
            EventoCambioSensor evt{};
            if (sensores[i].consumirCambioEstado(evt)) {
                outEventos[cant++] = evt;
            }
        }
    }
    return cant;
}

// --- MUTADORES CON ACTUALIZACIÓN EN RAM + BASE DE DATOS ---

bool GestorSensores::configurarOffsetPorMAC(const char* macHex, float nuevoOffsetC) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorMACInterno(macHex);
    if (slot < 0) return false;
    return configurarOffsetSlot(static_cast<uint8_t>(slot), nuevoOffsetC);
}

bool GestorSensores::configurarOffsetPorId(int32_t idSensorDB, float nuevoOffsetC) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorIdInterno(idSensorDB);
    if (slot < 0) return false;
    return configurarOffsetSlot(static_cast<uint8_t>(slot), nuevoOffsetC);
}

bool GestorSensores::configurarOffsetPorROM(const uint8_t* rom, float offsetC) {
    ScopedSensorLock lk(mutexSensores);
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot < 0) return false;
    return configurarOffsetSlot(static_cast<uint8_t>(slot), offsetC);
}

bool GestorSensores::configurarOffsetSlot(uint8_t slot, float offsetC) {
    ScopedSensorLock lk(mutexSensores);
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    s.setOffset(offsetC, SensorInicializador::obtenerRefCongelacion(), SensorInicializador::obtenerRefEbullicion());
    if (s.obtenerTempCruda() > DEVICE_DISCONNECTED_C) {
        s.setTempCelsius(s.aplicarCalibracion(s.obtenerTempCruda()));
        reevaluarAlarmaSlotInterno(s);
    }
    SensorInicializador::persistirConfiguracionSensor(s);
    return true;
}

bool GestorSensores::calibrarPuntosPorMAC(const char* macHex, float tempMedidaHirviendo, float tempMedidaHielo, float offsetManual) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorMACInterno(macHex);
    if (slot < 0) return false;
    Sensor& s = sensores[slot];
    s.setOffset(offsetManual, SensorInicializador::obtenerRefCongelacion(), SensorInicializador::obtenerRefEbullicion());
    s.setCorrecciones(tempMedidaHirviendo, tempMedidaHielo, SensorInicializador::obtenerRefCongelacion(), SensorInicializador::obtenerRefEbullicion());
    if (s.obtenerTempCruda() > DEVICE_DISCONNECTED_C) {
        s.setTempCelsius(s.aplicarCalibracion(s.obtenerTempCruda()));
        reevaluarAlarmaSlotInterno(s);
    }
    SensorInicializador::persistirConfiguracionSensor(s);
    return true;
}

bool GestorSensores::configurarUmbralesPorMAC(const char* macHex, float thAltoC, float tlBajoC, bool guardarEeprom) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorMACInterno(macHex);
    if (slot < 0) return false;
    return configurarAlarmasSlot(static_cast<uint8_t>(slot), thAltoC, tlBajoC, guardarEeprom);
}

bool GestorSensores::configurarUmbralesPorId(int32_t idSensorDB, float thAltoC, float tlBajoC, bool guardarEeprom) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorIdInterno(idSensorDB);
    if (slot < 0) return false;
    return configurarAlarmasSlot(static_cast<uint8_t>(slot), thAltoC, tlBajoC, guardarEeprom);
}

bool GestorSensores::configurarAlarmasSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom) {
    ScopedSensorLock lk(mutexSensores);
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];

    s.setUmbrales(thAltoC, tlBajoC);
    reevaluarAlarmaSlotInterno(s);
    SensorInicializador::persistirConfiguracionSensor(s);

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
    ScopedSensorLock lk(mutexSensores);
    if (rom == nullptr) return false;
    int8_t slot = buscarSlotPorROM(rom);
    if (slot >= 0) {
        return configurarAlarmasSlot(static_cast<uint8_t>(slot), thAltoC, tlBajoC, guardarEeprom);
    }
    return false;
}

bool GestorSensores::configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom) {
    ScopedSensorLock lk(mutexSensores);
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
    SensorInicializador::persistirConfiguracionSensor(s);
    return true;
}

bool GestorSensores::guardarEepromSlot(uint8_t slot) {
    ScopedSensorLock lk(mutexSensores);
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    return escribirScratchpadNativo(s.obtenerROM(), s.obtenerScratchpad(), true);
}

bool GestorSensores::restaurarEepromSlot(uint8_t slot) {
    ScopedSensorLock lk(mutexSensores);
    if (slot >= MAX_SENSORES || !sensores[slot].estaActivo()) return false;
    Sensor& s = sensores[slot];
    if (!bus.reset()) return false;
    bus.select(s.obtenerROM());
    bus.write(CMD_RECALL_EEPROM);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t* pad = s.obtenerScratchpad();
    if (!leerScratchpadNativo(s.obtenerROM(), pad)) return false;
    s.setResolucionBits(extraerResolucion(s.obtenerROM(), pad));
    return true;
}

bool GestorSensores::escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom) {
    ScopedSensorLock lk(mutexSensores);
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
    ScopedSensorLock lk(mutexSensores);
    uint8_t contador = 0;
    for (uint8_t i = 0; i < MAX_SENSORES; i++) {
        if (!sensores[i].estaActivo()) continue;
        reevaluarAlarmaSlotInterno(sensores[i]);
        if (sensores[i].estaEnAlarma()) {
            contador++;
            ultimoEvento = EventoGestor::ALARMA_DETECTADA_EN_BUS;
        }
    }
    return contador;
}

Sensor& GestorSensores::obtenerSensor(uint8_t slot) {
    ScopedSensorLock lk(mutexSensores);
    return sensores[(slot < MAX_SENSORES) ? slot : 0];
}

const Sensor& GestorSensores::obtenerSensor(uint8_t slot) const {
    ScopedSensorLock lk(mutexSensores);
    return sensores[(slot < MAX_SENSORES) ? slot : 0];
}

Sensor* GestorSensores::buscarSensorPorMAC(const char* macHex) {
    ScopedSensorLock lk(mutexSensores);
    int8_t slot = buscarSlotPorMACInterno(macHex);
    return (slot >= 0) ? &sensores[slot] : nullptr;
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