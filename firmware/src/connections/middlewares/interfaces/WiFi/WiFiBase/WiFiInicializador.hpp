#ifndef WIFI_INICIALIZADOR_H
#define WIFI_INICIALIZADOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <string.h>
#include "../../../../../database/repositories/repoConfiguracionRed.hpp"

struct EstadoRedWiFi {
    char ipPuntoAcceso[16];
    char ipEstacionLAN[16];
    bool puntoAccesoActivo;
    bool conectadoARouter;
};

class WiFiInicializador {
public:
    static constexpr uint32_t TIMEOUT_CONEXION_STA_MS = 8000;
    inline static WiFiInicializador* instanciaActiva = nullptr;

private:
    EstadoRedWiFi estadoActual{};
    int32_t idConfiguracionActiva = 0;

    char ssidStaCache[36]  = "";
    char claveStaCache[65] = "";
    char ssidApCache[36]   = "ESP32-SecureAP";
    char claveApCache[65]  = "ESP32secure1234@";

    bool cargarConfiguracionDesdeDB() {
        using DbNotRuntime::Engine::PhysicalSchemaCatalog;
        using DbRuntime::Orm::runtimeDb;
        using DbRuntime::Repositories::ConfiguracionRedDTO;
        using DbRuntime::Repositories::RepoConfiguracionRed;

        if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
            runtimeDb.bindToPhysicalCatalog();
        }

        ConfiguracionRedDTO cfg{};
        if (!RepoConfiguracionRed::obtenerOInicializar(cfg)) {
            Serial.println("[WiFi][DB] Error al consultar ConfiguracionRed, usando fallback en RAM:");
            Serial.println(RepoConfiguracionRed::ultimoError());
            return false;
        }

        idConfiguracionActiva = cfg.id_configuracion_red;
        strncpy(ssidStaCache,  cfg.ssid_sta.c_str(),     sizeof(ssidStaCache) - 1);
        strncpy(claveStaCache, cfg.password_sta.c_str(), sizeof(claveStaCache) - 1);
        if (!cfg.ssid_ap.vacio()) {
            strncpy(ssidApCache, cfg.ssid_ap.c_str(), sizeof(ssidApCache) - 1);
        }
        strncpy(claveApCache, cfg.password_ap.c_str(), sizeof(claveApCache) - 1);
        return true;
    }

    bool aplicarConfiguracionWiFi() {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);

        bool apLevantado = (strlen(claveApCache) >= 8)
            ? WiFi.softAP(ssidApCache, claveApCache)
            : WiFi.softAP(ssidApCache);

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

        return estadoActual.puntoAccesoActivo || estadoActual.conectadoARouter;
    }

public:
    WiFiInicializador() {
        memset(&estadoActual, 0, sizeof(EstadoRedWiFi));
        instanciaActiva = this;
    }

    bool iniciarRedDesdeBaseDeDatos() {
        cargarConfiguracionDesdeDB();
        return aplicarConfiguracionWiFi();
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
        if (nuevaClaveAp != nullptr && (strlen(nuevaClaveAp) >= 8 || nuevaClaveAp[0] == '\0')) {
            strncpy(claveApCache, nuevaClaveAp, sizeof(claveApCache) - 1);
            claveApCache[sizeof(claveApCache) - 1] = '\0';
        }

        if (idConfiguracionActiva > 0) {
            DbRuntime::Repositories::ConfiguracionRedDTO dto{};
            dto.id_configuracion_red = idConfiguracionActiva;
            dto.ssid_sta             = ssidStaCache;
            dto.password_sta         = claveStaCache;
            dto.ssid_ap              = ssidApCache;
            dto.password_ap          = claveApCache;

            if (!DbRuntime::Repositories::RepoConfiguracionRed::actualizarTodo(dto)) {
                Serial.println("[WiFi][DB] Advertencia: no se pudo guardar la nueva config en SD:");
                Serial.println(DbRuntime::Repositories::RepoConfiguracionRed::ultimoError());
            }
        }

        return aplicarConfiguracionWiFi();
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