#ifndef RP_AUTH_H
#define RP_AUTH_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>
#include <stdio.h>
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
    uint32_t   ventanaInicioMs;
    uint32_t   ultimaAuditoriaFloodMs;
    uint32_t   ultimoIntentoLoginMs;
    uint16_t   requestsEnVentana;
    uint8_t    infraccionesFlood;
    uint8_t    intentosFallidos;
    bool       autenticado;
};

class RPAuth {
public:
    static constexpr uint32_t EXPIRACION_SESION_SEGUNDOS = 1800;       // 30 minutos
    static constexpr uint32_t TIEMPO_MAXIMO_OCIO_MS      = EXPIRACION_SESION_SEGUNDOS * 1000UL;
    static constexpr uint32_t TIEMPO_BLOQUEO_INTENTOS_MS = 600000UL;   // 10 minutos
    static constexpr uint32_t TIEMPO_BLOQUEO_FLOOD_MS    = 120000UL;   // 2 minutos
    static constexpr uint32_t VENTANA_RATE_LIMIT_MS      = 5000UL;     // Ventana de 5 segundos
    static constexpr uint32_t COOLDOWN_LOGIN_MS          = 1000UL;     // 1 s mínimo tras fallo de login
    static constexpr uint32_t COOLDOWN_AUDITORIA_FLOOD   = 10000UL;    // Evita saturar cola/SD bajo ataque
    static constexpr uint16_t MAX_REQ_ANONIMO_VENTANA    = 15;         // 3 req/s sin autenticar
    static constexpr uint16_t MAX_REQ_AUTH_VENTANA       = 180;        // 36 req/s autenticado (exportaciones/tablas)
    static constexpr uint8_t  MAX_INFRACCIONES_FLOOD     = 3;
    static constexpr uint8_t  MAXIMO_INTENTOS_FALLIDOS   = 3;

private:
    inline static EstadoSesionSlot sesionesPorSlot[9]{};
    inline static portMUX_TYPE     muxSesiones = portMUX_INITIALIZER_UNLOCKED;

    static const char* medioPorSlot(uint8_t slotCliente) {
        return (slotCliente == 0) ? "UART-S" : "WIFI";
    }

    static void construirAccionAuditoria(char* dst, size_t cap, const char* prefijo, const char* accion) {
        if (!dst || cap == 0) return;
        if (accion && accion[0] != '\0') {
            snprintf(dst, cap, "%s:%s", prefijo, accion);
        } else {
            strncpy(dst, prefijo, cap - 1);
            dst[cap - 1] = '\0';
        }
    }

    static void verificarSiSlotEstaBloqueado(uint8_t slotCliente, const char* usuarioIntento, uint32_t ahoraMs) {
        bool bloqueado     = false;
        bool debeAuditar   = false;
        int32_t idUsr      = 0;

        portENTER_CRITICAL(&muxSesiones);
        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        if (slot.bloqueadoHastaMs != 0) {
            if (static_cast<int32_t>(ahoraMs - slot.bloqueadoHastaMs) < 0) {
                bloqueado = true;
                idUsr     = slot.idUsuario;
                if (slot.ultimaAuditoriaFloodMs == 0 ||
                    static_cast<int32_t>(ahoraMs - slot.ultimaAuditoriaFloodMs) >= static_cast<int32_t>(COOLDOWN_AUDITORIA_FLOOD)) {
                    slot.ultimaAuditoriaFloodMs = ahoraMs;
                    debeAuditar = true;
                }
            } else {
                slot.bloqueadoHastaMs  = 0;
                slot.intentosFallidos  = 0;
                slot.infraccionesFlood = 0;
            }
        }
        portEXIT_CRITICAL(&muxSesiones);

        if (bloqueado) {
            if (debeAuditar) {
                RTOS::encolarAuditoria(
                    RTOS::TipoRegistroDB::AUDITORIA_RED,
                    idUsr,
                    (usuarioIntento && usuarioIntento[0] != '\0') ? usuarioIntento : "ANONIMO",
                    medioPorSlot(slotCliente),
                    "SLOT_BLOQUEADO_TEMPORAL",
                    static_cast<uint8_t>(ResultadoAuditoria::USUARIO_BLOQUEADO),
                    static_cast<uint8_t>(CriticidadEvento::MEDIA)
                );
            }
            throw std::runtime_error("AuthenticationError: sesion bloqueada temporalmente por seguridad");
        }
    }

public:
    // Protección contra ataques de fatiga de mensajería (flooding) por slot
    static void verificarCuotaMensajeria(uint8_t slotCliente, const char* accionSolicitada = nullptr) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        const uint32_t ahoraMs = millis();
        bool excedido          = false;
        bool bloqueadoAhora    = false;
        bool debeAuditar       = false;
        int32_t idLocal        = 0;
        char nombreLocal[20]{};

