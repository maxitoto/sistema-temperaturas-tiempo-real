#ifndef GESTOR_RETRANSMISION_Y_ENVIO_H
#define GESTOR_RETRANSMISION_Y_ENVIO_H

#include <Arduino.h>
#include "../UART/AdaptadorHardwareUART.hpp"
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Constante: CAPACIDAD_COLA_SALIDA
 * ¿Para qué sirve?: Define cuántos segmentos pueden estar en tránsito o esperando confirmación simultáneamente.
 */
static const uint8_t CAPACIDAD_COLA_SALIDA = 32;

/**
 * Constante: MAXIMO_INTENTOS_RETRANSMISION
 * ¿Para qué sirve?: Limita cuántas veces se reintenta enviar un mismo segmento antes de declararlo fallido.
 */
static const uint8_t MAXIMO_INTENTOS_RETRANSMISION = 5;

/**
 * Estructura: RanuraTransmisionPendiente
 * ¿Para qué sirve?: Registra el estado de envío, tiempo y reintentos de cada segmento en la ventana de salida.
 */
struct RanuraTransmisionPendiente {
    // ¿Para qué sirve?: Indica si esta ranura contiene un segmento pendiente de confirmación.
    bool ocupada;
    // ¿Para qué sirve?: Indica si el segmento ya fue enviado al menos una vez por el cable serie.
    bool enviadoAlCable;
    // ¿Para qué sirve?: Guarda el instante (millis) del último envío para evaluar el timeout RTO.
    unsigned long instanteUltimoEnvioMs;
    // ¿Para qué sirve?: Cuenta cuántas veces se ha transmitido este segmento.
    uint8_t numeroIntentos;
    // ¿Para qué sirve?: Almacena la copia completa de la trama por si se requiere retransmitirla.
    TramaSegmentoUARTS trama;
};

/**
 * Clase: GestorRetransmisionYEnvio
 * ¿Para qué sirve?: Administra la cola de salida, vuelca las tramas hacia el hardware UART, respeta la ventana del receptor y ejecuta retransmisiones automáticas por timeout o NACK.
 */
class GestorRetransmisionYEnvio {
private:
    AdaptadorHardwareUART& adaptadorUART;
    RanuraTransmisionPendiente colaSalida[CAPACIDAD_COLA_SALIDA];
    uint8_t idxCabeza;
    uint8_t idxCola;
    uint8_t cantidadEnCola;
    uint8_t ventanaRemotaDisponible;
    bool turnoTransmisionActivo;
    unsigned long ultimoEnvioMicros;

    bool transmitirTramaAlHardware(const TramaSegmentoUARTS& t, bool esControlInmediato = false) {
        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        if (!esControlInmediato && (micros() - ultimoEnvioMicros < perfil.pausaEntreTramasUs)) {
            return false;
        }

        uint8_t bufferCable[TAMANO_MAXIMO_PAYLOAD_ESTATICO + TAMANO_CABECERA_Y_PIE_TRAMA];
        size_t idx = 0;

        bufferCable[idx++] = t.inicioTrama;
        bufferCable[idx++] = t.banderas;
        bufferCable[idx++] = static_cast<uint8_t>((t.numeroSecuencia >> 8) & 0xFF);
        bufferCable[idx++] = static_cast<uint8_t>(t.numeroSecuencia & 0xFF);
        bufferCable[idx++] = static_cast<uint8_t>((t.numeroConfirmacion >> 8) & 0xFF);
        bufferCable[idx++] = static_cast<uint8_t>(t.numeroConfirmacion & 0xFF);
        bufferCable[idx++] = t.ventanaRecepcion;
        bufferCable[idx++] = t.longitudDatos;

        if (t.longitudDatos > 0) {
            memcpy(&bufferCable[idx], t.datosUtiles, t.longitudDatos);
            idx += t.longitudDatos;
        }

        bufferCable[idx++] = t.finTrama;

        if (adaptadorUART.escribirBytesCrudos(bufferCable, idx)) {
            ultimoEnvioMicros = micros();
            if ((t.banderas & BANDERA_CEDER_TURNO_HD) && perfil.modoDuplex == MODO_HALF_DUPLEX) {
                turnoTransmisionActivo = false;
            }
            return true;
        }
        return false;
    }

    void compactarCabezaCola() {
        while (cantidadEnCola > 0 && !colaSalida[idxCabeza].ocupada) {
            idxCabeza = static_cast<uint8_t>((idxCabeza + 1) % CAPACIDAD_COLA_SALIDA);
            cantidadEnCola--;
        }
    }

public:
    GestorRetransmisionYEnvio(AdaptadorHardwareUART& uart)
        : adaptadorUART(uart), idxCabeza(0), idxCola(0), cantidadEnCola(0),
          ventanaRemotaDisponible(CAPACIDAD_COLA_SALIDA),
          turnoTransmisionActivo(true), ultimoEnvioMicros(0) {
        reiniciarColaSalida();
    }

    uint8_t obtenerRanurasLibres() {
        compactarCabezaCola();
        return (cantidadEnCola < CAPACIDAD_COLA_SALIDA)
               ? static_cast<uint8_t>(CAPACIDAD_COLA_SALIDA - cantidadEnCola)
               : 0;
    }

    bool encolarSegmentoParaEnvio(const TramaSegmentoUARTS& trama) {
        compactarCabezaCola();
        if (cantidadEnCola >= CAPACIDAD_COLA_SALIDA) {
            return false;
        }

        RanuraTransmisionPendiente& slot = colaSalida[idxCola];
        slot.ocupada               = true;
        slot.enviadoAlCable        = false;
        slot.instanteUltimoEnvioMs = 0;
        slot.numeroIntentos        = 0;
        slot.trama                 = trama;

        idxCola = static_cast<uint8_t>((idxCola + 1) % CAPACIDAD_COLA_SALIDA);
        cantidadEnCola++;
        return true;
    }

