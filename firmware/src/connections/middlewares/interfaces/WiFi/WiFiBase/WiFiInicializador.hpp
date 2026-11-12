#ifndef WIFI_INICIALIZADOR_H
#define WIFI_INICIALIZADOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <string.h>

struct EstadoRedWiFi {
    char ipPuntoAcceso[16];
    char ipEstacionLAN[16];
    bool puntoAccesoActivo;
    bool conectadoARouter;
};

class WiFiInicializador {
public:
    static constexpr uint32_t TIMEOUT_CONEXION_STA_MS = 15000;
    inline static WiFiInicializador* instanciaActiva = nullptr;

private:
    EstadoRedWiFi estadoActual{};
    // Credenciales por defecto de tu router (WiFi STA) y del AP del ESP32:
    char ssidStaCache[64]  = "Zhone_96F8";
    char claveStaCache[64] = "Ushuaia30";
    char ssidApCache[64]   = "ESP32-SecureAP";
    char claveApCache[64]  = "ESP32SecureAP1234";

public:
    WiFiInicializador() {
        memset(&estadoActual, 0, sizeof(EstadoRedWiFi));
        instanciaActiva = this;
    }

    bool iniciarRedDesdeBaseDeDatos() {
        // consulta DB

        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);

        bool apLevantado = false;
        if (strlen(claveApCache) >= 8) {
            apLevantado = WiFi.softAP(ssidApCache, claveApCache);
        } else {
            apLevantado = WiFi.softAP(ssidApCache);
        }

        estadoActual.puntoAccesoActivo = apLevantado;
        if (apLevantado) {
            IPAddress ipAp = WiFi.softAPIP();
            snprintf(estadoActual.ipPuntoAcceso, sizeof(estadoActual.ipPuntoAcceso),
                     "%u.%u.%u.%u", ipAp[0], ipAp[1], ipAp[2], ipAp[3]);
        } else {
            estadoActual.ipPuntoAcceso[0] = '\0';
        }

        if (strlen(ssidStaCache) > 0) {
            reconectarEstacionRouter();
        } else {
            estadoActual.conectadoARouter = false;
            estadoActual.ipEstacionLAN[0] = '\0';
        }

        return estadoActual.puntoAccesoActivo;
    }

    bool actualizarCredencialesYReconectar(
        const char* nuevoSsidSta,
        const char* nuevaClaveSta,
        const char* nuevoSsidAp,
        const char* nuevaClaveAp
    ) {
        if (nuevoSsidSta != nullptr) {
            strncpy(ssidStaCache, nuevoSsidSta, sizeof(ssidStaCache) - 1);
            ssidStaCache[sizeof(ssidStaCache) - 1] = '\0';
        }
        if (nuevaClaveSta != nullptr) {
            strncpy(claveStaCache, nuevaClaveSta, sizeof(claveStaCache) - 1);
            claveStaCache[sizeof(claveStaCache) - 1] = '\0';
        }
        if (nuevoSsidAp != nullptr && nuevoSsidAp[0] != '\0') {
            strncpy(ssidApCache, nuevoSsidAp, sizeof(ssidApCache) - 1);
            ssidApCache[sizeof(ssidApCache) - 1] = '\0';
        }
        if (nuevaClaveAp != nullptr && strlen(nuevaClaveAp) >= 8) {
            strncpy(claveApCache, nuevaClaveAp, sizeof(claveApCache) - 1);
            claveApCache[sizeof(claveApCache) - 1] = '\0';
        }
        return iniciarRedDesdeBaseDeDatos();
    }

    bool reconectarEstacionRouter() {
        if (strlen(ssidStaCache) == 0) {
            estadoActual.conectadoARouter = false;
            estadoActual.ipEstacionLAN[0] = '\0';
            return false;
        }

        WiFi.begin(ssidStaCache, claveStaCache);
        const uint32_t inicioMs = millis();

        while (WiFi.status() != WL_CONNECTED && (millis() - inicioMs) < TIMEOUT_CONEXION_STA_MS) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            estadoActual.conectadoARouter = true;
            IPAddress ipSta = WiFi.localIP();
            snprintf(estadoActual.ipEstacionLAN, sizeof(estadoActual.ipEstacionLAN),
                     "%u.%u.%u.%u", ipSta[0], ipSta[1], ipSta[2], ipSta[3]);
            return true;
        }

        estadoActual.conectadoARouter = false;
        estadoActual.ipEstacionLAN[0] = '\0';
        return false;
    }

    void actualizarEstadoEnCaliente(bool staConectado) {
        estadoActual.conectadoARouter = staConectado;
        if (staConectado) {
            IPAddress ipSta = WiFi.localIP();
            snprintf(estadoActual.ipEstacionLAN, sizeof(estadoActual.ipEstacionLAN),
                     "%u.%u.%u.%u", ipSta[0], ipSta[1], ipSta[2], ipSta[3]);
        } else {
            estadoActual.ipEstacionLAN[0] = '\0';
        }
    }

    const EstadoRedWiFi& obtenerEstadoRed() const {
        return estadoActual;
    }
};

#endif