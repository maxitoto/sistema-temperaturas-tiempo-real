#ifndef RP_AUTH_H
#define RP_AUTH_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
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
    inline static portMUX_TYPE     muxSesiones = portMUX_INITIALIZER_UNLOCKED;

    static const char* medioPorSlot(uint8_t slotCliente) {
        return (slotCliente == 0) ? "UART-S" : "WIFI";
    }

    static void verificarSiSlotEstaBloqueado(uint8_t slotCliente, const char* usuarioIntento, uint32_t ahoraMs) {
        bool bloqueado = false;
        int32_t idUsr  = 0;

        portENTER_CRITICAL(&muxSesiones);
        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        if (slot.bloqueadoHastaMs != 0) {
            if (static_cast<int32_t>(ahoraMs - slot.bloqueadoHastaMs) < 0) {
                bloqueado = true;
                idUsr     = slot.idUsuario;
            } else {
                slot.bloqueadoHastaMs = 0;
                slot.intentosFallidos = 0;
            }
        }
        portEXIT_CRITICAL(&muxSesiones);

        if (bloqueado) {
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                idUsr,
                usuarioIntento,
                medioPorSlot(slotCliente),
                "LOGIN_RECHAZADO_SLOT_BLOQUEADO",
                static_cast<uint8_t>(ResultadoAuditoria::USUARIO_BLOQUEADO),
                static_cast<uint8_t>(CriticidadEvento::MEDIA)
            );
            throw std::runtime_error("AuthenticationError: sesion bloqueada temporalmente por intentos fallidos");
        }
    }

