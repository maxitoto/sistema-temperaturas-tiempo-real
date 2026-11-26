#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "frecuencias.hpp"
#include "Buzz.hpp"

#include <iostream>

void setup() {
    Buzz& buzz = Buzz::getInstance();

    buzz.sonarBuzz(440, 0.5f);

    vTaskDelay(pdMS_TO_TICKS(1000));

    buzz.sonarBuzz(sirena, 5.0f);

    vTaskDelay(pdMS_TO_TICKS(6000));

    buzz.sonarBuzz(barrido, 3.0f);

    vTaskDelay(pdMS_TO_TICKS(4000));

    buzz.sonarBuzz(pitpit, 3.0f);

    vTaskDelete(nullptr);
}

void loop() {
  // Inaccesible después de eliminar esta tarea.
}   