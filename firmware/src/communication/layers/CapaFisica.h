// Archivo: ./src/communication/layers/CapaFisica.h
#pragma once
#include <Arduino.h>
#include "plugins/conectividad/usb/PuertoUSB.h"
#include "plugins/conectividad/modem/ServidorUDP.h"

class CapaFisica {
  private:
    PuertoUSB* usb;
    ServidorUDP* udpServer;

  public:
    CapaFisica(PuertoUSB* puertoUsb, ServidorUDP* servidorUdp)
        : usb(puertoUsb), udpServer(servidorUdp) {}

    int obtenerTotalMedios() const {
        int redes = udpServer ? udpServer->obtenerMaxClientes() : 0;
        return 1 + redes;
    }

    bool estaActivo(int idMedio) {
        if (idMedio == 0) {
            return usb && (usb->obtenerStream() != nullptr);
        }
        int indiceUDP = idMedio - 1;
        return udpServer ? udpServer->clienteActivo(indiceUDP) : false;
    }

    int bytesDisponibles(int idMedio) {
        if (idMedio == 0) {
            Stream* canal = usb ? usb->obtenerStream() : nullptr;
            return canal ? canal->available() : 0;
        }
        int indiceUDP = idMedio - 1;
        return udpServer ? udpServer->bytesDisponiblesCliente(indiceUDP) : 0;
    }

    // CAMINO DE SUBIDA (RX)
    size_t recibirBytes(int idMedio, uint8_t* bufferDestino, size_t maxLongitud) {
        if (!bufferDestino || maxLongitud == 0) return 0;

        if (idMedio == 0) {
            Stream* canal = usb ? usb->obtenerStream() : nullptr;
            if (!canal) return 0;
            int disponibles = canal->available();
            if (disponibles <= 0) return 0;
            size_t aLeer = (size_t)disponibles;
            if (aLeer > maxLongitud) aLeer = maxLongitud;
            return canal->readBytes(bufferDestino, aLeer);
        }

        int indiceUDP = idMedio - 1;
        return udpServer ? udpServer->recibirBytesCliente(indiceUDP, bufferDestino, maxLongitud) : 0;
    }

    // CAMINO DE BAJADA (TX)
    size_t enviarBytes(int idMedio, const uint8_t* datos, size_t longitud) {
        if (!datos || longitud == 0) return 0;

        if (idMedio == 0) {
            Stream* canal = usb ? usb->obtenerStream() : nullptr;
            if (!canal) return 0;
            size_t escritos = canal->write(datos, longitud);
            yield();
            return escritos;
        }

        int indiceUDP = idMedio - 1;
        return udpServer ? udpServer->enviarBytesCliente(indiceUDP, datos, longitud) : 0;
    }
};