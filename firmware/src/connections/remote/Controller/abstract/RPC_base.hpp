#ifndef RPC_BASE_H
#define RPC_BASE_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>


static constexpr uint16_t BANDERA_ERROR_RPC = 0x8000;

enum class CodigoErrorRPC : uint8_t {
    OK                          = 0,
    NO_AUTENTICADO              = 1,
    SESION_BLOQUEADA            = 2,
    ROL_INSUFICIENTE            = 3,
    ARGUMENTOS_INVALIDOS        = 4,
    PROCEDIMIENTO_NO_ENCONTRADO = 5,
    SERVIDOR_OCUPADO            = 6
};

struct ErrorRpcDTO {
    uint8_t codigoError;
    uint8_t rolActual;
    uint8_t rolRequerido;
    uint8_t reservado;
    char mensaje[128];
};

// Slot 0 = UART-S | Slots 1..8 = Clientes WiFi
using MascaraClientes = uint16_t;

enum class SemanticaEntrega : uint8_t {
    TAL_VEZ = 0,
    AL_MENOS_UNA_VEZ = 1,
    EXACTAMENTE_UNA_VEZ = 2
};

// Alineado con rol_usuario_enum de PhysicalModelsDefinition.hpp "te dije que no te guies por el modelo, es solo un ejemplo"
// devolví todo a la normalidad
enum class RolUsuario : uint8_t {
    NINGUNO = 0,
    NO_ADMIN = 1,
    ADMIN = 2
};

enum class CanalSalida : uint8_t {
    RESPUESTA_RPC_CONFIABLE = 0, // UART-S y/o WiFi TCP+TLS
    STREAMING_TELEMETRIA = 1     // UART-S y/o WiFi UDP Plano
};

struct SobreMensajeRPC {
    uint8_t slotClienteOrigen;
    uint16_t idProcedimiento;
    uint16_t idTicket;
    uint32_t hashPeticion;
    RolUsuario rolAutenticado;
    const uint8_t* payloadEntrada;
    size_t longitudEntrada;
};

struct TicketEnVuelo {
    uint16_t idTicket;
    uint16_t idProcedimiento;
    uint32_t hashPeticion;
    MascaraClientes clientesEsperando;
    uint32_t instanteCreacionMs;
    bool enUso;
};

class RPCBase {
public:
    static constexpr size_t CAPACIDAD_BUFFER_COMPARTIDO = 1024;
    static constexpr uint8_t MAXIMO_TICKETS_EN_VUELO = 12;
    static constexpr uint32_t TIMEOUT_TICKET_MS = 15000;

protected:
    inline static uint8_t bufferSalidaCompartido[CAPACIDAD_BUFFER_COMPARTIDO] = {0};
    inline static size_t longitudSalidaCompartida = 0;
    inline static TicketEnVuelo tablaTickets[MAXIMO_TICKETS_EN_VUELO] = {};

public:
    // ¿Cómo lo hace?: Calcula en O(N) un hash FNV-1a de 32 bits combinando idProcedimiento y los bytes crudos de los argumentos para detectar peticiones idénticas sin copiar memoria.
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

    // ¿Cómo lo hace?: Busca en tablaTickets si existe un hashPeticion idéntico en curso; si existe, enciende el bit (1 << slotCliente) en clientesEsperando y devuelve true para ahorrar la doble consulta.
    static bool intentarAcoplarAPeticionEnVuelo(uint32_t hashPeticion, uint8_t slotCliente) {
        if (slotCliente > 8) {
            return false;
        }
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

    // ¿Cómo lo hace?: Reserva la primera ranura libre en tablaTickets, registra el hashPeticion y activa el bit inicial del slotCliente solicitante.
    static int8_t abrirTicketEjecucion(uint16_t idTicket, uint16_t idProc, uint32_t hashPeticion, uint8_t slotCliente) {
        if (slotCliente > 8) {
            return -1;
        }
        const uint32_t ahora = millis();
        for (uint8_t i = 0; i < MAXIMO_TICKETS_EN_VUELO; ++i) {
            if (!tablaTickets[i].enUso || (ahora - tablaTickets[i].instanteCreacionMs > TIMEOUT_TICKET_MS)) {
                tablaTickets[i].idTicket = idTicket;
                tablaTickets[i].idProcedimiento = idProc;
                tablaTickets[i].hashPeticion = hashPeticion;
                tablaTickets[i].clientesEsperando = static_cast<MascaraClientes>(1u << slotCliente);
                tablaTickets[i].instanteCreacionMs = ahora;
                tablaTickets[i].enUso = true;
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    // ¿Cómo lo hace?: Libera la ranura del ticket finalizado y retorna la máscara con todos los clientes que se acoplaron durante su ejecución.
    static MascaraClientes cerrarTicketYObtenerDestinatarios(int8_t indiceTicket) {
        if (indiceTicket < 0 || indiceTicket >= MAXIMO_TICKETS_EN_VUELO || !tablaTickets[indiceTicket].enUso) {
            return 0;
        }
        MascaraClientes destinatarios = tablaTickets[indiceTicket].clientesEsperando;
        tablaTickets[indiceTicket].enUso = false;
        tablaTickets[indiceTicket].clientesEsperando = 0;
        return destinatarios;
    }

    // ¿Cómo lo hace?: Apaga el bit (1 << slotCliente) en todos los tickets activos y cierra aquellos que queden sin ningún destinatario esperando.
    static void desvincularClienteDeTickets(uint8_t slotCliente) {
        if (slotCliente > 8) {
            return;
        }
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

    // ¿Cómo lo hace?: Expone el puntero constante al bufferSalidaCompartido para que L1 lo transmita a los medios sin hacer copias.
    static const uint8_t* obtenerBufferSalidaCompartido() {
        return bufferSalidaCompartido;
    }

    // ¿Cómo lo hace?: Devuelve la cantidad de bytes válidos escritos en la última serialización sobre bufferSalidaCompartido.
    static size_t obtenerLongitudSalidaCompartida() {
        return longitudSalidaCompartida;
    }
};

#endif