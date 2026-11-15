#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include "../PrimitivasRTOS.hpp"
#include "../RelojSistema.hpp"
#include "../../hardware/driver/oled/Oled.hpp"
#include "../../hardware/driver/oled/icons/icons.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../connections/middlewares/interfaces/UART-S/UARTSRuntime.hpp"
#include "../../connections/middlewares/middleware_sesion.hpp"

namespace RTOS::Tasks {

    class TaskOled {
    public:
        static constexpr uint32_t DURACION_VISTA_GRILLA_MS   = 12000; // 12 segundos en vista grilla
        static constexpr uint32_t DURACION_DETALLE_SENSOR_MS = 10000; // 10 segundos por sensor en detalle

    private:
        inline static TareaSegura tarea;
        inline static UARTSRuntime*     refUarts  = nullptr;
        inline static MiddlewareSesion* refSesion = nullptr;

        inline static bool     logoMostrado        = false;
        inline static bool     enModoDetalle       = false;
        inline static uint8_t  indiceSensorDetalle = 0;
        inline static uint32_t cambioVistaMs       = 0;

        inline static int8_t nivelWifiPrevio = -1;
        inline static int8_t seriePrevio     = -1;
        inline static int8_t personaPrevia   = -1;

        static int8_t calcularNivelIconoWiFi() {
            if (WiFi.status() != WL_CONNECTED) return 0;
            int32_t rssi = WiFi.RSSI();
            if (rssi >= -60) return 4;
            if (rssi >= -70) return 3;
            if (rssi >= -80) return 2;
            return 1;
        }

        static bool hayAlgunClienteAutenticado() {
            if (!refSesion) return false;
            for (uint8_t s = 0; s <= 8; ++s) {
                if (refSesion->estaClienteAutenticado(s)) return true;
            }
            return false;
        }

        static void actualizarCabeceraEIconos() {
            RelojSistema::sincronizarNtpSiHayWiFi();

            char txtReloj[12];
            bool mostrarFecha = ((millis() / 3000UL) % 2UL) == 1UL;
            RelojSistema::formatearFechaHoraOled(txtReloj, mostrarFecha);
            oled.cabeceraIzq(txtReloj);

            int8_t nivelWifi = calcularNivelIconoWiFi();
            if (nivelWifi != nivelWifiPrevio) {
                nivelWifiPrevio = nivelWifi;
                if (nivelWifi == 4)      oled.slotDer1(WIFI1.data);
                else if (nivelWifi == 3) oled.slotDer1(WIFI2.data);
                else if (nivelWifi == 2) oled.slotDer1(WIFI3.data);
                else if (nivelWifi == 1) oled.slotDer1(WIFI4.data);
                else                     oled.limpiarSlotDer1();
            }

            int8_t serieActivo = (refUarts && refUarts->hayClienteConectado()) ? 1 : 0;
            if (serieActivo != seriePrevio) {
                seriePrevio = serieActivo;
                if (serieActivo) oled.slotDer2(SERIAL_CONNECTED.data);
                else             oled.limpiarSlotDer2();
            }

            int8_t hayPersona = hayAlgunClienteAutenticado() ? 1 : 0;
            if (hayPersona != personaPrevia) {
                personaPrevia = hayPersona;
                if (hayPersona) oled.slotDer3(PERSON.data);
                else            oled.limpiarSlotDer3();
            }
        }

        // Formatea el texto de 10 caracteres con "c°" ('c' + '\xF8') fuera de Oled.hpp
        static void formatearCeldaSensor(const SnapshotSensor& snap, char* outTxt12) {
            int tempRedondeada = static_cast<int>(roundf(snap.tempCorregida));
            if (snap.estado == EstadoSensor::OK) {
                snprintf(outTxt12, 12, "%3s:%2dc\xF8OK", snap.mac3, tempRedondeada);
            } else if (snap.estado == EstadoSensor::ALARMA_SUPERIOR) {
                snprintf(outTxt12, 12, "%3s:%2dc\xF8HI", snap.mac3, tempRedondeada);
            } else if (snap.estado == EstadoSensor::ALARMA_INFERIOR) {
                snprintf(outTxt12, 12, "%3s:%2dc\xF8LO", snap.mac3, tempRedondeada);
            } else {
                snprintf(outTxt12, 12, "%3s:---ERR", snap.mac3);
            }
        }

