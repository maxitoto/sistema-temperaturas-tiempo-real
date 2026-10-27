// Archivo: ./src/communication/conectividad/usb/PuertoUSB.h
#pragma once
#include <Arduino.h>

class PuertoUSB {
  private:
    unsigned long baudRate;

  public:
    static constexpr size_t TAMANO_BUFFER_RX_UART = 1024;

    explicit PuertoUSB(unsigned long baudios = 115200) : baudRate(baudios) {}

    void inicializar() {
        Serial.setRxBufferSize(TAMANO_BUFFER_RX_UART);
        Serial.begin(baudRate);
        while (!Serial) {
            delay(10);
        }
    }

    Stream* obtenerStream() {
        return &Serial;
    }

    int bytesDisponibles() {
        return Serial.available();
    }

    void vaciarBufferSalida() {
        Serial.flush();
    }
};