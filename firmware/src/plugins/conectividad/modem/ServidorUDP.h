// Archivo: ./src/plugins/conectividad/modem/ServidorUDP.h
#pragma once
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiUdp.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
#endif

class ServidorUDP {
  public:
    static const size_t CAP_BUFFER_RX_UDP = 512;
    static const unsigned long TIMEOUT_SLOT_UDP_MS = 60000UL;

    struct SlotClienteUDP {
        bool activo = false;
        IPAddress ip;
        uint16_t puerto = 0;
        unsigned long ultimaActividadMs = 0;
        uint8_t bufferRx[CAP_BUFFER_RX_UDP];
        size_t head = 0;
        size_t tail = 0;
        size_t count = 0;

        void reiniciar() {
            activo = false;
            puerto = 0;
            head = 0;
            tail = 0;
            count = 0;
        }

        void encolarBytes(const uint8_t* datos, size_t len) {
            for (size_t i = 0; i < len; i++) {
                if (count >= CAP_BUFFER_RX_UDP) {
                    // Si se llena el anillo, descartamos el byte más viejo
                    tail = (tail + 1) % CAP_BUFFER_RX_UDP;
                    count--;
                }
                bufferRx[head] = datos[i];
                head = (head + 1) % CAP_BUFFER_RX_UDP;
                count++;
            }
        }

        size_t desencolarBytes(uint8_t* destino, size_t maxLen) {
            size_t leidos = 0;
            while (leidos < maxLen && count > 0) {
                destino[leidos++] = bufferRx[tail];
                tail = (tail + 1) % CAP_BUFFER_RX_UDP;
                count--;
            }
            return leidos;
        }
    };

  private:
    WiFiUDP udp;
    int maxClientes;
    int puertoLocal;
    bool escuchando = false;
    SlotClienteUDP* clientes;

    int resolverOCrearSlot(const IPAddress& ipRemota, uint16_t puertoRemoto) {
        unsigned long ahora = millis();

        // 1. Buscar si el cliente (IP:puerto) ya tiene un slot asignado
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i].activo &&
                clientes[i].ip == ipRemota &&
                clientes[i].puerto == puertoRemoto) {
                clientes[i].ultimaActividadMs = ahora;
                return i;
            }
        }

        // 2. Buscar un slot libre o expirado
        for (int i = 0; i < maxClientes; i++) {
            if (!clientes[i].activo || (ahora - clientes[i].ultimaActividadMs > TIMEOUT_SLOT_UDP_MS)) {
                clientes[i].reiniciar();
                clientes[i].activo = true;
                clientes[i].ip = ipRemota;
                clientes[i].puerto = puertoRemoto;
                clientes[i].ultimaActividadMs = ahora;
                return i;
            }
        }

        // 3. Si todos están ocupados, reutilizar el slot 0 (reconexión inmediata desde nuevo puerto efímero)
        clientes[0].reiniciar();
        clientes[0].activo = true;
        clientes[0].ip = ipRemota;
        clientes[0].puerto = puertoRemoto;
        clientes[0].ultimaActividadMs = ahora;
        return 0;
    }

  public:
    ServidorUDP(int puerto, int limiteClientes)
        : maxClientes(limiteClientes), puertoLocal(puerto), escuchando(false) {
        clientes = new SlotClienteUDP[maxClientes];
    }

    ~ServidorUDP() {
        delete[] clientes;
    }

    void iniciar() {
        if (udp.begin((uint16_t)puertoLocal)) {
            escuchando = true;
        }
    }

    void detener() {
        udp.stop();
        escuchando = false;
        for (int i = 0; i < maxClientes; i++) {
            clientes[i].reiniciar();
        }
    }

    int obtenerPuerto() const {
        return puertoLocal;
    }

    int obtenerMaxClientes() const {
        return maxClientes;
    }

    // Drena los datagramas UDP entrantes del hardware Wi-Fi y los reparte al slot correspondiente
    void procesarNuevasConexiones() {
        if (!escuchando) return;

        uint8_t tempBuf[160];
        int packetSize = udp.parsePacket();
        while (packetSize > 0) {
            IPAddress ipRem = udp.remoteIP();
            uint16_t ptoRem = udp.remotePort();
            int slot = resolverOCrearSlot(ipRem, ptoRem);

            while (udp.available() > 0) {
                int leidos = udp.read(tempBuf, sizeof(tempBuf));
                if (leidos <= 0) break;
                if (slot >= 0) {
                    clientes[slot].encolarBytes(tempBuf, (size_t)leidos);
                }
            }
            packetSize = udp.parsePacket();
        }
    }

    bool clienteActivo(int indice) {
        procesarNuevasConexiones();
        if (indice < 0 || indice >= maxClientes) return false;
        return clientes[indice].activo;
    }

    int bytesDisponiblesCliente(int indice) {
        procesarNuevasConexiones();
        if (indice < 0 || indice >= maxClientes || !clientes[indice].activo) return 0;
        return (int)clientes[indice].count;
    }

    size_t recibirBytesCliente(int indice, uint8_t* destino, size_t maxLongitud) {
        procesarNuevasConexiones();
        if (indice < 0 || indice >= maxClientes || !clientes[indice].activo) return 0;
        return clientes[indice].desencolarBytes(destino, maxLongitud);
    }

    size_t enviarBytesCliente(int indice, const uint8_t* datos, size_t longitud) {
        if (!escuchando || indice < 0 || indice >= maxClientes || !clientes[indice].activo) return 0;
        if (!datos || longitud == 0 || clientes[indice].puerto == 0) return 0;

        if (!udp.beginPacket(clientes[indice].ip, clientes[indice].puerto)) return 0;
        size_t escritos = udp.write(datos, longitud);
        if (!udp.endPacket()) return 0;

        clientes[indice].ultimaActividadMs = millis();
        yield();
        return escritos;
    }

    bool hayClientesConectados() const {
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i].activo) return true;
        }
        return false;
    }
};