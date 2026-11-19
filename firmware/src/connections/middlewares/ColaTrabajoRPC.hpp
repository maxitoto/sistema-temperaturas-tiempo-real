#ifndef COLA_TRABAJO_RPC_HPP
#define COLA_TRABAJO_RPC_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../remote/Controller/abstract/RPC_base.hpp"

struct PeticionEncoladaRPC {
    uint8_t  slotClienteOrigen;
    uint16_t generacionSlot;
    uint16_t idProcedimiento;
    uint16_t idTicket;
    uint32_t hashPeticion;
    size_t   longitudEntrada;
    uint8_t  payloadEntrada[480]; // Cubre ConsultaSqlTerminalDTO (384 B) y cualquier entrada válida
};

struct RespuestaEncoladaRPC {
    MascaraClientes destinatarios;
    CanalSalida     canal;
    size_t          longitud;
    uint8_t         buffer[RPCBase::CAPACIDAD_BUFFER_COMPARTIDO];
};

class ColaTrabajoRPC {
public:
    static constexpr uint8_t CAPACIDAD_PETICIONES = 4; // Tope estricto de trabajo pendiente
    static constexpr uint8_t CAPACIDAD_RESPUESTAS = 2; // Doble buffer de salida hacia TaskComms

private:
    PeticionEncoladaRPC  poolPeticiones[CAPACIDAD_PETICIONES]{};
    RespuestaEncoladaRPC poolRespuestas[CAPACIDAD_RESPUESTAS]{};

    QueueHandle_t colaReqLibres{nullptr};
    QueueHandle_t colaReqPendientes{nullptr};
    QueueHandle_t colaResLibres{nullptr};
    QueueHandle_t colaResListas{nullptr};

    // Señales atómicas entre Core 0 y Core 1 (cero locks)
    std::atomic<uint16_t> mascaraLimpiezaSesion{0}; // Core 0 -> Core 1
    std::atomic<uint16_t> mascaraCierreFisico{0};   // Core 1 -> Core 0
    std::atomic<uint16_t> generacionPorSlot[9]{};
    std::atomic<uint32_t> totalDescartadasPorLlena{0};

public:
    bool inicializar() {
        if (colaReqLibres != nullptr) return true;

        colaReqLibres     = xQueueCreate(CAPACIDAD_PETICIONES, sizeof(uint8_t));
        colaReqPendientes = xQueueCreate(CAPACIDAD_PETICIONES, sizeof(uint8_t));
        colaResLibres     = xQueueCreate(CAPACIDAD_RESPUESTAS, sizeof(uint8_t));
        colaResListas     = xQueueCreate(CAPACIDAD_RESPUESTAS, sizeof(uint8_t));

        if (!colaReqLibres || !colaReqPendientes || !colaResLibres || !colaResListas) {
            return false;
        }

        for (uint8_t i = 0; i < CAPACIDAD_PETICIONES; ++i) {
            xQueueSend(colaReqLibres, &i, 0);
        }
        for (uint8_t i = 0; i < CAPACIDAD_RESPUESTAS; ++i) {
            xQueueSend(colaResLibres, &i, 0);
        }
        return true;
    }

    // --- SEÑALES DE CONTROL ENTRE NÚCLEOS ---

    void avanzarGeneracionSlot(uint8_t slot) {
        if (slot <= 8) {
            generacionPorSlot[slot].fetch_add(1, std::memory_order_acq_rel);
        }
    }

    uint16_t obtenerGeneracionSlot(uint8_t slot) const {
        return (slot <= 8) ? generacionPorSlot[slot].load(std::memory_order_acquire) : 0;
    }

    // Core 0 avisa a Core 1 que limpie tickets/streams/sesión de ese slot
    void solicitarLimpiezaSesion(uint8_t slot) {
        if (slot <= 8) {
            avanzarGeneracionSlot(slot);
            mascaraLimpiezaSesion.fetch_or(static_cast<uint16_t>(1u << slot), std::memory_order_acq_rel);
        }
    }

    uint16_t extraerLimpiezasSesionPendientes() {
        return mascaraLimpiezaSesion.exchange(0, std::memory_order_acq_rel);
    }

    // Core 1 avisa a Core 0 que cierre el socket/UART de ese slot (ej. sesión expirada por ocio)
    void solicitarCierreFisico(uint8_t slot) {
        if (slot <= 8) {
            mascaraCierreFisico.fetch_or(static_cast<uint16_t>(1u << slot), std::memory_order_acq_rel);
        }
    }

