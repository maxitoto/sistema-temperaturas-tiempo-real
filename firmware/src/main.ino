// Archivo: ./src/main.ino
#include <Arduino.h>
#include "plugins/conectividad/usb/PuertoUSB.h"
#include "plugins/conectividad/modem/RedInterna.h"
#include "plugins/conectividad/modem/RedHost.h"
#include "plugins/conectividad/modem/ServidorTCP.h"
#include "plugins/conectividad/modem/Modem.h"

#include "communication/ContenedorComunicacion.h"

#include "state/EstadoSistema.h"
#include "scheduler/Planificador.h"

#include "stubs/contratos.h"
#include "stubs/serverExample.h"
#include "stubs/clientExample.h"

#include "debug/DebugOLED.h"

EstadoSistema gEstadoSistema;

PuertoUSB puertoUsb(115200);
RedInterna redAp;
RedHost redSta;
ServidorTCP servidorTcp(80, 3);
Modem modem(&redAp, &redSta, &servidorTcp);

ContenedorComunicacion pila(&puertoUsb, &servidorTcp, "SECRETO_HMAC_DEFAULT");
Planificador scheduler;

BackendStub backendStub(pila.appRpc);
Contratos::IBackend& backend = backendStub;

void tareaComunicacion() {
    DebugOLED::contarCicloScheduler();
    modem.mantenerRed();
    pila.procesarCiclo();
}

void tareaSincronizacionPeriodica() {
    if (!pila.appRpc.hayClientesActivos()) return;

    const char* motivo = gEstadoSistema.relojSincronizado
                         ? "SYNC_MANTENIMIENTO_30S"
                         : "SYNC_INICIAL_PENDIENTE";

    uint16_t rid = backend.sincronizarReloj(motivo);
    DebugOLED::registrarTx(pila.presentacion.obtenerNodoPreferido(), "SYNC_CLK", rid);
}

void tareaEscribirEnOled() {
    bool usbOk  = pila.sesion.estaAutenticado(0);
    bool wifiOk = pila.sesion.estaAutenticado(1) || pila.sesion.estaAutenticado(2);
    DebugOLED::refrescar(usbOk, wifiOk, gEstadoSistema.totalLlamadasRecibidas);
}

void setup() {
    puertoUsb.inicializar();
    DebugOLED::inicializar();
    modem.configurarRedes("", "", false, "Wokwi-GUEST", "", true);

    ServerExampleStub::registrarStubsServidor(pila.appRpc, pila.presentacion);

    scheduler.registrarTarea("COMUNICACION_OSI", tareaComunicacion, 0);
    scheduler.registrarTarea("SYNC_RELOJ", tareaSincronizacionPeriodica, 30000UL);
    scheduler.registrarTarea("DEBUG_OLED", tareaEscribirEnOled, 250UL);
}

void loop() {
    scheduler.ejecutarCiclo();
}