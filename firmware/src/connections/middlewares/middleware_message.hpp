#ifndef MIDDLEWARE_MESSAGE_H
#define MIDDLEWARE_MESSAGE_H

#include "../remote/Controller/abstract/RPC_middleware_message.hpp"
#include "../remote/private/RPDatabase/RPSensor.hpp"
#include "../remote/private/RPSistema/RPSistema.hpp"

class MiddlewareMessage {
public:
    using FuncionDifusionL1 = void (*)(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal);

private:
    MascaraClientes suscriptoresSensoresTiempoReal{0};
    FuncionDifusionL1 callbackDifusionL1{nullptr};
    uint16_t contadorSecuenciaStream{0};

public:
    MiddlewareMessage() = default;

    // ¿Cómo lo hace?: Registra el puntero a función de L1 para que L3 pueda ordenar la difusión del bufferSalidaCompartido sin acoplamiento circular.
    void configurarCallbackDifusion(FuncionDifusionL1 callback) {
        callbackDifusionL1 = callback;
    }

    // ¿Cómo lo hace?: Enciende el bit (1 << slotCliente) en la máscara suscriptoresSensoresTiempoReal.
    void registrarSuscripcionSensores(uint8_t slotCliente) {
        if (slotCliente <= 8) {
            suscriptoresSensoresTiempoReal |= static_cast<MascaraClientes>(1u << slotCliente);
        }
    }

    // ¿Cómo lo hace?: Apaga el bit (1 << slotCliente) en suscriptoresSensoresTiempoReal y desvincula al cliente de cualquier ticket en vuelo en RPCBase.
    void limpiarSuscripcionesDeCliente(uint8_t slotCliente) {
        if (slotCliente <= 8) {
            suscriptoresSensoresTiempoReal &= static_cast<MascaraClientes>(~(1u << slotCliente));
            RPCBase::desvincularClienteDeTickets(slotCliente);
        }
    }

    // ¿Cómo lo hace?: Selecciona el procedimiento de negocio según sobre.idProcedimiento, aplica el control de Rol y Coalescing mediante RPCMiddlewareMessage y difunde el buffer único resultante por L1.
    void despacharPeticionAutorizada(const SobreMensajeRPC& sobre) {
        MascaraClientes destinatarios = 0;

        switch (sobre.idProcedimiento) {
            case PROC_L3_SUSCRIBIR_STREAM_SENSORES:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<uint8_t, EstadoSuscripcionDTO>(
                    sobre, RolUsuario::NO_ADMIN, SemanticaEntrega::TAL_VEZ, RPSensor::suscribirSensoresTiempoReal
                );
                if (destinatarios != 0) {
                    registrarSuscripcionSensores(sobre.slotClienteOrigen);
                }
                break;

            case PROC_L3_OBTENER_CENSOS_MAC:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<FiltroPorMacDTO, HistorialSensoresDTO>(
                    sobre, RolUsuario::NO_ADMIN, SemanticaEntrega::AL_MENOS_UNA_VEZ, RPSensor::obtenerCensosPorMac
                );
                break;

            case PROC_L3_OBTENER_CENSOS_MAC_FECHA:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<FiltroPorMacYFechaDTO, HistorialSensoresDTO>(
                    sobre, RolUsuario::NO_ADMIN, SemanticaEntrega::AL_MENOS_UNA_VEZ, RPSensor::obtenerCensosPorMacYFecha
                );
                break;

            case PROC_L3_REINICIAR_HARDWARE:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<uint8_t, ConfirmacionSistemaDTO>(
                    sobre, RolUsuario::ADMIN, SemanticaEntrega::EXACTAMENTE_UNA_VEZ, RPSistema::reiniciarHardware
                );
                break;

            case PROC_L3_ACTUALIZAR_CONFIG_WIFI:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<ConfiguracionWiFiDTO, ConfirmacionSistemaDTO>(
                    sobre, RolUsuario::ADMIN, SemanticaEntrega::EXACTAMENTE_UNA_VEZ, RPSistema::actualizarConfiguracionWiFi
                );
                break;

            case PROC_L3_DIAGNOSTICO_SISTEMA:
                destinatarios = RPCMiddlewareMessage::ejecutarProcedimientoNegocio<uint8_t, ConfirmacionSistemaDTO>(
                    sobre, RolUsuario::ADMIN, SemanticaEntrega::AL_MENOS_UNA_VEZ, RPSistema::ejecutarDiagnosticoSistema
                );
                break;

            default:
                return;
        }

        if (destinatarios != 0 && callbackDifusionL1 != nullptr && RPCBase::obtenerLongitudSalidaCompartida() > 0) {
            callbackDifusionL1(
                RPCBase::obtenerBufferSalidaCompartido(),
                RPCBase::obtenerLongitudSalidaCompartida(),
                destinatarios,
                CanalSalida::RESPUESTA_RPC_CONFIABLE
            );
        }
    }

    // ¿Cómo lo hace?: Si suscriptoresSensoresTiempoReal es 0 retorna de inmediato para ahorrar recursos; si hay suscriptores, serializa el lote una sola vez y lo envía a todos por el canal STREAMING_TELEMETRIA.
    void emitirCicloStreamingSensores(const HistorialSensoresDTO& loteActual) {
        if (suscriptoresSensoresTiempoReal == 0 || callbackDifusionL1 == nullptr) {
            return;
        }

        uint8_t bufferStream[RPCBase::CAPACIDAD_BUFFER_COMPARTIDO];
        const size_t longitud = CodificadorRPC::serializarEnBufferCompartido(
            contadorSecuenciaStream++,
            PROC_L3_SUSCRIBIR_STREAM_SENSORES,
            loteActual,
            bufferStream,
            sizeof(bufferStream)
        );

        if (longitud > 0) {
            callbackDifusionL1(bufferStream, longitud, suscriptoresSensoresTiempoReal, CanalSalida::STREAMING_TELEMETRIA);
        }
    }

    // ¿Cómo lo hace?: Devuelve la máscara de bits actual de clientes suscritos al streaming de sensores.
    MascaraClientes obtenerMascaraSuscriptores() const {
        return suscriptoresSensoresTiempoReal;
    }
};

#endif