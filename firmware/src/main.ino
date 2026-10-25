// Archivo: ./src/main.ino
#include <Arduino.h>
#include "const/Pines.h"
#include "plugins/conectividad/usb/PuertoUSB.h"
#include "plugins/conectividad/modem/RedInterna.h"
#include "plugins/conectividad/modem/RedHost.h"
#include "plugins/conectividad/modem/ServidorTCP.h"
#include "plugins/conectividad/modem/Modem.h"
#include "plugins/microsd/MicroSD.h"

#include "communication/ContenedorComunicacion.h"

#include "database/sgbd/MotorSGBD.h"
#include "database/orm/ClienteORM.h"
#include "database/repositories/FirmwareRepositories.h"

#include "state/EstadoSistema.h"
#include "scheduler/Planificador.h"

#include "stubs/contratos.h"
#include "stubs/serverExample.h"
#include "stubs/clientExample.h"

#include "debug/DebugOLED.h"

EstadoSistema gEstadoSistema;

// 1. Plugins de Hardware (Capa Física y Almacenamiento)
PuertoUSB puertoUsb(115200);
RedInterna redAp;
RedHost redSta;
ServidorTCP servidorTcp(80, 3);
Modem modem(&redAp, &redSta, &servidorTcp);
MicroSD tarjetaSd(PIN_CS_SD);

// 2. Base de Datos del Firmware (SGBD + ClienteORM)
SGBD::MotorSGBD motorSgbd(&tarjetaSd);
ORM::ClienteORM dbOrm(&motorSgbd);

// 3. Pila de Comunicaciones OSI y Planificador
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

    // 1. Monta la MicroSD, ejecuta el DDL de las 10 tablas, vincula Repositorios,
    //    siembra datos iniciales y registra los Skeletons RPC y la seguridad
    ServerExampleStub::registrarStubsServidor(pila.appRpc, pila.presentacion, dbOrm);

    // 2. Si hay credenciales de red guardadas en /db/wifi.bin las usa; si no, usa Wokwi-GUEST
    EsquemaFisicoDB::FilaConfiguracionConectividad cfgRed = {};
    if (dbOrm.conectividad.obtener(cfgRed) &&
        (cfgRed.SSID_STA[0] != '\0' || cfgRed.SSID_AP[0] != '\0')) {
        bool usarAp  = (cfgRed.SSID_AP[0] != '\0');
        bool usarSta = (cfgRed.SSID_STA[0] != '\0');
        modem.configurarRedes(cfgRed.SSID_AP, cfgRed.AP_password, usarAp,
                              cfgRed.SSID_STA, cfgRed.STA_password, usarSta);
    } else {
        modem.configurarRedes("", "", false, "Wokwi-GUEST", "", true);
    }

    // 3. Registrar tareas cooperativas
    scheduler.registrarTarea("COMUNICACION_OSI", tareaComunicacion, 0);
    scheduler.registrarTarea("SYNC_RELOJ", tareaSincronizacionPeriodica, 30000UL);
    scheduler.registrarTarea("DEBUG_OLED", tareaEscribirEnOled, 250UL);
}

void loop() {
    scheduler.ejecutarCiclo();
}