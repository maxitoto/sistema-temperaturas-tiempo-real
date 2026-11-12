#ifndef RPC_MIDDLEWARE_SESION_H
#define RPC_MIDDLEWARE_SESION_H

#include "RPC_base.hpp"
#include "../marshalling/encode.hpp"
#include "../unmarshalling/decode.hpp"
#include "../../public/RPAuth.hpp"

enum IdentificadorProcedimientoL2 : uint16_t {
    PROC_L2_LOGIN_USUARIO  = 0x0201,
    PROC_L2_LOGOUT_USUARIO = 0x0202
};

class RPCMiddlewareSesion : public RPCBase {
public:
    // ¿Cómo lo hace?: Procesa login (0x0201) y logout (0x0202), ejecuta RPAuth y serializa el ResultadoLoginDTO o acuse en bufferSalidaCompartido.
    static bool despacharProcedimientoAutenticacion(
        const SobreMensajeRPC& sobre,
        bool& loginExitosoOut,
        RolUsuario& rolAsignadoOut
    ) {
        loginExitosoOut = false;
        rolAsignadoOut = RolUsuario::NINGUNO;
        longitudSalidaCompartida = 0;

        if (sobre.idProcedimiento == PROC_L2_LOGIN_USUARIO) {
            CredencialesLoginDTO credenciales{};
            ResultadoLoginDTO resultado{false, 0, 0};

            if (DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, credenciales)) {
                RPAuth::iniciarSesionUsuario(credenciales, resultado);
                loginExitosoOut = resultado.autenticado;
                rolAsignadoOut = static_cast<RolUsuario>(resultado.rolAsignado);
            }

            longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                sobre.idTicket, sobre.idProcedimiento, resultado, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
            );
            return true;
        }

        if (sobre.idProcedimiento == PROC_L2_LOGOUT_USUARIO) {
            RPAuth::cerrarSesionUsuario();
            longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
            );
            return true;
        }

        return false;
    }
};

#endif