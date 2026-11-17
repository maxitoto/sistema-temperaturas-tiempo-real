#pragma once
#include "tasks/TaskSensores.hpp"
#include "tasks/TaskComunicaciones.hpp"
#include "tasks/TaskOled.hpp"
#include "tasks/TaskAlertasYPersistencia.hpp"

namespace RTOS {

    class GestorRTOS {
    public:
        static void iniciarSistemaCompleto(
            UARTSRuntime& uarts,
            WiFiRuntime& wifi,
            MiddlewareInterface& l1,
            MiddlewareSesion& l2,
            MiddlewareMessage& l3
        ) {
            Tasks::TaskComunicaciones::iniciar(uarts, wifi, l1, l2, l3);
            Tasks::TaskAlertasYPersistencia::iniciar();
            Tasks::TaskSensores::iniciar(l3);
            Tasks::TaskOled::iniciar(uarts, l2);
        }
    };

} // namespace RTOS