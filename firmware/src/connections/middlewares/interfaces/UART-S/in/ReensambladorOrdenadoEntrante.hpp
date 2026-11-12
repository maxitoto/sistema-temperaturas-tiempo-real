#ifndef REENSAMBLADOR_ORDENADO_ENTRANTE_H
#define REENSAMBLADOR_ORDENADO_ENTRANTE_H

#include <string.h>
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Constante: CAPACIDAD_BUFFER_MENSAJE_COMPLETO
 * ¿Para qué sirve?: Define el tamaño máximo en bytes que puede tener un mensaje reensamblado para la aplicación.
 */
static const size_t CAPACIDAD_BUFFER_MENSAJE_COMPLETO = 1024;

/**
 * Clase: ReensambladorOrdenadoEntrante
 * ¿Para qué sirve?: Valida que los segmentos lleguen en estricto orden secuencial, descarta duplicados y une los fragmentos hasta reconstruir el mensaje original.
 */
class ReensambladorOrdenadoEntrante {
private:
    // ¿Para qué sirve?: Almacena los bytes acumulados de los segmentos recibidos en orden.
    uint8_t bufferReensamblado[CAPACIDAD_BUFFER_MENSAJE_COMPLETO];
    // ¿Para qué sirve?: Indica cuántos bytes se han acumulado del mensaje actual.
    size_t longitudAcumulada;
    // ¿Para qué sirve?: Registra el número de secuencia que debe tener el próximo segmento entrante válido.
    uint16_t secuenciaEsperada;
    // ¿Para qué sirve?: Indica si ya se recibió el último fragmento y el mensaje está listo para ser leído por la aplicación.
    bool mensajeCompletoListo;

public:
    /**
     * Constructor: ReensambladorOrdenadoEntrante
     * ¿Para qué sirve?: Inicializa los contadores de secuencia y limpia el buffer de reensamblaje.
     * ¿Cómo lo hace?: Fija longitudAcumulada en 0, secuenciaEsperada en 0 y mensajeCompletoListo en falso.
     */
    ReensambladorOrdenadoEntrante()
        : longitudAcumulada(0), secuenciaEsperada(0), mensajeCompletoListo(false) {}

    /**
     * Función: procesarSegmentoDatosOrdenado
     * ¿Para qué sirve?: Verifica el orden de un segmento entrante y anexa sus datos al buffer de reconstrucción.
     * ¿Cómo lo hace?: Compara numeroSecuencia con secuenciaEsperada; si coincide y hay espacio, copia los datos con memcpy(), incrementa secuenciaEsperada y verifica si BANDERA_MAS_FRAGMENTOS está apagada para marcar el mensaje como completo. Si es un duplicado anterior, retorna true para re-confirmar su ACK sin duplicar datos.
     */
    bool procesarSegmentoDatosOrdenado(const TramaSegmentoUARTS& segmento, bool& eraDuplicado) {
        eraDuplicado = false;

        if (mensajeCompletoListo) {
            return false;
        }

        if (segmento.numeroSecuencia == secuenciaEsperada) {
            if (longitudAcumulada + segmento.longitudDatos > CAPACIDAD_BUFFER_MENSAJE_COMPLETO) {
                longitudAcumulada = 0;
                return false;
            }

            memcpy(bufferReensamblado + longitudAcumulada, segmento.datosUtiles, segmento.longitudDatos);
            longitudAcumulada += segmento.longitudDatos;
            secuenciaEsperada++;

            if (!(segmento.banderas & BANDERA_MAS_FRAGMENTOS)) {
                mensajeCompletoListo = true;
            }
            return true;
        }

        if (static_cast<int16_t>(segmento.numeroSecuencia - secuenciaEsperada) < 0) {
            eraDuplicado = true;
            return true;
        }

        return false;
    }

    /**
     * Función: hayMensajeCompletoDisponible
     * ¿Para qué sirve?: Informa a la capa superior si hay un mensaje entero listo para entregar.
     * ¿Cómo lo hace?: Retorna el estado booleano de mensajeCompletoListo.
     */
    bool hayMensajeCompletoDisponible() const {
        return mensajeCompletoListo;
    }

    /**
     * Función: extraerMensajeReensamblado
     * ¿Para qué sirve?: Copia el mensaje completo hacia el buffer de la aplicación y libera el reensamblador.
     * ¿Cómo lo hace?: Copia longitudAcumulada bytes hacia bufferDestino, reinicia longitudAcumulada a cero y apaga la bandera mensajeCompletoListo.
     */
    size_t extraerMensajeReensamblado(uint8_t* bufferDestino, size_t capacidadDestino) {
        if (!mensajeCompletoListo) {
            return 0;
        }
        size_t aCopiar = (longitudAcumulada < capacidadDestino) ? longitudAcumulada : capacidadDestino;
        memcpy(bufferDestino, bufferReensamblado, aCopiar);
        longitudAcumulada = 0;
        mensajeCompletoListo = false;
        return aCopiar;
    }

    /**
     * Función: calcularVentanaSegmentosDisponible
     * ¿Para qué sirve?: Calcula cuántos segmentos más caben en el buffer de entrada para informar al emisor (Control de Flujo).
     * ¿Cómo lo hace?: Resta longitudAcumulada a CAPACIDAD_BUFFER_MENSAJE_COMPLETO y lo divide por el tamaño máximo de segmento del perfil UART.
     */
    uint8_t calcularVentanaSegmentosDisponible(uint8_t tamanoMaximoSegmento) const {
        if (tamanoMaximoSegmento == 0) {
            return 0;
        }
        if (mensajeCompletoListo) {
            size_t ranurasTotales = CAPACIDAD_BUFFER_MENSAJE_COMPLETO / tamanoMaximoSegmento;
            return (ranurasTotales > 255) ? 255 : static_cast<uint8_t>(ranurasTotales);
        }
        size_t bytesLibres = CAPACIDAD_BUFFER_MENSAJE_COMPLETO - longitudAcumulada;
        size_t ranuras = bytesLibres / tamanoMaximoSegmento;
        if (ranuras == 0) {
            return 1;
        }
        return (ranuras > 255) ? 255 : static_cast<uint8_t>(ranuras);
    }

    /**
     * Función: obtenerSecuenciaEsperada
     * ¿Para qué sirve?: Devuelve el número de secuencia que este receptor espera recibir a continuación.
     * ¿Cómo lo hace?: Retorna el valor de secuenciaEsperada.
     */
    uint16_t obtenerSecuenciaEsperada() const {
        return secuenciaEsperada;
    }
};

#endif