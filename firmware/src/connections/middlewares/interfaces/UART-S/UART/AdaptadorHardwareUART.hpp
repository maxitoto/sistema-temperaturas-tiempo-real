#ifndef ADAPTADOR_HARDWARE_UART_H
#define ADAPTADOR_HARDWARE_UART_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Enumeración: ModoDuplexUART
 * ¿Para qué sirve?: Identifica si el canal serie permite transmisión y recepción simultánea o alternada.
 */
enum ModoDuplexUART : uint8_t {
    // ¿Para qué sirve?: Indica que TX y RX tienen líneas independientes y operan en simultáneo.
    MODO_FULL_DUPLEX = 0,
    // ¿Para qué sirve?: Indica que el canal comparte línea o requiere alternar turnos de habla/escucha.
    MODO_HALF_DUPLEX = 1
};

/**
 * Estructura: PerfilCapacidadesUART
 * ¿Para qué sirve?: Almacena las métricas adaptativas deducidas del hardware para configurar las capas de segmentación y control de flujo.
 */
struct PerfilCapacidadesUART {
    // ¿Para qué sirve?: Guarda la velocidad real de transmisión detectada en bits por segundo.
    uint32_t baudiosActuales;
    // ¿Para qué sirve?: Indica si el enlace opera en Full-Duplex o Half-Duplex.
    ModoDuplexUART modoDuplex;
    // ¿Para qué sirve?: Indica si los pines físicos CTS y RTS están activos para control de flujo por hardware.
    bool controlFlujoHardwareActivo;
    // ¿Para qué sirve?: Almacena la capacidad detectada en el buffer de salida de la UART.
    size_t tamanoBufferHardwareTX;
    // ¿Para qué sirve?: Define el número máximo de bytes útiles por segmento calculado según el FIFO y el modo duplex.
    uint8_t tamanoMaximoSegmentoDatos;
    // ¿Para qué sirve?: Define el tiempo de espera en milisegundos antes de retransmitir un paquete no confirmado, adaptado a los baudios.
    uint32_t tiempoEsperaRetransmisionMs;
    // ¿Para qué sirve?: Define la pausa mínima entre tramas en microsegundos para evitar saturar al receptor.
    uint32_t pausaEntreTramasUs;
};

/**
 * Clase: AdaptadorHardwareUART
 * ¿Para qué sirve?: Envuelve la clase nativa HardwareSerial, activa el bit de paridad físico (8E1) y deduce parámetros adaptativos del hardware.
 */
class AdaptadorHardwareUART {
private:
    // ¿Para qué sirve?: Referencia a la instancia nativa de HardwareSerial (Serial, Serial1 o Serial2).
    HardwareSerial& puertoSerie;
    // ¿Para qué sirve?: Almacena el perfil calculado tras inspeccionar el hardware.
    PerfilCapacidadesUART perfilActual;
    // ¿Para qué sirve?: Constante con el umbral del FIFO de recepción del ESP32.
    static const uint8_t UMBRAL_FIFO_ESP32 = 120;
    // ¿Para qué sirve?: Constante con el umbral para disparar la señal RTS por hardware.
    static const uint8_t UMBRAL_FLUJO_RTS_HW = 64;

public:
    /**
     * Constructor: AdaptadorHardwareUART
     * ¿Para qué sirve?: Vincula el adaptador con un puerto serie físico del microcontrolador.
     * ¿Cómo lo hace?: Inicializa la referencia puertoSerie y asigna valores base seguros en perfilActual.
     */
    AdaptadorHardwareUART(HardwareSerial& serialNativo)
        : puertoSerie(serialNativo), perfilActual{0, MODO_FULL_DUPLEX, false, 0, 64, 200, 1000} {}

