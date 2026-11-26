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
            ColaTrabajoRPC& colaRpc,
            MiddlewareInterface& l1,
            MiddlewareSesion& l2,
            MiddlewareMessage& l3
        ) {
            Tasks::TaskComunicaciones::iniciar(uarts, wifi, l1);
            Tasks::TaskAlertasYPersistencia::iniciar(colaRpc, l2, l3);
            Tasks::TaskSensores::iniciar(l3);
            Tasks::TaskOled::iniciar(uarts, l2);

            // Timeouts con margen real para SMTP/TLS (TaskAlertasDB) y conversiones 1-Wire (TaskSensores)
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskComunicaciones::obtenerTarea(),       10000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskAlertasYPersistencia::obtenerTarea(), 45000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskSensores::obtenerTarea(),             15000);
            Tasks::TaskBiosRTOS::registrarTareaVigilada(Tasks::TaskOled::obtenerTarea(),                 10000);
            Tasks::TaskBiosRTOS::iniciar();
        }
    };

} // namespace RTOS