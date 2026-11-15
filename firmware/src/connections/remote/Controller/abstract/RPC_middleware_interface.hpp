#ifndef RPC_MIDDLEWARE_INTERFACE_H
#define RPC_MIDDLEWARE_INTERFACE_H

#include <stdexcept>
#include "RPC_base.hpp"
#include "../unmarshalling/decode.hpp"
#include "../../public/RPConnectUART-S.hpp"
#include "../../public/RPConnectWiFi.hpp"

enum IdentificadorProcedimientoL1 : uint16_t {
    PROC_L1_HANDSHAKE_UARTS    = 0x0101,
    PROC_L1_KEEPALIVE_UARTS    = 0x0102,
    PROC_L1_CERRAR_UARTS       = 0x0103,
    PROC_L1_KEEPALIVE_WIFI     = 0x0104,
    PROC_L1_SUSCRIBIR_UDP_WIFI = 0x0105,
    PROC_L1_CERRAR_WIFI        = 0x0106
};

class RPCMiddlewareInterface : public RPCBase {
public:
    static bool despacharProcedimientoConexion(const SobreMensajeRPC& sobre, uint16_t& puertoUdpSolicitadoOut) {
        puertoUdpSolicitadoOut   = 0;
        longitudSalidaCompartida = 0;

        if (sobre.idProcedimiento < PROC_L1_HANDSHAKE_UARTS || sobre.idProcedimiento > PROC_L1_CERRAR_WIFI) {
            return false;
        }

        ejecutarConCapturaNativa(sobre.idTicket, sobre.idProcedimiento, [&]() {
            switch (sobre.idProcedimiento) {
                case PROC_L1_HANDSHAKE_UARTS: {
                    HandshakeUartsDTO entrada{};
                    if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                        throw std::invalid_argument("TypeError: payload invalido para HandshakeUartsDTO");
                    }
                    PerfilEnlaceUartsDTO salida = RPConnectUARTS::iniciarConversacionSerie(entrada);
                    longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;
                }
                case PROC_L1_KEEPALIVE_UARTS:
                    RPConnectUARTS::notificarEstoyVivoSerie();
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;

                case PROC_L1_CERRAR_UARTS:
                    RPConnectUARTS::cerrarConversacionSerie();
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;

                case PROC_L1_KEEPALIVE_WIFI:
                    RPConnectWiFi::notificarEstoyVivoWiFi();
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;

                case PROC_L1_SUSCRIBIR_UDP_WIFI: {
                    SuscripcionUdpDTO entrada{};
                    if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                        throw std::invalid_argument("TypeError: payload invalido para SuscripcionUdpDTO");
                    }
                    CanalStreamingUdpDTO salida = RPConnectWiFi::vincularCanalStreamingUDP(entrada);
                    puertoUdpSolicitadoOut = entrada.puertoEscuchaCliente;
                    longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;
                }
                case PROC_L1_CERRAR_WIFI:
                    RPConnectWiFi::cerrarConversacionWiFi();
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                    break;
            }
        });
        return true;
    }
};

#endif