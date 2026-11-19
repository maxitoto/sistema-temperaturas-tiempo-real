#pragma once
#include "tasks/TaskSensores.hpp"
#include "tasks/TaskComunicaciones.hpp"
#include "tasks/TaskOled.hpp"
#include "tasks/TaskAlertasYPersistencia.hpp"
#include "tasks/TaskBiosRTOS.hpp"

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

            // Registrar las 4 tareas en el Supervisor BIOS con sus umbrales máximos permitidos
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskComunicaciones::obtenerTarea(),       6000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskAlertasYPersistencia::obtenerTarea(), 12000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskSensores::obtenerTarea(),             8000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskOled::obtenerTarea(),                 6000);
            Tasks::TaskBiosRTOS::iniciar();
        }
    };

} // namespace RTOS