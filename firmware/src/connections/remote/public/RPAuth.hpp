#ifndef RP_AUTH_H
#define RP_AUTH_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../Controller/abstract/RPC_base.hpp"
#include "../../../database/repositories/repoUsuario.hpp"
#include "../../../rtos/EventosYColas.hpp"

struct CredencialesLoginDTO {
    char usuario[32];
    char clavePlana[32];
};

struct SesionIniciadaDTO {
    uint8_t  rolAsignado;
    uint32_t tiempoExpiracionSegundos;
};

struct EstadoSesionSlot {
    int32_t    idUsuario;
    char       usuario[20];
    RolUsuario rol;
    uint32_t   ultimoRequestMs;
    uint32_t   bloqueadoHastaMs;
    uint8_t    intentosFallidos;
    bool       autenticado;
};

class RPAuth {
public:
    static constexpr uint32_t EXPIRACION_SESION_SEGUNDOS = 1800;       // 30 minutos
    static constexpr uint32_t TIEMPO_MAXIMO_OCIO_MS      = EXPIRACION_SESION_SEGUNDOS * 1000UL;
    static constexpr uint32_t TIEMPO_BLOQUEO_INTENTOS_MS = 600000UL;   // 10 minutos
    static constexpr uint8_t  MAXIMO_INTENTOS_FALLIDOS   = 3;

private:
    inline static EstadoSesionSlot sesionesPorSlot[9]{};

    static const char* medioPorSlot(uint8_t slotCliente) {
        return (slotCliente == 0) ? "UART-S" : "WIFI";
    }

    static void verificarSiSlotEstaBloqueado(EstadoSesionSlot& slot, uint8_t slotCliente, const char* usuarioIntento, uint32_t ahoraMs) {
        if (slot.bloqueadoHastaMs != 0) {
            if (static_cast<int32_t>(ahoraMs - slot.bloqueadoHastaMs) < 0) {
                RTOS::encolarAuditoria(
                    RTOS::TipoRegistroDB::AUDITORIA_RED,
                    slot.idUsuario,
                    usuarioIntento,
                    medioPorSlot(slotCliente),
                    "LOGIN_RECHAZADO_SLOT_BLOQUEADO",
                    static_cast<uint8_t>(ResultadoAuditoria::USUARIO_BLOQUEADO),
                    static_cast<uint8_t>(CriticidadEvento::MEDIA)
                );
                throw std::runtime_error("AuthenticationError: sesion bloqueada temporalmente por intentos fallidos");
            }
            slot.bloqueadoHastaMs = 0;
            slot.intentosFallidos = 0;
        }
    }

public:
    static SesionIniciadaDTO iniciarSesionUsuario(uint8_t slotCliente, const CredencialesLoginDTO& entrada) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        const uint32_t ahoraMs = millis();
        verificarSiSlotEstaBloqueado(slot, slotCliente, entrada.usuario, ahoraMs);

        if (entrada.usuario[0] == '\0' || entrada.clavePlana[0] == '\0') {
            throw std::invalid_argument("ValueError: el usuario y la contrasena no pueden estar vacios");
        }

