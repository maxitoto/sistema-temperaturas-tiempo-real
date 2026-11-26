#include <Arduino.h>

#include "Oled.hpp"

void setup() {
    Oled& oled = Oled::getInstance();

    if (!oled.estaInicializada()) {
        Serial.println("Error al inicializar OLED");
        return;
    }

    oled.limpiar();

    oled.establecerTamanoTexto(2);
    oled.escribir(0, 0, "SISTEMA");

    oled.establecerTamanoTexto(1);
    oled.escribir(0, 25, "Temperatura: 25.6 C");
    oled.escribir(0, 40, "Estado: OK");

    oled.actualizar();
}

void loop() {}