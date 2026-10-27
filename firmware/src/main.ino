// Archivo: ./src/main.ino
#include <Arduino.h>
#include <time.h>
#include "const/Pines.h"
#include "communication/conectividad/usb/PuertoUSB.h"
#include "communication/conectividad/modem/RedInterna.h"
#include "communication/conectividad/modem/RedHost.h"
#include "communication/conectividad/modem/ServidorWiFi.h"
#include "communication/conectividad/modem/Modem.h"
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

// 1. Transceptores de Red y Almacenamiento Físico
PuertoUSB puertoUsb(115200);
RedInterna redAp;
RedHost redSta;
ServidorWiFi servidorWifi(80, 3);
Modem modem(&redAp, &redSta, &servidorWifi);
MicroSD tarjetaSd(PIN_CS_SD);

// 2. Base de Datos del Firmware (SGBD + ClienteORM)
SGBD::MotorSGBD motorSgbd(&tarjetaSd);
ORM::ClienteORM dbOrm(&motorSgbd);

// 3. Pila de Comunicaciones OSI y Planificador Multinúcleo
ContenedorComunicacion pila(&puertoUsb, &servidorWifi, "SECRETO_HMAC_DEFAULT");
Planificador scheduler;

BackendStub backendStub(pila.appRpc);
Contratos::IBackend& backend = backendStub;

// =========================================================================
// TAREA NÚCLEO 1 (APP_CPU): Pila OSI de Tiempo Real y SGBD
// =========================================================================
void tareaComunicacion() {
    DebugOLED::contarCicloScheduler();
    modem.mantenerRed();
    pila.procesarCiclo();
}

// =========================================================================
// TAREAS NÚCLEO 0 (PRO_CPU): Sincronización de Reloj (SNTP / RPC) y Pantalla OLED
// =========================================================================
void tareaSincronizacionPeriodica() {
    // A. Si el RTC interno del ESP32 ya obtuvo hora válida por SNTP, actualizar timestamp
    time_t ahoraRtc = time(nullptr);
    if (ahoraRtc > 1700000000L) {
        gEstadoSistema.ultimoUnixTimestamp = (uint32_t)ahoraRtc;
        strncpy(gEstadoSistema.zonaHoraria, "UTC-3 (SNTP)", sizeof(gEstadoSistema.zonaHoraria) - 1);
        gEstadoSistema.relojSincronizado = true;
    }

    // B. Si hay un cliente autenticado en la Pila OSI, sincronizar también vía RPC
    if (!pila.appRpc.hayClientesActivos()) return;

    const char* motivo = gEstadoSistema.relojSincronizado
                         ? "SYNC_MANTENIMIENTO_30S"
                         : "SYNC_INICIAL_PENDIENTE";

    uint16_t rid = backend.sincronizarReloj(motivo);
    DebugOLED::registrarTx(pila.presentacion.obtenerNodoPreferido(), "SYNC_CLK", rid);
}

void tareaEscribirEnOled() {
    bool usbOk  = pila.sesion.estaAutenticado(0);
    bool wifiOk = pila.sesion.estaAutenticado(1) ||
                  pila.sesion.estaAutenticado(2) ||
                  pila.sesion.estaAutenticado(3);
    DebugOLED::refrescar(usbOk, wifiOk, gEstadoSistema.totalLlamadasRecibidas);
}

void setup() {
    puertoUsb.inicializar();
    DebugOLED::inicializar();

    // 1. Monta la MicroSD (con fix MISO Pull-Up), crea las 10 tablas si no existen,
    //    siembra la clave AES-256 + los 3 usuarios y registra los Skeletons RPC
    ServerExampleStub::registrarStubsServidor(pila.appRpc, pila.presentacion, dbOrm);

    // 2. Configurar conectividad Wi-Fi según /db/wifi.bin o el entorno de compilación
    EsquemaFisicoDB::FilaConfiguracionConectividad cfgRed = {};
    if (dbOrm.conectividad.obtener(cfgRed) &&
        (cfgRed.SSID_STA[0] != '\0' || cfgRed.SSID_AP[0] != '\0')) {
        bool usarAp  = (cfgRed.SSID_AP[0] != '\0');
        bool usarSta = (cfgRed.SSID_STA[0] != '\0');
        modem.configurarRedes(cfgRed.SSID_AP, cfgRed.AP_password, usarAp,
                              cfgRed.SSID_STA, cfgRed.STA_password, usarSta);
    } else {
        #if defined(ENTORNO_WOKWI)
            modem.configurarRedes("", "", false, "Wokwi-GUEST", "", true);
        #else
            // En hardware físico sin red guardada: levanta AP local y deja USB listo al instante
            modem.configurarRedes("UNTDF_ESP32_AP", "12345678", true, "", "", false);
        #endif
    }

    // 3. Si el módem conectó a una red STA con Internet, activar SNTP nativo del ESP32
    if (modem.estaStaConectado()) {
        EsquemaFisicoDB::FilaServidorNTP srvNtp = {};
        if (dbOrm.servidoresNtp.buscarPorId(1, srvNtp)) {
            modem.configurarSNTPNativo(srvNtp.server);
        } else {
            modem.configurarSNTPNativo();
        }
    }

    // 4. Registrar tareas repartidas en los 2 núcleos del ESP-WROOM-32:
    //    · Core 1 (APP_CPU): Pila OSI a máxima velocidad (0ms de espera)
    //    · Core 0 (PRO_CPU): Sincronización de reloj y refresco I2C de la OLED
    scheduler.registrarTarea("COMUNICACION_OSI", tareaComunicacion, 0,       1); // Core 1
    scheduler.registrarTarea("SYNC_RELOJ",       tareaSincronizacionPeriodica, 30000UL, 0); // Core 0
    scheduler.registrarTarea("DEBUG_OLED",       tareaEscribirEnOled,          250UL,   0); // Core 0

    scheduler.iniciarMultinucleo();
}

void loop() {
    scheduler.ejecutarCiclo();
}