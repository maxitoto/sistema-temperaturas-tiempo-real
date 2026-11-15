#ifndef RP_AUTH_H
#define RP_AUTH_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../Controller/abstract/RPC_base.hpp"
#include "../../../database/repositories/repoUsuario.hpp"

struct CredencialesLoginDTO {
    char usuario[32];
    char clavePlana[32];
};

struct SesionIniciadaDTO {
    uint8_t  rolAsignado;
    uint32_t tiempoExpiracionSegundos;
};

struct EstadoSesionSlot {
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

    static void verificarSiSlotEstaBloqueado(EstadoSesionSlot& slot, uint32_t ahoraMs) {
        if (slot.bloqueadoHastaMs != 0) {
            if (static_cast<int32_t>(ahoraMs - slot.bloqueadoHastaMs) < 0) {
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
        verificarSiSlotEstaBloqueado(slot, ahoraMs);

        if (entrada.usuario[0] == '\0' || entrada.clavePlana[0] == '\0') {
            throw std::invalid_argument("ValueError: el usuario y la contrasena no pueden estar vacios");
        }

        try {
            // Autentica contra la tabla "Usuario" en MicroSD (actualiza intentos_fallidos y bloqueado en DB)
            const auto usuarioDb = DbRuntime::Repositories::RepoUsuario::autenticarUsuario(
                entrada.usuario,
                entrada.clavePlana
            );

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
            slot.rol         = RolUsuario::NINGUNO;
            slot.intentosFallidos++;
            if (slot.intentosFallidos >= MAXIMO_INTENTOS_FALLIDOS) {
                slot.bloqueadoHastaMs = ahoraMs + TIEMPO_BLOQUEO_INTENTOS_MS;
            }
            throw; // Re-lanza la excepción exacta que produjo RepoUsuario hacia el cliente RPC
        }
    }

    static void cerrarSesionUsuario(uint8_t slotCliente) {
        if (slotCliente <= 8) {
            sesionesPorSlot[slotCliente].autenticado     = false;
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
        verificarSiSlotEstaBloqueado(slot, ahoraMs);

        if (!slot.autenticado) {
            throw std::runtime_error("AuthenticationError: sesion no autenticada. Inicie sesion primero");
        }

        if (static_cast<uint8_t>(slot.rol) < static_cast<uint8_t>(rolMinimoRequerido)) {
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

    static bool expiroPorOcio(uint8_t slotCliente, uint32_t ahoraMs) {
        if (slotCliente > 8 || !sesionesPorSlot[slotCliente].autenticado) return false;
        return static_cast<int32_t>(ahoraMs - sesionesPorSlot[slotCliente].ultimoRequestMs) >
               static_cast<int32_t>(TIEMPO_MAXIMO_OCIO_MS);
    }
};

#endif