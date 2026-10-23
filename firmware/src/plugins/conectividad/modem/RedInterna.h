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
        ssidActual = ssid;
        activa = WiFi.softAP(ssid.c_str(), password.c_str());
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
        return WiFi.softAPIP().toString();
    }

    String obtenerSSID() const {
        return ssidActual;
    }
};