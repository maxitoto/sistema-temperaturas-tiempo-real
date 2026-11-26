#ifndef RP_CONNECT_WIFI_H
#define RP_CONNECT_WIFI_H

#include <stdint.h>
#include <stdexcept>
#include "../Controller/abstract/RPC_base.hpp"

struct SuscripcionUdpDTO {
    uint16_t puertoEscuchaCliente;
};

struct CanalStreamingUdpDTO {
    uint16_t puertoEmisorServidor;
};

class RPConnectWiFi {
public:
    static constexpr uint16_t PUERTO_SERVIDOR_UDP_FIJO = 8266;

    static void notificarEstoyVivoWiFi() {}

    static CanalStreamingUdpDTO vincularCanalStreamingUDP(const SuscripcionUdpDTO& entrada) {
        if (entrada.puertoEscuchaCliente == 0) {
            throw std::invalid_argument("ValueError: el puerto UDP de escucha no puede ser 0");
        }
        return CanalStreamingUdpDTO{PUERTO_SERVIDOR_UDP_FIJO};
    }

    static void cerrarConversacionWiFi() {}
};

#endif