#ifndef GESTOR_RETRANSMISION_Y_ENVIO_H
#define GESTOR_RETRANSMISION_Y_ENVIO_H

#include <Arduino.h>
#include "../UART/AdaptadorHardwareUART.hpp"
#include "../utils/DefinicionTramaUARTS.hpp"

/**
 * Constante: CAPACIDAD_COLA_SALIDA
 * ¿Para qué sirve?: Define cuántos segmentos pueden estar en tránsito o esperando confirmación simultáneamente.
 */
static const uint8_t CAPACIDAD_COLA_SALIDA = 16;

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
    // ¿Para qué sirve?: Referencia al adaptador de hardware UART para transmitir los bytes.
    AdaptadorHardwareUART& adaptadorUART;
    // ¿Para qué sirve?: Cola estática de segmentos pendientes de ACK.
    RanuraTransmisionPendiente colaSalida[CAPACIDAD_COLA_SALIDA];
    // ¿Para qué sirve?: Almacena la ventana de recepción reportada por el nodo remoto (Electron).
    uint8_t ventanaRemotaDisponible;
    // ¿Para qué sirve?: Registra si este nodo tiene permiso de transmitir cuando el enlace es Half-Duplex.
    bool turnoTransmisionActivo;
    // ¿Para qué sirve?: Guarda el instante en microsegundos del último paquete enviado para respetar la pausa entre tramas.
    unsigned long ultimoEnvioMicros;

    /**
     * Función: transmitirTramaAlHardware
     * ¿Para qué sirve?: Aplana una estructura TramaSegmentoUARTS en un arreglo continuo de bytes y la envía por UART.
     * ¿Cómo lo hace?: Empaqueta en orden SOF, banderas, secuencia (2 bytes), confirmación (2 bytes), ventana, longitud, datos útiles y EOF (0x55) en un buffer temporal y llama a escribirBytesCrudos().
     */
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

