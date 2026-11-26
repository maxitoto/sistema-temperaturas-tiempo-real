#ifndef DESERIALIZADOR_TRAMAS_ENTRANTES_H
#define DESERIALIZADOR_TRAMAS_ENTRANTES_H

#include "../UART/AdaptadorHardwareUART.hpp"
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Enumeración: ResultadoLecturaTrama
 * ¿Para qué sirve?: Informa el estado del lector después de procesar los bytes disponibles en el buffer RX.
 */
enum ResultadoLecturaTrama : uint8_t {
    // ¿Para qué sirve?: Indica que aún no se completó de recibir una trama entera.
    LECTURA_EN_PROGRESO = 0,
    // ¿Para qué sirve?: Indica que se recibió una trama completa con delimitadores y longitud válidos.
    TRAMA_VALIDA_COMPLETA = 1,
    // ¿Para qué sirve?: Indica que se detectó una longitud inválida o no coincidió el byte de cierre EOF (0x55).
    ERROR_FORMATO_TRAMA = 2
};

/**
 * Clase: DeserializadorTramasEntrantes
 * ¿Para qué sirve?: Reconstruye estructuras TramaSegmentoUARTS byte por byte desde el flujo de la UART sin bloquear el procesador.
 */
class DeserializadorTramasEntrantes {
private:
    /**
     * Enumeración: EstadoParserBytes
     * ¿Para qué sirve?: Define los pasos secuenciales de la máquina de estados de lectura.
     */
    enum EstadoParserBytes : uint8_t {
        ESPERANDO_SOF = 0,
        LEYENDO_CABECERA,
        LEYENDO_PAYLOAD,
        LEYENDO_EOF
    };

    // ¿Para qué sirve?: Mantiene el estado actual de reconstrucción de la trama.
    EstadoParserBytes estadoActual;
    // ¿Para qué sirve?: Almacena temporalmente los 7 bytes de cabecera conforme llegan.
    uint8_t bufferCabecera[7];
    // ¿Para qué sirve?: Cuenta cuántos bytes se han leído dentro del estado actual.
    uint8_t indiceBytesEstado;
    // ¿Para qué sirve?: Contiene la trama que se está ensamblando actualmente.
    TramaSegmentoUARTS tramaEnConstruccion;

public:
    /**
     * Constructor: DeserializadorTramasEntrantes
     * ¿Para qué sirve?: Inicia la máquina de estados buscando el byte de inicio de trama (0xAA).
     * ¿Cómo lo hace?: Invoca al procedimiento reiniciarEstadoParser().
     */
    DeserializadorTramasEntrantes() {
        reiniciarEstadoParser();
    }

    /**
     * Procedimiento: reiniciarEstadoParser
     * ¿Para qué sirve?: Devuelve la máquina de estados a la búsqueda de un nuevo inicio de paquete (0xAA).
     * ¿Cómo lo hace?: Fija estadoActual en ESPERANDO_SOF y pone indiceBytesEstado en cero.
     */
    void reiniciarEstadoParser() {
        estadoActual = ESPERANDO_SOF;
        indiceBytesEstado = 0;
    }

    /**
     * Función: procesarBytesDisponibles
     * ¿Para qué sirve?: Lee los bytes acumulados en el hardware UART y avanza la reconstrucción de la trama.
     * ¿Cómo lo hace?: Extrae bytes con leerByteEntrante() mientras cantidadBytesDisponiblesLectura() sea mayor a cero, llena los 7 bytes de cabecera, extrae la cantidad exacta de bytes de payload indicada por longitudDatos y verifica que el último byte coincida con BYTE_FIN_TRAMA (0x55).
     */
    ResultadoLecturaTrama procesarBytesDisponibles(AdaptadorHardwareUART& uart, TramaSegmentoUARTS& tramaSalida) {
        while (uart.cantidadBytesDisponiblesLectura() > 0) {
            uint8_t byteLeido = static_cast<uint8_t>(uart.leerByteEntrante());

            switch (estadoActual) {
                case ESPERANDO_SOF:
                    if (byteLeido == BYTE_INICIO_TRAMA) {
                        tramaEnConstruccion.inicioTrama = byteLeido;
                        indiceBytesEstado = 0;
                        estadoActual = LEYENDO_CABECERA;
                    }
                    break;

                case LEYENDO_CABECERA:
                    bufferCabecera[indiceBytesEstado++] = byteLeido;
                    if (indiceBytesEstado == 7) {
                        tramaEnConstruccion.banderas = bufferCabecera[0];
                        tramaEnConstruccion.numeroSecuencia = (static_cast<uint16_t>(bufferCabecera[1]) << 8) | bufferCabecera[2];
                        tramaEnConstruccion.numeroConfirmacion = (static_cast<uint16_t>(bufferCabecera[3]) << 8) | bufferCabecera[4];
                        tramaEnConstruccion.ventanaRecepcion = bufferCabecera[5];
                        tramaEnConstruccion.longitudDatos = bufferCabecera[6];

                        if (tramaEnConstruccion.longitudDatos > TAMANO_MAXIMO_PAYLOAD_ESTATICO) {
                            reiniciarEstadoParser();
                            return ERROR_FORMATO_TRAMA;
                        }

                        indiceBytesEstado = 0;
                        estadoActual = (tramaEnConstruccion.longitudDatos > 0) ? LEYENDO_PAYLOAD : LEYENDO_EOF;
                    }
                    break;

                case LEYENDO_PAYLOAD:
                    tramaEnConstruccion.datosUtiles[indiceBytesEstado++] = byteLeido;
                    if (indiceBytesEstado == tramaEnConstruccion.longitudDatos) {
                        indiceBytesEstado = 0;
                        estadoActual = LEYENDO_EOF;
                    }
                    break;

                case LEYENDO_EOF:
                    tramaEnConstruccion.finTrama = byteLeido;
                    reiniciarEstadoParser();

                    if (tramaEnConstruccion.finTrama != BYTE_FIN_TRAMA) {
                        return ERROR_FORMATO_TRAMA;
                    }

                    tramaSalida = tramaEnConstruccion;
                    return TRAMA_VALIDA_COMPLETA;
            }
        }
        return LECTURA_EN_PROGRESO;
    }

    /**
     * Función: obtenerUltimaSecuenciaDetectada
     * ¿Para qué sirve?: Permite conocer qué número de secuencia tenía una trama que falló en su delimitador EOF para emitir su NACK correspondiente.
     * ¿Cómo lo hace?: Retorna el campo numeroSecuencia almacenado en tramaEnConstruccion.
     */
    uint16_t obtenerUltimaSecuenciaDetectada() const {
        return tramaEnConstruccion.numeroSecuencia;
    }
};

#endif