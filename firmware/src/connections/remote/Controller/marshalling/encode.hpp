#ifndef ENCODE_H
#define ENCODE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

class CodificadorRPC {
public:
    static constexpr size_t TAMANO_CABECERA_MENSAJE_RPC = 8;

    // ¿Cómo lo hace?: Escribe los 4 bytes de longitud útil (Big-Endian), los 2 bytes de idTicket, los 2 bytes de idProcedimiento y copia la estructura SalidaDTO de forma plana sobre bufferDestino.
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

        const uint32_t longitudPayload = static_cast<uint32_t>(4 + tamanoDto); // idTicket(2) + idProc(2) + DTO
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

    // ¿Cómo lo hace?: Serializa únicamente la cabecera de confirmación (8 bytes sin payload adicional) para procedimientos void sin DTO de salida.
    static size_t serializarAcuseSinPayload(
        uint16_t idTicket,
        uint16_t idProcedimiento,
        uint8_t* bufferDestino,
        size_t capacidadMaxima
    ) {
        if (bufferDestino == nullptr || capacidadMaxima < TAMANO_CABECERA_MENSAJE_RPC) {
            return 0;
        }

        const uint32_t longitudPayload = 4; // Solo idTicket(2) + idProc(2)
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

    static size_t serializarErrorRPC(
        uint16_t idTicket,
        uint16_t idProcedimiento,
        CodigoErrorRPC codigo,
        RolUsuario rolActual,
        RolUsuario rolRequerido,
        const char* mensajeExplicito,
        uint8_t* bufferDestino,
        size_t capacidadMaxima
    ) {
        ErrorRpcDTO err{};
        err.codigoError = static_cast<uint8_t>(codigo);
        err.rolActual = static_cast<uint8_t>(rolActual);
        err.rolRequerido = static_cast<uint8_t>(rolRequerido);
        err.reservado = 0;
        if (mensajeExplicito != nullptr) {
            strncpy(err.mensaje, mensajeExplicito, sizeof(err.mensaje) - 1);
            err.mensaje[sizeof(err.mensaje) - 1] = '\0';
        }
        return serializarEnBufferCompartido(
            idTicket,
            static_cast<uint16_t>(idProcedimiento | BANDERA_ERROR_RPC),
            err,
            bufferDestino,
            capacidadMaxima
        );
    }
};

#endif