    uint16_t extraerCierresFisicosPendientes() {
        return mascaraCierreFisico.exchange(0, std::memory_order_acq_rel);
    }

    // --- LADO PRODUCTOR (Core 0 · TaskComms): NUNCA bloquea ---
    // Política tail-drop: conserva pendientes actuales y rechaza lo nuevo si está llena.
    bool intentarEncolarPeticion(const SobreMensajeRPC& sobre) {
        if (!colaReqLibres || !colaReqPendientes ||
            sobre.slotClienteOrigen > 8 ||
            sobre.longitudEntrada > sizeof(PeticionEncoladaRPC::payloadEntrada)) {
            return false;
        }

        uint8_t idx = 0;
        if (xQueueReceive(colaReqLibres, &idx, 0) != pdTRUE) {
            totalDescartadasPorLlena.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        PeticionEncoladaRPC& dst = poolPeticiones[idx];
        dst.slotClienteOrigen = sobre.slotClienteOrigen;
        dst.generacionSlot    = obtenerGeneracionSlot(sobre.slotClienteOrigen);
        dst.idProcedimiento   = sobre.idProcedimiento;
        dst.idTicket          = sobre.idTicket;
        dst.hashPeticion      = sobre.hashPeticion;
        dst.longitudEntrada   = sobre.longitudEntrada;
        if (sobre.longitudEntrada > 0 && sobre.payloadEntrada != nullptr) {
            memcpy(dst.payloadEntrada, sobre.payloadEntrada, sobre.longitudEntrada);
        }

        if (xQueueSend(colaReqPendientes, &idx, 0) != pdTRUE) {
            xQueueSend(colaReqLibres, &idx, 0);
            totalDescartadasPorLlena.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    bool tomarRespuestaLista(uint8_t& idxOut, const RespuestaEncoladaRPC*& ptrOut) {
        if (!colaResListas || xQueueReceive(colaResListas, &idxOut, 0) != pdTRUE) {
            ptrOut = nullptr;
            return false;
        }
        ptrOut = &poolRespuestas[idxOut];
        return true;
    }

    void liberarRespuesta(uint8_t idx) {
        if (colaResLibres && idx < CAPACIDAD_RESPUESTAS) {
            xQueueSend(colaResLibres, &idx, 0);
        }
    }

    // --- LADO CONSUMIDOR (Core 1 · TaskAlertasDB) ---
    bool esperarSiguientePeticion(uint8_t& idxOut, PeticionEncoladaRPC*& ptrOut, TickType_t esperaTicks) {
        if (!colaReqPendientes || xQueueReceive(colaReqPendientes, &idxOut, esperaTicks) != pdTRUE) {
            ptrOut = nullptr;
            return false;
        }
        ptrOut = &poolPeticiones[idxOut];
        return true;
    }

    void liberarPeticion(uint8_t idx) {
        if (colaReqLibres && idx < CAPACIDAD_PETICIONES) {
            xQueueSend(colaReqLibres, &idx, 0);
        }
    }

    bool publicarRespuesta(
        const uint8_t* datos,
        size_t longitud,
        MascaraClientes destinatarios,
        CanalSalida canal,
        TickType_t esperaTicks = pdMS_TO_TICKS(200)
    ) {
        if (!colaResLibres || !colaResListas ||
            datos == nullptr || longitud == 0 ||
            longitud > RPCBase::CAPACIDAD_BUFFER_COMPARTIDO || destinatarios == 0) {
            return false;
        }

        uint8_t idx = 0;
        if (xQueueReceive(colaResLibres, &idx, esperaTicks) != pdTRUE) {
            return false;
        }

        RespuestaEncoladaRPC& dst = poolRespuestas[idx];
        dst.destinatarios = destinatarios;
        dst.canal         = canal;
        dst.longitud      = longitud;
        memcpy(dst.buffer, datos, longitud);

        if (xQueueSend(colaResListas, &idx, 0) != pdTRUE) {
            xQueueSend(colaResLibres, &idx, 0);
            return false;
        }
        return true;
    }

    uint8_t cantidadPendientes() const {
        return colaReqPendientes ? static_cast<uint8_t>(uxQueueMessagesWaiting(colaReqPendientes)) : 0;
    }

    uint32_t obtenerTotalDescartadas() const {
        return totalDescartadasPorLlena.load(std::memory_order_relaxed);
    }
};

#endif