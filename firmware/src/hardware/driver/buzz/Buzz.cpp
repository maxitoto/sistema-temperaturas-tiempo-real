#include "Buzz.hpp"
#include "frecuencias.hpp"
#include "../led/Led.hpp"

#include <Arduino.h>
#include <esp32-hal-ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

Buzz::Buzz() {
    mutexBuzz = xSemaphoreCreateMutex();
    for (uint8_t i = 0; i < MAX_SLOTS_ALARMA; ++i) {
        alarmasPorSlot[i] = EstadoSensor::OK;
        slotSilenciadoManual[i] = false;
    }

    pinMode(Pin::PIN_BOTON_BOOT, INPUT_PULLUP);
    ledcAttach(Pin::PIN_BUZZ, 5000, 10);

    xTaskCreate(
        _task,
        "BuzzTask",
        2048,
        this,
        1,
        nullptr
    );
}

void Buzz::_task(void* pvParameters) {
    Buzz* self = static_cast<Buzz*>(pvParameters);

    while (true) {
        self->_tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Buzz::_tick() {
    // Lectura no bloqueante del botón físico BOOT (GPIO 0) con antirrebote de 250 ms
    if (digitalRead(Pin::PIN_BOTON_BOOT) == LOW) {
        uint32_t ahora = millis();
        if (ahora - ultimoReboteBotonMs > 250) {
            ultimoReboteBotonMs = ahora;
            silenciarAlarmaActiva();
        }
    }

    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(5)) != pdTRUE) return;

    if (!state) {
        xSemaphoreGive(mutexBuzz);
        return;
    }

    float t = (millis() / 1000.0f) - inicio;

    if (duracion > 0 && t >= duracion) {
        ledcWriteTone(Pin::PIN_BUZZ, 0);
        frec = nullptr;
        hz = 0;
        duracion = -1;
        state = false;
        // Si terminó un sonido corto (ej. MuteAck) y aún hay una alarma no silenciada, la retoma
        evaluarPrioridadAlarmaLatente();
        xSemaphoreGive(mutexBuzz);
        return;
    }

    int frecuencia = frec ? static_cast<int>(frec(t)) : hz;
    xSemaphoreGive(mutexBuzz);

    if (frecuencia > 0) {
        ledcWriteTone(Pin::PIN_BUZZ, frecuencia);
    } else {
        ledcWriteTone(Pin::PIN_BUZZ, 0);
    }
}

void Buzz::evaluarPrioridadAlarmaLatente() {
    bool hayCriticaSinSilenciar = false;
    bool hayMediaSinSilenciar   = false;
    bool hayCualquierAlarma     = false;

    if (alarmaBusActual != EstadoBus::OK) {
        hayCualquierAlarma = true;
        if (!busSilenciadoManual) {
            hayMediaSinSilenciar = true;
        }
    }

    for (uint8_t i = 0; i < MAX_SLOTS_ALARMA; ++i) {
        EstadoSensor e = alarmasPorSlot[i];
        if (e == EstadoSensor::OK || e == EstadoSensor::SLOT_VACIO) {
            continue;
        }
        hayCualquierAlarma = true;
        if (!slotSilenciadoManual[i]) {
            if (e == EstadoSensor::ALARMA_SUPERIOR || e == EstadoSensor::ALARMA_INFERIOR) {
                hayCriticaSinSilenciar = true;
            } else {
                hayMediaSinSilenciar = true;
            }
        }
    }

    if (hayCualquierAlarma) {
        Led::getInstance().encenderLed();
    } else {
        Led::getInstance().apagarLed();
    }

    if (hayCriticaSinSilenciar) {
        if (frec != &sirenaCritica || !state) {
            frec = &sirenaCritica;
            hz = 0;
            inicio = millis() / 1000.0f;
            duracion = -1.0f;
            state = true;
        }
    } else if (hayMediaSinSilenciar) {
        if (frec != &alertaTecnicaMedia || !state) {
            frec = &alertaTecnicaMedia;
            hz = 0;
            inicio = millis() / 1000.0f;
            duracion = -1.0f;
            state = true;
        }
    } else if (duracion < 0) {
        // Solo apaga el buzzer si estaba en modo alarma continua (-1.0f)
        ledcWriteTone(Pin::PIN_BUZZ, 0);
        frec = nullptr;
        hz = 0;
        state = false;
    }
}

