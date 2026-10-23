// Archivo: ./src/scheduler/Planificador.h
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

typedef void (*FuncionTarea)(void);

struct DescriptorTarea {
    const char* nombre = nullptr;
    FuncionTarea fn = nullptr;
    unsigned long intervaloMs = 0;
    unsigned long ultimaEjecucionMs = 0;
    bool activa = false;

    DescriptorTarea() = default;
    DescriptorTarea(const char* n, FuncionTarea f, unsigned long iv,
                    unsigned long ult, bool act)
        : nombre(n), fn(f), intervaloMs(iv), ultimaEjecucionMs(ult), activa(act) {}
};

class Planificador {
  public:
    static const size_t MAX_TAREAS = 8;

  private:
    DescriptorTarea tareas[MAX_TAREAS];
    size_t totalTareas = 0;

  public:
    bool registrarTarea(const char* nombre, FuncionTarea fn, unsigned long intervaloMs) {
        if (!fn || totalTareas >= MAX_TAREAS) return false;
        tareas[totalTareas++] = { nombre, fn, intervaloMs, millis(), true };
        return true;
    }

    void ejecutarCiclo() {
        unsigned long ahora = millis();
        for (size_t i = 0; i < totalTareas; i++) {
            if (!tareas[i].activa) continue;
            if (tareas[i].intervaloMs == 0 ||
                (ahora - tareas[i].ultimaEjecucionMs >= tareas[i].intervaloMs)) {
                tareas[i].ultimaEjecucionMs = ahora;
                tareas[i].fn();          
                yield();               
            }
        }
    }
};