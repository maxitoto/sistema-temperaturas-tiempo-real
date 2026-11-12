#ifndef MIDDLEWARE_SESION_H
#define MIDDLEWARE_SESION_H

#include <Arduino.h>
#include "../remote/Controller/abstract/RPC_middleware_sesion.hpp"
#include "middleware_message.hpp"

struct EntradaCacheUsuario {
    char usuario[32];
    char claveHash[64];
    RolUsuario rol;
    bool ocupado;
};

struct EstadoSesionSlot {
    RolUsuario rol;
    uint32_t ultimoRequestMs;
    uint32_t bloqueadoHastaMs;
    uint8_t intentosFallidos;
    bool autenticado;
};

class MiddlewareSesion {
public:
    static constexpr uint32_t TIEMPO_MAXIMO_OCIO_MS = RPAuth::EXPIRACION_SESION_SEGUNDOS * 1000UL;
    static constexpr uint32_t TIEMPO_BLOQUEO_INTENTOS_MS = 600000;  // 10 minutos
    static constexpr uint8_t MAXIMO_INTENTOS_FALLIDOS = 3;
    static constexpr uint8_t CAPACIDAD_CACHE_USUARIOS = 8;

    using FuncionCierreFisicoL1 = void (*)(uint8_t slotCliente);
    using FuncionDifusionL1 = void (*)(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal);

private:
    MiddlewareMessage& capaMensajeL3;
    EstadoSesionSlot sesionesPorSlot[9]{};
    EntradaCacheUsuario cacheUsuarios[CAPACIDAD_CACHE_USUARIOS]{};
    FuncionCierreFisicoL1 callbackCierreL1{nullptr};
    FuncionDifusionL1 callbackDifusionL1{nullptr};

public:
    explicit MiddlewareSesion(MiddlewareMessage& l3) : capaMensajeL3(l3) {}

    void configurarCallbacksL1(FuncionDifusionL1 cbDifusion, FuncionCierreFisicoL1 cbCierre) {
        callbackDifusionL1 = cbDifusion;
        callbackCierreL1 = cbCierre;
    }

    void procesarMensajeEntrante(SobreMensajeRPC& sobre) {
        if (sobre.slotClienteOrigen > 8) {
            return;
        }

        EstadoSesionSlot& slot = sesionesPorSlot[sobre.slotClienteOrigen];
        const uint32_t ahoraMs = millis();
        const MascaraClientes mascaraSlot = static_cast<MascaraClientes>(1u << sobre.slotClienteOrigen);

        if (slot.bloqueadoHastaMs != 0 && static_cast<int32_t>(ahoraMs - slot.bloqueadoHastaMs) < 0) {
            if (callbackDifusionL1 != nullptr) {
                uint8_t bufferErr[128];
                const size_t lenErr = CodificadorRPC::serializarErrorRPC(
                    sobre.idTicket,
                    sobre.idProcedimiento,
                    CodigoErrorRPC::SESION_BLOQUEADA,
                    slot.rol,
                    RolUsuario::NINGUNO,
                    "PROXY_SESSION_LOCKED: Sesion bloqueada temporalmente por intentos fallidos.",
                    bufferErr,
                    sizeof(bufferErr)
                );
                if (lenErr > 0) {
                    callbackDifusionL1(bufferErr, lenErr, mascaraSlot, CanalSalida::RESPUESTA_RPC_CONFIABLE);
                }
            }
            return;
        } else if (slot.bloqueadoHastaMs != 0) {
            slot.bloqueadoHastaMs = 0;
            slot.intentosFallidos = 0;
        }

        if (sobre.idProcedimiento == PROC_L2_LOGIN_USUARIO || sobre.idProcedimiento == PROC_L2_LOGOUT_USUARIO) {
            bool loginOk = false;
            RolUsuario rolObtenido = RolUsuario::NINGUNO;

            // consulta DB

            if (RPCMiddlewareSesion::despacharProcedimientoAutenticacion(sobre, loginOk, rolObtenido)) {
                if (sobre.idProcedimiento == PROC_L2_LOGIN_USUARIO) {
                    if (loginOk) {
                        slot.autenticado = true;
                        slot.rol = rolObtenido;
                        slot.ultimoRequestMs = ahoraMs;
                        slot.intentosFallidos = 0;
                    } else {
                        slot.autenticado = false;
                        slot.rol = RolUsuario::NINGUNO;
                        slot.intentosFallidos++;
                        if (slot.intentosFallidos >= MAXIMO_INTENTOS_FALLIDOS) {
                            slot.bloqueadoHastaMs = ahoraMs + TIEMPO_BLOQUEO_INTENTOS_MS;
                        }
                    }
                } else if (sobre.idProcedimiento == PROC_L2_LOGOUT_USUARIO) {
                    forzarCierreSesionPorDesconexion(sobre.slotClienteOrigen);
                }

                if (callbackDifusionL1 != nullptr && RPCBase::obtenerLongitudSalidaCompartida() > 0) {
                    callbackDifusionL1(
                        RPCBase::obtenerBufferSalidaCompartido(),
                        RPCBase::obtenerLongitudSalidaCompartida(),
                        mascaraSlot,
                        CanalSalida::RESPUESTA_RPC_CONFIABLE
                    );
                }
            }
            return;
        }

        if (!slot.autenticado) {
            if (callbackDifusionL1 != nullptr) {
                uint8_t bufferErr[128];
                const size_t lenErr = CodificadorRPC::serializarErrorRPC(
                    sobre.idTicket,
                    sobre.idProcedimiento,
                    CodigoErrorRPC::NO_AUTENTICADO,
                    RolUsuario::NINGUNO,
                    RolUsuario::NO_ADMIN,
                    "PROXY_UNAUTHENTICATED: Sesion no autenticada. Inicie sesion primero.",
                    bufferErr,
                    sizeof(bufferErr)
                );
                if (lenErr > 0) {
                    callbackDifusionL1(bufferErr, lenErr, mascaraSlot, CanalSalida::RESPUESTA_RPC_CONFIABLE);
                }
            }
            return;
        }

        slot.ultimoRequestMs = ahoraMs;
        sobre.rolAutenticado = slot.rol;
        capaMensajeL3.despacharPeticionAutorizada(sobre);
    }

    void verificarExpiracionSesionesPorOcio() {
        const uint32_t ahoraMs = millis();
        for (uint8_t i = 0; i <= 8; ++i) {
            if (sesionesPorSlot[i].autenticado) {
                if (static_cast<int32_t>(ahoraMs - sesionesPorSlot[i].ultimoRequestMs) > static_cast<int32_t>(TIEMPO_MAXIMO_OCIO_MS)) {
                    forzarCierreSesionPorDesconexion(i);
                    if (callbackCierreL1 != nullptr) {
                        callbackCierreL1(i);
                    }
                }
            }
        }
    }

    void forzarCierreSesionPorDesconexion(uint8_t slotCliente) {
        if (slotCliente > 8) {
            return;
        }
        sesionesPorSlot[slotCliente].autenticado = false;
        sesionesPorSlot[slotCliente].rol = RolUsuario::NINGUNO;
        sesionesPorSlot[slotCliente].ultimoRequestMs = 0;
        capaMensajeL3.limpiarSuscripcionesDeCliente(slotCliente);
    }

    bool estaClienteAutenticado(uint8_t slotCliente) const {
        return (slotCliente <= 8) ? sesionesPorSlot[slotCliente].autenticado : false;
    }
};

#endif