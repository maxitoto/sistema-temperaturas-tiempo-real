#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setup() {
    Serial.println("Hola Mundo.");
    delay(250);
    vTaskDelete(nullptr);
}

void loop() {
    // Inaccesible después de eliminar esta tarea.
}