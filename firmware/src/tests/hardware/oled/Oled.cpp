#include "Oled.hpp"

#include <Wire.h>

Oled::Oled()
    : pantalla(ANCHO, ALTO, &Wire, -1) {

    Wire.begin(Pin::PIN_I2C_SDA, Pin::PIN_I2C_SCL);

    inicializada = pantalla.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
    );

    if (!inicializada) return;

    pantalla.clearDisplay();
    pantalla.setTextColor(SSD1306_WHITE);
    pantalla.setTextSize(1);
    pantalla.display();
}

void Oled::limpiar() {
    if (!inicializada) return;

    pantalla.clearDisplay();
}

void Oled::actualizar() {
    if (!inicializada) return;

    pantalla.display();
}

void Oled::escribir(const char* texto) {
    if (!inicializada || texto == nullptr) return;

    pantalla.print(texto);
}

void Oled::escribir(int x, int y, const char* texto) {
    if (!inicializada || texto == nullptr) return;

    pantalla.setCursor(x, y);
    pantalla.print(texto);
}

void Oled::establecerTamanoTexto(uint8_t tamano) {
    if (!inicializada) return;

    pantalla.setTextSize(tamano);
}

void Oled::establecerColor(uint16_t color) {
    if (!inicializada) return;

    pantalla.setTextColor(color);
}

void Oled::dibujarPixel(int x, int y, uint16_t color) {
    if (!inicializada) return;

    pantalla.drawPixel(x, y, color);
}

void Oled::dibujarLinea(
    int x1, int y1,
    int x2, int y2,
    uint16_t color
) {
    if (!inicializada) return;

    pantalla.drawLine(x1, y1, x2, y2, color);
}

void Oled::dibujarRectangulo(
    int x, int y,
    int ancho, int alto,
    uint16_t color
) {
    if (!inicializada) return;

    pantalla.drawRect(x, y, ancho, alto, color);
}

bool Oled::estaInicializada() const {
    return inicializada;
}