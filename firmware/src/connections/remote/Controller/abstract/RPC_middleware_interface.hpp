#ifndef RPC_MIDDLEWARE_INTERFACE_H
#define RPC_MIDDLEWARE_INTERFACE_H

#include "RPC_base.hpp"
#include "../marshalling/encode.hpp"
#include "../unmarshalling/decode.hpp"
#include "../../public/RPConnectUART-S.hpp"
#include "../../public/RPConnectWiFi.hpp"
#include <connections/remote/public/RPConnectUART-S.hpp>

enum IdentificadorProcedimientoL1 : uint16_t {
    PROC_L1_HANDSHAKE_UARTS      = 0x0101,
    PROC_L1_KEEPALIVE_UARTS      = 0x0102,
    PROC_L1_CERRAR_UARTS         = 0x0103,
    PROC_L1_KEEPALIVE_WIFI       = 0x0104,
    PROC_L1_SUSCRIBIR_UDP_WIFI   = 0x0105,
    PROC_L1_CERRAR_WIFI          = 0x0106
};

class RPCMiddlewareInterface : public RPCBase {
public:
    // ¿Cómo lo hace?: Evalúa si idProcedimiento está en el rango 0x0100..0x01FF, deserializa el DTO de conexión, ejecuta el método de RPConnect* y escribe la respuesta en bufferSalidaCompartido.
    static bool despacharProcedimientoConexion(const SobreMensajeRPC& sobre, uint16_t& puertoUdpSolicitadoOut) {
        puertoUdpSolicitadoOut = 0;
        longitudSalidaCompartida = 0;

        switch (sobre.idProcedimiento) {
            case PROC_L1_HANDSHAKE_UARTS: {
                HandshakeUartsDTO entrada{};
                EstadoEnlaceUartsDTO salida{false};
                if (DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                    RPConnectUARTS::iniciarConversacionSerie(entrada, salida);
                }
                longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                    sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            case PROC_L1_KEEPALIVE_UARTS: {
                RPConnectUARTS::notificarEstoyVivoSerie();
                longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                    sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            case PROC_L1_CERRAR_UARTS: {
                RPConnectUARTS::cerrarConversacionSerie();
                longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                    sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            case PROC_L1_KEEPALIVE_WIFI: {
                RPConnectWiFi::notificarEstoyVivoWiFi();
                longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                    sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            case PROC_L1_SUSCRIBIR_UDP_WIFI: {
                SuscripcionUdpDTO entrada{0};
                RespuestaSuscripcionUdpDTO salida{0, false};
                if (DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                    RPConnectWiFi::vincularCanalStreamingUDP(entrada, salida);
                    if (salida.registrado) {
                        puertoUdpSolicitadoOut = entrada.puertoEscuchaCliente;
                    }
                }
                longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                    sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            case PROC_L1_CERRAR_WIFI: {
                RPConnectWiFi::cerrarConversacionWiFi();
                longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                    sobre.idTicket, sobre.idProcedimiento, bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                );
                return true;
            }

            default:
                return false;
        }
    }
};

#endif