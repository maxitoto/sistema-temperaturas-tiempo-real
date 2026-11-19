#pragma once
#include <Arduino.h>
#include "../PrimitivasRTOS.hpp"
#include "../RelojSistema.hpp"
#include "../EventosYColas.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../connections/middlewares/middleware_intereface.hpp"

namespace RTOS::Tasks {

    class TaskComunicaciones {
    private:
        inline static TareaSegura tarea;
        inline static UARTSRuntime*        refUarts = nullptr;
        inline static WiFiRuntime*         refWiFi  = nullptr;
        inline static MiddlewareInterface* refL1    = nullptr;
        inline static MiddlewareSesion*    refL2    = nullptr;
        inline static MiddlewareMessage*   refL3    = nullptr;

        inline static uint32_t ultimoStreamingMs = 0;
        inline static uint16_t mascaraAutenticadosPrevia = 0;
        inline static bool     serieConectadoPrevio = false;
        inline static uint8_t  clientesWifiPrevios  = 0;

        static uint16_t calcularMascaraAutenticadosActual() {
            if (!refL2) return 0;
            uint16_t m = 0;
            for (uint8_t s = 0; s <= 8; ++s) {
                if (refL2->estaClienteAutenticado(s)) {
                    m |= static_cast<uint16_t>(1u << s);
                }
            }
            return m;
        }

        static void verificarTransicionesClientesYAuditoria() {
            const bool serieAhora    = refUarts ? refUarts->hayClienteConectado() : false;
            const uint8_t wifiAhora  = refWiFi  ? refWiFi->obtenerCantidadClientesActivos() : 0;
            const uint16_t authAhora = calcularMascaraAutenticadosActual();

            bool sonidoConexionEmitido    = false;
            bool sonidoDesconexionEmitido = false;

            // 1. Detectar nuevo cliente autenticado o cierre de sesión autenticada
            if (authAhora != mascaraAutenticadosPrevia) {
                for (uint8_t s = 0; s <= 8; ++s) {
                    const bool antes = (mascaraAutenticadosPrevia & (1u << s)) != 0;
                    const bool ahora = (authAhora & (1u << s)) != 0;
                    const char* origen = (s == 0) ? "UART-S" : (refWiFi ? refWiFi->obtenerIdentidadPeer(s).ip : "WIFI");
                    if (!antes && ahora) {
                        if (!sonidoConexionEmitido) {
                            encolarSonido(TipoSonidoEvento::CLIENTE_CONECTADO);
                            sonidoConexionEmitido = true;
                        }
                        encolarAuditoria(
                            TipoRegistroDB::AUDITORIA_RED,
                            RPAuth::obtenerIdUsuarioSlot(s),
                            RPAuth::obtenerNombreUsuarioSlot(s),
                            origen,
                            "LOGIN_SESION_OK",
                            static_cast<uint8_t>(ResultadoAuditoria::EXITO),
                            static_cast<uint8_t>(CriticidadEvento::BAJA)
                        );
                    } else if (antes && !ahora) {
                        if (!sonidoDesconexionEmitido) {
                            encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);
                            sonidoDesconexionEmitido = true;
                        }
                        encolarAuditoria(
                            TipoRegistroDB::AUDITORIA_RED,
                            0,
                            "",
                            origen,
                            "CIERRE_SESION",
                            static_cast<uint8_t>(ResultadoAuditoria::EXITO),
                            static_cast<uint8_t>(CriticidadEvento::BAJA)
                        );
                    }
                }
                mascaraAutenticadosPrevia = authAhora;
            }

            // 2. Detectar desconexión física solo si no sonó ya por cierre de sesión en este ciclo
            if (!sonidoDesconexionEmitido) {
                if ((serieConectadoPrevio && !serieAhora) || (wifiAhora < clientesWifiPrevios)) {
                    encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);
                }
            }

            serieConectadoPrevio = serieAhora;
            clientesWifiPrevios  = wifiAhora;
        }

        static void ejecutarCiclo(void*) {
            if (!refL1) return;

            refL1->procesarCicloEntrada();
            verificarTransicionesClientesYAuditoria();

            const uint32_t ahoraMs = millis();
            if (ahoraMs - ultimoStreamingMs >= 500) {
                ultimoStreamingMs = ahoraMs;
                if (refL3) {
                    refL3->emitirStreamsActivos();
                }
            }
        }

    public:
        static bool iniciar(
            UARTSRuntime& uarts,
            WiFiRuntime& wifi,
            MiddlewareInterface& l1,
            MiddlewareSesion& l2,
            MiddlewareMessage& l3
        ) {
            refUarts = &uarts;
            refWiFi  = &wifi;
            refL1    = &l1;
            refL2    = &l2;
            refL3    = &l3;

            // Prioridad 3 (alta) y período de 10 ms para baja latencia en UART-S y TCP
            return tarea.iniciarEnNucleo("TaskComms", &ejecutarCiclo, 10, 8192, 3, 1);
        }

        static TareaSegura& obtenerTarea() { return tarea; }
    };

} // namespace RTOS::Tasks