        static void dibujarGrillaSeisSensores() {
            for (uint8_t slot = 0; slot < GestorSensores::MAX_SENSORES; ++slot) {
                SnapshotSensor snap{};
                char textoCelda[12];
                if (gestorSensores.obtenerSensorPorSlot(slot, snap)) {
                    formatearCeldaSensor(snap, textoCelda);
                } else {
                    snprintf(textoCelda, sizeof(textoCelda), "S%u: LIBRE ", static_cast<unsigned>(slot + 1));
                }
                oled.celdaGrilla(slot, textoCelda);
            }
        }

        static void dibujarDetalleSensor(const SnapshotSensor& s, uint8_t posActual, uint8_t totalConectados) {
            char f1[22], f2[22], f3[22], f4[22];
            snprintf(f1, sizeof(f1), "S%u:%s", static_cast<unsigned>(s.slot + 1), s.mac);
            snprintf(f2, sizeof(f2), "Cor:%.1fc\xF8 Cru:%.1fc\xF8", s.tempCorregida, s.tempCruda);
            snprintf(f3, sizeof(f3), "TH:%.1fc\xF8  TL:%.1fc\xF8", s.umbralSuperior, s.umbralInferior);
            snprintf(f4, sizeof(f4), "%s [%u/%u]",
                     GestorSensores::nombreEstadoSensor(s.estado),
                     static_cast<unsigned>(posActual + 1),
                     static_cast<unsigned>(totalConectados));

            oled.fila1(f1);
            oled.fila2(f2);
            oled.fila3(f3);
            oled.fila4(f4);
        }

        static void ejecutarCiclo(void*) {
            if (!logoMostrado) {
                oled.iniciar();
                oled.fullScreen(LOGO_UNTDF.data, LOGO_UNTDF.ancho, LOGO_UNTDF.alto, true);
                TareaSegura::dormirMs(2500);
                oled.limpiarTodo();
                logoMostrado  = true;
                cambioVistaMs = millis();
            }

            actualizarCabeceraEIconos();

            SnapshotSensor conectados[GestorSensores::MAX_SENSORES];
            const uint8_t cantConectados = gestorSensores.obtenerSensoresConectados(conectados);
            const uint32_t ahoraMs       = millis();

            if (!enModoDetalle) {
                dibujarGrillaSeisSensores();
                if (cantConectados > 0 && (ahoraMs - cambioVistaMs >= DURACION_VISTA_GRILLA_MS)) {
                    enModoDetalle       = true;
                    indiceSensorDetalle = 0;
                    cambioVistaMs       = ahoraMs;
                    oled.limpiarAreaContenido();
                }
            } else {
                if (cantConectados == 0 || indiceSensorDetalle >= cantConectados) {
                    enModoDetalle = false;
                    cambioVistaMs = ahoraMs;
                    oled.limpiarAreaContenido();
                    dibujarGrillaSeisSensores();
                    return;
                }

                dibujarDetalleSensor(conectados[indiceSensorDetalle], indiceSensorDetalle, cantConectados);

                if (ahoraMs - cambioVistaMs >= DURACION_DETALLE_SENSOR_MS) {
                    cambioVistaMs = ahoraMs;
                    indiceSensorDetalle++;
                    oled.limpiarAreaContenido();
                    if (indiceSensorDetalle >= cantConectados) {
                        enModoDetalle = false;
                    }
                }
            }
        }

    public:
        static bool iniciar(UARTSRuntime& uarts, MiddlewareSesion& sesionL2) {
            refUarts  = &uarts;
            refSesion = &sesionL2;
            return tarea.iniciarEnNucleo("TaskOled", &ejecutarCiclo, 250, 4096, 1, 1);
        }
    };

} // namespace RTOS::Tasks