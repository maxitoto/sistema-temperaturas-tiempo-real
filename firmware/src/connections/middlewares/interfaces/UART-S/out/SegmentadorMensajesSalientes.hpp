#ifndef SEGMENTADOR_MENSAJES_SALIENTES_H
#define SEGMENTADOR_MENSAJES_SALIENTES_H

#include <string.h>
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Clase: SegmentadorMensajesSalientes
 * ¿Para qué sirve?: Divide mensajes largos de aplicación en segmentos numerados que respetan el tamaño del FIFO de hardware UART sin alterar el formato del mensaje.
 */
class SegmentadorMensajesSalientes {
public:
    /**
     * Función: dividirMensajeEnSegmentos
     * ¿Para qué sirve?: Fracciona un buffer de bytes en múltiples estructuras TramaSegmentoUARTS listas para encolar.
     * ¿Cómo lo hace?: Toma el tamaño máximo de payload permitido por el perfil del hardware, copia fragmentos sucesivos con memcpy(), asigna números de secuencia correlativos, activa BANDERA_MAS_FRAGMENTOS en todos los segmentos excepto el último y cierra cada paquete con BYTE_FIN_TRAMA.
     */
    static uint8_t dividirMensajeEnSegmentos(
        const uint8_t* mensajeCompleto,
        size_t longitudMensaje,
        uint8_t tamanoMaximoPayloadHardware,
        uint16_t& contadorSecuenciaActual,
        uint16_t ultimoAckRecibido,
        uint8_t ventanaLocalDisponible,
        bool esHalfDuplex,
        TramaSegmentoUARTS* arregloSalidaSegmentos,
        uint8_t capacidadMaximaArreglo
    ) {
        if (longitudMensaje == 0 || tamanoMaximoPayloadHardware == 0) {
            return 0;
        }

        uint8_t pasoCorte = (tamanoMaximoPayloadHardware > TAMANO_MAXIMO_PAYLOAD_ESTATICO)
                            ? TAMANO_MAXIMO_PAYLOAD_ESTATICO
                            : tamanoMaximoPayloadHardware;

        size_t offset = 0;
        uint8_t segmentosCreados = 0;

        while (offset < longitudMensaje && segmentosCreados < capacidadMaximaArreglo) {
            size_t restantes = longitudMensaje - offset;
            uint8_t longitudTrozo = (restantes > pasoCorte) ? pasoCorte : static_cast<uint8_t>(restantes);
            bool hayMasFragmentos = (offset + longitudTrozo) < longitudMensaje;

            TramaSegmentoUARTS& seg = arregloSalidaSegmentos[segmentosCreados];
            seg.inicioTrama = BYTE_INICIO_TRAMA;
            seg.banderas = BANDERA_DATOS;

            if (hayMasFragmentos) {
                seg.banderas |= BANDERA_MAS_FRAGMENTOS;
            }
            if (esHalfDuplex && !hayMasFragmentos) {
                seg.banderas |= BANDERA_CEDER_TURNO_HD;
            }

            seg.numeroSecuencia = contadorSecuenciaActual++;
            seg.numeroConfirmacion = ultimoAckRecibido;
            seg.ventanaRecepcion = ventanaLocalDisponible;
            seg.longitudDatos = longitudTrozo;

            memcpy(seg.datosUtiles, mensajeCompleto + offset, longitudTrozo);

            seg.finTrama = BYTE_FIN_TRAMA;

            offset += longitudTrozo;
            segmentosCreados++;
        }

        return segmentosCreados;
    }
};

#endif