    void reiniciarColaSalida() {
        idxCabeza               = 0;
        idxCola                 = 0;
        cantidadEnCola          = 0;
        ventanaRemotaDisponible = CAPACIDAD_COLA_SALIDA;
        turnoTransmisionActivo  = true;
        ultimoEnvioMicros       = 0;
        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            colaSalida[i].ocupada               = false;
            colaSalida[i].enviadoAlCable        = false;
            colaSalida[i].instanteUltimoEnvioMs = 0;
            colaSalida[i].numeroIntentos        = 0;
        }
    }

    bool enviarTramaControlInmediata(uint8_t banderasControl, uint16_t numConfirmacion, uint8_t ventanaLocal) {
        TramaSegmentoUARTS control;
        control.inicioTrama        = BYTE_INICIO_TRAMA;
        control.banderas           = banderasControl;
        control.numeroSecuencia    = 0;
        control.numeroConfirmacion = numConfirmacion;
        control.ventanaRecepcion   = (ventanaLocal == 0) ? 1 : ventanaLocal;
        control.longitudDatos      = 0;
        control.finTrama           = BYTE_FIN_TRAMA;
        return transmitirTramaAlHardware(control, true);
    }

    void procesarConfirmacionEntrante(uint8_t banderasRecibidas, uint16_t numAckRecibido, uint8_t ventanaRemota) {
        ventanaRemotaDisponible = (ventanaRemota == 0) ? 1 : ventanaRemota;
        if (banderasRecibidas & BANDERA_CEDER_TURNO_HD) {
            turnoTransmisionActivo = true;
        }

        if (banderasRecibidas & BANDERA_ACK) {
            // Confirmación acumulativa: libera cualquier segmento enviado con secuencia <= numAckRecibido
            for (uint8_t k = 0; k < cantidadEnCola; k++) {
                uint8_t i = static_cast<uint8_t>((idxCabeza + k) % CAPACIDAD_COLA_SALIDA);
                if (colaSalida[i].ocupada && colaSalida[i].enviadoAlCable) {
                    const int16_t diff = static_cast<int16_t>(colaSalida[i].trama.numeroSecuencia - numAckRecibido);
                    if (diff <= 0) {
                        colaSalida[i].ocupada = false;
                        turnoTransmisionActivo = true;
                    }
                }
            }
        } else if (banderasRecibidas & BANDERA_NACK) {
            // Go-Back-N: marca desde la secuencia pedida por el receptor en adelante para reenvío en orden
            for (uint8_t k = 0; k < cantidadEnCola; k++) {
                uint8_t i = static_cast<uint8_t>((idxCabeza + k) % CAPACIDAD_COLA_SALIDA);
                if (colaSalida[i].ocupada && colaSalida[i].enviadoAlCable) {
                    const int16_t diff = static_cast<int16_t>(colaSalida[i].trama.numeroSecuencia - numAckRecibido);
                    if (diff == 0) {
                        colaSalida[i].instanteUltimoEnvioMs = 0;
                        turnoTransmisionActivo = true;
                    } else if (diff > 0) {
                        colaSalida[i].enviadoAlCable        = false;
                        colaSalida[i].instanteUltimoEnvioMs = 0;
                    }
                }
            }
        }
        compactarCabezaCola();
    }

    void actualizarCicloEnvioYRetransmision() {
        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        if (perfil.modoDuplex == MODO_HALF_DUPLEX && !turnoTransmisionActivo) {
            return;
        }

        compactarCabezaCola();
        unsigned long ahoraMs = millis();
        uint8_t enVuelo = 0;

        for (uint8_t k = 0; k < cantidadEnCola; k++) {
            uint8_t i = static_cast<uint8_t>((idxCabeza + k) % CAPACIDAD_COLA_SALIDA);
            if (colaSalida[i].ocupada && colaSalida[i].enviadoAlCable) {
                enVuelo++;
                if (ahoraMs - colaSalida[i].instanteUltimoEnvioMs >= perfil.tiempoEsperaRetransmisionMs) {
                    if (colaSalida[i].numeroIntentos >= MAXIMO_INTENTOS_RETRANSMISION) {
                        colaSalida[i].ocupada = false;
                        compactarCabezaCola();
                    } else if (transmitirTramaAlHardware(colaSalida[i].trama)) {
                        colaSalida[i].instanteUltimoEnvioMs = ahoraMs;
                        colaSalida[i].numeroIntentos++;
                        return;
                    }
                }
            }
        }

        uint8_t limiteVentanaEfectivo = ventanaRemotaDisponible;
        if (perfil.modoDuplex == MODO_HALF_DUPLEX || !perfil.controlFlujoHardwareActivo) {
            uint8_t topeSoftware = (perfil.modoDuplex == MODO_HALF_DUPLEX) ? 1 : 2;
            if (limiteVentanaEfectivo > topeSoftware) {
                limiteVentanaEfectivo = topeSoftware;
            }
        }

        if (enVuelo >= limiteVentanaEfectivo) {
            return;
        }

        for (uint8_t k = 0; k < cantidadEnCola; k++) {
            uint8_t i = static_cast<uint8_t>((idxCabeza + k) % CAPACIDAD_COLA_SALIDA);
            if (colaSalida[i].ocupada && !colaSalida[i].enviadoAlCable) {
                if (transmitirTramaAlHardware(colaSalida[i].trama)) {
                    colaSalida[i].enviadoAlCable        = true;
                    colaSalida[i].instanteUltimoEnvioMs = ahoraMs;
                    colaSalida[i].numeroIntentos        = 1;
                }
                break;
            }
        }
    }
};

#endif