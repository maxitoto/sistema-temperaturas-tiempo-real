// Archivo: ./src/communication/conectividad/modem/RedHost.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>

class RedHost {
  private:
    String ssidObjetivo;

  public:
    RedHost() {}

    bool conectar(const String& ssid, const String& password, int maxIntentos = 10) {
        if (ssid.length() == 0) return false;
        ssidObjetivo = ssid;

        if (password.length() > 0) {
            WiFi.begin(ssid.c_str(), password.c_str());
        } else {
            WiFi.begin(ssid.c_str());
        }

        int intentos = 0;
        while (WiFi.status() != WL_CONNECTED && intentos < maxIntentos) {
            delay(500);
            intentos++;
            yield();
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
        return estaConectado() ? WiFi.localIP().toString() : String("0.0.0.0");
    }

    String obtenerSSID() const {
        return ssidObjetivo;
    }
};