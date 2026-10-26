// Archivo: ./src/plugins/conectividad/modem/RedInterna.h
#pragma once
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

class RedInterna {
  private:
    String ssidActual;
    bool activa;

  public:
    RedInterna() : activa(false) {}

    bool iniciar(const String& ssid, const String& password) {
        if (ssid.length() == 0) {
            activa = false;
            return false;
        }
        ssidActual = ssid;
        if (password.length() >= 8) {
            activa = WiFi.softAP(ssid.c_str(), password.c_str());
        } else {
            // Si no se especificó clave (o < 8 chars), levanta el AP abierto para no fallar en ESP8266
            activa = WiFi.softAP(ssid.c_str());
        }
        return activa;
    }

    void detener() {
        WiFi.softAPdisconnect(true);
        activa = false;
        ssidActual = "";
    }

    bool estaActiva() const {
        return activa;
    }

    String obtenerIP() const {
        return activa ? WiFi.softAPIP().toString() : String("0.0.0.0");
    }

    String obtenerSSID() const {
        return ssidActual;
    }
};