#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include "database/InicializadorSGBD.hpp"
#include "database/EsquemaFisico.hpp"


void setup() {
    delay(1000);

    iniciarSGBDyORM(); // 1. Inicia la base de datos (SD + SGBD) y el ORM
    iniciarEsquema();  // 2. Ejecuta el modelo físico (diccionario de datos)

    Serial.println("Hola Mundo.");
    std::cout << "hola mundo!" << std::endl; //se escribe literal!
    delay(250);
    vTaskDelete(nullptr);
}

void loop() {
    // Inaccesible después de eliminar esta tarea.
}