public:
    /**
     * Constructor: GestorRetransmisionYEnvio
     * ¿Para qué sirve?: Inicializa la cola de transmisión y el control de flujo saliente.
     * ¿Cómo lo hace?: Marca todas las ranuras de colaSalida como desocupadas y habilita el turno de transmisión inicial.
     */
    GestorRetransmisionYEnvio(AdaptadorHardwareUART& uart)
        : adaptadorUART(uart), ventanaRemotaDisponible(CAPACIDAD_COLA_SALIDA),
          turnoTransmisionActivo(true), ultimoEnvioMicros(0) {
        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            colaSalida[i].ocupada = false;
        }
    }

    /**
     * Función: encolarSegmentoParaEnvio
     * ¿Para qué sirve?: Inserta un segmento nuevo en la cola de fiabilidad para ser enviado y monitoreado.
     * ¿Cómo lo hace?: Busca la primera ranura libre en colaSalida, copia la trama e inicializa su contador de intentos en cero.
     */
    bool encolarSegmentoParaEnvio(const TramaSegmentoUARTS& trama) {
        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            if (!colaSalida[i].ocupada) {
                colaSalida[i].ocupada = true;
                colaSalida[i].enviadoAlCable = false;
                colaSalida[i].instanteUltimoEnvioMs = 0;
                colaSalida[i].numeroIntentos = 0;
                colaSalida[i].trama = trama;
                return true;
            }
        }
        return false;
    }

    void reiniciarColaSalida() {
        ventanaRemotaDisponible = CAPACIDAD_COLA_SALIDA;
        turnoTransmisionActivo = true;
        ultimoEnvioMicros = 0;
        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            colaSalida[i].ocupada = false;
            colaSalida[i].enviadoAlCable = false;
            colaSalida[i].instanteUltimoEnvioMs = 0;
            colaSalida[i].numeroIntentos = 0;
        }
    }

    /**
     * Función: enviarTramaControlInmediata
     * ¿Para qué sirve?: Envía paquetes de control puros (ACK o NACK) de 8 bytes sin ocupar ranuras de retransmisión de datos.
     * ¿Cómo lo hace?: Construye una trama sin carga útil (longitudDatos = 0) con la bandera solicitada y la transmite directamente al adaptador UART.
     */
    bool enviarTramaControlInmediata(uint8_t banderasControl, uint16_t numConfirmacion, uint8_t ventanaLocal) {
        TramaSegmentoUARTS control;
        control.inicioTrama = BYTE_INICIO_TRAMA;
        control.banderas = banderasControl;
        control.numeroSecuencia = 0;
        control.numeroConfirmacion = numConfirmacion;
        control.ventanaRecepcion = (ventanaLocal == 0) ? 1 : ventanaLocal;
        control.longitudDatos = 0;
        control.finTrama = BYTE_FIN_TRAMA;
        return transmitirTramaAlHardware(control, true);
    }

    /**
     * Procedimiento: procesarConfirmacionEntrante
     * ¿Para qué sirve?: Libera de la cola los segmentos que ya fueron confirmados (ACK) o dispara reenvío inmediato ante un NACK.
     * ¿Cómo lo hace?: Actualiza la ventana remota disponible; si recibe BANDERA_ACK libera la ranura cuyo numeroSecuencia coincide, y si recibe BANDERA_NACK reinicia el temporizador de esa ranura para forzar su retransmisión en el ciclo actual.
     */
    void procesarConfirmacionEntrante(uint8_t banderasRecibidas, uint16_t numAckRecibido, uint8_t ventanaRemota) {
        ventanaRemotaDisponible = (ventanaRemota == 0) ? 1 : ventanaRemota;
        if (banderasRecibidas & BANDERA_CEDER_TURNO_HD) {
            turnoTransmisionActivo = true;
        }

        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            if (colaSalida[i].ocupada && colaSalida[i].trama.numeroSecuencia == numAckRecibido) {
                if (banderasRecibidas & BANDERA_ACK) {
                    colaSalida[i].ocupada = false;
                    turnoTransmisionActivo = true;
                } else if (banderasRecibidas & BANDERA_NACK) {
                    colaSalida[i].instanteUltimoEnvioMs = 0;
                    turnoTransmisionActivo = true;
                }
            }
        }
    }

    /**
     * Procedimiento: actualizarCicloEnvioYRetransmision
     * ¿Para qué sirve?: Ejecuta el envío de segmentos nuevos y la retransmisión de segmentos vencidos respetando el control de flujo y el modo duplex.
     * ¿Cómo lo hace?: Verifica cuántos segmentos están en vuelo sin ACK, los compara contra el límite de ventana (1 en Half-Duplex, 2 sin RTS/CTS físico, o hasta ventanaRemotaDisponible con flujo HW) y transmite o retransmite cuando se supera el tiempoEsperaRetransmisionMs del perfil UART.
     */
    void actualizarCicloEnvioYRetransmision() {
        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        if (perfil.modoDuplex == MODO_HALF_DUPLEX && !turnoTransmisionActivo) {
            return;
        }

        unsigned long ahoraMs = millis();
        uint8_t enVuelo = 0;

        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            if (colaSalida[i].ocupada && colaSalida[i].enviadoAlCable) {
                enVuelo++;
                if (ahoraMs - colaSalida[i].instanteUltimoEnvioMs >= perfil.tiempoEsperaRetransmisionMs) {
                    if (colaSalida[i].numeroIntentos >= MAXIMO_INTENTOS_RETRANSMISION) {
                        colaSalida[i].ocupada = false;
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

        for (uint8_t i = 0; i < CAPACIDAD_COLA_SALIDA; i++) {
            if (colaSalida[i].ocupada && !colaSalida[i].enviadoAlCable) {
                if (transmitirTramaAlHardware(colaSalida[i].trama)) {
                    colaSalida[i].enviadoAlCable = true;
                    colaSalida[i].instanteUltimoEnvioMs = ahoraMs;
                    colaSalida[i].numeroIntentos = 1;
                }
                break;
            }
        }
    }
};

#endif