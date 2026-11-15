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
            const bool serieAhora   = refUarts ? refUarts->hayClienteConectado() : false;
            const uint8_t wifiAhora = refWiFi  ? refWiFi->obtenerCantidadClientesActivos() : 0;
            const uint16_t authAhora = calcularMascaraAutenticadosActual();

            // 1. Detectar nuevo cliente autenticado o desconectado
            if (authAhora != mascaraAutenticadosPrevia) {
                for (uint8_t s = 0; s <= 8; ++s) {
                    const bool antes = (mascaraAutenticadosPrevia & (1u << s)) != 0;
                    const bool ahora = (authAhora & (1u << s)) != 0;
                    if (!antes && ahora) {
                        encolarSonido(TipoSonidoEvento::CLIENTE_CONECTADO);
                        RegistroPersistenciaPOD aud{};
                        aud.tipo          = TipoRegistroDB::AUDITORIA_RED;
                        aud.timestampUnix = RelojSistema::obtenerTimestampActual();
                        aud.idRelacionado = s;
                        strncpy(aud.origenOClave, (s == 0) ? "UART-S" : refWiFi->obtenerIdentidadPeer(s).ip,
                                sizeof(aud.origenOClave) - 1);
                        strncpy(aud.detalle, "LOGIN_SESION_OK", sizeof(aud.detalle) - 1);
                        encolarPersistencia(aud);
                    } else if (antes && !ahora) {
                        encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);
                        RegistroPersistenciaPOD aud{};
                        aud.tipo          = TipoRegistroDB::AUDITORIA_RED;
                        aud.timestampUnix = RelojSistema::obtenerTimestampActual();
                        aud.idRelacionado = s;
                        strncpy(aud.origenOClave, (s == 0) ? "UART-S" : "WIFI", sizeof(aud.origenOClave) - 1);
                        strncpy(aud.detalle, "CIERRE_SESION", sizeof(aud.detalle) - 1);
                        encolarPersistencia(aud);
                    }
                }
                mascaraAutenticadosPrevia = authAhora;
            }

            // 2. Detectar desconexión física sin sesión
            if ((serieConectadoPrevio && !serieAhora) || (wifiAhora < clientesWifiPrevios)) {
                encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);
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
            return tarea.iniciarEnNucleo("TaskComms", &ejecutarCiclo, 10, 6144, 3, 1);
        }
    };

} // namespace RTOS::Tasks