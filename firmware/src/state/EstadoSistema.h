// Archivo: ./src/state/EstadoSistema.h
#pragma once
#include <stdint.h>

struct EstadoSistema {
    // Datos actualizados por tareas locales y consultados por ServerExampleStub
    float ultimoResultadoCalculado = 0.0f;
    uint32_t totalLlamadasRecibidas = 0;

    // Datos actualizados cuando ClientExampleStub recibe respuesta del Backend
    uint32_t ultimoUnixTimestamp = 0;
    char zonaHoraria[24] = "SIN_SYNC";
    bool relojSincronizado = false;

    // Cola ligera de banderas (Outbox) para que otras tareas pidan RPCs a TareaComunicacion
    bool solicitarSyncReloj = false;
    const char* motivoSyncReloj = "BOOT";
};

// Instancia global única en segmento .bss (0 bytes en Heap)
extern EstadoSistema gEstadoSistema;