// Archivo: ./src/plugins/conectividad/modem/RedHost.h
#pragma once
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

class RedHost {
  private:
    String ssidObjetivo;

  public:
    RedHost() {}

    bool conectar(const String& ssid, const String& password, int maxIntentos = 15) {
        ssidObjetivo = ssid;
        WiFi.begin(ssid.c_str(), password.c_str());

        int intentos = 0;
        while (WiFi.status() != WL_CONNECTED && intentos < maxIntentos) {
            delay(250);
            yield();
            intentos++;
        }
        return estaConectado();
    }

    void desconectar() {
        WiFi.disconnect(true);
        ssidObjetivo = "";
    }

    bool estaConectado() const {
        return WiFi.status() == WL_CONNECTED;
    }

    String obtenerIP() const {
        return WiFi.localIP().toString();
    }

    String obtenerSSID() const {
        return ssidObjetivo;
    }
};