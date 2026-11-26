#ifndef MIDDLEWARE_SESION_H
#define MIDDLEWARE_SESION_H

#include <Arduino.h>
#include "../remote/Controller/abstract/RPC_middleware_sesion.hpp"
#include "middleware_message.hpp"

class MiddlewareSesion {
public:
    using FuncionCierreFisicoL1 = void (*)(uint8_t slotCliente);
    using FuncionDifusionL1     = void (*)(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal);

private:
    MiddlewareMessage&    capaMensajeL3;
    FuncionCierreFisicoL1 callbackCierreL1{nullptr};
    FuncionDifusionL1     callbackDifusionL1{nullptr};

public:
    explicit MiddlewareSesion(MiddlewareMessage& l3) : capaMensajeL3(l3) {}

    void configurarCallbacksL1(FuncionDifusionL1 cbDifusion, FuncionCierreFisicoL1 cbCierre) {
        callbackDifusionL1 = cbDifusion;
        callbackCierreL1   = cbCierre;
    }

    void procesarMensajeEntrante(SobreMensajeRPC& sobre) {
        if (sobre.slotClienteOrigen > 8) return;

        const MascaraClientes mascaraSlot = static_cast<MascaraClientes>(1u << sobre.slotClienteOrigen);
        bool fueLogout = false;

        // 1. Si es Login o Logout, lo procesa RPAuth (cualquier error de login o bloqueo lo emite RPAuth como mensaje)
        if (RPCMiddlewareSesion::despacharProcedimientoAutenticacion(sobre, fueLogout)) {
            if (fueLogout) {
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
            return;
        }

        // 2. Cualquier otro procedimiento pasa a L3 (donde RPAuth valida sesión/rol dentro del try/catch del procedimiento)
        capaMensajeL3.despacharPeticionAutorizada(sobre);
    }

    void verificarExpiracionSesionesPorOcio() {
        const uint32_t ahoraMs = millis();
        for (uint8_t i = 0; i <= 8; ++i) {
            if (RPAuth::expiroPorOcio(i, ahoraMs)) {
                forzarCierreSesionPorDesconexion(i);
                if (callbackCierreL1 != nullptr) {
                    callbackCierreL1(i);
                }
            }
        }
    }

    void forzarCierreSesionPorDesconexion(uint8_t slotCliente) {
        if (slotCliente > 8) return;
        RPAuth::cerrarSesionUsuario(slotCliente);
        capaMensajeL3.limpiarSuscripcionesDeCliente(slotCliente);
    }

    bool estaClienteAutenticado(uint8_t slotCliente) const {
        return RPAuth::estaClienteAutenticado(slotCliente);
    }
};

#endif