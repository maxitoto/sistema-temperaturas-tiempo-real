#ifndef DECODE_H
#define DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

class DecodificadorRPC {
public:
    static constexpr size_t TAMANO_MINIMO_CABECERA_ENTRADA = 4; // [idTicket:2 BE][idProc:2 BE]

    // ¿Cómo lo hace?: Extrae en Big-Endian el idTicket y el idProcedimiento de los primeros 4 bytes del paquete y apunta punteroArgumentos al byte 4 sin copiar memoria.
    static bool extraerCabeceraSinCopia(
        const uint8_t* bufferCrudo,
        size_t longitudCruda,
        uint16_t& idTicketOut,
        uint16_t& idProcedimientoOut,
        const uint8_t*& punteroArgumentosOut,
        size_t& longitudArgumentosOut
    ) {
        if (bufferCrudo == nullptr || longitudCruda < TAMANO_MINIMO_CABECERA_ENTRADA) {
            return false;
        }

        idTicketOut = (static_cast<uint16_t>(bufferCrudo[0]) << 8) | bufferCrudo[1];
        idProcedimientoOut = (static_cast<uint16_t>(bufferCrudo[2]) << 8) | bufferCrudo[3];

        punteroArgumentosOut = bufferCrudo + TAMANO_MINIMO_CABECERA_ENTRADA;
        longitudArgumentosOut = longitudCruda - TAMANO_MINIMO_CABECERA_ENTRADA;
        return true;
    }

    // ¿Cómo lo hace?: Comprueba que longitudArgumentos coincida exactamente con sizeof(EntradaDTO) y vuelca los bytes sobre dtoDestino en el stack.
    template <typename EntradaDTO>
    static bool deserializarDesdePayload(
        const uint8_t* payloadEntrada,
        size_t longitudArgumentos,
        EntradaDTO& dtoDestino
    ) {
        if (payloadEntrada == nullptr || longitudArgumentos < sizeof(EntradaDTO)) {
            return false;
        }
        memcpy(&dtoDestino, payloadEntrada, sizeof(EntradaDTO));
        return true;
    }
};

#endif