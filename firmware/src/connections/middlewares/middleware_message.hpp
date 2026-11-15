#ifndef MIDDLEWARE_MESSAGE_H
#define MIDDLEWARE_MESSAGE_H

#include "../remote/Controller/abstract/RPC_middleware_message.hpp"
#include "../remote/private/RPDatabase/RPSensor.hpp"
#include "../remote/private/RPDatabase/RPUsuario.hpp"
#include "../remote/private/RPSistema/RPSistema.hpp"


class MiddlewareMessage {
public:
    using FuncionDifusionL1 = RPCMiddlewareMessage::FuncionDifusionL1;

private:
    FuncionDifusionL1 callbackDifusionL1{nullptr};

public:
    MiddlewareMessage() = default;

    void configurarCallbackDifusion(FuncionDifusionL1 callback) {
        callbackDifusionL1 = callback;
    }

    void limpiarSuscripcionesDeCliente(uint8_t slotCliente) {
        if (slotCliente <= 8) {
            RPCMiddlewareMessage::desuscribirClienteDeTodosLosStreams(slotCliente);
            RPCBase::desvincularClienteDeTickets(slotCliente);
        }
    }

    void despacharPeticionAutorizada(const SobreMensajeRPC& sobre) {
        const MascaraClientes destinatarios = RPCMiddlewareMessage::despacharDesdeRegistro(sobre);

        if (destinatarios != 0 && callbackDifusionL1 != nullptr && RPCBase::obtenerLongitudSalidaCompartida() > 0) {
            callbackDifusionL1(
                RPCBase::obtenerBufferSalidaCompartido(),
                RPCBase::obtenerLongitudSalidaCompartida(),
                destinatarios,
                CanalSalida::RESPUESTA_RPC_CONFIABLE
            );
        }
    }

    // Ejecuta y difunde todos los procedimientos de semántica talVez que tengan clientes suscritos
    void emitirStreamsActivos() {
        RPCMiddlewareMessage::emitirStreamsTalVezActivos(callbackDifusionL1);
    }

    MascaraClientes obtenerMascaraSuscriptores() const {
        return RPCMiddlewareMessage::obtenerMascaraTotalSuscriptoresStream();
    }
};

#endif