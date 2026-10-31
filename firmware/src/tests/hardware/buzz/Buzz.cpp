#include "Buzz.hpp"

#include <esp32-hal-ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <esp32-hal.h>

Buzz::Buzz() {
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
    if (!state) return;

    float t = (millis() / 1000.0f) - inicio;

    if (duracion > 0 && t >= duracion) {
        silenciarBuzz();
        return;
    }

    int frecuencia = frec
        ? static_cast<int>(frec(t))
        : hz;

    if (frecuencia > 0) {
        ledcWriteTone(Pin::PIN_BUZZ, frecuencia);
    }
}

void Buzz::silenciarBuzz() {
    ledcWriteTone(Pin::PIN_BUZZ, 0);

    frec = nullptr;
    hz = 0;
    duracion = -1;
    state = false;
}

void Buzz::sonarBuzz(int frecuencia, float duracion) {
    frec = nullptr;
    hz = frecuencia;

    inicio = millis() / 1000.0f;
    this->duracion = duracion;

    state = true;
}

void Buzz::sonarBuzz(float (*funcion)(float), float duracion) {
    frec = funcion;
    hz = 0;

    inicio = millis() / 1000.0f;
    this->duracion = duracion;

    state = true;
}

bool Buzz::buzzEstaEncendido() {
    return state;
}