        portENTER_CRITICAL(&muxSesiones);
        EstadoSesionSlot& slot = sesionesPorSlot[slotCliente];
        if (slot.ventanaInicioMs == 0 ||
            static_cast<int32_t>(ahoraMs - slot.ventanaInicioMs) >= static_cast<int32_t>(VENTANA_RATE_LIMIT_MS)) {
            slot.ventanaInicioMs   = ahoraMs;
            slot.requestsEnVentana = 1;
            if (slot.infraccionesFlood > 0) {
                slot.infraccionesFlood--;
            }
        } else {
            slot.requestsEnVentana++;
            const uint16_t limite = slot.autenticado ? MAX_REQ_AUTH_VENTANA : MAX_REQ_ANONIMO_VENTANA;
            if (slot.requestsEnVentana > limite) {
                excedido = true;
                idLocal  = slot.idUsuario;
                strncpy(nombreLocal, slot.autenticado ? slot.usuario : "ANONIMO", sizeof(nombreLocal) - 1);

                // Solo incrementa infracción una vez por ventana superada
                if (slot.requestsEnVentana == static_cast<uint16_t>(limite + 1)) {
                    slot.infraccionesFlood++;
                }
                if (slot.infraccionesFlood >= MAX_INFRACCIONES_FLOOD) {
                    bloqueadoAhora        = true;
                    slot.bloqueadoHastaMs = ahoraMs + TIEMPO_BLOQUEO_FLOOD_MS;
                    slot.autenticado      = false;
                    slot.rol              = RolUsuario::NINGUNO;
                }
                if (slot.ultimaAuditoriaFloodMs == 0 ||
                    static_cast<int32_t>(ahoraMs - slot.ultimaAuditoriaFloodMs) >= static_cast<int32_t>(COOLDOWN_AUDITORIA_FLOOD)) {
                    slot.ultimaAuditoriaFloodMs = ahoraMs;
                    debeAuditar = true;
                }
            }
        }
        portEXIT_CRITICAL(&muxSesiones);

