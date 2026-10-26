#pragma once
#include <stdint.h>

struct EstadoSistema {
    float ultimoResultadoCalculado = 0.0f;
    uint32_t totalLlamadasRecibidas = 0;

    uint32_t ultimoUnixTimestamp = 0;
    char zonaHoraria[24] = "SIN_SYNC";
    bool relojSincronizado = false;

    bool reinicioPendiente = false;
    unsigned long instanteSolicitudReinicioMs = 0;
    
};

extern EstadoSistema gEstadoSistema;