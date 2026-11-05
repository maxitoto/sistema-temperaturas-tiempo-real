#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdio.h>

#include "../../placa/esp32S/pines_nodemcu32s.hpp"

class Oled {
public:
    enum EstiloLinea : uint8_t { SOLIDA = 0, PUNTEADA, PARCIAL, NINGUNA };

private:
    static constexpr int ANCHO          = 128;
    static constexpr int ALTO           = 64;
    static constexpr uint8_t I2C_ADDR   = 0x3C;
    static constexpr uint32_t I2C_CLOCK = 400000UL;

    Adafruit_SSD1306 display;
    EstiloLinea estiloActual = NINGUNA;
    bool inicializada = false;

    // Cache de texto de los 6 sensores para evitar redibujar si el valor no cambió
    char cacheSensores[6][12] = {{0}};

    Oled() : display(ANCHO, ALTO, &Wire, -1) {}
    ~Oled() = default;

    // Envía por I2C únicamente la ventana de páginas afectada (Dirty Rectangle Hardware)
    void enviarVentanaI2C(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
        if (!inicializada) return;
        const uint8_t* buffer = display.getBuffer();
        if (!buffer) {
            display.display();
            return;
        }

        uint8_t colIni = (x < ANCHO) ? x : (ANCHO - 1);
        uint8_t colFin = ((x + w - 1) < ANCHO) ? (x + w - 1) : (ANCHO - 1);
        uint8_t pagIni = y / 8;
        uint8_t pagFin = (y + h - 1) / 8;
        if (pagFin > 7) pagFin = 7;

        // Comandos nativos SSD1306 para direccionar solo el bloque modificado
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0x00);
        Wire.write(SSD1306_COLUMNADDR);
        Wire.write(colIni);
        Wire.write(colFin);
        Wire.write(SSD1306_PAGEADDR);
        Wire.write(pagIni);
        Wire.write(pagFin);
        Wire.endTransmission();

        for (uint8_t pag = pagIni; pag <= pagFin; pag++) {
            uint16_t offsetBase = static_cast<uint16_t>(pag) * ANCHO;
            uint8_t col = colIni;
            while (col <= colFin) {
                Wire.beginTransmission(I2C_ADDR);
                Wire.write(0x40); // Co = 0, D/C# = 1 (Datos de RAM gráfica)
                uint8_t bytesEnPaquete = 0;
                while (col <= colFin && bytesEnPaquete < 16) {
                    Wire.write(buffer[offsetBase + col]);
                    col++;
                    bytesEnPaquete++;
                }
                Wire.endTransmission();
            }
        }
    }

    void motorTexto(int x, int y, int w, int h, const char* texto, bool justificarDerecha, bool autoRefrescar = true) {
        if (!inicializada || !texto) return;

        display.fillRect(x, y, w, h, SSD1306_BLACK);

        size_t maxChars = static_cast<size_t>(w / 6);
        if (maxChars >= 22) maxChars = 21;

        char buf[22];
        strncpy(buf, texto, maxChars);
        buf[maxChars] = '\0';

        int len = static_cast<int>(strlen(buf));
        int cursorX = x;
        int cursorY = y + (h - 8) / 2;

        if (justificarDerecha) {
            cursorX = x + w - (len * 6);
            if (cursorX < x) cursorX = x;
        }

        display.setCursor(cursorX, cursorY);
        display.print(buf);

        if (autoRefrescar) {
            enviarVentanaI2C(x, y, w, h);
        }
    }

    void motorIcono(int x, int y, int w, int h, const unsigned char* icono, int icoW, int icoH, bool autoRefrescar = true) {
        if (!inicializada || !icono) return;
        display.fillRect(x, y, w, h, SSD1306_BLACK);
        int cursorY = y + (h - icoH) / 2;
        display.drawBitmap(x, cursorY, icono, icoW, icoH, SSD1306_WHITE);
        if (autoRefrescar) {
            enviarVentanaI2C(x, y, w, h);
        }
    }

