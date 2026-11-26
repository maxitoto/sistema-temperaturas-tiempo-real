#ifndef WIFI_SECURITY_RATE_LIMITER_HPP
#define WIFI_SECURITY_RATE_LIMITER_HPP

#include <Arduino.h>
#include <stdint.h>

namespace WiFiSecurity {

    enum class DecisionTrafico : uint8_t {
        PERMITIDO = 0,
        LIMITADO_RECHAZO_SUAVE, // Responder BusyError sin tocar ORM/SD
        EXPULSAR_CONEXION       // Reincidencia grave: cerrar socket
    };

    struct EstadoRateLimitSlot {
        uint32_t conectadoDesdeMs{0};
        uint32_t ultimaRecargaMs{0};
        uint32_t ultimaAuditoriaMs{0};
        uint8_t  tokensDisponibles{0};
        uint8_t  violacionesConsecutivas{0};
        bool     activo{false};

        static constexpr uint32_t TIMEOUT_PRE_AUTH_MS         = 30000;  // 3 s para completar login L2
        static constexpr uint32_t COOLDOWN_AUDITORIA_MS       = 30000; // Máx. 1 log SD cada 30 s por slot

        // Perfil estricto antes del login (permite keepalive + login, frena floods)
        static constexpr uint8_t  CAP_RAFAGA_PRE_AUTH         = 3;
        static constexpr uint32_t MS_POR_TOKEN_PRE_AUTH       = 1000;  // 1 req/s

        // Perfil normal autenticado (cubre navegación y consultas del dashboard)
        static constexpr uint8_t  CAP_RAFAGA_AUTENTICADO      = 8;
        static constexpr uint32_t MS_POR_TOKEN_AUTENTICADO    = 200;   // 5 req/s sostenidos

        static constexpr uint8_t  MAX_VIOLACIONES_PRE_AUTH    = 2;
        static constexpr uint8_t  MAX_VIOLACIONES_AUTENTICADO = 6;

        void iniciarConexion(uint32_t ahoraMs) {
            conectadoDesdeMs        = ahoraMs;
            ultimaRecargaMs         = ahoraMs;
            ultimaAuditoriaMs       = 0;
            tokensDisponibles       = CAP_RAFAGA_PRE_AUTH;
            violacionesConsecutivas = 0;
            activo                  = true;
        }

        void reiniciar() {
            conectadoDesdeMs        = 0;
            ultimaRecargaMs         = 0;
            ultimaAuditoriaMs       = 0;
            tokensDisponibles       = 0;
            violacionesConsecutivas = 0;
            activo                  = false;
        }

        bool expiroVentanaPreAuth(bool autenticado, uint32_t ahoraMs) const {
            if (!activo || autenticado) return false;
            return static_cast<int32_t>(ahoraMs - conectadoDesdeMs) > static_cast<int32_t>(TIMEOUT_PRE_AUTH_MS);
        }

        DecisionTrafico evaluarPeticion(bool autenticado, uint32_t ahoraMs) {
            const uint8_t  capMax     = autenticado ? CAP_RAFAGA_AUTENTICADO : CAP_RAFAGA_PRE_AUTH;
            const uint32_t msPorToken = autenticado ? MS_POR_TOKEN_AUTENTICADO : MS_POR_TOKEN_PRE_AUTH;
            const uint8_t  maxViol    = autenticado ? MAX_VIOLACIONES_AUTENTICADO : MAX_VIOLACIONES_PRE_AUTH;

            const uint32_t transcurridoMs = ahoraMs - ultimaRecargaMs;
            if (transcurridoMs >= msPorToken) {
                const uint32_t nuevos = transcurridoMs / msPorToken;
                const uint32_t suma   = static_cast<uint32_t>(tokensDisponibles) + nuevos;
                tokensDisponibles     = (suma > capMax) ? capMax : static_cast<uint8_t>(suma);
                ultimaRecargaMs      += nuevos * msPorToken;
            }

            if (tokensDisponibles > 0) {
                tokensDisponibles--;
                violacionesConsecutivas = 0;
                return DecisionTrafico::PERMITIDO;
            }

            if (violacionesConsecutivas < 255) {
                violacionesConsecutivas++;
            }
            return (violacionesConsecutivas >= maxViol)
                ? DecisionTrafico::EXPULSAR_CONEXION
                : DecisionTrafico::LIMITADO_RECHAZO_SUAVE;
        }

        // Evita que un flood de red amplifique escrituras hacia la MicroSD
        bool puedeAuditarIncidente(uint32_t ahoraMs) {
            if (ultimaAuditoriaMs != 0 &&
                static_cast<int32_t>(ahoraMs - ultimaAuditoriaMs) < static_cast<int32_t>(COOLDOWN_AUDITORIA_MS)) {
                return false;
            }
            ultimaAuditoriaMs = ahoraMs;
            return true;
        }
    };

} // namespace WiFiSecurity

#endif