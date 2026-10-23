// Archivo: ./src/communication/layers/CapaRed.h
#pragma once
#include <Arduino.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaEnlace.h"

class CapaRed {
  public:
    static const uint8_t NODO_BROADCAST = 0x0F;
    static const uint8_t DEFAULT_TTL    = 8;

  private:
    CapaEnlace enlaces[LimitesPila::MAX_NODOS_RED];
    int cantidadNodos = 0;
    int ultimoNodoConsultado = 0;

  public:
    CapaRed() {}

    void vincularMedios(CapaFisica* capaFisica) {
        if (!capaFisica) return;
        int disponibles = capaFisica->obtenerTotalMedios();
        cantidadNodos = (disponibles > LimitesPila::MAX_NODOS_RED)
                          ? LimitesPila::MAX_NODOS_RED
                          : disponibles;

        for (int i = 0; i < cantidadNodos; i++) {
            enlaces[i].configurarEnlace(capaFisica, i);
        }
    }

    int obtenerCantidadNodos() const {
        return cantidadNodos;
    }

    // ==========================================================
    // CAMINO DE SUBIDA (RX): Valida TTL y consume cabecera de 2B
    // ==========================================================
    bool subirPaquete(BufferPaquete& pktSalida) {
        if (cantidadNodos <= 0) return false;

        for (int i = 0; i < cantidadNodos; i++) {
            int idx = (ultimoNodoConsultado + i) % cantidadNodos;
            if (enlaces[idx].subirTrama(pktSalida)) {
                ultimoNodoConsultado = (idx + 1) % cantidadNodos;

                const uint8_t* hdr = pktSalida.consumirCabecera(LimitesPila::TAMANO_CABECERA_RED);
                if (!hdr) return false;

                uint8_t ttl = hdr[1];
                if (ttl == 0) return false; // Paquete expirado descartado

                pktSalida.idNodo = idx;
                return true;
            }
        }
        return false;
    }

    // ==========================================================
    // CAMINO DE BAJADA (TX): Empuja cabecera de 2B sin memcpy
    // ==========================================================
    bool bajarPaqueteUnicast(int idDestino, BufferPaquete& pkt) {
        if (idDestino < 0 || idDestino >= cantidadNodos) return false;

        uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
        if (!hdr) return false;

        uint8_t origenLogico = (uint8_t)(idDestino & 0x0F);
        uint8_t destinoLogico = (uint8_t)(idDestino & 0x0F);
        hdr[0] = (origenLogico << 4) | destinoLogico;
        hdr[1] = DEFAULT_TTL;

        return enlaces[idDestino].bajarTrama(pkt);
    }

    void bajarPaqueteBroadcast(BufferPaquete& pkt) {
        uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
        if (!hdr) return;

        hdr[0] = (0x00 << 4) | NODO_BROADCAST;
        hdr[1] = DEFAULT_TTL;

        for (int i = 0; i < cantidadNodos; i++) {
            enlaces[i].bajarTrama(pkt);
        }
    }
};