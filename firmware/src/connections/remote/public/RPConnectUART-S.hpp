#ifndef RP_CONNECT_UARTS_H
#define RP_CONNECT_UARTS_H

#include <stdint.h>
#include <stdexcept>
#include "../Controller/abstract/RPC_base.hpp"

struct HandshakeUartsDTO {
    uint8_t versionProtocolo;
};

struct PerfilEnlaceUartsDTO {
    uint8_t  versionServidor;
    uint16_t mtuBytes;
};

class RPConnectUARTS {
public:
    static PerfilEnlaceUartsDTO iniciarConversacionSerie(const HandshakeUartsDTO& entrada) {
        if (entrada.versionProtocolo != 1) {
            throw std::invalid_argument("ProtocolError: version UART-S no soportada (se esperaba 1)");
        }
        return PerfilEnlaceUartsDTO{1, static_cast<uint16_t>(RPCBase::CAPACIDAD_BUFFER_COMPARTIDO)};
    }

    static void notificarEstoyVivoSerie() {}
    static void cerrarConversacionSerie() {}
};

#endif