#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>

void setup() {

    Serial.println("Hola Mundo.");
    std::cout << "hola mundo!" << std::endl; //se escribe literal!
    delay(250);
    vTaskDelete(nullptr);
}

void loop() {
    // Inaccesible después de eliminar esta tarea.
}