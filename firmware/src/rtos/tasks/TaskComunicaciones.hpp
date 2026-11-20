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

        inline static uint16_t mascaraAutenticadosPrevia = 0;
        inline static bool     serieConectadoPrevio      = false;
        inline static uint8_t  clientesWifiPrevios       = 0;
        inline static uint32_t ultimoSonidoDesconexionMs = 0;

        static uint16_t calcularMascaraAutenticadosActual() {
            uint16_t m = 0;
            for (uint8_t s = 0; s <= 8; ++s) {
                if (RPAuth::estaClienteAutenticado(s)) {
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
                        int32_t idUsr = 0;
                        char nombreUsr[20]{};
                        RPAuth::obtenerSnapshotUsuarioSlot(s, idUsr, nombreUsr, sizeof(nombreUsr));
                        encolarAuditoria(
                            TipoRegistroDB::AUDITORIA_RED,
                            idUsr,
                            nombreUsr,
                            origen,
                            "LOGIN_SESION_OK",
                            static_cast<uint8_t>(ResultadoAuditoria::EXITO),
                            static_cast<uint8_t>(CriticidadEvento::BAJA)
                        );
                    } else if (antes && !ahora) {
                        const uint32_t ahoraMs = millis();
                        if (!sonidoDesconexionEmitido &&
                            (ultimoSonidoDesconexionMs == 0 || (ahoraMs - ultimoSonidoDesconexionMs) >= 3000)) {
                            encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);  
                            sonidoDesconexionEmitido = true;
                            ultimoSonidoDesconexionMs = ahoraMs;
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

            if (!sonidoDesconexionEmitido) {
                const uint32_t ahoraMs = millis();
                if (((serieConectadoPrevio && !serieAhora) || (wifiAhora < clientesWifiPrevios)) &&
                    (ultimoSonidoDesconexionMs == 0 || (ahoraMs - ultimoSonidoDesconexionMs) >= 3000)) {
                    encolarSonido(TipoSonidoEvento::CLIENTE_DESCONECTADO);
                    ultimoSonidoDesconexionMs = ahoraMs;
                }
            }

            serieConectadoPrevio = serieAhora;
            clientesWifiPrevios  = wifiAhora;
        }

        static void ejecutarCiclo(void*) {
            if (!refL1) return;
            refL1->procesarCicloEntrada();
            verificarTransicionesClientesYAuditoria();
        }

    public:
        static bool iniciar(
            UARTSRuntime& uarts,
            WiFiRuntime& wifi,
            MiddlewareInterface& l1
        ) {
            refUarts = &uarts;
            refWiFi  = &wifi;
            refL1    = &l1;

            // Core 0 junto a LwIP/WiFi, prioridad 3, stack 4 KB (ya no toca SD ni ORM)
            return tarea.iniciarEnNucleo("TaskComms", &ejecutarCiclo, 10, 4096, 3, 0);
        }

        static TareaSegura& obtenerTarea() { return tarea; }
    };

} // namespace RTOS::Tasks