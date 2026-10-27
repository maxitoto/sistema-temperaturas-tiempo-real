// Archivo: ./src/communication/layers/CapaFisica.h
#pragma once
#include <Arduino.h>
#include "communication/conectividad/usb/PuertoUSB.h"
#include "communication/conectividad/modem/ServidorWiFi.h"

class CapaFisica {
  private:
    PuertoUSB* usb;
    ServidorWiFi* wifi;

    Stream* resolverStream(int idMedio) {
        if (idMedio == 0) {
            return usb ? usb->obtenerStream() : nullptr;
        }
        int indiceWiFi = idMedio - 1;
        if (wifi && indiceWiFi >= 0 && indiceWiFi < wifi->obtenerMaxClientes()) {
            WiFiClient* cliente = wifi->obtenerCliente(indiceWiFi);
            if (cliente && cliente->connected()) {
                return cliente;
            }
        }
        return nullptr;
    }

  public:
    CapaFisica(PuertoUSB* puertoUsb, ServidorWiFi* servidorWifi)
        : usb(puertoUsb), wifi(servidorWifi) {}

    int obtenerTotalMedios() const {
        int redes = wifi ? wifi->obtenerMaxClientes() : 0;
        return 1 + redes;
    }

    // Indica si el medio ya cuenta con MAC/IP/Transporte en el kernel (Wi-Fi / Bluetooth)
    bool esMedioNativo(int idMedio) const {
        return idMedio >= 1;
    }

    bool estaActivo(int idMedio) {
        return resolverStream(idMedio) != nullptr;
    }

    int bytesDisponibles(int idMedio) {
        if (esMedioNativo(idMedio) && wifi && wifi->hayDatagramaUDP(idMedio - 1)) {
            return 1;
        }
        Stream* canal = resolverStream(idMedio);
        return canal ? canal->available() : 0;
    }

    // CAMINO DE SUBIDA (RX): Soporta tanto el stream de bytes (USB/TCP) como datagramas nativos (UDP)
    size_t recibirBytes(int idMedio, uint8_t* bufferDestino, size_t maxLongitud, bool& esDatagramaSalida) {
        esDatagramaSalida = false;
        if (!bufferDestino || maxLongitud == 0) return 0;

        // Prioridad a datagramas en tiempo real si llegaron por el canal UDP de ese cliente Wi-Fi
        if (esMedioNativo(idMedio) && wifi && wifi->hayDatagramaUDP(idMedio - 1)) {
            esDatagramaSalida = true;
            return wifi->recibirDatagramaUDP(idMedio - 1, bufferDestino, maxLongitud);
        }

        Stream* canal = resolverStream(idMedio);
        if (!canal) return 0;

        int disponibles = canal->available();
        if (disponibles <= 0) return 0;

        size_t aLeer = (size_t)disponibles;
        if (aLeer > maxLongitud) aLeer = maxLongitud;

        return canal->readBytes(bufferDestino, aLeer);
    }

    // Sobrecarga compatible para consumidores de stream directo (CapaEnlace USB)
    size_t recibirBytes(int idMedio, uint8_t* bufferDestino, size_t maxLongitud) {
        bool dummy = false;
        return recibirBytes(idMedio, bufferDestino, maxLongitud, dummy);
    }

    // CAMINO DE BAJADA (TX): Escribe en el stream (USB/TCP) o despacha un datagrama (UDP Wi-Fi)
    size_t enviarBytes(int idMedio, const uint8_t* datos, size_t longitud, bool comoDatagrama = false) {
        if (!datos || longitud == 0) return 0;

        if (esMedioNativo(idMedio) && comoDatagrama && wifi) {
            return wifi->enviarDatagramaUDP(idMedio - 1, datos, longitud);
        }

        Stream* canal = resolverStream(idMedio);
        if (!canal) return 0;

        size_t escritos = canal->write(datos, longitud);
        yield();
        return escritos;
    }
};