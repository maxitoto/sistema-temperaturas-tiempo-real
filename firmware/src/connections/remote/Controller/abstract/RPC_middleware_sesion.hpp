#ifndef RPC_MIDDLEWARE_SESION_H
#define RPC_MIDDLEWARE_SESION_H

#include <stdexcept>
#include "RPC_base.hpp"
#include "../unmarshalling/decode.hpp"
#include "../../public/RPAuth.hpp"

enum IdentificadorProcedimientoL2 : uint16_t {
    PROC_L2_LOGIN_USUARIO  = 0x0201,
    PROC_L2_LOGOUT_USUARIO = 0x0202
};

class RPCMiddlewareSesion : public RPCBase {
public:
    static bool despacharProcedimientoAutenticacion(const SobreMensajeRPC& sobre, bool& fueLogoutOut) {
        fueLogoutOut             = false;
        longitudSalidaCompartida = 0;

        if (sobre.idProcedimiento != PROC_L2_LOGIN_USUARIO && sobre.idProcedimiento != PROC_L2_LOGOUT_USUARIO) {
            return false;
        }

        ejecutarConCapturaNativa(sobre.idTicket, sobre.idProcedimiento, [&]() {
            if (sobre.idProcedimiento == PROC_L2_LOGIN_USUARIO) {
                CredencialesLoginDTO credenciales{};
                if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, credenciales)) {
                    throw std::invalid_argument("TypeError: payload invalido para CredencialesLoginDTO");
                }
                SesionIniciadaDTO sesion = RPAuth::iniciarSesionUsuario(sobre.slotClienteOrigen, credenciales);
                longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                    sobre.idTicket, sobre.idProcedimiento, sesion, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
            } else {
                RPAuth::cerrarSesionUsuario(sobre.slotClienteOrigen);
                fueLogoutOut = true;
                longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                    sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
            }
        });
        return true;
    }
};

#endif