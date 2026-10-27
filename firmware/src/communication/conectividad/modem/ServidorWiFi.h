// Archivo: ./src/communication/conectividad/modem/ServidorTCP.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

class ServidorWiFi {
  public:
    static constexpr size_t MAX_DATAGRAMA_UDP = 512;

  private:
    struct SlotDatagramaRx {
        bool pendiente = false;
        uint8_t datos[MAX_DATAGRAMA_UDP];
        size_t longitud = 0;
    };

    WiFiServer servidor;
    WiFiUDP udp;
    int maxClientes;
    int puertoTcp;
    int puertoUdp;
    bool udpActivo = false;
    bool servidorActivo = false;
    WiFiClient* clientes;
    uint16_t* puertosUdpRemotos;
    SlotDatagramaRx* datagramasRx;
    

    int buscarSlotPorIP(const IPAddress& ipRemota) const {
        int primerActivo = -1;
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i] && clientes[i].connected()) {
                if (primerActivo < 0) primerActivo = i;
                if (clientes[i].remoteIP() == ipRemota) return i;
            }
        }
        // Fallback para entornos con NAT local (ej. túnel Wokwi 127.0.0.1)
        return primerActivo;
    }

  public:
    ServidorWiFi(int puerto = 80, int limiteClientes = 3)
        : servidor(puerto),
          maxClientes(limiteClientes),
          puertoTcp(puerto),
          puertoUdp(puerto + 1) {
        clientes          = new WiFiClient[maxClientes];
        puertosUdpRemotos = new uint16_t[maxClientes]();
        datagramasRx      = new SlotDatagramaRx[maxClientes]();
    }

    ~ServidorWiFi() {
        delete[] clientes;
        delete[] puertosUdpRemotos;
        delete[] datagramasRx;
    }

    void iniciar() {
        servidor.begin();
        servidor.setNoDelay(true);
        udpActivo = udp.begin((uint16_t)puertoUdp);
        servidorActivo = true;
    }

    void detener() {
        if (!servidorActivo) return; // Evita tocar lwIP si aún no se inicializó tcpip_mbox
        if (udpActivo) {
            udp.stop();
            udpActivo = false;
        }
        servidor.end();
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i]) clientes[i].stop();
            puertosUdpRemotos[i] = 0;
            datagramasRx[i].pendiente = false;
            datagramasRx[i].longitud = 0;
        }
        servidorActivo = false;
    }

    void cambiarPuerto(int nuevoPuertoTcp) {
        if (nuevoPuertoTcp <= 0 || nuevoPuertoTcp > 65534) return;
        if (nuevoPuertoTcp == puertoTcp) return;

        bool estabaActivo = servidorActivo;
        if (estabaActivo) {
            detener();
        }
        puertoTcp = nuevoPuertoTcp;
        puertoUdp = nuevoPuertoTcp + 1;
        servidor = WiFiServer(puertoTcp);
        if (estabaActivo) {
            iniciar();
        }
    }

    int obtenerPuerto() const { return puertoTcp; }
    int obtenerPuertoUDP() const { return puertoUdp; }
    int obtenerMaxClientes() const { return maxClientes; }

    void procesarNuevasConexiones() {
        // 1. Aceptar nuevas conexiones TCP
        if (servidor.hasClient()) {
            WiFiClient entrante = servidor.accept();
            if (entrante) {
                entrante.setNoDelay(true);
                int slotAsignado = -1;

                // Si la misma IP se reconecta, reciclar su propio slot
                for (int i = 0; i < maxClientes; i++) {
                    if (clientes[i] && clientes[i].connected() &&
                        clientes[i].remoteIP() == entrante.remoteIP()) {
                        clientes[i].stop();
                        slotAsignado = i;
                        break;
                    }
                }

                // Si no, buscar el primer slot libre
                if (slotAsignado < 0) {
                    for (int i = 0; i < maxClientes; i++) {
                        if (!clientes[i] || !clientes[i].connected()) {
                            if (clientes[i]) clientes[i].stop();
                            slotAsignado = i;
                            break;
                        }
                    }
                }

                if (slotAsignado >= 0) {
                    clientes[slotAsignado] = entrante;
                    puertosUdpRemotos[slotAsignado] = 0;
                    datagramasRx[slotAsignado].pendiente = false;
                } else {
                    entrante.stop();
                }
            }
        }

        // 2. Despachar datagramas UDP entrantes hacia el slot del cliente correspondiente
        if (udpActivo) {
            int tam = udp.parsePacket();
            while (tam > 0) {
                int slot = buscarSlotPorIP(udp.remoteIP());
                if (slot >= 0 && (size_t)tam <= MAX_DATAGRAMA_UDP) {
                    int leidos = udp.read(datagramasRx[slot].datos, MAX_DATAGRAMA_UDP);
                    if (leidos > 0) {
                        puertosUdpRemotos[slot]       = udp.remotePort();
                        datagramasRx[slot].longitud   = (size_t)leidos;
                        datagramasRx[slot].pendiente  = true;
                    }
                } else {
                    udp.flush();
                }
                tam = udp.parsePacket();
            }
        }
    }

    WiFiClient* obtenerCliente(int indice) {
        if (indice >= 0 && indice < maxClientes) {
            return &clientes[indice];
        }
        return nullptr;
    }

    bool hayDatagramaUDP(int indice) const {
        if (indice < 0 || indice >= maxClientes) return false;
        return datagramasRx[indice].pendiente && datagramasRx[indice].longitud > 0;
    }

    size_t recibirDatagramaUDP(int indice, uint8_t* destino, size_t maxLongitud) {
        if (!hayDatagramaUDP(indice) || !destino) return 0;
        size_t aCopiar = datagramasRx[indice].longitud;
        if (aCopiar > maxLongitud) aCopiar = maxLongitud;
        memcpy(destino, datagramasRx[indice].datos, aCopiar);
        datagramasRx[indice].pendiente = false;
        datagramasRx[indice].longitud  = 0;
        return aCopiar;
    }

    size_t enviarDatagramaUDP(int indice, const uint8_t* datos, size_t longitud) {
        if (!udpActivo || indice < 0 || indice >= maxClientes || !datos || longitud == 0) return 0;
        if (!clientes[indice] || !clientes[indice].connected()) return 0;

        uint16_t ptoRem = (puertosUdpRemotos[indice] > 0)
                            ? puertosUdpRemotos[indice]
                            : (uint16_t)puertoUdp;

        if (!udp.beginPacket(clientes[indice].remoteIP(), ptoRem)) return 0;
        size_t escritos = udp.write(datos, longitud);
        if (!udp.endPacket()) return 0;
        return escritos;
    }

    int contarClientesActivos() const {
        int activos = 0;
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i] && clientes[i].connected()) activos++;
        }
        return activos;
    }

    bool hayClientesConectados() const {
        return contarClientesActivos() > 0;
    }
};