#ifndef RPC_MIDDLEWARE_MESSAGE_H
#define RPC_MIDDLEWARE_MESSAGE_H

#include "RPC_base.hpp"
#include "../marshalling/encode.hpp"
#include "../unmarshalling/decode.hpp"

enum IdentificadorProcedimientoL3 : uint16_t {
    PROC_L3_SUSCRIBIR_STREAM_SENSORES = 0x0301,
    PROC_L3_OBTENER_CENSOS_MAC        = 0x0302,
    PROC_L3_OBTENER_CENSOS_MAC_FECHA  = 0x0303,
    PROC_L3_REINICIAR_HARDWARE        = 0x0310,
    PROC_L3_ACTUALIZAR_CONFIG_WIFI    = 0x0311,
    PROC_L3_DIAGNOSTICO_SISTEMA       = 0x0312
};

class RPCMiddlewareMessage : public RPCBase {
public:
    template <typename EntradaDTO, typename SalidaDTO>
    static MascaraClientes ejecutarProcedimientoNegocio(
        const SobreMensajeRPC& sobre,
        RolUsuario rolMinimoRequerido,
        SemanticaEntrega semantica,
        void (*procedimientoPuro)(const EntradaDTO&, SalidaDTO&)
    ) {
        (void)semantica;
        longitudSalidaCompartida = 0;
        const MascaraClientes mascaraSolicitante = static_cast<MascaraClientes>(1u << sobre.slotClienteOrigen);

        if (static_cast<uint8_t>(sobre.rolAutenticado) < static_cast<uint8_t>(rolMinimoRequerido)) {
            const char* msgRol = (rolMinimoRequerido == RolUsuario::ADMIN)
                ? "RPC_FORBIDDEN_ADMIN: Operacion exclusiva para rol ADMIN"
                : "RPC_FORBIDDEN_NO_ADMIN: Operacion requiere rol NO_ADMIN o ADMIN";

            longitudSalidaCompartida = CodificadorRPC::serializarErrorRPC(
                sobre.idTicket,
                sobre.idProcedimiento,
                CodigoErrorRPC::ROL_INSUFICIENTE,
                sobre.rolAutenticado,
                rolMinimoRequerido,
                msgRol,
                bufferSalidaCompartido,
                CAPACIDAD_BUFFER_COMPARTIDO
            );
            return mascaraSolicitante;
        }

        if (intentarAcoplarAPeticionEnVuelo(sobre.hashPeticion, sobre.slotClienteOrigen)) {
            return 0;
        }

        const int8_t idxTicket = abrirTicketEjecucion(
            sobre.idTicket, sobre.idProcedimiento, sobre.hashPeticion, sobre.slotClienteOrigen
        );
        if (idxTicket < 0) {
            longitudSalidaCompartida = CodificadorRPC::serializarErrorRPC(
                sobre.idTicket,
                sobre.idProcedimiento,
                CodigoErrorRPC::SERVIDOR_OCUPADO,
                sobre.rolAutenticado,
                rolMinimoRequerido,
                "Tabla de tickets llena en el servidor ESP32",
                bufferSalidaCompartido,
                CAPACIDAD_BUFFER_COMPARTIDO
            );
            return mascaraSolicitante;
        }

        EntradaDTO dtoEntrada{};
        if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, dtoEntrada)) {
            cerrarTicketYObtenerDestinatarios(idxTicket);
            longitudSalidaCompartida = CodificadorRPC::serializarErrorRPC(
                sobre.idTicket,
                sobre.idProcedimiento,
                CodigoErrorRPC::ARGUMENTOS_INVALIDOS,
                sobre.rolAutenticado,
                rolMinimoRequerido,
                "Payload de argumentos invalido o incompleto",
                bufferSalidaCompartido,
                CAPACIDAD_BUFFER_COMPARTIDO
            );
            return mascaraSolicitante;
        }

        SalidaDTO dtoSalida{};
        procedimientoPuro(dtoEntrada, dtoSalida);

        longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
            sobre.idTicket, sobre.idProcedimiento, dtoSalida, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
        );

        return cerrarTicketYObtenerDestinatarios(idxTicket);
    }
};

#endif