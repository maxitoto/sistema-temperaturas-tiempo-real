#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "EstadoSensor.hpp"
#include "Sensor.hpp"
#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class GestorSensores {
public:
    static constexpr uint8_t MAX_SENSORES      = 6;
    static constexpr uint8_t REINTENTOS_LECTURA = 3;

private:
    OneWire           bus;
    DallasTemperature driver;
    Sensor            sensores[MAX_SENSORES];

    EstadoBus         estadoBus          = EstadoBus::OK;
    EventoGestor      ultimoEvento       = EventoGestor::NINGUNO;
    uint8_t           totalDetectadosBus = 0;
    uint8_t           totalAsignados     = 0;
    unsigned long     tsUltimaConversion = 0;

    GestorSensores();
    ~GestorSensores() = default;

    int8_t buscarSlotPorROM(const DeviceAddress rom) const;
    EstadoSensor traducirCodigoRaw(int32_t raw, const uint8_t* scratchpad) const;

public:
    static GestorSensores& getInstance() {
        static GestorSensores instance;
        return instance;
    }

    GestorSensores(const GestorSensores&)            = delete;
    GestorSensores& operator=(const GestorSensores&) = delete;
    GestorSensores(GestorSensores&&)                 = delete;
    GestorSensores& operator=(GestorSensores&&)      = delete;

    EstadoBus verificarEstadoElectricoBus();
    uint8_t escanearYAsignarSensores(uint8_t resolucionGlobal = 12, bool guardarEnEeprom = false);
    bool verificarCambiosEnCaliente();

    bool solicitarConversionGlobal(bool bloqueante = true);
    bool solicitarConversionPorSlot(uint8_t slot, bool bloqueante = true);
    bool estaConversionCompleta();
    uint16_t tiempoConversionSegunResolucion(uint8_t bits) const;

    EstadoSensor actualizarLecturaSlot(uint8_t slot);
    void actualizarTodosLosSlots();

    bool configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom = false);
    bool configurarOffsetPorROM(const uint8_t* rom, float offsetC);
    bool configurarOffsetSlot(uint8_t slot, float offsetC);
    bool configurarAlarmasSlot(uint8_t slot, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom = false);
    bool configurarAlarmasPorROM(const uint8_t* rom, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom = false);
    bool guardarEepromSlot(uint8_t slot);
    bool restaurarEepromSlot(uint8_t slot);
    bool escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom);
    uint8_t procesarBusquedaAlarmasHardware();

    Sensor& obtenerSensor(uint8_t slot);
    const Sensor& obtenerSensor(uint8_t slot) const;
    Sensor* buscarSensorPorMAC(const char* macHex);

    DallasTemperature& obtenerDriverDallas() { return driver; }
    EstadoBus          obtenerEstadoBus()    const { return estadoBus; }
    EventoGestor       obtenerUltimoEvento() const { return ultimoEvento; }
    uint8_t            obtenerTotalAsignados() const { return totalAsignados; }
    uint8_t            obtenerTotalEnBus()   const { return totalDetectadosBus; }
    unsigned long      obtenerTimestampConversion() const { return tsUltimaConversion; }
    bool               hayModoParasitoGlobal();

    static const char* nombreEstadoSensor(EstadoSensor estado);
    static const char* nombreEstadoBus(EstadoBus estado);
    static const char* nombreEventoGestor(EventoGestor evento);
    static const char* nombreFamilia(uint8_t codigoFamilia);
};

inline GestorSensores& gestorSensores = GestorSensores::getInstance();