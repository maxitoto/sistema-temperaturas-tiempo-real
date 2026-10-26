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
        if (udp) udp->detener();
        if (ap)  ap->detener();
        if (sta) sta->desconectar();
        WiFi.persistent(false);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        servidorIniciado = false;
    }

    // Levanta las interfaces en caliente (RAM) con los datos de la base de datos (incluyendo puerto UDP)
    void configurarRedes(const String& ssidAP, const String& passAP, bool requiereAP,
                         const String& ssidSTA, const String& passSTA, bool requiereSTA,
                         uint16_t puertoUdp = 81) {
        inicializar();
        if (udp && puertoUdp > 0) {
            udp->cambiarPuerto((int)puertoUdp);
        }

        bool apValido  = requiereAP && ssidAP.length() > 0;
        bool staValido = requiereSTA && ssidSTA.length() > 0;

        // Si no hay redes configuradas, el sistema despierta sin saber nada (Wi-Fi OFF, solo USB activo)
        if (!apValido && !staValido) {
            return;
        }

        if (apValido && staValido) {
            establecerModo(WIFI_AP_STA);
        } else if (apValido) {
            establecerModo(WIFI_AP);
        } else {
            establecerModo(WIFI_STA);
        }

        if (apValido) {
            ap->iniciar(ssidAP, passAP);
        }

        if (staValido) {
            sta->conectar(ssidSTA, passSTA, 15); // Hasta 5s esperando DHCP del edificio
        }

        if ((ap && ap->estaActiva()) || (sta && sta->estaConectado())) {
            udp->iniciar();
            servidorIniciado = true;
        }

        ultimoChequeoInternet = millis();
    }

    void mantenerRed() {
        if (!servidorIniciado && ((ap && ap->estaActiva()) || (sta && sta->estaConectado()))) {
            udp->iniciar();
            servidorIniciado = true;
        }

        if (servidorIniciado) {
            udp->procesarNuevasConexiones();
        }

        if (sta && sta->estaConectado()) {
            unsigned long ahora = millis();
            if (ahora - ultimoChequeoInternet >= INTERVALO_CHEQUEO_INTERNET) {
                estadoInternet = tieneGateway();
                ultimoChequeoInternet = ahora;
            }
        }
    }

    void establecerModo(WiFiMode_t modo) { WiFi.mode(modo); }
    WiFiMode_t obtenerModo() const { return WiFi.getMode(); }

    bool estaStaConectado() const { return sta ? sta->estaConectado() : false; }
    bool estaApActiva() const     { return ap ? ap->estaActiva() : false; }

    String obtenerIPSta() const {
        return (sta && sta->estaConectado()) ? sta->obtenerIP() : String("0.0.0.0");
    }

    String obtenerIPAp() const {
        return (ap && ap->estaActiva()) ? ap->obtenerIP() : String("0.0.0.0");
    }

    uint16_t obtenerPuertoUDP() const {
        return udp ? (uint16_t)udp->obtenerPuerto() : 81;
    }

    uint8_t obtenerClientesUDPActivos() const {
        return udp ? (uint8_t)udp->contarClientesActivos() : 0;
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

    bool tieneInternetReal() const { return estadoInternet; }
    String obtenerMAC() const      { return WiFi.macAddress(); }
};