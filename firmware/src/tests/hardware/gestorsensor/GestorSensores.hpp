// Archivo: src/tests/hardware/gestorsensor/GestorSensores.hpp
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "EstadoSensor.hpp"
#include "Sensor.hpp"
#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

struct DiagnosticoSlot {
    bool activo = false;
    uint8_t indiceSlot = 0;
    DeviceAddress rom = {0};
    char romHex[17] = "0000000000000000";
    EstadoSensor estado = EstadoSensor::SLOT_VACIO;
    float tempCelsius = DEVICE_DISCONNECTED_C;
    int32_t tempRaw = DEVICE_DISCONNECTED_RAW;
    uint8_t resolucionBits = 12;
    bool alimentacionParasita = false;
    int8_t alarmaAltaC = 0;
    int8_t alarmaBajaC = 0;
    bool enAlarma = false;
    int16_t datoUsuario = 0;
    uint8_t ultimoScratchpad[9] = {0};
};

class GestorSensores {
public:
    static constexpr uint8_t MAX_SENSORES = 6;
    static constexpr uint8_t REINTENTOS_LECTURA = 3;

private:
    OneWire bus;
    DallasTemperature driver;

    // Las 6 instancias de Sensor administradas por el gestor
    Sensor sensores[MAX_SENSORES];
    DiagnosticoSlot slots[MAX_SENSORES];

    uint8_t totalDetectadosBus = 0;
    uint8_t totalAsignados = 0;
    unsigned long tsUltimaConversion = 0;

    GestorSensores();
    ~GestorSensores() = default;

    static void formatearRomHex(const DeviceAddress rom, char* salida17B);
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

    // --- 1. Inicialización, Bus y Descubrimiento (Hasta 6 Sensores) ---
    EstadoBus verificarEstadoElectricoBus();
    uint8_t escanearYAsignarSensores(uint8_t resolucionGlobal = 12, bool guardarEnEeprom = true);
    bool verificarCambiosEnCaliente(); // Usa verifyDeviceCount()

    // --- 2. Conversión y Adquisición con Diagnóstico Completo ---
    bool solicitarConversionGlobal(bool bloqueante = true);
    bool solicitarConversionPorSlot(uint8_t slot, bool bloqueante = true);
    bool estaConversionCompleta();
    uint16_t tiempoConversionSegunResolucion(uint8_t bits) const;

    EstadoSensor actualizarLecturaSlot(uint8_t slot);
    void actualizarTodosLosSlots();

    // --- 3. Configuración, Alarmas Hardware y EEPROM por Slot ---
    bool configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom = false);
    bool configurarOffsetPorROM(const uint8_t* rom, float offsetC);
    bool configurarOffsetSlot(uint8_t slot, float offsetC);
    bool configurarAlarmasSlot(uint8_t slot, int8_t thAltoC, int8_t tlBajoC, bool guardarEeprom = false);
    bool guardarEepromSlot(uint8_t slot);
    bool restaurarEepromSlot(uint8_t slot);
    bool escribirIdUsuarioSlot(uint8_t slot, int16_t idCustom);
    uint8_t procesarBusquedaAlarmasHardware();

    Sensor &obtenerSensor(uint8_t slot);

    // --- 4. Accesores de las 6 Instancias y Diagnósticos ---
    const DiagnosticoSlot& obtenerDiagnostico(uint8_t slot) const;
    uint8_t obtenerTotalAsignados() const { return totalAsignados; }
    uint8_t obtenerTotalEnBus() const { return totalDetectadosBus; }
    bool hayModoParasitoGlobal();

    static const char* nombreEstado(EstadoSensor estado);
    static const char* nombreFamilia(uint8_t codigoFamilia);

    unsigned long obtenerTimestampConversion() const { return tsUltimaConversion; }
};

inline GestorSensores& gestorSensores = GestorSensores::getInstance();