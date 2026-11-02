#pragma once
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <cstring>
#include "../ICapaFisica.hpp"

namespace OSI {

    class CanalBluetooth : public ICanalFisico {
    private:
        BluetoothSerial btSerial;
        const char*     nombreDispositivo;
        const char*     pinSeguridad;
        bool            modoCrudo;
        bool            iniciado = false;
        MetricasCanal   metricas;

    public:
        explicit CanalBluetooth(const char* nombreBT = "UNTDF_LAB_ESP32",
                                const char* pinOpcional = nullptr,
                                bool tratarComoMedioCrudo = true)
            : nombreDispositivo(nombreBT),
              pinSeguridad(pinOpcional),
              modoCrudo(tratarComoMedioCrudo) {}

        const char* obtenerNombre()     const override { return nombreDispositivo; }
        TipoMedio   obtenerTipo()       const override { return TipoMedio::BLUETOOTH_SPP; }
        ModoDuplex  obtenerModoDuplex() const override { return ModoDuplex::FULL_DUPLEX; }
        bool        esMedioCrudo()      const override { return modoCrudo; }

        bool inicializar() override {
            if (iniciado) return true;
            if (pinSeguridad && pinSeguridad[0] != '\0') {
                btSerial.setPin(pinSeguridad, strlen(pinSeguridad));
            }
            iniciado = btSerial.begin(nombreDispositivo);
            return iniciado;
        }

        void mantener() override {}

        void desconectar() override {
            if (iniciado) {
                btSerial.disconnect();
            }
        }

        bool estaActivo() const override {
            return iniciado && const_cast<BluetoothSerial&>(btSerial).hasClient();
        }

        size_t bytesDisponibles() override {
            if (!estaActivo()) return 0;
            int disp = btSerial.available();
            return (disp > 0) ? static_cast<size_t>(disp) : 0;
        }

        size_t recibir(uint8_t* destino, size_t maxLongitud) override {
            if (!destino || maxLongitud == 0 || !estaActivo()) return 0;
            int disp = btSerial.available();
            if (disp <= 0) return 0;

            size_t aLeer = (static_cast<size_t>(disp) > maxLongitud)
                             ? maxLongitud
                             : static_cast<size_t>(disp);

            size_t leidos = btSerial.readBytes(destino, aLeer);
            if (leidos > 0) {
                metricas.bytesRx += leidos;
                metricas.tramasRx++;
                metricas.ultimoRxMs = millis();
            }
            return leidos;
        }

        size_t enviar(const uint8_t* datos, size_t longitud) override {
            if (!datos || longitud == 0 || !estaActivo()) return 0;
            size_t escritos = btSerial.write(datos, longitud);
            if (escritos == longitud) {
                metricas.bytesTx += escritos;
                metricas.tramasTx++;
                metricas.ultimoTxMs = millis();
            } else {
                metricas.erroresTx++;
            }
            return escritos;
        }

        const MetricasCanal& obtenerMetricas() const override { return metricas; }
        void reiniciarMetricas() override { metricas.reiniciar(); }
    };

}