        try {
            const auto usuarioDb = DbRuntime::Repositories::RepoUsuario::autenticarUsuario(
                entrada.usuario,
                entrada.clavePlana
            );

            slot.idUsuario        = usuarioDb.idUsuario;
            strncpy(slot.usuario, usuarioDb.usuario.c_str(), sizeof(slot.usuario) - 1);
            slot.usuario[sizeof(slot.usuario) - 1] = '\0';
            slot.autenticado      = true;
            slot.rol              = usuarioDb.rol;
            slot.ultimoRequestMs  = ahoraMs;
            slot.intentosFallidos = 0;

            return SesionIniciadaDTO{
                static_cast<uint8_t>(usuarioDb.rol),
                EXPIRACION_SESION_SEGUNDOS
            };
        } catch (...) {
            slot.autenticado = false;
            slot.idUsuario   = 0;
            slot.rol         = RolUsuario::NINGUNO;
            slot.intentosFallidos++;
            const bool bloqueadoAhora = (slot.intentosFallidos >= MAXIMO_INTENTOS_FALLIDOS);
            if (bloqueadoAhora) {
                slot.bloqueadoHastaMs = ahoraMs + TIEMPO_BLOQUEO_INTENTOS_MS;
            }

            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                0,
                entrada.usuario,
                medioPorSlot(slotCliente),
                bloqueadoAhora ? "LOGIN_BLOQUEO_INTENTOS" : "LOGIN_CREDENCIAL_INVALIDA",
                static_cast<uint8_t>(bloqueadoAhora ? ResultadoAuditoria::USUARIO_BLOQUEADO : ResultadoAuditoria::CREDENCIAL_INVALIDA),
                static_cast<uint8_t>(bloqueadoAhora ? CriticidadEvento::ALTA : CriticidadEvento::MEDIA)
            );
            throw;
        }
    }

    static void cerrarSesionUsuario(uint8_t slotCliente) {
        if (slotCliente <= 8) {
            sesionesPorSlot[slotCliente].autenticado     = false;
            sesionesPorSlot[slotCliente].idUsuario       = 0;
            sesionesPorSlot[slotCliente].usuario[0]      = '\0';
            sesionesPorSlot[slotCliente].rol             = RolUsuario::NINGUNO;
            sesionesPorSlot[slotCliente].ultimoRequestMs = 0;
        }
    }

    static RolUsuario exigirSesionAutorizada(uint8_t slotCliente, RolUsuario rolMinimoRequerido) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        const uint32_t ahoraMs = millis();
        verificarSiSlotEstaBloqueado(slot, slotCliente, slot.usuario, ahoraMs);

        if (!slot.autenticado) {
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                0,
                "ANONIMO",
                medioPorSlot(slotCliente),
                "RPC_SIN_SESION",
                static_cast<uint8_t>(ResultadoAuditoria::INTRUSION_TERMINAL),
                static_cast<uint8_t>(CriticidadEvento::MEDIA)
            );
            throw std::runtime_error("AuthenticationError: sesion no autenticada. Inicie sesion primero");
        }

        if (static_cast<uint8_t>(slot.rol) < static_cast<uint8_t>(rolMinimoRequerido)) {
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
                slot.idUsuario,
                slot.usuario,
                medioPorSlot(slotCliente),
                "RPC_DENEGADO_POR_ROL",
                static_cast<uint8_t>(ResultadoAuditoria::DENEGADO_ROL),
                static_cast<uint8_t>(CriticidadEvento::MEDIA)
            );
            throw std::runtime_error(
                (rolMinimoRequerido == RolUsuario::ADMIN)
                    ? "PermissionError: operacion exclusiva para rol ADMIN"
                    : "PermissionError: rol insuficiente para ejecutar este procedimiento"
            );
        }

        slot.ultimoRequestMs = ahoraMs;
        return slot.rol;
    }

    static bool estaClienteAutenticado(uint8_t slotCliente) {
        return (slotCliente <= 8) ? sesionesPorSlot[slotCliente].autenticado : false;
    }

    static int32_t obtenerIdUsuarioSlot(uint8_t slotCliente) {
        return (slotCliente <= 8 && sesionesPorSlot[slotCliente].autenticado)
            ? sesionesPorSlot[slotCliente].idUsuario
            : 0;
    }

    static const char* obtenerNombreUsuarioSlot(uint8_t slotCliente) {
        return (slotCliente <= 8 && sesionesPorSlot[slotCliente].usuario[0] != '\0')
            ? sesionesPorSlot[slotCliente].usuario
            : "";
    }

    static bool expiroPorOcio(uint8_t slotCliente, uint32_t ahoraMs) {
        if (slotCliente > 8 || !sesionesPorSlot[slotCliente].autenticado) return false;
        return static_cast<int32_t>(ahoraMs - sesionesPorSlot[slotCliente].ultimoRequestMs) >
               static_cast<int32_t>(TIEMPO_MAXIMO_OCIO_MS);
    }
};

#endif