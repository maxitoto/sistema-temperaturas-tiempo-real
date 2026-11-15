#ifndef ENCODE_H
#define ENCODE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static constexpr uint16_t BANDERA_ERROR_RPC = 0x8000;

class CodificadorRPC {
public:
    static constexpr size_t TAMANO_CABECERA_MENSAJE_RPC = 8;

    template <typename SalidaDTO>
    static size_t serializarEnBufferCompartido(
        uint16_t idTicket,
        uint16_t idProcedimiento,
        const SalidaDTO& dto,
        uint8_t* bufferDestino,
        size_t capacidadMaxima
    ) {
        const size_t tamanoDto = sizeof(SalidaDTO);
        const size_t totalRequerido = TAMANO_CABECERA_MENSAJE_RPC + tamanoDto;

        if (bufferDestino == nullptr || capacidadMaxima < totalRequerido) {
            return 0;
        }

        const uint32_t longitudPayload = static_cast<uint32_t>(4 + tamanoDto);
        bufferDestino[0] = static_cast<uint8_t>((longitudPayload >> 24) & 0xFF);
        bufferDestino[1] = static_cast<uint8_t>((longitudPayload >> 16) & 0xFF);
        bufferDestino[2] = static_cast<uint8_t>((longitudPayload >> 8) & 0xFF);
        bufferDestino[3] = static_cast<uint8_t>(longitudPayload & 0xFF);

        bufferDestino[4] = static_cast<uint8_t>((idTicket >> 8) & 0xFF);
        bufferDestino[5] = static_cast<uint8_t>(idTicket & 0xFF);

        bufferDestino[6] = static_cast<uint8_t>((idProcedimiento >> 8) & 0xFF);
        bufferDestino[7] = static_cast<uint8_t>(idProcedimiento & 0xFF);

        memcpy(bufferDestino + TAMANO_CABECERA_MENSAJE_RPC, &dto, tamanoDto);
        return totalRequerido;
    }

    static size_t serializarAcuseSinPayload(
        uint16_t idTicket,
        uint16_t idProcedimiento,
        uint8_t* bufferDestino,
        size_t capacidadMaxima
    ) {
        if (bufferDestino == nullptr || capacidadMaxima < TAMANO_CABECERA_MENSAJE_RPC) {
            return 0;
        }

        const uint32_t longitudPayload = 4;
        bufferDestino[0] = 0;
        bufferDestino[1] = 0;
        bufferDestino[2] = 0;
        bufferDestino[3] = static_cast<uint8_t>(longitudPayload);

        bufferDestino[4] = static_cast<uint8_t>((idTicket >> 8) & 0xFF);
        bufferDestino[5] = static_cast<uint8_t>(idTicket & 0xFF);

        bufferDestino[6] = static_cast<uint8_t>((idProcedimiento >> 8) & 0xFF);
        bufferDestino[7] = static_cast<uint8_t>(idProcedimiento & 0xFF);

        return TAMANO_CABECERA_MENSAJE_RPC;
    }

    static size_t serializarExcepcionCruda(
        uint16_t idTicket,
        uint16_t idProcedimiento,
        const char* textoExcepcion,
        uint8_t* bufferDestino,
        size_t capacidadMaxima
    ) {
        const char* msg = (textoExcepcion != nullptr) ? textoExcepcion : "RuntimeError";
        size_t lenTexto = strlen(msg);
        if (TAMANO_CABECERA_MENSAJE_RPC + lenTexto > capacidadMaxima) {
            lenTexto = (capacidadMaxima > TAMANO_CABECERA_MENSAJE_RPC)
                       ? (capacidadMaxima - TAMANO_CABECERA_MENSAJE_RPC)
                       : 0;
        }

        const size_t totalRequerido = TAMANO_CABECERA_MENSAJE_RPC + lenTexto;
        if (bufferDestino == nullptr || capacidadMaxima < totalRequerido) {
            return 0;
        }

        const uint16_t idConBanderaError = static_cast<uint16_t>(idProcedimiento | BANDERA_ERROR_RPC);
        const uint32_t longitudPayload   = static_cast<uint32_t>(4 + lenTexto);

        bufferDestino[0] = static_cast<uint8_t>((longitudPayload >> 24) & 0xFF);
        bufferDestino[1] = static_cast<uint8_t>((longitudPayload >> 16) & 0xFF);
        bufferDestino[2] = static_cast<uint8_t>((longitudPayload >> 8) & 0xFF);
        bufferDestino[3] = static_cast<uint8_t>(longitudPayload & 0xFF);

        bufferDestino[4] = static_cast<uint8_t>((idTicket >> 8) & 0xFF);
        bufferDestino[5] = static_cast<uint8_t>(idTicket & 0xFF);

        bufferDestino[6] = static_cast<uint8_t>((idConBanderaError >> 8) & 0xFF);
        bufferDestino[7] = static_cast<uint8_t>(idConBanderaError & 0xFF);

        if (lenTexto > 0) {
            memcpy(bufferDestino + TAMANO_CABECERA_MENSAJE_RPC, msg, lenTexto);
        }
        return totalRequerido;
    }
};

#endif