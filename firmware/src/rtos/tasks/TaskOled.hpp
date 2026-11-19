
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
        static TareaSegura& obtenerTarea() { return tarea; }
        static constexpr uint32_t DURACION_VISTA_GRILLA_MS   = 12000;
        static constexpr uint32_t DURACION_DETALLE_SENSOR_MS = 10000;

    private:

        inline static TareaSegura tarea;

        inline static UARTSRuntime* refUarts = nullptr;
        inline static MiddlewareSesion* refSesion = nullptr;

        inline static bool logoMostrado = false;
        inline static bool enModoDetalle = false;

        inline static uint8_t indiceSensorDetalle = 0;

        inline static uint32_t cambioVistaMs = 0;

        inline static int8_t nivelWifiPrevio = -1;
        inline static int8_t seriePrevio = -1;
        inline static int8_t personaPrevia = -1;


        // =====================================================
        // 1. ESTADO WIFI
        // =====================================================

        static int8_t calcularNivelIconoWiFi() {

            if (WiFi.status() != WL_CONNECTED) {
                return 0;
            }

            const int32_t rssi = WiFi.RSSI();

            if (rssi >= -60) return 4;
            if (rssi >= -70) return 3;
            if (rssi >= -80) return 2;

            return 1;
        }


        // =====================================================
        // 2. SESIONES AUTENTICADAS
        // =====================================================

        static bool hayAlgunClienteAutenticado() {

            if (!refSesion) {
                return false;
            }

            for (uint8_t s = 0; s <= 8; ++s) {

                if (refSesion->estaClienteAutenticado(s)) {
                    return true;
                }
            }

            return false;
        }


        // =====================================================
        // 3. CABECERA E ICONOS
        // =====================================================

        static void actualizarCabeceraEIconos() {

            RelojSistema::sincronizarNtpSiHayWiFi();

            char txtReloj[12] = {};

            const bool mostrarFecha =
                ((millis() / 3000UL) % 2UL) == 1UL;

            RelojSistema::formatearFechaHoraOled(
                txtReloj,
                mostrarFecha
            );

            oled.cabeceraIzq(txtReloj);


            // WIFI

            const int8_t nivelWifi = calcularNivelIconoWiFi();

            if (nivelWifi != nivelWifiPrevio) {

                nivelWifiPrevio = nivelWifi;

                switch (nivelWifi) {

                    case 4:
                        oled.slotDer1(WIFI1.data);
                        break;

                    case 3:
                        oled.slotDer1(WIFI2.data);
                        break;

                    case 2:
                        oled.slotDer1(WIFI3.data);
                        break;

                    case 1:
                        oled.slotDer1(WIFI4.data);
                        break;

                    default:
                        oled.limpiarSlotDer1();
                        break;
                }
            }


            // CONEXION SERIAL

            const int8_t serieActivo =
                (refUarts && refUarts->hayClienteConectado()) ? 1 : 0;

            if (serieActivo != seriePrevio) {

                seriePrevio = serieActivo;

                if (serieActivo) {
                    oled.slotDer2(SERIAL_CONNECTED.data);
                } else {
                    oled.limpiarSlotDer2();
                }
            }


            // PERSONA AUTENTICADA

            const int8_t hayPersona =
                hayAlgunClienteAutenticado() ? 1 : 0;

            if (hayPersona != personaPrevia) {

                personaPrevia = hayPersona;

                if (hayPersona) {
                    oled.slotDer3(PERSON.data);
                } else {
                    oled.limpiarSlotDer3();
                }
            }
        }


        // =====================================================
        // 4. FORMATEO SEGURO DE TEMPERATURAS
        // =====================================================

        /*
         * Convierte un float a texto con una decimal.
         *
         * No utiliza:
         *   %f
         *   dtoa
         *   lroundf
         *   snprintf con argumentos float
         *
         * Rango representable:
         *   -999.9 a 999.9
         *
         * Ejemplos:
         *   23.56  -> 23.6
         *   -5.24  -> -5.2
         *   100.0  -> 100.0
         */

        static void formatearTempDecima(
            float valor,
            char* salida,
            size_t capacidad
        ) {

            if (!salida || capacidad == 0) {
                return;
            }

            salida[0] = '\0';

            if (!isfinite(valor)) {

                snprintf(salida, capacidad, "---");
                return;
            }

            // Limitar antes de convertir para evitar overflow.

            if (valor > 999.9f) {
                valor = 999.9f;
            }

            if (valor < -999.9f) {
                valor = -999.9f;
            }

            // Redondeo manual a décimas.

            const float escalado = valor * 10.0f;

            int32_t decimas = static_cast<int32_t>(
                escalado >= 0.0f
                    ? escalado + 0.5f
                    : escalado - 0.5f
            );

            const bool negativo = decimas < 0;

            if (negativo) {
                decimas = -decimas;
            }

            const int32_t entero = decimas / 10;
            const int32_t decimal = decimas % 10;

            snprintf(
                salida,
                capacidad,
                "%s%ld.%ld",
                negativo ? "-" : "",
                static_cast<long>(entero),
                static_cast<long>(decimal)
            );
        }


        // =====================================================
        // 5. GRILLA DE SEIS SENSORES
        // =====================================================

        static void formatearCeldaSensor(
            const SnapshotSensor& snap,
            char* salida
        ) {

            if (!salida) {
                return;
            }

            int temperatura = 0;

            if (isfinite(snap.tempCorregida)) {

                float t = snap.tempCorregida;

                if (t > 999.0f) t = 999.0f;
                if (t < -99.0f) t = -99.0f;

                temperatura = static_cast<int>(
                    t >= 0.0f ? t + 0.5f : t - 0.5f
                );
            }

            const char* estado = "ERR";

            switch (snap.estado) {

                case EstadoSensor::OK:
                    estado = "OK";
                    break;

                case EstadoSensor::ALARMA_SUPERIOR:
                    estado = "HI";
                    break;

                case EstadoSensor::ALARMA_INFERIOR:
                    estado = "LO";
                    break;

                default:
                    estado = "ERR";
                    break;
            }

            if (snap.estado == EstadoSensor::OK ||
                snap.estado == EstadoSensor::ALARMA_SUPERIOR ||
                snap.estado == EstadoSensor::ALARMA_INFERIOR) {

                snprintf(
                    salida,
                    12,
                    "%3s:%2dc\xF8%s",
                    snap.mac3,
                    temperatura,
                    estado
                );

            } else {

                snprintf(
                    salida,
                    12,
                    "%3s:---ERR",
                    snap.mac3
                );
            }
        }


        static void dibujarGrillaSeisSensores() {

            for (
                uint8_t slot = 0;
                slot < GestorSensores::MAX_SENSORES;
                ++slot
            ) {

                SnapshotSensor snap{};

                char textoCelda[12] = {};

                if (gestorSensores.obtenerSensorPorSlot(slot, snap)) {

                    formatearCeldaSensor(
                        snap,
                        textoCelda
                    );

                } else {

                    snprintf(
                        textoCelda,
                        sizeof(textoCelda),
                        "S%u: LIBRE",
                        static_cast<unsigned>(slot + 1)
                    );
                }

                oled.celdaGrilla(
                    slot,
                    textoCelda
                );
            }
        }


        // =====================================================
        // 6. DETALLE DE SENSOR
        // =====================================================

        static void dibujarDetalleSensor(
            const SnapshotSensor& sensor,
            uint8_t posicionActual,
            uint8_t totalConectados
        ) {

            /*
             * La OLED tiene 128 píxeles de ancho.
             * Con fuente tamaño 1:
             *
             * 128 / 6 = 21 caracteres completos.
             *
             * Cada fila tiene capacidad para 21 caracteres
             * visibles, más el terminador '\0'.
             */

            char fila1[22] = {};
            char fila2[22] = {};
            char fila3[22] = {};
            char fila4[22] = {};

            char tempCorregida[8] = {};
            char tempCruda[8] = {};
            char tempSuperior[8] = {};
            char tempInferior[8] = {};


            // FORMATEAR TEMPERATURAS

            formatearTempDecima(
                sensor.tempCorregida,
                tempCorregida,
                sizeof(tempCorregida)
            );

            formatearTempDecima(
                sensor.tempCruda,
                tempCruda,
                sizeof(tempCruda)
            );

            formatearTempDecima(
                sensor.umbral_sup,
                tempSuperior,
                sizeof(tempSuperior)
            );

            formatearTempDecima(
                sensor.umbral_inf,
                tempInferior,
                sizeof(tempInferior)
            );


            // FILA 1: IDENTIFICACION

            snprintf(
                fila1,
                sizeof(fila1),
                "S%u:%s",
                static_cast<unsigned>(sensor.slot + 1),
                sensor.mac
            );


            // FILA 2: TEMPERATURAS

            snprintf(
                fila2,
                sizeof(fila2),
                "C:%s R:%s",
                tempCorregida,
                tempCruda
            );


            // FILA 3: UMBRALES

            snprintf(
                fila3,
                sizeof(fila3),
                "H:%s L:%s",
                tempSuperior,
                tempInferior
            );


            // FILA 4: ESTADO Y POSICION

            snprintf(
                fila4,
                sizeof(fila4),
                "%s [%u/%u]",
                GestorSensores::nombreEstadoSensor(sensor.estado),
                static_cast<unsigned>(posicionActual + 1),
                static_cast<unsigned>(totalConectados)
            );


            // DIBUJAR UTILIZANDO LA LIBRERIA PROPIA

            oled.fila1(fila1);
            oled.fila2(fila2);
            oled.fila3(fila3);
            oled.fila4(fila4);
        }


        // =====================================================
        // 7. CAMBIO DE VISTA
        // =====================================================

        static void volverAGrilla(uint32_t ahoraMs) {

            enModoDetalle = false;

            indiceSensorDetalle = 0;

            cambioVistaMs = ahoraMs;

            oled.limpiarAreaContenido();

            dibujarGrillaSeisSensores();
        }


        // =====================================================
        // 8. CICLO PRINCIPAL DE LA TAREA
        // =====================================================

        static void ejecutarCiclo(void*) {

            // ---------------------------------------------
            // INICIALIZACION
            // ---------------------------------------------

            if (!logoMostrado) {

                if (!oled.iniciar()) {
                    return;
                }

                oled.fullScreen(
                    LOGO_UNTDF.data,
                    LOGO_UNTDF.ancho,
                    LOGO_UNTDF.alto,
                    true
                );

                TareaSegura::dormirMs(2500);

                oled.limpiarTodo();

                logoMostrado = true;

                cambioVistaMs = millis();
            }


            // ---------------------------------------------
            // CABECERA
            // ---------------------------------------------

            actualizarCabeceraEIconos();


            // ---------------------------------------------
            // OBTENER SENSORES
            // ---------------------------------------------

            SnapshotSensor conectados[
                GestorSensores::MAX_SENSORES
            ];

            const uint8_t cantConectados =
                gestorSensores.obtenerSensoresConectados(conectados);

            const uint32_t ahoraMs = millis();


            // ---------------------------------------------
            // MODO GRILLA
            // ---------------------------------------------

            if (!enModoDetalle) {

                dibujarGrillaSeisSensores();

                if (
                    cantConectados > 0 &&
                    (ahoraMs - cambioVistaMs >= DURACION_VISTA_GRILLA_MS)
                ) {

                    enModoDetalle = true;

                    indiceSensorDetalle = 0;

                    cambioVistaMs = ahoraMs;

                    oled.limpiarAreaContenido();
                }

                return;
            }


            // ---------------------------------------------
            // MODO DETALLE
            // ---------------------------------------------

            if (
                cantConectados == 0 ||
                indiceSensorDetalle >= cantConectados
            ) {

                volverAGrilla(ahoraMs);

                return;
            }


            // ---------------------------------------------
            // DIBUJAR SENSOR ACTUAL
            // ---------------------------------------------

            dibujarDetalleSensor(
                conectados[indiceSensorDetalle],
                indiceSensorDetalle,
                cantConectados
            );


            // ---------------------------------------------
            // ROTACION DE SENSORES
            // ---------------------------------------------

            if (
                ahoraMs - cambioVistaMs >= DURACION_DETALLE_SENSOR_MS
            ) {

                cambioVistaMs = ahoraMs;

                ++indiceSensorDetalle;

                oled.limpiarAreaContenido();

                if (indiceSensorDetalle >= cantConectados) {

                    volverAGrilla(ahoraMs);
                }
            }
        }


    public:

        // =====================================================
        // 9. INICIAR TAREA
        // =====================================================

        static bool iniciar(
            UARTSRuntime& uarts,
            MiddlewareSesion& sesionL2
        ) {

            refUarts = &uarts;

            refSesion = &sesionL2;

            return tarea.iniciarEnNucleo(
                "TaskOled",
                &ejecutarCiclo,
                250,
                4096,
                1,
                1
            );
        }
    };

} // namespace RTOS::Tasks
