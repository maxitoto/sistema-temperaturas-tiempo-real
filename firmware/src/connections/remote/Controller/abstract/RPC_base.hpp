#ifndef RPC_BASE_H
#define RPC_BASE_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdexcept>
#include "../marshalling/encode.hpp"

using MascaraClientes = uint16_t;

enum class SemanticaEntrega : uint8_t {
    TAL_VEZ             = 0,
    AL_MENOS_UNA_VEZ    = 1,
    EXACTAMENTE_UNA_VEZ = 2
};

enum class RolUsuario : uint8_t {
    NINGUNO  = 0,
    NO_ADMIN = 1,
    ADMIN    = 2
};

enum class CanalSalida : uint8_t {
    RESPUESTA_RPC_CONFIABLE = 0,
    STREAMING_TELEMETRIA    = 1
};

struct SobreMensajeRPC {
    uint8_t        slotClienteOrigen;
    uint16_t       idProcedimiento;
    uint16_t       idTicket;
    uint32_t       hashPeticion;
    RolUsuario     rolAutenticado;
    const uint8_t* payloadEntrada;
    size_t         longitudEntrada;
};

struct TicketEnVuelo {
    uint16_t        idTicket;
    uint16_t        idProcedimiento;
    uint32_t        hashPeticion;
    MascaraClientes clientesEsperando;
    uint32_t        instanteCreacionMs;
    bool            enUso;
};

constexpr uint16_t hashNombreProcedimiento(const char* nombre) {
    uint32_t h = 2166136261u;
    if (nombre) {
        while (*nombre) {
            h ^= static_cast<uint8_t>(*nombre++);
            h *= 16777619u;
        }
    }
    uint16_t id15 = static_cast<uint16_t>((h ^ (h >> 15)) & 0x7FFFu);
    return (id15 < 0x0300u) ? static_cast<uint16_t>(id15 + 0x0300u) : id15;
}

class RPCBase {
public:
    static constexpr size_t   CAPACIDAD_BUFFER_COMPARTIDO = 1024;
    static constexpr uint8_t  MAXIMO_TICKETS_EN_VUELO     = 12;
    static constexpr uint32_t TIMEOUT_TICKET_MS           = 15000;

protected:
    inline static uint8_t        bufferSalidaCompartido[CAPACIDAD_BUFFER_COMPARTIDO] = {0};
    inline static size_t         longitudSalidaCompartida = 0;
    inline static TicketEnVuelo  tablaTickets[MAXIMO_TICKETS_EN_VUELO] = {};
    inline static SobreMensajeRPC sobreEnEjecucion{};

public:
    static const SobreMensajeRPC& contextoActual() { return sobreEnEjecucion; }

    // Ejecuta cualquier bloque de código capturando excepciones nativas de C++ y serializando e.what() al cliente
    template <typename Callable>
    static bool ejecutarConCapturaNativa(uint16_t idTicket, uint16_t idProcedimiento, Callable&& fn) {
        try {
            fn();
            return true;
        } catch (const std::exception& e) {
            longitudSalidaCompartida = CodificadorRPC::serializarExcepcionCruda(
                idTicket, idProcedimiento, e.what(),
                bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
            );
            return false;
        } catch (const char* msgTexto) {
            longitudSalidaCompartida = CodificadorRPC::serializarExcepcionCruda(
                idTicket, idProcedimiento, msgTexto,
                bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
            );
            return false;
        } catch (...) {
            longitudSalidaCompartida = CodificadorRPC::serializarExcepcionCruda(
                idTicket, idProcedimiento, "Unhandled runtime exception on server",
                bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
            );
            return false;
        }
    }

    static uint32_t calcularHashPeticion(uint16_t idProcedimiento, const uint8_t* payload, size_t longitud) {
        uint32_t hash = 2166136261u;
        hash ^= static_cast<uint8_t>(idProcedimiento >> 8);
        hash *= 16777619u;
        hash ^= static_cast<uint8_t>(idProcedimiento & 0xFF);
        hash *= 16777619u;

        if (payload != nullptr) {
            for (size_t i = 0; i < longitud; ++i) {
                hash ^= payload[i];
                hash *= 16777619u;
            }
        }
        return hash;
    }

    static bool intentarAcoplarAPeticionEnVuelo(uint32_t hashPeticion, uint8_t slotCliente) {
        if (slotCliente > 8) return false;
        const uint32_t ahora = millis();
        for (uint8_t i = 0; i < MAXIMO_TICKETS_EN_VUELO; ++i) {
            if (tablaTickets[i].enUso) {
                if (ahora - tablaTickets[i].instanteCreacionMs > TIMEOUT_TICKET_MS) {
                    tablaTickets[i].enUso = false;
                    continue;
                }
                if (tablaTickets[i].hashPeticion == hashPeticion) {
                    tablaTickets[i].clientesEsperando |= static_cast<MascaraClientes>(1u << slotCliente);
                    return true;
                }
            }
        }
        return false;
    }

    static int8_t abrirTicketEjecucion(uint16_t idTicket, uint16_t idProc, uint32_t hashPeticion, uint8_t slotCliente) {
        if (slotCliente > 8) return -1;
        const uint32_t ahora = millis();
        for (uint8_t i = 0; i < MAXIMO_TICKETS_EN_VUELO; ++i) {
            if (!tablaTickets[i].enUso || (ahora - tablaTickets[i].instanteCreacionMs > TIMEOUT_TICKET_MS)) {
                tablaTickets[i].idTicket           = idTicket;
                tablaTickets[i].idProcedimiento    = idProc;
                tablaTickets[i].hashPeticion       = hashPeticion;
                tablaTickets[i].clientesEsperando  = static_cast<MascaraClientes>(1u << slotCliente);
                tablaTickets[i].instanteCreacionMs = ahora;
                tablaTickets[i].enUso              = true;
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    static MascaraClientes cerrarTicketYObtenerDestinatarios(int8_t indiceTicket) {
        if (indiceTicket < 0 || indiceTicket >= MAXIMO_TICKETS_EN_VUELO || !tablaTickets[indiceTicket].enUso) {
            return 0;
        }
        MascaraClientes destinatarios = tablaTickets[indiceTicket].clientesEsperando;
        tablaTickets[indiceTicket].enUso             = false;
        tablaTickets[indiceTicket].clientesEsperando = 0;
        return destinatarios;
    }

    static void desvincularClienteDeTickets(uint8_t slotCliente) {
        if (slotCliente > 8) return;
        const MascaraClientes mascaraInversa = static_cast<MascaraClientes>(~(1u << slotCliente));
        for (uint8_t i = 0; i < MAXIMO_TICKETS_EN_VUELO; ++i) {
            if (tablaTickets[i].enUso) {
                tablaTickets[i].clientesEsperando &= mascaraInversa;
                if (tablaTickets[i].clientesEsperando == 0) {
                    tablaTickets[i].enUso = false;
                }
            }
        }
    }

    static const uint8_t* obtenerBufferSalidaCompartido() { return bufferSalidaCompartido; }
    static size_t obtenerLongitudSalidaCompartida()       { return longitudSalidaCompartida; }
};

#endif