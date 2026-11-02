#pragma once
#include <Arduino.h>
#include "../ICapaFisica.hpp"

namespace OSI {

    class CanalUSB : public ICanalFisico {
    public:
        static constexpr size_t   FIFO_RX_HARDWARE_BYTES = 1024;
        static constexpr uint32_t GUARD_TIME_DUPLEX_MS   = 5;

    private:
        HardwareSerial& puerto;
        uint32_t        baudios;
        ModoDuplex      modoDuplex;
        uint8_t         byteSincronia;
        bool            filtroSincroniaActivo;
        bool            sincronizado       = false;
        bool            iniciado           = false;
        MetricasCanal   metricas;

        bool esperarVentanaSinColision() {
            if (modoDuplex == ModoDuplex::FULL_DUPLEX) return true;

            uint32_t t0 = millis();
            if (puerto.available() > 0 || (t0 - metricas.ultimoRxMs < GUARD_TIME_DUPLEX_MS)) {
                metricas.colisionesEvitadas++;
                uint32_t esperaIni = millis();
                while ((millis() - esperaIni) < GUARD_TIME_DUPLEX_MS) {
                    yield();
                }
            }
            return true;
        }

    public:
        explicit CanalUSB(uint32_t baudRate = 115200,
                          ModoDuplex duplex = ModoDuplex::FULL_DUPLEX,
                          bool activarFiltroSincronia = false,
                          HardwareSerial& serialPort = Serial,
                          uint8_t delimitadorSync = 0x00)
            : puerto(serialPort),
              baudios(baudRate),
              modoDuplex(duplex),
              byteSincronia(delimitadorSync),
              filtroSincroniaActivo(activarFiltroSincronia) {}

        void configurarDelimitadorSincronia(uint8_t delimitador, bool activar = true) {
            byteSincronia         = delimitador;
            filtroSincroniaActivo = activar;
            sincronizado          = !activar;
        }

        const char* obtenerNombre()     const override { return "USB_UART0"; }
        TipoMedio   obtenerTipo()       const override { return TipoMedio::USB_UART; }
        ModoDuplex  obtenerModoDuplex() const override { return modoDuplex; }
        bool        esMedioCrudo()      const override { return true; }

        bool inicializar() override {
            puerto.setRxBufferSize(FIFO_RX_HARDWARE_BYTES);
            puerto.begin(baudios);
            delay(30);
            purgarRuidoArranque();
            sincronizado = !filtroSincroniaActivo;
            iniciado     = true;
            return true;
        }

        size_t purgarRuidoArranque() {
            size_t purgados = 0;
            while (puerto.available() > 0) {
                puerto.read();
                purgados++;
            }
            metricas.bytesRuidoPurgados += purgados;
            sincronizado = !filtroSincroniaActivo;
            return purgados;
        }

        void mantener() override {
            if (!iniciado) return;
            if (filtroSincroniaActivo && !sincronizado) {
                while (puerto.available() > 0) {
                    int peekByte = puerto.peek();
                    if (peekByte == byteSincronia) {
                        sincronizado = true;
                        break;
                    }
                    puerto.read();
                    metricas.bytesRuidoPurgados++;
                }
            }
        }

        void desconectar() override {
            purgarRuidoArranque();
        }

        bool estaActivo() const override {
            return iniciado && static_cast<bool>(puerto);
        }

        size_t bytesDisponibles() override {
            mantener();
            if (!estaActivo()) return 0;
            int disp = puerto.available();
            return (disp > 0) ? static_cast<size_t>(disp) : 0;
        }

        size_t recibir(uint8_t* destino, size_t maxLongitud) override {
            if (!destino || maxLongitud == 0 || !estaActivo()) return 0;
            mantener();

            int disp = puerto.available();
            if (disp <= 0) return 0;

            size_t aLeer = (static_cast<size_t>(disp) > maxLongitud)
                             ? maxLongitud
                             : static_cast<size_t>(disp);

            size_t leidos = puerto.readBytes(destino, aLeer);
            if (leidos > 0) {
                metricas.bytesRx += leidos;
                metricas.tramasRx++;
                metricas.ultimoRxMs = millis();
            }
            return leidos;
        }

        size_t enviar(const uint8_t* datos, size_t longitud) override {
            if (!datos || longitud == 0 || !estaActivo()) return 0;

            esperarVentanaSinColision();

            size_t escritos = puerto.write(datos, longitud);
            if (escritos == longitud) {
                metricas.bytesTx += escritos;
                metricas.tramasTx++;
                metricas.ultimoTxMs = millis();
            } else {
                metricas.erroresTx++;
            }
            return escritos;
        }

        void reactivarFiltroRuido() {
            sincronizado = false;
        }

        const MetricasCanal& obtenerMetricas() const override { return metricas; }
        void reiniciarMetricas() override { metricas.reiniciar(); }
    };

}