#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include "EstadoSensor.hpp"
#include "Sensor.hpp"
#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class GestorSensores {
public:
    static constexpr uint8_t MAX_SENSORES      = 6;
    static constexpr uint8_t REINTENTOS_LECTURA = 3;

private:
    // Comandos nativos de protocolo Maxim DS18B20 (1-Wire)
    static constexpr uint8_t CMD_CONVERT_T         = 0x44;
    static constexpr uint8_t CMD_READ_SCRATCHPAD   = 0xBE;
    static constexpr uint8_t CMD_WRITE_SCRATCHPAD  = 0x4E;
    static constexpr uint8_t CMD_COPY_SCRATCHPAD   = 0x48;
    static constexpr uint8_t CMD_RECALL_EEPROM     = 0xB8;
    static constexpr uint8_t CMD_READ_POWER_SUPPLY = 0xB4;

    OneWire       bus;
    Sensor        sensores[MAX_SENSORES];

    EstadoBus     estadoBus          = EstadoBus::OK;
    EventoGestor  ultimoEvento       = EventoGestor::NINGUNO;
    uint8_t       totalDetectadosBus = 0;
    uint8_t       totalAsignados     = 0;
    unsigned long tsUltimaConversion = 0;
    bool          modoParasitoGlobal = false;

    GestorSensores();
    ~GestorSensores() = default;

    int8_t buscarSlotPorROM(const DeviceAddress rom) const;
    EstadoSensor evaluarFalloScratchpad(const uint8_t* pad) const;

    // Helpers nativos 1-Wire (reemplazan a DallasTemperature)
    static bool esFamiliaValida(uint8_t fam);
    bool leerScratchpadNativo(const uint8_t* rom, uint8_t* pad);
    bool escribirScratchpadNativo(const uint8_t* rom, const uint8_t* pad, bool copiarAEeprom);
    bool leerFuenteAlimentacionNativa(const uint8_t* rom);
    static uint8_t extraerResolucion(const uint8_t* rom, const uint8_t* pad);
    static float convertirScratchpadACelsius(const uint8_t* rom, const uint8_t* pad, int32_t& outRaw);
    static int8_t clampToInt8(float val);

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

    void consumirEvento() { ultimoEvento = EventoGestor::NINGUNO; }

    bool configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom = false);
    bool configurarOffsetPorROM(const uint8_t* rom, float offsetC);
    bool configurarOffsetSlot(uint8_t slot, float offsetC);

    // Ahora los umbrales de alarma son 100% float con soporte de decimales
    bool configurarAlarmasSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarAlarmasPorROM(const uint8_t* rom, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarUmbralesSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom = false) {
        return configurarAlarmasSlot(slot, thAltoC, tlBajoC, guardarEeprom);
    }

    bool guardarEepromSlot(uint8_t slot);
    bool restaurarEepromSlot(uint8_t slot);
    bool escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom);
    uint8_t procesarBusquedaAlarmasHardware();

    Sensor& obtenerSensor(uint8_t slot);
    const Sensor& obtenerSensor(uint8_t slot) const;
    Sensor* buscarSensorPorMAC(const char* macHex);

    EstadoBus     obtenerEstadoBus()           const { return estadoBus; }
    EventoGestor  obtenerUltimoEvento()        const { return ultimoEvento; }
    uint8_t       obtenerTotalAsignados()      const { return totalAsignados; }
    uint8_t       obtenerTotalEnBus()          const { return totalDetectadosBus; }
    unsigned long obtenerTimestampConversion() const { return tsUltimaConversion; }
    bool          hayModoParasitoGlobal()      const { return modoParasitoGlobal; }

    static const char* nombreEstadoSensor(EstadoSensor estado);
    static const char* nombreEstadoBus(EstadoBus estado);
    static const char* nombreEventoGestor(EventoGestor evento);
    static const char* nombreFamilia(uint8_t codigoFamilia);
};

inline GestorSensores& gestorSensores = GestorSensores::getInstance();