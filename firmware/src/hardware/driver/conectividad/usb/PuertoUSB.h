#pragma once
#include <Arduino.h>

class PuertoUSB {
  private:
    unsigned long baudRate;

  public:
    explicit PuertoUSB(unsigned long baudios = 115200) : baudRate(baudios) {}

    void inicializar() {
      Serial.begin(baudRate);
      // Opcional: Espera de estabilización para ciertas placas (ej. ESP32 nativo)
      while (!Serial) {
        delay(10);
      }
    }

    // Exponemos el puente físico para que la capa superior (Sesión/Router) lo consuma
    Stream* obtenerStream() {
      return &Serial;
    }

    // Funciones utilitarias agnósticas de hardware
    int bytesDisponibles() {
      return Serial.available();
    }

    void vaciarBufferSalida() {
      Serial.flush();
    }
};