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
private:
    inline static uint8_t bufferSalidaL1[96] = {0};
    inline static size_t  longitudSalidaL1   = 0;

public:
    static const uint8_t* obtenerBufferSalidaL1()   { return bufferSalidaL1; }
    static size_t         obtenerLongitudSalidaL1() { return longitudSalidaL1; }

    static bool despacharProcedimientoConexion(const SobreMensajeRPC& sobre, uint16_t& puertoUdpSolicitadoOut) {
        puertoUdpSolicitadoOut = 0;
        longitudSalidaL1       = 0;

        if (sobre.idProcedimiento < PROC_L1_HANDSHAKE_UARTS || sobre.idProcedimiento > PROC_L1_CERRAR_WIFI) {
            return false;
        }

        try {
            switch (sobre.idProcedimiento) {
                case PROC_L1_HANDSHAKE_UARTS: {
                    HandshakeUartsDTO entrada{};
                    if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                        throw std::invalid_argument("TypeError: payload invalido para HandshakeUartsDTO");
                    }
                    PerfilEnlaceUartsDTO salida = RPConnectUARTS::iniciarConversacionSerie(entrada);
                    longitudSalidaL1 = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;
                }
                case PROC_L1_KEEPALIVE_UARTS:
                    RPConnectUARTS::notificarEstoyVivoSerie();
                    longitudSalidaL1 = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;

                case PROC_L1_CERRAR_UARTS:
                    RPConnectUARTS::cerrarConversacionSerie();
                    longitudSalidaL1 = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;

                case PROC_L1_KEEPALIVE_WIFI:
                    RPConnectWiFi::notificarEstoyVivoWiFi();
                    longitudSalidaL1 = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;

                case PROC_L1_SUSCRIBIR_UDP_WIFI: {
                    SuscripcionUdpDTO entrada{};
                    if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, entrada)) {
                        throw std::invalid_argument("TypeError: payload invalido para SuscripcionUdpDTO");
                    }
                    CanalStreamingUdpDTO salida = RPConnectWiFi::vincularCanalStreamingUDP(entrada);
                    puertoUdpSolicitadoOut = entrada.puertoEscuchaCliente;
                    longitudSalidaL1 = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, salida, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;
                }
                case PROC_L1_CERRAR_WIFI:
                    RPConnectWiFi::cerrarConversacionWiFi();
                    longitudSalidaL1 = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento, bufferSalidaL1, sizeof(bufferSalidaL1)
                    );
                    break;
            }
        } catch (const std::exception& e) {
            longitudSalidaL1 = CodificadorRPC::serializarExcepcionCruda(
                sobre.idTicket, sobre.idProcedimiento, e.what(), bufferSalidaL1, sizeof(bufferSalidaL1)
            );
        } catch (...) {
            longitudSalidaL1 = CodificadorRPC::serializarExcepcionCruda(
                sobre.idTicket, sobre.idProcedimiento, "RuntimeError en L1", bufferSalidaL1, sizeof(bufferSalidaL1)
            );
        }

        return true;
    }
};

#endif