    /**
     * Procedimiento: inicializarAdaptativo
     * ¿Para qué sirve?: Configura el puerto UART con paridad par nativa (SERIAL_8E1), evalúa los pines para determinar el modo duplex/control de flujo y calcula el tamaño de segmento y timeouts.
     * ¿Cómo lo hace?: Llama a begin() con SERIAL_8E1 (8 bits de datos, paridad Par nativa, 1 bit de stop), configura pines CTS/RTS con setPins() y setHwFlowCtrlMode() si fueron provistos, evalúa si es Half-Duplex o Full-Duplex y ejecuta recalcularParametrosAdaptativos().
     */
    void inicializarAdaptativo(
        uint32_t baudios,
        int8_t pinRX,
        int8_t pinTX,
        int8_t pinCTS = -1,
        int8_t pinRTS = -1,
        bool forzarHalfDuplex = false
    ) {
        puertoSerie.begin(baudios, SERIAL_8E1, pinRX, pinTX, false, 200, UMBRAL_FIFO_ESP32);

        if (pinCTS >= 0 && pinRTS >= 0) {
            puertoSerie.setPins(pinRX, pinTX, pinCTS, pinRTS);
            puertoSerie.setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS, UMBRAL_FLUJO_RTS_HW);
            perfilActual.controlFlujoHardwareActivo = true;
        } else {
            puertoSerie.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE, 0);
            perfilActual.controlFlujoHardwareActivo = false;
        }

        if (forzarHalfDuplex || pinRX == pinTX || pinRX < 0 || pinTX < 0) {
            perfilActual.modoDuplex = MODO_HALF_DUPLEX;
        } else {
            perfilActual.modoDuplex = MODO_FULL_DUPLEX;
        }

        recalcularParametrosAdaptativos(baudios);
    }

    /**
     * Procedimiento: recalcularParametrosAdaptativos
     * ¿Para qué sirve?: Ajusta el tamaño de segmentación y los tiempos de control de flujo en función de los baudios detectados y del tamaño de la cola FIFO del hardware.
     * ¿Cómo lo hace?: Obtiene la tasa de baudios con baudRate() y el tamaño del buffer TX con availableForWrite(). Calcula cuántos bytes caben en el FIFO restando los 8 bytes de TAMANO_CABECERA_Y_PIE_TRAMA (reduciendo a la mitad en Half-Duplex) y estima el tiempo por byte (11 bits en 8E1) para fijar el RTO y la pausa entre tramas.
     */
    void recalcularParametrosAdaptativos(uint32_t baudiosRespaldo) {
        uint32_t baudiosDetectados = puertoSerie.baudRate();
        perfilActual.baudiosActuales = (baudiosDetectados > 0) ? baudiosDetectados : baudiosRespaldo;

        size_t espacioLibreTX = puertoSerie.availableForWrite();
        perfilActual.tamanoBufferHardwareTX = (espacioLibreTX > 0) ? espacioLibreTX : UMBRAL_FIFO_ESP32;

        size_t limiteFisicoTrama = (perfilActual.tamanoBufferHardwareTX < UMBRAL_FIFO_ESP32)
                                   ? perfilActual.tamanoBufferHardwareTX
                                   : UMBRAL_FIFO_ESP32;

        if (perfilActual.modoDuplex == MODO_HALF_DUPLEX) {
            limiteFisicoTrama /= 2;
        }

        size_t payloadCalculado = (limiteFisicoTrama > TAMANO_CABECERA_Y_PIE_TRAMA + 8)
                                  ? (limiteFisicoTrama - TAMANO_CABECERA_Y_PIE_TRAMA)
                                  : 16;

        if (payloadCalculado > TAMANO_MAXIMO_PAYLOAD_ESTATICO) {
            payloadCalculado = TAMANO_MAXIMO_PAYLOAD_ESTATICO;
        }
        perfilActual.tamanoMaximoSegmentoDatos = static_cast<uint8_t>(payloadCalculado);

        // En formato 8E1 cada byte requiere 11 bits físicos (1 Start + 8 Data + 1 Parity + 1 Stop)
        uint32_t microsegundosPorByte = (11UL * 1000000UL) / perfilActual.baudiosActuales;
        uint32_t tiempoTransmisionTramaMs = (((perfilActual.tamanoMaximoSegmentoDatos + TAMANO_CABECERA_Y_PIE_TRAMA) * microsegundosPorByte) / 1000UL) + 1;

        perfilActual.pausaEntreTramasUs = microsegundosPorByte * 3;

        uint32_t rtoCalculado = (tiempoTransmisionTramaMs * 4) + 40;
        perfilActual.tiempoEsperaRetransmisionMs = (rtoCalculado < 50) ? 50 : rtoCalculado;
    }

    /**
     * Función: escribirBytesCrudos
     * ¿Para qué sirve?: Envía un bloque de bytes por el puerto serie verificando previamente que el buffer de hardware no se desborde.
     * ¿Cómo lo hace?: Comprueba con availableForWrite() que exista espacio suficiente, escribe con write() y, si el modo es Half-Duplex, invoca flush(true) para esperar a que termine la transmisión física antes de liberar la línea.
     */
    bool escribirBytesCrudos(const uint8_t* buffer, size_t longitud) {
        if (static_cast<size_t>(puertoSerie.availableForWrite()) < longitud) {
            return false;
        }
        size_t escritos = puertoSerie.write(buffer, longitud);
        if (perfilActual.modoDuplex == MODO_HALF_DUPLEX) {
            puertoSerie.flush(true);
        }
        return escritos == longitud;
    }

    /**
     * Función: cantidadBytesDisponiblesLectura
     * ¿Para qué sirve?: Informa cuántos bytes han llegado al buffer RX del hardware.
     * ¿Cómo lo hace?: Invoca directamente al método nativo available() de HardwareSerial.
     */
    int cantidadBytesDisponiblesLectura() {
        return puertoSerie.available();
    }

    /**
     * Función: leerByteEntrante
     * ¿Para qué sirve?: Extrae y consume el siguiente byte de la cola de recepción UART.
     * ¿Cómo lo hace?: Llama al método read() de la instancia HardwareSerial.
     */
    int leerByteEntrante() {
        return puertoSerie.read();
    }

    /**
     * Función: espiarSiguienteByte
     * ¿Para qué sirve?: Permite observar el próximo byte en el buffer RX sin extraerlo de la cola.
     * ¿Cómo lo hace?: Llama al método nativo peek() de HardwareSerial.
     */
    int espiarSiguienteByte() {
        return puertoSerie.peek();
    }

    /**
     * Función: obtenerPerfilCapacidades
     * ¿Para qué sirve?: Expone las métricas adaptativas calculadas a las capas de entrada y salida.
     * ¿Cómo lo hace?: Retorna una referencia constante a la estructura interna perfilActual.
     */
    const PerfilCapacidadesUART& obtenerPerfilCapacidades() const {
        return perfilActual;
    }
};

#endif