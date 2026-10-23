// Archivo: ./src/communication/CapaFisica.h
#pragma once
#include <Arduino.h>
#include "plugins/conectividad/usb/PuertoUSB.h"
#include "plugins/conectividad/modem/ServidorTCP.h"

class CapaFisica {
  private:
    PuertoUSB* usb;
    ServidorTCP* tcp;

    // Medio 0: Cable USB | Medio 1..N: Clientes Wi-Fi (TCP)
    Stream* resolverStream(int idMedio) {
        if (idMedio == 0) {
            return usb ? usb->obtenerStream() : nullptr;
        }
        int indiceTCP = idMedio - 1;
        if (tcp && indiceTCP >= 0 && indiceTCP < tcp->obtenerMaxClientes()) {
            WiFiClient* cliente = tcp->obtenerCliente(indiceTCP);
            if (cliente && cliente->connected()) {
                return cliente;
            }
        }
        return nullptr;
    }

  public:
    CapaFisica(PuertoUSB* puertoUsb, ServidorTCP* servidorTcp)
        : usb(puertoUsb), tcp(servidorTcp) {}

    int obtenerTotalMedios() const {
        int redes = tcp ? tcp->obtenerMaxClientes() : 0;
        return 1 + redes;
    }

    bool estaActivo(int idMedio) {
        return resolverStream(idMedio) != nullptr;
    }

    int bytesDisponibles(int idMedio) {
        Stream* canal = resolverStream(idMedio);
        return canal ? canal->available() : 0;
    }

    // CAMINO DE SUBIDA (RX): Lectura en bloque no bloqueante desde el FIFO de hardware
    size_t recibirBytes(int idMedio, uint8_t* bufferDestino, size_t maxLongitud) {
        Stream* canal = resolverStream(idMedio);
        if (!canal || maxLongitud == 0) return 0;

        int disponibles = canal->available();
        if (disponibles <= 0) return 0;

        size_t aLeer = (size_t)disponibles;
        if (aLeer > maxLongitud) aLeer = maxLongitud;

        return canal->readBytes(bufferDestino, aLeer);
    }

    // CAMINO DE BAJADA (TX): Escritura directa al buffer TX sin bloquear la CPU esperando UART
    size_t enviarBytes(int idMedio, const uint8_t* datos, size_t longitud) {
        Stream* canal = resolverStream(idMedio);
        if (!canal || !datos || longitud == 0) return 0;

        size_t escritos = canal->write(datos, longitud);
        yield(); // Alimenta al Watchdog del ESP8266 sin forzar flush() bloqueante
        return escritos;
    }
};