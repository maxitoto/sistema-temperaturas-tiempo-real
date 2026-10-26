// Archivo: ./src/communication/BufferRed.h
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace LimitesPila {
    static const int    MAX_NODOS_RED         = 4;   // 1 USB (0) + 3 Clientes UDP Wi-Fi (1..3)
    static const size_t TAMANO_MAX_CHUNK      = 64;  // Fragmento físico máximo por ráfaga
    static const size_t MAX_TRAMA_ENLACE      = 512; // MTU de la Capa de Enlace

    static const size_t TAMANO_CABECERA_RED   = 2;   // [origen:4|destino:4][ttl:8]
    static const size_t TAMANO_CABECERA_TRANS = 10;  // [proto][srcPort][dstPort][flags][seq:2][ack:2][crc16:2]
    static const size_t TAMANO_SOBRE_SESION   = 8;   // [ver][tipo][sesId:2][reqId:2][len:2]

    static const size_t OFFSET_TRANSPORTE     = TAMANO_CABECERA_RED;
    static const size_t OFFSET_SESION         = OFFSET_TRANSPORTE + TAMANO_CABECERA_TRANS;
    static const size_t OFFSET_PAYLOAD_APP    = OFFSET_SESION + TAMANO_SOBRE_SESION; // 20 bytes
    static const size_t MAX_PAYLOAD_APP       = MAX_TRAMA_ENLACE - OFFSET_PAYLOAD_APP; // 492 bytes
}

// Estructura Zero-Copy equivalente a sk_buff (Linux) / pbuf (lwIP)
struct BufferPaquete {
    int idNodo = -1;
    uint8_t raw[LimitesPila::MAX_TRAMA_ENLACE];
    size_t inicio = LimitesPila::OFFSET_PAYLOAD_APP;
    size_t longitud = 0;

    // Prepara el buffer para que Capa 6 escriba el payload directamente en el offset 20
    void prepararParaTx(int destino = -1) {
        idNodo = destino;
        inicio = LimitesPila::OFFSET_PAYLOAD_APP;
        longitud = 0;
    }

    // Prepara el buffer para que Capa 2 escriba desde el byte 0 en recepción
    void prepararParaRx(int origen = -1) {
        idNodo = origen;
        inicio = 0;
        longitud = 0;
    }

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

// Polinomio CRC-16-CCITT (0x1021) compartido por Capa 2 (FCS) y Capa 4 (Checksum de Transporte)
inline uint16_t calcularCRC16(const uint8_t* datos, size_t longitud, uint16_t crcInicial = 0xFFFF) {
    uint16_t crc = crcInicial;
    for (size_t i = 0; i < longitud; i++) {
        crc ^= (uint16_t)datos[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc <<= 1;
        }
    }
    return crc;
}