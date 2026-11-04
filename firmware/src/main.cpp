#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "connections/CommunicationContainer.hpp"
#include "apis/apiRegistry.hpp"
#include "database/InicializadorSGBD.hpp"
#include "database/EsquemaFisico.hpp"
#include "controllers/ControllerRegistry.hpp"

void setup() {
    // 1. Inicializar almacenamiento MicroSD, SGBD y esquema físico del ORM
    if (iniciarSGBDyORM()) {
        iniciarEsquema();
    }

    // 2. Inicializar contenedor de comunicaciones y registrar las APIs por dominio
    Connections::comm.begin();
    Apis::registrarTodasLasApis();

    // 3. Arrancar todas las tareas y controladores FreeRTOS (Comm, SensorDb, Alarm, Oled)
    Controllers::iniciarTodosLosControladores();

    // 4. Eliminar la tarea de arranque de Arduino (toda la ejecución queda gobernada por FreeRTOS)
    vTaskDelete(nullptr);
}

void loop() {
    // Inaccesible: gobernado por los controladores FreeRTOS.
}