// Archivo: ./src/scheduler/Planificador.h
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <stddef.h>

typedef void (*FuncionTarea)(void);

struct DescriptorTarea {
    const char* nombre = nullptr;
    FuncionTarea fn = nullptr;
    unsigned long intervaloMs = 0;
    unsigned long ultimaEjecucionMs = 0;
    uint8_t nucleo = 1; // 0 = PRO_CPU ( Fondo / Red / OLED ), 1 = APP_CPU ( Pila OSI Tiempo Real )
    bool activa = false;

    DescriptorTarea() = default;
    DescriptorTarea(const char* n, FuncionTarea f, unsigned long iv,
                    unsigned long ult, uint8_t core, bool act)
        : nombre(n), fn(f), intervaloMs(iv), ultimaEjecucionMs(ult), nucleo(core), activa(act) {}
};

class Planificador {
  public:
    static constexpr size_t MAX_TAREAS = 8;

  private:
    DescriptorTarea tareas[MAX_TAREAS];
    size_t totalTareas = 0;
    SemaphoreHandle_t mutexGlobal = nullptr;
    TaskHandle_t handleTareaCore0 = nullptr;

    static void loopNucleo0(void* param) {
        Planificador* self = static_cast<Planificador*>(param);
        for (;;) {
            self->ejecutarCicloNucleo(0);
            vTaskDelay(pdMS_TO_TICKS(5)); // Cede 5ms al driver Wi-Fi/lwIP en Core 0
        }
    }

  public:
    Planificador() {
        mutexGlobal = xSemaphoreCreateMutex();
    }

    // Registra una tarea asignándola al Núcleo 1 (por defecto) o al Núcleo 0
    bool registrarTarea(const char* nombre, FuncionTarea fn,
                        unsigned long intervaloMs, uint8_t nucleo = 1) {
        if (!fn || totalTareas >= MAX_TAREAS) return false;
        tareas[totalTareas++] = { nombre, fn, intervaloMs, millis(), nucleo, true };
        return true;
    }

    // Lanza el hilo paralelo en el Núcleo 0 (PRO_CPU) si hay tareas asignadas a él
    void iniciarMultinucleo(uint32_t stackBytes = 4096, UBaseType_t prioridad = 1) {
        if (handleTareaCore0 != nullptr) return;
        bool hayEnCore0 = false;
        for (size_t i = 0; i < totalTareas; i++) {
            if (tareas[i].activa && tareas[i].nucleo == 0) {
                hayEnCore0 = true;
                break;
            }
        }
        if (hayEnCore0) {
            xTaskCreatePinnedToCore(
                loopNucleo0,
                "SchedulerCore0",
                stackBytes,
                this,
                prioridad,
                &handleTareaCore0,
                0 // Anclado físicamente al Core 0 (PRO_CPU)
            );
        }
    }

    void ejecutarCicloNucleo(uint8_t nucleoObjetivo) {
        unsigned long ahora = millis();
        for (size_t i = 0; i < totalTareas; i++) {
            if (!tareas[i].activa || tareas[i].nucleo != nucleoObjetivo) continue;
            if (tareas[i].intervaloMs == 0 ||
                (ahora - tareas[i].ultimaEjecucionMs >= tareas[i].intervaloMs)) {
                tareas[i].ultimaEjecucionMs = ahora;
                if (mutexGlobal) xSemaphoreTake(mutexGlobal, portMAX_DELAY);
                tareas[i].fn();
                if (mutexGlobal) xSemaphoreGive(mutexGlobal);
                yield();
            }
        }
    }

    // Llamado desde loop() en el Núcleo 1 (APP_CPU)
    void ejecutarCiclo() {
        ejecutarCicloNucleo(1);
    }
};