public:
    static Oled& getInstance() {
        static Oled instance;
        return instance;
    }

    Oled(const Oled&)            = delete;
    Oled& operator=(const Oled&) = delete;
    Oled(Oled&&)                 = delete;
    Oled& operator=(Oled&&)      = delete;

    // Se autoinicializa usando los pines de pines_nodemcu32s.hpp
    bool iniciar() {
        if (inicializada) return true;

        Wire.begin(Pin::PIN_I2C_SDA, Pin::PIN_I2C_SCL);
        Wire.setClock(I2C_CLOCK);

        if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
            inicializada = false;
            return false;
        }

        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        dibujarDivisor(SOLIDA);
        display.display();
        inicializada = true;
        return true;
    }

    bool estaInicializada() const { return inicializada; }

    // Volcado completo (solo cuando se redibuja toda la pantalla)
    void actualizar() {
        if (inicializada) display.display();
    }

    void dibujarDivisor(EstiloLinea estilo) {
        estiloActual = estilo;
        display.drawLine(0, 16, 127, 16, SSD1306_BLACK);
        if (estilo == SOLIDA) {
            display.drawLine(0, 16, 127, 16, SSD1306_WHITE);
        } else if (estilo == PUNTEADA) {
            for (int i = 0; i < 128; i += 2) display.drawPixel(i, 16, SSD1306_WHITE);
        } else if (estilo == PARCIAL) {
            display.drawLine(10, 16, 117, 16, SSD1306_WHITE);
        }
        if (inicializada) enviarVentanaI2C(0, 16, 128, 1);
    }

    // =======================================================
    // 1. GRILLA WEB PARA 6 SENSORES (2 Columnas x 3 Filas)
    //    indice: 0..5 -> Actualiza por I2C SOLO esa celda
    // =======================================================

    void sensor(const char* mac, const char* texto) {
        if (!inicializada || !mac || !texto) return;
        // Combina "MAC + texto" y lo muestra en la fila correspondiente
        char buf[24];
        snprintf(buf, sizeof(buf), "%s %s", mac, texto);
        // Usá la primera fila libre o una fija según tu layout:
        fila1(buf);
    }

    void sensor(uint8_t indice, const char* texto) {
        if (!inicializada || indice >= 6 || !texto) return;

        // Si el texto es idéntico al que ya está en pantalla, ahorra el bus I2C
        if (strncmp(cacheSensores[indice], texto, sizeof(cacheSensores[indice]) - 1) == 0) {
            return;
        }
        strncpy(cacheSensores[indice], texto, sizeof(cacheSensores[indice]) - 1);
        cacheSensores[indice][sizeof(cacheSensores[indice]) - 1] = '\0';

        uint8_t col  = indice % 2; // 0 = Izq (x=0), 1 = Der (x=64)
        uint8_t fila = indice / 2; // 0, 1, 2

        int x = col * 64;
        int y = 18 + (fila * 15);
        motorTexto(x, y, 64, 14, cacheSensores[indice], false, true);
    }

    // Helper directo para pasar número de sensor y temperatura formateada
    void sensorTemperatura(uint8_t indice, const char* mac, float tempC, bool ok = true) {
        if (indice >= 6 || !mac) return;

        char txt[16];   // 12 quedaba justo con temps negativas
        if (!ok) {
            snprintf(txt, sizeof(txt), "%s:--", mac);
        } else {
            snprintf(txt, sizeof(txt), "%s:%4.1fC", mac, tempC);
        }
        sensor(indice, txt);
    }

    // =======================================================
    // 2. CABECERA E ICONOS DE ESTADO (Actualización parcial)
    // =======================================================
    void cabeceraIzq(const char* txt) { motorTexto(0, 0, 64, 16, txt, false, true); }

    void slotDer4(const unsigned char* ico) { motorIcono(64,  0, 16, 16, ico, 16, 16, true); }
    void slotDer3(const unsigned char* ico) { motorIcono(80,  0, 16, 16, ico, 16, 16, true); }
    void slotDer2(const unsigned char* ico) { motorIcono(96,  0, 16, 16, ico, 16, 16, true); }
    void slotDer1(const unsigned char* ico) { motorIcono(112, 0, 16, 16, ico, 16, 16, true); }

    void limpiarSlotDer4() { display.fillRect(64,  0, 16, 16, SSD1306_BLACK); enviarVentanaI2C(64,  0, 16, 16); }
    void limpiarSlotDer3() { display.fillRect(80,  0, 16, 16, SSD1306_BLACK); enviarVentanaI2C(80,  0, 16, 16); }
    void limpiarSlotDer2() { display.fillRect(96,  0, 16, 16, SSD1306_BLACK); enviarVentanaI2C(96,  0, 16, 16); }
    void limpiarSlotDer1() { display.fillRect(112, 0, 16, 16, SSD1306_BLACK); enviarVentanaI2C(112, 0, 16, 16); }

    // =======================================================
    // 3. FILAS DE ANCHO COMPLETO (Para pantallas de mensajes)
    // =======================================================
    void fila1(const char* txt) { motorTexto(0, 18, 128, 11, txt, false, true); }
    void fila2(const char* txt) { motorTexto(0, 30, 128, 11, txt, false, true); }
    void fila3(const char* txt) { motorTexto(0, 42, 128, 11, txt, false, true); }
    void fila4(const char* txt) { motorTexto(0, 54, 128, 10, txt, false, true); }

    // =======================================================
    // 4. PANTALLA COMPLETA / LOGO
    // =======================================================
    void fullScreen(const unsigned char* icono, int icoW, int icoH,
                    bool autoRefrescar = true) {
        if (!inicializada || !icono) return;
        display.clearDisplay();
        // Centrado vertical; horizontal se dibuja desde x=0
        int cursorY = (ALTO - icoH) / 2;
        display.drawBitmap(0, cursorY, icono, icoW, icoH, SSD1306_WHITE);
        if (autoRefrescar) {
            enviarVentanaI2C(0, 0, ANCHO, ALTO);
        }
    }

    // =======================================================
    // 5. PANTALLA COMPLETA con CABECERA / PARA LOGO y cabecera
    // =======================================================
    void fullScreenConCabecera(const unsigned char* icono, int icoW, int icoH,
                               bool autoRefrescar = true) {
        if (!inicializada || !icono) return;
        display.fillRect(0, 17, ANCHO, ALTO - 17, SSD1306_BLACK);
        int y = 17 + ((ALTO - 17) - icoH) / 2;
        if (y < 17) y = 17;
        display.drawBitmap(0, y, icono, icoW, icoH, SSD1306_WHITE);
        if (autoRefrescar) {
            enviarVentanaI2C(0, 0, ANCHO, ALTO);
        }
    }

    void limpiarTodo() {
        if (!inicializada) return;
        memset(cacheSensores, 0, sizeof(cacheSensores));
        display.clearDisplay();
        dibujarDivisor(estiloActual);
        display.display();
    }
};

// Instancia global y función de arranque limpia
inline Oled& oled = Oled::getInstance();

inline bool iniciarOled() {
    return oled.iniciar();
}