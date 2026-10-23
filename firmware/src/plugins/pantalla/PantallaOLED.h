#pragma once
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class PantallaOLED {
  public:
    enum EstiloLinea { SOLIDA, PUNTEADA, PARCIAL, NINGUNA };

  private:
    private:
    Adafruit_SSD1306 display;
    EstiloLinea estiloActual;
    uint8_t sdaPin;
    uint8_t sclPin;
    uint8_t i2cAddr;
    uint32_t i2cClock;

    // =======================================================
    // MOTORES INTERNOS
    // =======================================================
    void motorTexto(int x, int y, int w, int h, const String& texto, bool justificarDerecha) {
        display.fillRect(x, y, w, h, SSD1306_BLACK); 
        
        unsigned int maxCaracteres = w / 6;
        String txt = texto;
        if (txt.length() > maxCaracteres) {
            txt = txt.substring(0, maxCaracteres);
        }

        int cursorX = x;
        int cursorY = y + (h - 8) / 2; 
        
        if (justificarDerecha) {
            int anchoTexto = txt.length() * 6; 
            cursorX = x + w - anchoTexto;
            if (cursorX < x) cursorX = x; 
        }

        display.setCursor(cursorX, cursorY);
        display.print(txt);
    }

    void motorIcono(int x, int y, int w, int h, const unsigned char* icono, int icoW, int icoH, bool justificarDerecha) {
        display.fillRect(x, y, w, h, SSD1306_BLACK); 
        
        int cursorX = x;
        int cursorY = y + (h - icoH) / 2; 
        
        if (justificarDerecha) {
            cursorX = x + w - icoW;
            if (cursorX < x) cursorX = x; 
        }

        display.drawBitmap(cursorX, cursorY, icono, icoW, icoH, SSD1306_WHITE);
    }

  public:
    PantallaOLED(uint8_t pinSDA = 255, uint8_t pinSCL = 255, uint8_t direccionI2C = 0x3C, uint32_t velocidadReloj = 400000, int ancho = 128, int alto = 64)
        : display(ancho, alto, &Wire, -1),
          estiloActual(NINGUNA),
          sdaPin(pinSDA),
          sclPin(pinSCL),
          i2cAddr(direccionI2C),
          i2cClock(velocidadReloj) {}
          
    bool inicializar() {
        return inicializar(sdaPin, sclPin, i2cAddr, i2cClock);
    }

    bool inicializar(uint8_t pinSDA, uint8_t pinSCL, uint8_t direccionI2C = 0x3C, uint32_t velocidadReloj = 400000) {
        Wire.begin(pinSDA, pinSCL);
        Wire.setClock(velocidadReloj);  
        
        if(!display.begin(SSD1306_SWITCHCAPVCC, direccionI2C)) {
            return false;
        }
        
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.display();
        return true;
    }

    void actualizar() {
        display.display();
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
    }

    // =======================================================
    // API PÚBLICA: DISEÑO DE REJILLA (TEXTO)
    // =======================================================
    void cabeceraIzq(const String& txt) { motorTexto(0, 0, 64, 16, txt, false); }
    void fila1(const String& txt)       { motorTexto(0, 18, 128, 11, txt, false); }
    void fila2(const String& txt)       { motorTexto(0, 30, 128, 11, txt, false); }
    void fila3(const String& txt)       { motorTexto(0, 42, 128, 11, txt, false); }
    void fila4(const String& txt)       { motorTexto(0, 54, 128, 10, txt, false); }

    // =======================================================
    // API PÚBLICA: DISEÑO DE REJILLA (ICONOS)
    // =======================================================
    void cabeceraIzq(const unsigned char* ico, int w=16, int h=16) { motorIcono(0, 0, 64, 16, ico, w, h, false); }
    void fila1(const unsigned char* ico, int w=10, int h=10)       { motorIcono(0, 18, 128, 11, ico, w, h, false); }
    void fila2(const unsigned char* ico, int w=10, int h=10)       { motorIcono(0, 30, 128, 11, ico, w, h, false); }
    void fila3(const unsigned char* ico, int w=10, int h=10)       { motorIcono(0, 42, 128, 11, ico, w, h, false); }
    void fila4(const unsigned char* ico, int w=10, int h=10)       { motorIcono(0, 54, 128, 10, ico, w, h, false); }

    // =======================================================
    // CABECERA DERECHA: 4 SLOTS EXCLUSIVOS PARA ICONOS (16x16)
    // =======================================================
    void slotDer4(const unsigned char* ico) { motorIcono(64, 0, 16, 16, ico, 16, 16, false); }
    void limpiarSlotDer4()                  { display.fillRect(64, 0, 16, 16, SSD1306_BLACK); }

    void slotDer3(const unsigned char* ico) { motorIcono(80, 0, 16, 16, ico, 16, 16, false); }
    void limpiarSlotDer3()                  { display.fillRect(80, 0, 16, 16, SSD1306_BLACK); }

    void slotDer2(const unsigned char* ico) { motorIcono(96, 0, 16, 16, ico, 16, 16, false); }
    void limpiarSlotDer2()                  { display.fillRect(96, 0, 16, 16, SSD1306_BLACK); }

    void slotDer1(const unsigned char* ico) { motorIcono(112, 0, 16, 16, ico, 16, 16, false); }
    void limpiarSlotDer1()                  { display.fillRect(112, 0, 16, 16, SSD1306_BLACK); }

    // =======================================================
    // LIMPIEZA
    // =======================================================
    void limpiarCabeceraIzq() { display.fillRect(0, 0, 64, 16, SSD1306_BLACK); }
    void limpiarFila1()       { display.fillRect(0, 18, 128, 11, SSD1306_BLACK); }
    void limpiarFila2()       { display.fillRect(0, 30, 128, 11, SSD1306_BLACK); }
    void limpiarFila3()       { display.fillRect(0, 42, 128, 11, SSD1306_BLACK); }
    void limpiarFila4()       { display.fillRect(0, 54, 128, 10, SSD1306_BLACK); }
    void limpiarTodo()        { display.clearDisplay(); }
};