void Buzz::actualizarAlarmaDesdeSensores(const EstadoSensor estadosSlots[MAX_SLOTS_ALARMA], EstadoBus estadoBus) {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(20)) != pdTRUE) return;

    if (estadoBus == EstadoBus::OK) {
        busSilenciadoManual = false;
    } else if (alarmaBusActual == EstadoBus::OK) {
        busSilenciadoManual = false;
    }
    alarmaBusActual = estadoBus;

    for (uint8_t i = 0; i < MAX_SLOTS_ALARMA; ++i) {
        EstadoSensor nuevo = estadosSlots[i];
        EstadoSensor previo = alarmasPorSlot[i];

        if (nuevo == EstadoSensor::OK || nuevo == EstadoSensor::SLOT_VACIO) {
            slotSilenciadoManual[i] = false;
        } else if (nuevo != previo) {
            // Si el sensor entró en un estado de fallo nuevo o distinto, reactiva sonido para este slot
            slotSilenciadoManual[i] = false;
        }
        alarmasPorSlot[i] = nuevo;
    }

    evaluarPrioridadAlarmaLatente();
    xSemaphoreGive(mutexBuzz);
}

void Buzz::silenciarAlarmaActiva() {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(20)) != pdTRUE) return;

    for (uint8_t i = 0; i < MAX_SLOTS_ALARMA; ++i) {
        if (alarmasPorSlot[i] != EstadoSensor::OK && alarmasPorSlot[i] != EstadoSensor::SLOT_VACIO) {
            slotSilenciadoManual[i] = true;
        }
    }
    if (alarmaBusActual != EstadoBus::OK) {
        busSilenciadoManual = true;
    }

    // Emite confirmación corta (0.10 s) y luego queda en silencio mientras no ocurra otra falla nueva
    frec = &sonidoMuteAck;
    hz = 0;
    inicio = millis() / 1000.0f;
    duracion = 0.10f;
    state = true;

    xSemaphoreGive(mutexBuzz);
}

bool Buzz::hayAlarmaFisicaActiva() {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    bool activa = (alarmaBusActual != EstadoBus::OK);
    for (uint8_t i = 0; i < MAX_SLOTS_ALARMA && !activa; ++i) {
        if (alarmasPorSlot[i] != EstadoSensor::OK && alarmasPorSlot[i] != EstadoSensor::SLOT_VACIO) {
            activa = true;
        }
    }
    xSemaphoreGive(mutexBuzz);
    return activa;
}

void Buzz::silenciarBuzz() {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(20)) != pdTRUE) return;
    ledcWriteTone(Pin::PIN_BUZZ, 0);
    frec = nullptr;
    hz = 0;
    duracion = -1;
    state = false;
    xSemaphoreGive(mutexBuzz);
}

void Buzz::sonarBuzz(int frecuencia, float dur) {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(20)) != pdTRUE) return;
    frec = nullptr;
    hz = frecuencia;
    inicio = millis() / 1000.0f;
    this->duracion = dur;
    state = true;
    xSemaphoreGive(mutexBuzz);
}

void Buzz::sonarBuzz(float (*funcion)(float), float dur) {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(20)) != pdTRUE) return;
    frec = funcion;
    hz = 0;
    inicio = millis() / 1000.0f;
    this->duracion = dur;
    state = true;
    xSemaphoreGive(mutexBuzz);
}

bool Buzz::buzzEstaEncendido() {
    if (xSemaphoreTake(mutexBuzz, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    bool s = state;
    xSemaphoreGive(mutexBuzz);
    return s;
}