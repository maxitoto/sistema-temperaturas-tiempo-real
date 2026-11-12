#pragma once

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../placa/esp32S/pines_nodemcu32s.hpp"
#include "../gestorsensores/EstadoSensor.hpp"

class Buzz {
public:
    static constexpr uint8_t MAX_SLOTS_ALARMA = 6;

private:
    SemaphoreHandle_t mutexBuzz = nullptr;
    bool state = false;
    float (*frec)(float) = nullptr;

    int hz = 0;
    float inicio = 0;
    float duracion = -1;

    // Estado encolado por cada sensor y bus para mantener viva la alarma mientras quede algún problema
    EstadoSensor alarmasPorSlot[MAX_SLOTS_ALARMA];
    bool slotSilenciadoManual[MAX_SLOTS_ALARMA];
    EstadoBus alarmaBusActual = EstadoBus::OK;
    bool busSilenciadoManual = false;
    uint32_t ultimoReboteBotonMs = 0;

    Buzz();
    ~Buzz() = default;

    void _tick();
    void evaluarPrioridadAlarmaLatente();
    static void _task(void* pvParameters);

public:
    static Buzz& getInstance() {
        static Buzz instance;
        return instance;
    }

    Buzz(const Buzz&) = delete;
    Buzz& operator=(const Buzz&) = delete;
    Buzz(Buzz&&) = delete;
    Buzz& operator=(Buzz&&) = delete;

    void silenciarBuzz();
    void sonarBuzz(int frecuencia, float duracion);
    void sonarBuzz(float (*frec)(float), float duracion);
    bool buzzEstaEncendido();

    // Actualiza el estado de los sensores/bus; enciende LED + Buzzer si hay alerta, o los apaga cuando TODOS vuelven a OK
    void actualizarAlarmaDesdeSensores(const EstadoSensor estadosSlots[MAX_SLOTS_ALARMA], EstadoBus estadoBus);

    // Silencia la bocina (por RPC o por botón físico BOOT) manteniendo el LED encendido mientras dure la anomalía
    void silenciarAlarmaActiva();
    bool hayAlarmaFisicaActiva();
};

inline Buzz& buzz = Buzz::getInstance();