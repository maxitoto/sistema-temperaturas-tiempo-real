#ifndef WIFI_UDP_PLAIN_RUNTIME_H
#define WIFI_UDP_PLAIN_RUNTIME_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct SuscripcionUDP {
    char ipDestino[16];
    uint16_t puertoDestino;
    uint32_t ultimoEnvioMs;
    bool activa;
};

class WiFiUdpPlainRuntime {
public:
    static constexpr uint16_t PUERTO_ORIGEN_STREAM_UDP = 8266;
    static constexpr size_t TAMANO_MAXIMO_DATAGRAMA = 1024;
    static constexpr uint8_t MAXIMO_SLOTS_WIFI = 8;

private:
    WiFiUDP socketUdp;
    SuscripcionUDP suscriptoresPorSlot[9]{};

public:
    WiFiUdpPlainRuntime() {
        for (uint8_t i = 0; i <= MAXIMO_SLOTS_WIFI; ++i) {
            suscriptoresPorSlot[i].ipDestino[0] = '\0';
            suscriptoresPorSlot[i].puertoDestino = 0;
            suscriptoresPorSlot[i].ultimoEnvioMs = 0;
            suscriptoresPorSlot[i].activa = false;
        }
    }

    bool iniciarSocketStreaming() {
        return socketUdp.begin(PUERTO_ORIGEN_STREAM_UDP) == 1;
    }

    bool suscribirClienteAStream(uint8_t slotCliente, const char* ipCliente, uint16_t puertoUdpCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_SLOTS_WIFI || ipCliente == nullptr || ipCliente[0] == '\0' || puertoUdpCliente == 0) {
            return false;
        }

        strncpy(suscriptoresPorSlot[slotCliente].ipDestino, ipCliente, sizeof(suscriptoresPorSlot[slotCliente].ipDestino) - 1);
        suscriptoresPorSlot[slotCliente].ipDestino[sizeof(suscriptoresPorSlot[slotCliente].ipDestino) - 1] = '\0';
        suscriptoresPorSlot[slotCliente].puertoDestino = puertoUdpCliente;
        suscriptoresPorSlot[slotCliente].activa = true;
        return true;
    }

    void desuscribirClienteDeStream(uint8_t slotCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_SLOTS_WIFI) {
            return;
        }
        suscriptoresPorSlot[slotCliente].activa = false;
        suscriptoresPorSlot[slotCliente].ipDestino[0] = '\0';
        suscriptoresPorSlot[slotCliente].puertoDestino = 0;
    }

    uint8_t emitirDatagramaASuscriptores(const uint8_t* payloadCompartido, size_t longitud, uint16_t mascaraDestinatarios) {
        if (payloadCompartido == nullptr || longitud == 0 || longitud > TAMANO_MAXIMO_DATAGRAMA) {
            return 0;
        }

        uint8_t enviadosOk = 0;
        const uint32_t ahoraMs = millis();

        for (uint8_t slot = 1; slot <= MAXIMO_SLOTS_WIFI; ++slot) {
            const uint16_t bitSlot = static_cast<uint16_t>(1u << slot);
            if ((mascaraDestinatarios & bitSlot) && suscriptoresPorSlot[slot].activa) {
                if (socketUdp.beginPacket(suscriptoresPorSlot[slot].ipDestino, suscriptoresPorSlot[slot].puertoDestino) == 1) {
                    const size_t escritos = socketUdp.write(payloadCompartido, longitud);
                    if (escritos == longitud && socketUdp.endPacket() == 1) {
                        suscriptoresPorSlot[slot].ultimoEnvioMs = ahoraMs;
                        enviadosOk++;
                    }
                }
            }
        }
        return enviadosOk;
    }

    uint8_t obtenerCantidadSuscriptoresActivos() const {
        uint8_t activos = 0;
        for (uint8_t slot = 1; slot <= MAXIMO_SLOTS_WIFI; ++slot) {
            if (suscriptoresPorSlot[slot].activa) {
                activos++;
            }
        }
        return activos;
    }
};

#endif