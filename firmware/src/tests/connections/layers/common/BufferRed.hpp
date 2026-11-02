#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <rom/crc.h>

namespace OSI {

    namespace LimitesPila {
        static constexpr int    MAX_NODOS_RED            = 8;
        static constexpr size_t TAMANO_MAX_CHUNK         = 64;
        static constexpr size_t MAX_TRAMA_ENLACE         = 512;

        static constexpr size_t TAMANO_CABECERA_RED      = 2;
        static constexpr size_t TAMANO_CABECERA_TRANS    = 10;
        static constexpr size_t TAMANO_RECORD_MARKER_TCP = 4;
        static constexpr size_t TAMANO_SOBRE_SESION      = 8;

        static constexpr size_t OFFSET_TRANSPORTE        = TAMANO_CABECERA_RED;
        static constexpr size_t OFFSET_SESION            = OFFSET_TRANSPORTE + TAMANO_CABECERA_TRANS;
        static constexpr size_t OFFSET_PAYLOAD_APP       = OFFSET_SESION + TAMANO_SOBRE_SESION;
        static constexpr size_t MAX_PAYLOAD_APP          = MAX_TRAMA_ENLACE - OFFSET_PAYLOAD_APP;
    }

    struct BufferPaquete {
        int     idNodo            = -1;
        bool    esDatagramaNativo = false;
        alignas(4) uint8_t raw[LimitesPila::MAX_TRAMA_ENLACE] = {0};
        size_t  inicio            = LimitesPila::OFFSET_PAYLOAD_APP;
        size_t  longitud          = 0;

        void prepararParaTx(int destino = -1) {
            idNodo            = destino;
            esDatagramaNativo = false;
            inicio            = LimitesPila::OFFSET_PAYLOAD_APP;
            longitud          = 0;
        }

        void prepararParaRx(int origen = -1) {
            idNodo            = origen;
            esDatagramaNativo = false;
            inicio            = 0;
            longitud          = 0;
        }

        uint8_t*       ptrActual()             { return &raw[inicio]; }
        const uint8_t* ptrActual()       const { return &raw[inicio]; }
        uint8_t*       ptrPayloadApp()         { return &raw[LimitesPila::OFFSET_PAYLOAD_APP]; }

        bool escribirPayload(const uint8_t* datos, size_t len, size_t offsetBase = LimitesPila::OFFSET_PAYLOAD_APP) {
            if (!datos || len > LimitesPila::MAX_TRAMA_ENLACE) return false;
            if (offsetBase + len > LimitesPila::MAX_TRAMA_ENLACE) {
                offsetBase = 0;
            }
            inicio   = offsetBase;
            longitud = len;
            memcpy(&raw[inicio], datos, len);
            return true;
        }

        uint8_t* empujarCabecera(size_t bytesCabecera) {
            if (inicio < bytesCabecera) return nullptr;
            inicio   -= bytesCabecera;
            longitud += bytesCabecera;
            return &raw[inicio];
        }

        const uint8_t* consumirCabecera(size_t bytesCabecera) {
            if (longitud < bytesCabecera) return nullptr;
            const uint8_t* ptr = &raw[inicio];
            inicio   += bytesCabecera;
            longitud -= bytesCabecera;
            return ptr;
        }
    };

    inline uint16_t calcularCRC16(const uint8_t* datos, size_t longitud, uint16_t crcInicial = 0xFFFF) {
        if (!datos || longitud == 0) return crcInicial;
        return static_cast<uint16_t>(
            ~crc16_be(static_cast<uint16_t>(~crcInicial), datos, static_cast<uint32_t>(longitud))
        );
    }

}