public:
    static SesionIniciadaDTO iniciarSesionUsuario(uint8_t slotCliente, const CredencialesLoginDTO& entrada) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        const uint32_t ahoraMs = millis();
        verificarSiSlotEstaBloqueado(slotCliente, entrada.usuario, ahoraMs);

        if (entrada.usuario[0] == '\0' || entrada.clavePlana[0] == '\0') {
            throw std::invalid_argument("ValueError: el usuario y la contrasena no pueden estar vacios");
        }

        try {
            const auto usuarioDb = DbRuntime::Repositories::RepoUsuario::autenticarUsuario(
                entrada.usuario,
                entrada.clavePlana
            );

            portENTER_CRITICAL(&muxSesiones);
            EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
            slot.idUsuario         = usuarioDb.idUsuario;
            strncpy(slot.usuario, usuarioDb.usuario.c_str(), sizeof(slot.usuario) - 1);
            slot.usuario[sizeof(slot.usuario) - 1] = '\0';
            slot.autenticado       = true;
            slot.rol               = usuarioDb.rol;
            slot.ultimoRequestMs   = ahoraMs;
            slot.intentosFallidos  = 0;
            portEXIT_CRITICAL(&muxSesiones);

            return SesionIniciadaDTO{
                static_cast<uint8_t>(usuarioDb.rol),
                EXPIRACION_SESION_SEGUNDOS
            };
        } catch (...) {
            bool bloqueadoAhora = false;

            portENTER_CRITICAL(&muxSesiones);
            EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
            slot.autenticado       = false;
            slot.idUsuario         = 0;
            slot.rol               = RolUsuario::NINGUNO;
            slot.intentosFallidos++;
            bloqueadoAhora = (slot.intentosFallidos >= MAXIMO_INTENTOS_FALLIDOS);
            if (bloqueadoAhora) {
                slot.bloqueadoHastaMs = ahoraMs + TIEMPO_BLOQUEO_INTENTOS_MS;
            }
            portEXIT_CRITICAL(&muxSesiones);

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
        if (slotCliente > 8) return;
        portENTER_CRITICAL(&muxSesiones);
        sesionesPorSlot[slotCliente].autenticado     = false;
        sesionesPorSlot[slotCliente].idUsuario       = 0;
        sesionesPorSlot[slotCliente].usuario[0]      = '\0';
        sesionesPorSlot[slotCliente].rol             = RolUsuario::NINGUNO;
        sesionesPorSlot[slotCliente].ultimoRequestMs = 0;
        portEXIT_CRITICAL(&muxSesiones);
    }

    // Llamado por Core 0 al cerrar o reciclar un socket para bajar el permiso en el acto
    static void invalidarAutenticacionInmediata(uint8_t slotCliente) {
        cerrarSesionUsuario(slotCliente);
    }

    static RolUsuario exigirSesionAutorizada(uint8_t slotCliente, RolUsuario rolMinimoRequerido) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        const uint32_t ahoraMs = millis();
        char nombreLocal[20]{};
        int32_t idLocal = 0;
        bool estaAuth   = false;
        RolUsuario rolActual = RolUsuario::NINGUNO;

        portENTER_CRITICAL(&muxSesiones);
        strncpy(nombreLocal, sesionesPorSlot[slotCliente].usuario, sizeof(nombreLocal) - 1);
        portEXIT_CRITICAL(&muxSesiones);

        verificarSiSlotEstaBloqueado(slotCliente, nombreLocal, ahoraMs);

        portENTER_CRITICAL(&muxSesiones);
        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        estaAuth  = slot.autenticado;
        idLocal   = slot.idUsuario;
        rolActual = slot.rol;
        strncpy(nombreLocal, slot.usuario, sizeof(nombreLocal) - 1);
        if (estaAuth && static_cast<uint8_t>(rolActual) >= static_cast<uint8_t>(rolMinimoRequerido)) {
            slot.ultimoRequestMs = ahoraMs;
        }
        portEXIT_CRITICAL(&muxSesiones);

        if (!estaAuth) {
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

        if (static_cast<uint8_t>(rolActual) < static_cast<uint8_t>(rolMinimoRequerido)) {
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
                idLocal,
                nombreLocal,
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

        return rolActual;
    }

    static bool estaClienteAutenticado(uint8_t slotCliente) {
        if (slotCliente > 8) return false;
        portENTER_CRITICAL(&muxSesiones);
        const bool ok = sesionesPorSlot[slotCliente].autenticado;
        portEXIT_CRITICAL(&muxSesiones);
        return ok;
    }

    static void obtenerSnapshotUsuarioSlot(uint8_t slotCliente, int32_t& idOut, char* nombreOut, size_t capNombre) {
        idOut = 0;
        if (nombreOut && capNombre > 0) nombreOut[0] = '\0';
        if (slotCliente > 8) return;

        portENTER_CRITICAL(&muxSesiones);
        if (sesionesPorSlot[slotCliente].autenticado) {
            idOut = sesionesPorSlot[slotCliente].idUsuario;
            if (nombreOut && capNombre > 0) {
                strncpy(nombreOut, sesionesPorSlot[slotCliente].usuario, capNombre - 1);
                nombreOut[capNombre - 1] = '\0';
            }
        }
        portEXIT_CRITICAL(&muxSesiones);
    }

    static int32_t obtenerIdUsuarioSlot(uint8_t slotCliente) {
        if (slotCliente > 8) return 0;
        portENTER_CRITICAL(&muxSesiones);
        const int32_t id = sesionesPorSlot[slotCliente].autenticado ? sesionesPorSlot[slotCliente].idUsuario : 0;
        portEXIT_CRITICAL(&muxSesiones);
        return id;
    }

    static const char* obtenerNombreUsuarioSlot(uint8_t slotCliente) {
        return (slotCliente <= 8 && sesionesPorSlot[slotCliente].usuario[0] != '\0')
            ? sesionesPorSlot[slotCliente].usuario
            : "";
    }

    static bool expiroPorOcio(uint8_t slotCliente, uint32_t ahoraMs) {
        if (slotCliente > 8) return false;
        portENTER_CRITICAL(&muxSesiones);
        const bool auth = sesionesPorSlot[slotCliente].autenticado;
        const uint32_t ult = sesionesPorSlot[slotCliente].ultimoRequestMs;
        portEXIT_CRITICAL(&muxSesiones);
        if (!auth) return false;
        return static_cast<int32_t>(ahoraMs - ult) > static_cast<int32_t>(TIEMPO_MAXIMO_OCIO_MS);
    }
};

#endif