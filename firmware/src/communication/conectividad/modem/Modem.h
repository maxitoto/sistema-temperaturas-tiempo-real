// Archivo: ./src/communication/conectividad/modem/Modem.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "RedInterna.h"
#include "RedHost.h"
#include "ServidorWiFi.h"

class Modem {
  private:
    RedInterna* ap;
    RedHost* sta;
    ServidorWiFi* tcp;

    bool redInicializada = false;
    bool servidorIniciado = false;
    bool estadoInternet = false;
    unsigned long ultimoChequeoInternet = 0;
    static constexpr unsigned long INTERVALO_CHEQUEO_INTERNET = 300000UL;

  public:
    Modem(RedInterna* redAp, RedHost* redSta, ServidorWiFi* servidor)
        : ap(redAp), sta(redSta), tcp(servidor) {}

    void inicializar() {
        if (!redInicializada) {
            WiFi.persistent(false);
            redInicializada = true;
            servidorIniciado = false;
            return;
        }
        if (tcp) tcp->detener();
        if (ap && ap->estaActiva())   ap->detener();
        if (sta && sta->estaConectado()) sta->desconectar();
        WiFi.mode(WIFI_OFF);
        servidorIniciado = false;
    }

    void configurarRedes(const String& ssidAP, const String& passAP, bool requiereAP,
                         const String& ssidSTA, const String& passSTA, bool requiereSTA,
                         uint16_t puertoBase = 80) {
        inicializar();
        if (tcp && puertoBase > 0) {
            tcp->cambiarPuerto((int)puertoBase);
        }

        bool apValido  = requiereAP && ssidAP.length() > 0;
        bool staValido = requiereSTA && ssidSTA.length() > 0;

        if (!apValido && !staValido) {
            return; // Sin redes configuradas: queda lista la comunicación por USB
        }

        // 1. Levantar primero el modo Wi-Fi (crea el tcpip_mbox de lwIP en el ESP32)
        if (apValido && staValido) {
            establecerModo(WIFI_AP_STA);
        } else if (apValido) {
            establecerModo(WIFI_AP);
        } else {
            establecerModo(WIFI_STA);
        }
        delay(50);

        // 2. Iniciar interfaces solicitadas
        if (apValido && ap) {
            ap->iniciar(ssidAP, passAP);
        }

        if (staValido && sta) {
            sta->conectar(ssidSTA, passSTA, 8);
        }

        // 3. Abrir sockets TCP/UDP solo si la interfaz de red está activa
        if ((ap && ap->estaActiva()) || (sta && sta->estaConectado())) {
            if (tcp) {
                tcp->iniciar();
                servidorIniciado = true;
            }
        }

        ultimoChequeoInternet = millis();
    }

    // Configura el cliente SNTP nativo del kernel ESP32 (UTC-3 Tierra del Fuego)
    void configurarSNTPNativo(const char* srv1 = "pool.ntp.org",
                              const char* srv2 = "time.nist.gov") {
        if (sta && sta->estaConectado()) {
            configTime(-3 * 3600, 0, srv1, srv2);
        }
    }

    void mantenerRed() {
        if (!servidorIniciado && ((ap && ap->estaActiva()) || (sta && sta->estaConectado()))) {
            if (tcp) {
                tcp->iniciar();
                servidorIniciado = true;
            }
        }

        if (servidorIniciado && tcp) {
            tcp->procesarNuevasConexiones();
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
        return tcp ? (uint16_t)tcp->obtenerPuerto() : 80;
    }

    uint8_t obtenerClientesUDPActivos() const {
        return tcp ? (uint8_t)tcp->contarClientesActivos() : 0;
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