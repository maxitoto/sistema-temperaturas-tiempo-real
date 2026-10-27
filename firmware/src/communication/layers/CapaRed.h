// Archivo: ./src/communication/layers/CapaRed.h
#pragma once
#include <Arduino.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaEnlace.h"

class CapaRed {
  public:
    static constexpr uint8_t NODO_SERVIDOR  = 0x00;
    static constexpr uint8_t NODO_BROADCAST = 0x0F;
    static constexpr uint8_t DEFAULT_TTL    = 8;

  private:
    CapaFisica* fisicaRef = nullptr;
    CapaEnlace enlaces[LimitesPila::MAX_NODOS_RED];
    int cantidadNodos = 0;
    int ultimoNodoConsultado = 0;

  public:
    CapaRed() {}

    void vincularMedios(CapaFisica* capaFisica) {
        if (!capaFisica) return;
        fisicaRef = capaFisica;
        int disponibles = capaFisica->obtenerTotalMedios();
        cantidadNodos = (disponibles > LimitesPila::MAX_NODOS_RED)
                          ? LimitesPila::MAX_NODOS_RED
                          : disponibles;

        for (int i = 0; i < cantidadNodos; i++) {
            enlaces[i].configurarEnlace(capaFisica, i);
        }
    }

    int obtenerCantidadNodos() const { return cantidadNodos; }

    bool esNodoNativo(int idNodo) const {
        return fisicaRef ? fisicaRef->esMedioNativo(idNodo) : (idNodo >= 1);
    }

    bool estaNodoActivo(int idNodo) const {
        return fisicaRef ? fisicaRef->estaActivo(idNodo) : false;
    }

    bool subirPaquete(BufferPaquete& pktSalida) {
        if (cantidadNodos <= 0) return false;

        for (int i = 0; i < cantidadNodos; i++) {
            int idx = (ultimoNodoConsultado + i) % cantidadNodos;
            if (enlaces[idx].subirTrama(pktSalida)) {
                ultimoNodoConsultado = (idx + 1) % cantidadNodos;
                pktSalida.idNodo = idx;

                if (esNodoNativo(idx)) return true;

                const uint8_t* hdr = pktSalida.consumirCabecera(LimitesPila::TAMANO_CABECERA_RED);
                if (!hdr) return false;
                if (hdr[1] == 0) return false;
                return true;
            }
        }
        return false;
    }

    bool bajarPaqueteUnicast(int idDestino, BufferPaquete& pkt) {
        if (idDestino < 0 || idDestino >= cantidadNodos) return false;

        if (esNodoNativo(idDestino)) {
            return enlaces[idDestino].bajarTrama(pkt);
        }

        uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
        if (!hdr) return false;

        uint8_t destinoLogico = (uint8_t)(idDestino & 0x0F);
        hdr[0] = (NODO_SERVIDOR << 4) | destinoLogico;
        hdr[1] = DEFAULT_TTL;

        return enlaces[idDestino].bajarTrama(pkt);
    }

    void bajarPaqueteBroadcast(BufferPaquete& pkt) {
        for (int i = 0; i < cantidadNodos; i++) {
            BufferPaquete copia = pkt;
            bajarPaqueteUnicast(i, copia);
        }
    }
};