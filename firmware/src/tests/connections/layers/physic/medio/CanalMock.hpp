#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Arduino.h>
#include "../ICapaFisica.hpp"

namespace OSI {

    template <size_t Capacidad = 512>
    class CanalMock : public ICanalFisico {
    private:
        const char*   nombre;
        TipoMedio     tipoSimulado;
        ModoDuplex    duplexSimulado;
        bool          crudoSimulado;
        bool          activo = true;

        uint8_t       fifoRx[Capacidad] = {0};
        size_t        lenRx             = 0;

        uint8_t       fifoTx[Capacidad] = {0};
        size_t        lenTx             = 0;

        MetricasCanal metricas;

    public:
        explicit CanalMock(const char* etiqueta = "MOCK_CH",
                           TipoMedio tipo = TipoMedio::MOCK_LOOPBACK,
                           ModoDuplex duplex = ModoDuplex::FULL_DUPLEX,
                           bool esCrudo = true)
            : nombre(etiqueta),
              tipoSimulado(tipo),
              duplexSimulado(duplex),
              crudoSimulado(esCrudo) {}

        const char* obtenerNombre()     const override { return nombre; }
        TipoMedio   obtenerTipo()       const override { return tipoSimulado; }
        ModoDuplex  obtenerModoDuplex() const override { return duplexSimulado; }
        bool        esMedioCrudo()      const override { return crudoSimulado; }

        bool inicializar() override { activo = true; return true; }
        void mantener()    override {}
        void desconectar() override { activo = false; lenRx = 0; lenTx = 0; }
        bool estaActivo()  const override { return activo; }

        size_t bytesDisponibles() override { return activo ? lenRx : 0; }

        size_t recibir(uint8_t* destino, size_t maxLongitud) override {
            if (!activo || !destino || maxLongitud == 0 || lenRx == 0) return 0;
            size_t aCopiar = (lenRx > maxLongitud) ? maxLongitud : lenRx;
            memcpy(destino, fifoRx, aCopiar);
            if (lenRx > aCopiar) {
                memmove(fifoRx, fifoRx + aCopiar, lenRx - aCopiar);
            }
            lenRx -= aCopiar;
            metricas.bytesRx += aCopiar;
            metricas.tramasRx++;
            metricas.ultimoRxMs = millis();
            return aCopiar;
        }

        size_t enviar(const uint8_t* datos, size_t longitud) override {
            if (!activo || !datos || longitud == 0) return 0;
            if (lenTx + longitud > Capacidad) {
                metricas.erroresTx++;
                return 0;
            }
            memcpy(fifoTx + lenTx, datos, longitud);
            lenTx += longitud;
            metricas.bytesTx += longitud;
            metricas.tramasTx++;
            metricas.ultimoTxMs = millis();
            return longitud;
        }

        // --- MÉTODOS DE INYECCIÓN Y EXTRACCIÓN PARA TESTS (CAJA NEGRA / BLANCA) ---
        bool inyectarRxDesdeExterior(const uint8_t* datos, size_t longitud) {
            if (lenRx + longitud > Capacidad) {
                metricas.desbordesBufferRx++;
                return false;
            }
            memcpy(fifoRx + lenRx, datos, longitud);
            lenRx += longitud;
            return true;
        }

        size_t extraerTxCapturado(uint8_t* destino, size_t maxLongitud) {
            size_t aCopiar = (lenTx > maxLongitud) ? maxLongitud : lenTx;
            memcpy(destino, fifoTx, aCopiar);
            if (lenTx > aCopiar) {
                memmove(fifoTx, fifoTx + aCopiar, lenTx - aCopiar);
            }
            lenTx -= aCopiar;
            return aCopiar;
        }

        size_t obtenerBytesTxEnEspera() const { return lenTx; }
        const MetricasCanal& obtenerMetricas() const override { return metricas; }
        void reiniciarMetricas() override {
            metricas.reiniciar();
            lenRx = 0;
            lenTx = 0;
        }
    };

}