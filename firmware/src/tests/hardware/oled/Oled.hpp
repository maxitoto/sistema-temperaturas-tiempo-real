#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class Oled {
private:
    int ANCHO = 128;
    int ALTO = 64;

    Adafruit_SSD1306 pantalla;

    bool inicializada = false;

    Oled();
    ~Oled() = default;

public:
    static Oled& getInstance() {
        static Oled instance;
        return instance;
    }

    Oled(const Oled&) = delete;
    Oled& operator=(const Oled&) = delete;
    Oled(Oled&&) = delete;
    Oled& operator=(Oled&&) = delete;

    void limpiar();
    void actualizar();

    void escribir(const char* texto);
    void escribir(int x, int y, const char* texto);

    void establecerTamanoTexto(uint8_t tamano);
    void establecerColor(uint16_t color);

    void dibujarPixel(int x, int y, uint16_t color);
    void dibujarLinea(int x1, int y1, int x2, int y2, uint16_t color);
    void dibujarRectangulo(int x, int y, int ancho, int alto, uint16_t color);

    bool estaInicializada() const;
};