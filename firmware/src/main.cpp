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

uint32_t ultimoChequeoMantenimientoMs = 0;
uint32_t ultimoCicloStreamingMs = 0;

// ¿Cómo lo hace?: Inicializa primero la base de datos (comentado), luego abre UART-S (115200, 8E1) y levanta WiFi AP+STA junto con los sockets TCP (8443) y UDP (8266).
void setup() {
    // DbNotRuntime::PhysicalModels::migrateAllPhysicalModels();
    DbRuntime::Orm::runtimeDb.bindToPhysicalCatalog();

    // consulta DB (verificar existencia de catálogo físico; si falla, detener el sistema)

    uartsRuntime.abrirPuertoServidor();

    if (wifiInicializador.iniciarRedDesdeBaseDeDatos()) {
        wifiTcpRuntime.iniciarServidorSeguro();
        wifiUdpRuntime.iniciarSocketStreaming();
    }
}

// ¿Cómo lo hace?: Procesa la entrada de paquetes en tiempo real, verifica timeouts de keepalive y ocio cada 1 segundo, y emite el lote de streaming cada 500 ms solo si hay suscriptores.
void loop() {
    middlewareInterfaceL1.procesarCicloEntrada();

    const uint32_t ahoraMs = millis();

    if (ahoraMs - ultimoChequeoMantenimientoMs >= 1000) {
        ultimoChequeoMantenimientoMs = ahoraMs;
        middlewareInterfaceL1.verificarKeepalivesYExpulsarInactivos();
        middlewareSesionL2.verificarExpiracionSesionesPorOcio();
    }

    if (ahoraMs - ultimoCicloStreamingMs >= 500) {
        ultimoCicloStreamingMs = ahoraMs;
        if (middlewareMessageL3.obtenerMascaraSuscriptores() != 0) {
            HistorialSensoresDTO loteTiempoReal{};
            // Aquí se vuelcan las lecturas actuales de los sensores conectados en loteTiempoReal
            middlewareMessageL3.emitirCicloStreamingSensores(loteTiempoReal);
        }
    }
}