        if (excedido) {
            if (debeAuditar) {
                char accionAud[32]{};
                construirAccionAuditoria(
                    accionAud,
                    sizeof(accionAud),
                    bloqueadoAhora ? "FLOOD_BLOQ" : "FLOOD_LIMITE",
                    accionSolicitada
                );
                RTOS::encolarAuditoria(
                    RTOS::TipoRegistroDB::AUDITORIA_RED,
                    idLocal,
                    nombreLocal,
                    medioPorSlot(slotCliente),
                    accionAud,
                    static_cast<uint8_t>(ResultadoAuditoria::INTRUSION_TERMINAL),
                    static_cast<uint8_t>(CriticidadEvento::ALTA)
                );
            }
            throw std::runtime_error(
                bloqueadoAhora
                    ? "SecurityError: slot bloqueado 2 minutos por fatiga de mensajeria (flood)"
                    : "SecurityError: limite de frecuencia de mensajes excedido, reduzca la tasa"
            );
        }
    }

    static SesionIniciadaDTO iniciarSesionUsuario(uint8_t slotCliente, const CredencialesLoginDTO& entrada) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        const uint32_t ahoraMs = millis();
        verificarSiSlotEstaBloqueado(slotCliente, entrada.usuario, ahoraMs);
        verificarCuotaMensajeria(slotCliente, "LOGIN");

        // Freno mínimo de 1 s tras intento fallido previo (anti fuerza bruta en ráfaga)
        bool enCooldownLogin = false;
        portENTER_CRITICAL(&muxSesiones);
        const EstadoSesionSlot& slotPrev = sesionesPorSlot[slotCliente];
        if (slotPrev.intentosFallidos > 0 && slotPrev.ultimoIntentoLoginMs != 0 &&
            static_cast<int32_t>(ahoraMs - slotPrev.ultimoIntentoLoginMs) < static_cast<int32_t>(COOLDOWN_LOGIN_MS)) {
            enCooldownLogin = true;
        }
        portEXIT_CRITICAL(&muxSesiones);

        if (enCooldownLogin) {
            throw std::runtime_error("AuthenticationError: espere 1 segundo antes de reintentar el inicio de sesion");
        }

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
            slot.infraccionesFlood = 0;
            slot.bloqueadoHastaMs  = 0;
            portEXIT_CRITICAL(&muxSesiones);

            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                usuarioDb.idUsuario,
                usuarioDb.usuario.c_str(),
                medioPorSlot(slotCliente),
                (usuarioDb.rol == RolUsuario::NINGUNO) ? "LOGIN_ROL_NINGUNO" : "LOGIN_EXITO",
                static_cast<uint8_t>(ResultadoAuditoria::EXITO),
                static_cast<uint8_t>(usuarioDb.rol == RolUsuario::NINGUNO ? CriticidadEvento::MEDIA : CriticidadEvento::BAJA)
            );

            return SesionIniciadaDTO{
                static_cast<uint8_t>(usuarioDb.rol),
                EXPIRACION_SESION_SEGUNDOS
            };
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            bool bloqueadoAhora = false;

            portENTER_CRITICAL(&muxSesiones);
            EstadoSesionSlot& slot    = sesionesPorSlot[slotCliente];
            slot.autenticado          = false;
            slot.idUsuario            = 0;
            slot.rol                  = RolUsuario::NINGUNO;
            slot.ultimoIntentoLoginMs = ahoraMs;
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

    static void cerrarSesionUsuario(uint8_t slotCliente, bool registrarAuditoria = true) {
        if (slotCliente > 8) return;
        bool estabaAuth = false;
        int32_t idPrev  = 0;
        char usrPrev[20]{};

        portENTER_CRITICAL(&muxSesiones);
        estabaAuth = sesionesPorSlot[slotCliente].autenticado;
        idPrev     = sesionesPorSlot[slotCliente].idUsuario;
        strncpy(usrPrev, sesionesPorSlot[slotCliente].usuario, sizeof(usrPrev) - 1);
        sesionesPorSlot[slotCliente].autenticado     = false;
        sesionesPorSlot[slotCliente].idUsuario       = 0;
        sesionesPorSlot[slotCliente].usuario[0]      = '\0';
        sesionesPorSlot[slotCliente].rol             = RolUsuario::NINGUNO;
        sesionesPorSlot[slotCliente].ultimoRequestMs = 0;
        portEXIT_CRITICAL(&muxSesiones);

        if (registrarAuditoria && estabaAuth) {
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                idPrev,
                usrPrev,
                medioPorSlot(slotCliente),
                "LOGOUT_USUARIO",
                static_cast<uint8_t>(ResultadoAuditoria::EXITO),
                static_cast<uint8_t>(CriticidadEvento::BAJA)
            );
        }
    }

    // Llamado por Core 0 al cerrar o reciclar un socket para bajar el permiso en el acto
    static void invalidarAutenticacionInmediata(uint8_t slotCliente) {
        cerrarSesionUsuario(slotCliente, false);
    }

    // Sincroniza o revoca al instante sesiones activas cuando un ADMIN cambia rol/estado de un usuario
    static uint8_t sincronizarSesionUsuario(const char* nombreUsuario, RolUsuario nuevoRol, bool activa) {
        if (!nombreUsuario || nombreUsuario[0] == '\0') return 0;
        uint8_t afectadas = 0;

        portENTER_CRITICAL(&muxSesiones);
        for (uint8_t i = 0; i < 9; ++i) {
            EstadoSesionSlot& slot = sesionesPorSlot[i];
            if (slot.autenticado && strcmp(slot.usuario, nombreUsuario) == 0) {
                if (!activa) {
                    slot.autenticado     = false;
                    slot.idUsuario       = 0;
                    slot.usuario[0]      = '\0';
                    slot.rol             = RolUsuario::NINGUNO;
                    slot.ultimoRequestMs = 0;
                } else {
                    slot.rol = nuevoRol;
                }
                afectadas++;
            }
        }
        portEXIT_CRITICAL(&muxSesiones);
        return afectadas;
    }

    // Revoca de inmediato todas las sesiones activas de un usuario (al eliminar o resetear clave)
    static uint8_t revocarSesionesDeUsuario(const char* nombreUsuario) {
        return sincronizarSesionUsuario(nombreUsuario, RolUsuario::NINGUNO, false);
    }

    static RolUsuario exigirSesionAutorizada(
        uint8_t slotCliente,
        RolUsuario rolMinimoRequerido,
        const char* accionSolicitada = nullptr
    ) {
        if (slotCliente > 8) {
            throw std::out_of_range("IndexError: slot de cliente invalido");
        }

        const uint32_t ahoraMs = millis();
        char nombreLocal[20]{};
        int32_t idLocal      = 0;
        bool estaAuth        = false;
        RolUsuario rolActual = RolUsuario::NINGUNO;

        portENTER_CRITICAL(&muxSesiones);
        strncpy(nombreLocal, sesionesPorSlot[slotCliente].usuario, sizeof(nombreLocal) - 1);
        portEXIT_CRITICAL(&muxSesiones);

        verificarSiSlotEstaBloqueado(slotCliente, nombreLocal, ahoraMs);
        verificarCuotaMensajeria(slotCliente, accionSolicitada);

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
            char accionAud[32]{};
            construirAccionAuditoria(accionAud, sizeof(accionAud), "SIN_SESION", accionSolicitada);
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_RED,
                0,
                "ANONIMO",
                medioPorSlot(slotCliente),
                accionAud,
                static_cast<uint8_t>(ResultadoAuditoria::INTRUSION_TERMINAL),
                static_cast<uint8_t>(CriticidadEvento::MEDIA)
            );
            throw std::runtime_error("AuthenticationError: sesion no autenticada. Inicie sesion primero");
        }

        if (static_cast<uint8_t>(rolActual) < static_cast<uint8_t>(rolMinimoRequerido)) {
            const bool esRolNinguno = (rolActual == RolUsuario::NINGUNO);
            char accionAud[32]{};
            construirAccionAuditoria(
                accionAud,
                sizeof(accionAud),
                esRolNinguno ? "DENEG_NINGUNO" : "DENEG_ROL",
                accionSolicitada
            );
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
                idLocal,
                nombreLocal,
                medioPorSlot(slotCliente),
                accionAud,
                static_cast<uint8_t>(ResultadoAuditoria::DENEGADO_ROL),
                static_cast<uint8_t>(esRolNinguno ? CriticidadEvento::ALTA : CriticidadEvento::MEDIA)
            );
            throw std::runtime_error(
                esRolNinguno
                    ? "PermissionError: cuenta con rol NINGUNO sin autorizacion para procedimientos remotos"
                    : (rolMinimoRequerido == RolUsuario::ADMIN
                        ? "PermissionError: operacion exclusiva para rol ADMIN"
                        : "PermissionError: rol insuficiente para ejecutar este procedimiento")
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
        const bool auth    = sesionesPorSlot[slotCliente].autenticado;
        const uint32_t ult = sesionesPorSlot[slotCliente].ultimoRequestMs;
        portEXIT_CRITICAL(&muxSesiones);
        if (!auth) return false;
        return static_cast<int32_t>(ahoraMs - ult) > static_cast<int32_t>(TIEMPO_MAXIMO_OCIO_MS);
    }
};

#endif