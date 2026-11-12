#ifndef DEFINICION_TRAMA_UARTS_H
#define DEFINICION_TRAMA_UARTS_H

#include <stdint.h>
#include <stddef.h>

/**
 * Constante: BYTE_INICIO_TRAMA
 * ¿Para qué sirve?: Marca el inicio físico de un paquete binario en el flujo de bytes (valor 0xAA).
 */
static const uint8_t BYTE_INICIO_TRAMA = 0xAA;

/**
 * Constante: BYTE_FIN_TRAMA
 * ¿Para qué sirve?: Marca el cierre físico de un paquete binario para validar que no se perdieron ni desplazaron bytes (valor 0x55).
 */
static const uint8_t BYTE_FIN_TRAMA = 0x55;

/**
 * Constante: TAMANO_CABECERA_Y_PIE_TRAMA
 * ¿Para qué sirve?: Define el costo fijo en bytes de control por cada segmento sin checksum (SOF + FLAGS + SEQ + ACK + WIN + LEN + EOF = 9 bytes).
 */
static const uint8_t TAMANO_CABECERA_Y_PIE_TRAMA = 9;

/**
 * Constante: TAMANO_MAXIMO_PAYLOAD_ESTATICO
 * ¿Para qué sirve?: Establece el límite superior de memoria estática reservada para los datos útiles de un segmento (112 bytes, para completar el umbral de 120 bytes del FIFO).
 */
static const uint8_t TAMANO_MAXIMO_PAYLOAD_ESTATICO = 112;

/**
 * Enumeración: BanderasControlTrama
 * ¿Para qué sirve?: Define los bits de control de transporte estilo TCP dentro del byte FLAGS de cada segmento.
 */
enum BanderasControlTrama : uint8_t {
    // ¿Para qué sirve?: Indica que la trama transporta datos de aplicación.
    BANDERA_DATOS          = 0x01,
    // ¿Para qué sirve?: Confirma la recepción exitosa de un número de secuencia (ACK).
    BANDERA_ACK            = 0x02,
    // ¿Para qué sirve?: Solicita retransmisión inmediata por error de formato o desorden (NACK).
    BANDERA_NACK           = 0x04,
    // ¿Para qué sirve?: Indica que este segmento es parte de un mensaje mayor y faltan fragmentos por llegar.
    BANDERA_MAS_FRAGMENTOS = 0x08,
    // ¿Para qué sirve?: Cede el turno de transmisión del canal cuando el hardware opera en modo Half-Duplex.
    BANDERA_CEDER_TURNO_HD = 0x10,
    // ¿Para qué sirve?: Sincroniza los contadores de secuencia al iniciar la conexión.
    BANDERA_SINCRONIZAR    = 0x20,
    // ¿Para qué sirve?: ESP32 avise instantáneamente a Electron cuando cierre la conexión por inactividad
    BANDERA_CERRAR         = 0x40
};

/**
 * Estructura: TramaSegmentoUARTS
 * ¿Para qué sirve?: Representa en memoria un paquete individual con control de flujo, orden y longitud, delegando la integridad al bit de paridad del hardware UART.
 */
struct TramaSegmentoUARTS {
    // ¿Para qué sirve?: Almacena el byte delimitador de apertura (0xAA).
    uint8_t inicioTrama;
    // ¿Para qué sirve?: Contiene la combinación de bits de BanderasControlTrama.
    uint8_t banderas;
    // ¿Para qué sirve?: Identifica el orden secuencial del segmento enviado.
    uint16_t numeroSecuencia;
    // ¿Para qué sirve?: Indica el último número de secuencia recibido correctamente desde el otro extremo.
    uint16_t numeroConfirmacion;
    // ¿Para qué sirve?: Informa cuántos segmentos adicionales puede recibir este nodo antes de saturar su buffer (Control de Flujo).
    uint8_t ventanaRecepcion;
    // ¿Para qué sirve?: Indica la cantidad exacta de bytes útiles contenidos en el arreglo datosUtiles.
    uint8_t longitudDatos;
    // ¿Para qué sirve?: Almacena la carga útil (fragmento del mensaje original sin alterar su formato).
    uint8_t datosUtiles[TAMANO_MAXIMO_PAYLOAD_ESTATICO];
    // ¿Para qué sirve?: Almacena el byte delimitador de cierre (0x55).
    uint8_t finTrama;
};

#endif