#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "connections/CommunicationContainer.hpp"
#include "apis/apiRegistry.hpp"
#include "database/InicializadorSGBD.hpp"
#include "database/EsquemaFisico.hpp"
#include "controllers/ControllerRegistry.hpp"

void setup() {

    Connections::comm.begin();
    Apis::registrarTodasLasApis();
    // 2. SD, SGBD y esquema
    if (iniciarSGBDyORM()) {
        iniciarEsquema();
    }

    // 3. Arrancar todas las tareas y controladores FreeRTOS (Comm, SensorDb, Alarm, Oled)
    Controllers::iniciarTodosLosControladores();

    // 4. Eliminar la tarea de arranque de Arduino (toda la ejecución queda gobernada por FreeRTOS)
    vTaskDelete(nullptr);
}

void loop() {
    // Inaccesible: gobernado por los controladores FreeRTOS.
}