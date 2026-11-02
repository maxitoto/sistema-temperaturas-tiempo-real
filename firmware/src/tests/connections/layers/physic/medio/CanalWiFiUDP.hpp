#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../ICapaFisica.hpp"

namespace OSI {

    template <size_t MaxClientes = 3, size_t CapBufferRx = 512>
    class MultiplexorWiFiUDP {
    public:
        static constexpr uint32_t TIMEOUT_INACTIVIDAD_MS = 15000UL;

        class CanalSlotUDP : public ICanalFisico {
        private:
            MultiplexorWiFiUDP* padre = nullptr;
            uint8_t  idSlot           = 0;
            bool     activo           = false;
            IPAddress ipRemota;
            uint16_t puertoRemoto     = 0;
            char     etiqueta[16]     = "WIFI_UDP_0";

            uint8_t  ringRx[CapBufferRx];
            size_t   head                = 0;
            size_t   tail                = 0;
            size_t   count               = 0;
            uint32_t ultimaActividadRxMs = 0;

            MetricasCanal metricas;

        public:
            void vincular(MultiplexorWiFiUDP* p, uint8_t idx) {
                padre  = p;
                idSlot = idx;
                snprintf(etiqueta, sizeof(etiqueta), "WIFI_UDP_%u", idx);
            }

            void asignarCliente(const IPAddress& ip, uint16_t puerto) {
                activo              = true;
                ipRemota            = ip;
                puertoRemoto        = puerto;
                head                = 0;
                tail                = 0;
                count               = 0;
                ultimaActividadRxMs = millis();
                metricas.ultimoRxMs = ultimaActividadRxMs;
            }

            void encolarRx(const uint8_t* datos, size_t len) {
                for (size_t i = 0; i < len; i++) {
                    if (count >= CapBufferRx) {
                        tail = (tail + 1) % CapBufferRx;
                        count--;
                        metricas.desbordesBufferRx++;
                    }
                    ringRx[head] = datos[i];
                    head = (head + 1) % CapBufferRx;
                    count++;
                }
                metricas.bytesRx += len;
                metricas.tramasRx++;
                ultimaActividadRxMs = millis();
                metricas.ultimoRxMs = ultimaActividadRxMs;
            }

            bool coincideEndpoint(const IPAddress& ip, uint16_t pto) const {
                return activo && (ipRemota == ip) && (puertoRemoto == pto);
            }

            bool expiroTimeout(uint32_t ahoraMs) const {
                return activo && (ahoraMs - ultimaActividadRxMs > TIMEOUT_INACTIVIDAD_MS);
            }

            const char* obtenerNombre()     const override { return etiqueta; }
            TipoMedio   obtenerTipo()       const override { return TipoMedio::WIFI_UDP; }
            ModoDuplex  obtenerModoDuplex() const override { return ModoDuplex::FULL_DUPLEX; }
            bool        esMedioCrudo()      const override { return true; }

            bool inicializar() override { return true; }
            void mantener()    override { if (padre) padre->bombearPaquetesEntrantes(); }

            void desconectar() override {
                activo       = false;
                puertoRemoto = 0;
                head         = 0;
                tail         = 0;
                count        = 0;
            }

            bool estaActivo() const override { return activo; }

            size_t bytesDisponibles() override {
                if (padre) padre->bombearPaquetesEntrantes();
                return activo ? count : 0;
            }

            size_t recibir(uint8_t* destino, size_t maxLongitud) override {
                if (padre) padre->bombearPaquetesEntrantes();
                if (!activo || !destino || maxLongitud == 0 || count == 0) return 0;

                size_t leidos = 0;
                while (leidos < maxLongitud && count > 0) {
                    destino[leidos++] = ringRx[tail];
                    tail = (tail + 1) % CapBufferRx;
                    count--;
                }
                return leidos;
            }

            size_t enviar(const uint8_t* datos, size_t longitud) override {
                if (!padre || !activo || !datos || longitud == 0 || puertoRemoto == 0) return 0;
                size_t enviados = padre->transmitirDatagrama(ipRemota, puertoRemoto, datos, longitud);
                if (enviados == longitud) {
                    metricas.bytesTx += enviados;
                    metricas.tramasTx++;
                    metricas.ultimoTxMs = millis();
                } else {
                    metricas.erroresTx++;
                }
                return enviados;
            }

            const MetricasCanal& obtenerMetricas() const override { return metricas; }
            void reiniciarMetricas() override { metricas.reiniciar(); }
        };

    private:
        WiFiUDP      udp;
        uint16_t     puertoLocal;
        bool         escuchando = false;
        CanalSlotUDP slots[MaxClientes];

        int resolverSlot(const IPAddress& ip, uint16_t puerto) {
            uint32_t ahora = millis();
            for (size_t i = 0; i < MaxClientes; i++) {
                if (slots[i].coincideEndpoint(ip, puerto)) {
                    return static_cast<int>(i);
                }
            }
            for (size_t i = 0; i < MaxClientes; i++) {
                if (!slots[i].estaActivo() || slots[i].expiroTimeout(ahora)) {
                    slots[i].asignarCliente(ip, puerto);
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

    public:
        explicit MultiplexorWiFiUDP(uint16_t puerto = 81) : puertoLocal(puerto) {
            for (size_t i = 0; i < MaxClientes; i++) {
                slots[i].vincular(this, static_cast<uint8_t>(i));
            }
        }

        bool iniciar(uint16_t nuevoPuerto = 0) {
            if (nuevoPuerto > 0) puertoLocal = nuevoPuerto;
            escuchando = udp.begin(puertoLocal);
            return escuchando;
        }

        void detener() {
            if (escuchando) {
                udp.stop();
                escuchando = false;
            }
            for (size_t i = 0; i < MaxClientes; i++) {
                slots[i].desconectar();
            }
        }

        void bombearPaquetesEntrantes() {
            if (!escuchando) return;
            uint8_t temp[128];
            int tam = udp.parsePacket();
            while (tam > 0) {
                int idx = resolverSlot(udp.remoteIP(), udp.remotePort());
                while (udp.available() > 0) {
                    int leidos = udp.read(temp, sizeof(temp));
                    if (leidos <= 0) break;
                    if (idx >= 0) {
                        slots[idx].encolarRx(temp, static_cast<size_t>(leidos));
                    }
                }
                tam = udp.parsePacket();
            }
        }

        size_t transmitirDatagrama(const IPAddress& ip, uint16_t puerto, const uint8_t* datos, size_t len) {
            if (!escuchando) return 0;
            if (!udp.beginPacket(ip, puerto)) return 0;
            size_t esc = udp.write(datos, len);
            if (!udp.endPacket()) return 0;
            return esc;
        }

        ICanalFisico* obtenerRaizSlot(size_t indice) {
            return (indice < MaxClientes) ? &slots[indice] : nullptr;
        }
    };

}