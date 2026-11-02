#pragma once
#include <stdint.h>
#include <stddef.h>

namespace OSI {

    enum class TipoMedio : uint8_t {
        NINGUNO = 0,
        USB_UART,
        USB_SEGURO,
        WIFI_UDP,
        WIFI_TCP,
        WIFI_SEGURO_TLS,
        BLUETOOTH_SPP,
        BLUETOOTH_BLE,
        MOCK_LOOPBACK
    };

    enum class ModoDuplex : uint8_t {
        FULL_DUPLEX = 0,
        HALF_DUPLEX = 1
    };

    // Estado interno observable para pruebas de Caja Blanca
    struct MetricasCanal {
        uint32_t bytesTx            = 0;
        uint32_t bytesRx            = 0;
        uint32_t tramasTx           = 0;
        uint32_t tramasRx           = 0;
        uint32_t bytesRuidoPurgados = 0;
        uint32_t colisionesEvitadas = 0;
        uint32_t erroresTx          = 0;
        uint32_t desbordesBufferRx  = 0;
        uint32_t ultimoTxMs         = 0;
        uint32_t ultimoRxMs         = 0;

        void reiniciar() { *this = MetricasCanal{}; }
    };

    // Contrato que debe implementar toda raíz (medio físico) del árbol
    class ICanalFisico {
    public:
        virtual ~ICanalFisico() = default;

        virtual const char* obtenerNombre() const = 0;
        virtual TipoMedio   obtenerTipo() const = 0;
        virtual ModoDuplex  obtenerModoDuplex() const = 0;

        // Si devuelve false, el canal ya garantiza entrega/framing en hardware/OS
        // y permite elevar el paquete sin alterarlo en capas intermedias si se desea.
        virtual bool esMedioCrudo() const = 0;

        virtual bool inicializar() = 0;
        virtual void mantener() = 0;
        virtual void desconectar() = 0;

        virtual bool   estaActivo() const = 0;
        virtual size_t bytesDisponibles() = 0;
        virtual size_t recibir(uint8_t* destino, size_t maxLongitud) = 0;
        virtual size_t enviar(const uint8_t* datos, size_t longitud) = 0;

        virtual const MetricasCanal& obtenerMetricas() const = 0;
        virtual void reiniciarMetricas() = 0;
    };

}