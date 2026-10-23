// Archivo: ./src/debug/DebugOLED.h
// MÓDULO DE DIAGNÓSTICO BORRABLE: Para desactivar, cambiar DEBUG_OLED_ACTIVO a 0 o borrar el archivo.
#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "const/Pines.h"
#include "plugins/pantalla/PantallaOLED.h"

#define DEBUG_OLED_ACTIVO 1

namespace DebugOLED {

#if DEBUG_OLED_ACTIVO
    static PantallaOLED pantalla(PIN_I2C_SDA, PIN_I2C_SCL);
    static bool inicializada = false;
    static bool pendienteRefresco = true;

    static uint32_t ciclosLoop = 0;
    static uint32_t hzActuales = 0;
    static unsigned long ultimoCalculoHzMs = 0;

    static char lineaRx[22]  = "RX: esperando...";
    static char lineaTx[22]  = "TX: esperando...";
    static char lineaErr[22] = "OK sin errores";
    static uint32_t contErr  = 0;

    inline void inicializar() {
        inicializada = pantalla.inicializar(PIN_I2C_SDA, PIN_I2C_SCL);
        if (inicializada) {
            pantalla.limpiarTodo();
            pantalla.dibujarDivisor(PantallaOLED::SOLIDA);
            pantalla.cabeceraIzq("DBG OSI");
            pantalla.fila1(lineaRx);
            pantalla.fila2(lineaTx);
            pantalla.fila3(lineaErr);
            pantalla.fila4("BOOT...");
            pantalla.actualizar();
        }
    }

    inline void contarCicloScheduler() {
        ciclosLoop++;
        unsigned long ahora = millis();
        if (ahora - ultimoCalculoHzMs >= 1000) {
            hzActuales = ciclosLoop;
            ciclosLoop = 0;
            ultimoCalculoHzMs = ahora;
            pendienteRefresco = true;
        }
    }

    inline void registrarRx(int idNodo, const char* etiqueta, uint16_t id) {
        snprintf(lineaRx, sizeof(lineaRx), "RX%d:%s #%u", idNodo, etiqueta, (unsigned)id);
        pendienteRefresco = true;
    }

    inline void registrarTx(int idNodo, const char* etiqueta, uint16_t id) {
        snprintf(lineaTx, sizeof(lineaTx), "TX%d:%s #%u", idNodo, etiqueta, (unsigned)id);
        pendienteRefresco = true;
    }

    inline void registrarError(const char* motivo) {
        contErr++;
        snprintf(lineaErr, sizeof(lineaErr), "E%lu:%s", (unsigned long)contErr, motivo);
        pendienteRefresco = true;
    }

    inline void refrescar(bool usbAuth, bool wifiAuth, uint32_t llamadasRpc) {
        if (!inicializada || !pendienteRefresco) return;
        pendienteRefresco = false;

        char cab[16];
        snprintf(cab, sizeof(cab), "U:%d W:%d", usbAuth ? 1 : 0, wifiAuth ? 1 : 0);
        pantalla.cabeceraIzq(cab);

        char f4[22];
        snprintf(f4, sizeof(f4), "%luHz RPC:%lu", (unsigned long)hzActuales, (unsigned long)llamadasRpc);

        pantalla.fila1(lineaRx);
        pantalla.fila2(lineaTx);
        pantalla.fila3(lineaErr);
        pantalla.fila4(f4);
        pantalla.actualizar();
    }
#else
    inline void inicializar() {}
    inline void contarCicloScheduler() {}
    inline void registrarRx(int, const char*, uint16_t) {}
    inline void registrarTx(int, const char*, uint16_t) {}
    inline void registrarError(const char*) {}
    inline void refrescar(bool, bool, uint32_t) {}
#endif

}