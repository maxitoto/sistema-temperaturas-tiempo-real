#include <Arduino.h>

#include "connections/middlewares/interfaces/UART-S/UARTSRuntime.hpp"
#include "connections/middlewares/interfaces/WiFi/WiFiBase/WiFiInicializador.hpp"
#include "connections/middlewares/interfaces/WiFi/WiFiBase/WiFiRuntime.hpp"
#include "connections/middlewares/interfaces/WiFi/WiFiTcpTls/WiFiTcpTlsRuntime.hpp"
#include "connections/middlewares/interfaces/WiFi/WiFiUdpPlain/WiFiUdpPlainRuntime.hpp"
#include "connections/middlewares/middleware_message.hpp"
#include "connections/middlewares/middleware_sesion.hpp"
#include "connections/middlewares/middleware_intereface.hpp"

#include "database/DB_DDL/models/PhysicalModelsDefinition.hpp"
#include "database/DB_DQL_DML/orm/DjangoManager.hpp"
#include "rtos/GestorRTOS.hpp"

// Instancias globales de transporte (L0/L1 físico)
UARTSRuntime uartsRuntime(Serial);
WiFiInicializador wifiInicializador;
WiFiRuntime wifiRuntime(wifiInicializador);
WiFiTcpTlsRuntime wifiTcpRuntime;
WiFiUdpPlainRuntime wifiUdpRuntime;

// Instancias globales de los 3 dominios de middlewares (L3, L2 y L1)
MiddlewareMessage middlewareMessageL3;
MiddlewareSesion middlewareSesionL2(middlewareMessageL3);
MiddlewareInterface middlewareInterfaceL1(
    uartsRuntime,
    wifiRuntime,
    wifiTcpRuntime, 
    wifiUdpRuntime,
    middlewareSesionL2,
    middlewareMessageL3
);

void setup() {
    // DbNotRuntime::PhysicalModels::migrateAllPhysicalModels();
    DbRuntime::Orm::runtimeDb.bindToPhysicalCatalog();

    // consulta DB (verificar existencia de catálogo físico; si falla, detener el sistema)

    uartsRuntime.abrirPuertoServidor();

    if (wifiInicializador.iniciarRedDesdeBaseDeDatos()) {
        wifiTcpRuntime.iniciarServidorSeguro();
        wifiUdpRuntime.iniciarSocketStreaming();
    }

    // Lanza las 4 tareas definitivas de FreeRTOS (Alertas/DB, Sensores, OLED y Comunicaciones)
    RTOS::GestorRTOS::iniciarSistemaCompleto(
        uartsRuntime,
        wifiRuntime,
        middlewareInterfaceL1,
        middlewareSesionL2,
        middlewareMessageL3
    );
}

void loop() {
    // El trabajo pesado vive en las 4 tareas dedicadas de FreeRTOS; cedemos el hilo base de Arduino
    vTaskDelay(pdMS_TO_TICKS(1000));
}