// Archivo: ./src/plugins/conectividad/modem/Modem.h
#pragma once
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include "RedInterna.h"
#include "RedHost.h"
#include "ServidorUDP.h"

class Modem {
  private:
    RedInterna* ap;
    RedHost* sta;
    ServidorUDP* udp;

    bool servidorIniciado = false;
    bool estadoInternet = false;
    unsigned long ultimoChequeoInternet = 0;
    const unsigned long INTERVALO_CHEQUEO_INTERNET = 300000UL;

  public:
    Modem(RedInterna* redAp, RedHost* redSta, ServidorUDP* servidor)
        : ap(redAp), sta(redSta), udp(servidor) {}

    void inicializar() {
        WiFi.persistent(false);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        servidorIniciado = false;
    }

    // Orquesta el levantamiento de las interfaces según las credenciales recibidas en el boot
    void configurarRedes(const String& ssidAP, const String& passAP, bool requiereAP,
                         const String& ssidSTA, const String& passSTA, bool requiereSTA) {
        inicializar();
        bool apValido = requiereAP && ssidAP.length() > 0;

        if (apValido && requiereSTA) {
            establecerModo(WIFI_AP_STA);
        } else if (apValido) {
            establecerModo(WIFI_AP);
        } else if (requiereSTA) {
            establecerModo(WIFI_STA);
        } else {
            // Modo rescate si no hay configuración previa
            establecerModo(WIFI_AP);
            ap->iniciar("Rescate_ESP", "12345678");
        }

        if (apValido) {
            ap->iniciar(ssidAP, passAP);
        }

        if (requiereSTA) {
            sta->conectar(ssidSTA, passSTA);
        }

        if (ap->estaActiva() || sta->estaConectado()) {
            udp->iniciar();
            servidorIniciado = true;
        }

        // Evita disparar verificarDNS() bloqueante en el primer ciclo tras conectar
        ultimoChequeoInternet = millis();
    }

    // Mantenimiento de conexiones físicas del módem (llamado en el loop de tareas)
    void mantenerRed() {
        if (!servidorIniciado && (ap->estaActiva() || sta->estaConectado())) {
            udp->iniciar();
            servidorIniciado = true;
        }

        if (servidorIniciado) {
            udp->procesarNuevasConexiones();
        }

        if (sta->estaConectado()) {
            unsigned long ahora = millis();
            if (ahora - ultimoChequeoInternet >= INTERVALO_CHEQUEO_INTERNET) {
                estadoInternet = tieneGateway();
                ultimoChequeoInternet = ahora;
            }
        }
    }

    void establecerModo(WiFiMode_t modo) {
        WiFi.mode(modo);
    }

    WiFiMode_t obtenerModo() const {
        return WiFi.getMode();
    }

    bool tieneGateway() const {
        if (WiFi.status() != WL_CONNECTED) return false;
        IPAddress gw = WiFi.gatewayIP();
        return (gw != IPAddress(0, 0, 0, 0) && gw != IPAddress(255, 255, 255, 255));
    }

    bool verificarDNS(const char* hostPrueba = "google.com") const {
        if (!tieneGateway()) return false;
        IPAddress ipTest;
        int err = WiFi.hostByName(hostPrueba, ipTest);
        return (err == 1 && ipTest.toString() != "0.0.0.0");
    }

    bool tieneInternetReal() const {
        return estadoInternet;
    }

    String obtenerMAC() const {
        return WiFi.macAddress();
    }
};