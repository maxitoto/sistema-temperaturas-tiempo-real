#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "EstadoSensor.hpp"
#include "Sensor.hpp"
#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class GestorSensores {
public:
    static constexpr uint8_t MAX_SENSORES       = 6;
    static constexpr uint8_t REINTENTOS_LECTURA = 3;

private:
    static constexpr uint8_t CMD_CONVERT_T         = 0x44;
    static constexpr uint8_t CMD_READ_SCRATCHPAD   = 0xBE;
    static constexpr uint8_t CMD_WRITE_SCRATCHPAD  = 0x4E;
    static constexpr uint8_t CMD_COPY_SCRATCHPAD   = 0x48;
    static constexpr uint8_t CMD_RECALL_EEPROM     = 0xB8;
    static constexpr uint8_t CMD_READ_POWER_SUPPLY = 0xB4;

    mutable SemaphoreHandle_t mutexSensores = nullptr;
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

    class ScopedSensorLock {
    private:
        SemaphoreHandle_t mtx;
    public:
        explicit ScopedSensorLock(SemaphoreHandle_t m) : mtx(m) {
            if (mtx) xSemaphoreTakeRecursive(mtx, portMAX_DELAY);
        }
        ScopedSensorLock(const ScopedSensorLock&) = delete;
        ScopedSensorLock& operator=(const ScopedSensorLock&) = delete;
        ~ScopedSensorLock() {
            if (mtx) xSemaphoreGiveRecursive(mtx);
        }
    };

    int8_t buscarSlotPorROM(const DeviceAddress rom) const;
    int8_t buscarSlotPorMACInterno(const char* macHex) const;
    int8_t buscarSlotPorIdInterno(int32_t idSensorDB) const;
    EstadoSensor evaluarFalloScratchpad(const uint8_t* pad) const;
    void reevaluarAlarmaSlotInterno(Sensor& s);

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

    // --- 1. CICLO DE HARDWARE 1-WIRE (Llamado por TaskSensores) ---
    EstadoBus verificarEstadoElectricoBus();
    uint8_t escanearYAsignarSensores(uint8_t resolucionGlobal = 12, bool guardarEnEeprom = false);
    bool verificarCambiosEnCaliente();

    bool solicitarConversionGlobal(bool bloqueante = true);
    bool solicitarConversionPorSlot(uint8_t slot, bool bloqueante = true);
    bool estaConversionCompleta();
    uint16_t tiempoConversionSegunResolucion(uint8_t bits) const;

    EstadoSensor actualizarLecturaSlot(uint8_t slot);
    void actualizarTodosLosSlots();
    void consumirEvento() { ScopedSensorLock lk(mutexSensores); ultimoEvento = EventoGestor::NINGUNO; }

    // --- 2. CONSULTAS FÁCILES Y THREAD-SAFE (Por MAC, por ID, por Slot o Lote Completo) ---
    bool obtenerSensorPorMAC(const char* macHex, SnapshotSensor& outSnapshot) const;
    bool obtenerSensorPorROM(const uint8_t rom8[8], SnapshotSensor& outSnapshot) const;
    bool obtenerSensorPorId(int32_t idSensorDB, SnapshotSensor& outSnapshot) const;
    bool obtenerSensorPorSlot(uint8_t slot, SnapshotSensor& outSnapshot) const;

    uint8_t obtenerSensoresConectados(SnapshotSensor outLista[MAX_SENSORES]) const;
    uint8_t obtenerMacsConectadas(char outMacs[MAX_SENSORES][17]) const;
    ResumenSistemaSensores obtenerEstadoSistema() const;
    void obtenerEstadosParaAlarma(EstadoSensor outEstados[MAX_SENSORES], EstadoBus& outEstadoBus) const;

    // Extrae cualquier transición de estado ocurrida (ej. OK -> ALARMA_SUPERIOR) para guardarla en EventoSensor
    uint8_t extraerEventosCambioEstado(EventoCambioSensor outEventos[MAX_SENSORES]);

    // --- 3. MUTADORES THREAD-SAFE CON PERSISTENCIA AUTOMÁTICA EN DB ---
    bool configurarOffsetPorMAC(const char* macHex, float nuevoOffsetC);
    bool configurarOffsetPorId(int32_t idSensorDB, float nuevoOffsetC);
    bool configurarOffsetPorROM(const uint8_t* rom, float offsetC);
    bool configurarOffsetSlot(uint8_t slot, float offsetC);

    // Calibración de 2 puntos ingresando lo que midió el sensor en hielo (3 min) y en agua hirviendo (3 min)
    bool calibrarPuntosPorMAC(const char* macHex, float tempMedidaHirviendo, float tempMedidaHielo, float offsetManual = 0.0f);

    bool configurarUmbralesPorMAC(const char* macHex, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarUmbralesPorId(int32_t idSensorDB, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarAlarmasSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarAlarmasPorROM(const uint8_t* rom, float thAltoC, float tlBajoC, bool guardarEeprom = false);
    bool configurarUmbralesSlot(uint8_t slot, float thAltoC, float tlBajoC, bool guardarEeprom = false) {
        return configurarAlarmasSlot(slot, thAltoC, tlBajoC, guardarEeprom);
    }

    bool configurarResolucionSlot(uint8_t slot, uint8_t bits, bool guardarEeprom = false);
    bool guardarEepromSlot(uint8_t slot);
    bool restaurarEepromSlot(uint8_t slot);
    uint8_t procesarBusquedaAlarmasHardware();



    EstadoBus     obtenerEstadoBus()           const { ScopedSensorLock lk(mutexSensores); return estadoBus; }
    EventoGestor  obtenerUltimoEvento()        const { ScopedSensorLock lk(mutexSensores); return ultimoEvento; }
    uint8_t       obtenerTotalAsignados()      const { ScopedSensorLock lk(mutexSensores); return totalAsignados; }
    uint8_t       obtenerTotalEnBus()          const { ScopedSensorLock lk(mutexSensores); return totalDetectadosBus; }
    unsigned long obtenerTimestampConversion() const { ScopedSensorLock lk(mutexSensores); return tsUltimaConversion; }
    bool          hayModoParasitoGlobal()      const { ScopedSensorLock lk(mutexSensores); return modoParasitoGlobal; }

    static const char* nombreEstadoSensor(EstadoSensor estado);
    static const char* nombreEstadoBus(EstadoBus estado);
    static const char* nombreEventoGestor(EventoGestor evento);
    static const char* nombreFamilia(uint8_t codigoFamilia);
};

inline GestorSensores& gestorSensores = GestorSensores::getInstance();