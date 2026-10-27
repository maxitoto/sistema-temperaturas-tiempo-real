// Archivo: ./src/communication/common/BufferRed.h
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <rom/crc.h>   // crc16_be() acelerado por hardware ROM del ESP32

namespace LimitesPila {
    static constexpr int    MAX_NODOS_RED            = 4;   
    static constexpr size_t TAMANO_MAX_CHUNK         = 64;
    static constexpr size_t MAX_TRAMA_ENLACE         = 512;

    static constexpr size_t TAMANO_CABECERA_RED      = 2;
    static constexpr size_t TAMANO_CABECERA_TRANS    = 10;
    static constexpr size_t TAMANO_RECORD_MARKER_TCP = 4; // RFC 5531 Record Marking (Wi-Fi TCP)
    static constexpr size_t TAMANO_SOBRE_SESION      = 8;

    static constexpr size_t OFFSET_TRANSPORTE        = TAMANO_CABECERA_RED;
    static constexpr size_t OFFSET_SESION            = OFFSET_TRANSPORTE + TAMANO_CABECERA_TRANS;
    static constexpr size_t OFFSET_PAYLOAD_APP       = OFFSET_SESION + TAMANO_SOBRE_SESION; // 20 bytes
    static constexpr size_t MAX_PAYLOAD_APP          = MAX_TRAMA_ENLACE - OFFSET_PAYLOAD_APP; // 492 bytes
}

struct BufferPaquete {
    int idNodo = -1;
    bool esDatagramaNativo = false;
    alignas(4) uint8_t raw[LimitesPila::MAX_TRAMA_ENLACE];
    size_t inicio = LimitesPila::OFFSET_PAYLOAD_APP;
    size_t longitud = 0;

    void prepararParaTx(int destino = -1) {
        idNodo = destino;
        esDatagramaNativo = false;
        inicio = LimitesPila::OFFSET_PAYLOAD_APP;
        longitud = 0;
    }

    void prepararParaRx(int origen = -1) {
        idNodo = origen;
        esDatagramaNativo = false;
        inicio = 0;
        longitud = 0;
    }
    // ... resto igual ...

    uint8_t* ptrActual() {
        return &raw[inicio];
    }

    const uint8_t* ptrActual() const {
        return &raw[inicio];
    }

    uint8_t* ptrPayloadApp() {
        return &raw[LimitesPila::OFFSET_PAYLOAD_APP];
    }

    // Retrocede el puntero 'inicio' para anteponer una cabecera en TX sin usar memcpy
    uint8_t* empujarCabecera(size_t bytesCabecera) {
        if (inicio < bytesCabecera) return nullptr;
        inicio -= bytesCabecera;
        longitud += bytesCabecera;
        return &raw[inicio];
    }

    // Avanza el puntero 'inicio' para desencapsular una cabecera en RX sin usar memcpy
    const uint8_t* consumirCabecera(size_t bytesCabecera) {
        if (longitud < bytesCabecera) return nullptr;
        const uint8_t* ptrCabecera = &raw[inicio];
        inicio += bytesCabecera;
        longitud -= bytesCabecera;
        return ptrCabecera;
    }
};

// Polinomio CRC-16-CCITT (0x1021) acelerado mediante la tabla en ROM del ESP32 (crc16_be).
// Nota: crc16_be() en la ROM de Espressif aplica NOT (~) al inicio y al final; al invertir
// la semilla y el retorno mantenemos compatibilidad bit a bit con calcularCRC16() de Node.js.
inline uint16_t calcularCRC16(const uint8_t* datos, size_t longitud, uint16_t crcInicial = 0xFFFF) {
    if (!datos || longitud == 0) return crcInicial;
    return static_cast<uint16_t>(
        ~crc16_be(static_cast<uint16_t>(~crcInicial), datos, static_cast<uint32_t>(longitud))
    );
}