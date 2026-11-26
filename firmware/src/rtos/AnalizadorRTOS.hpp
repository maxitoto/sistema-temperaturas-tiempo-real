#pragma once
#include <Arduino.h>
#include <esp_timer.h>
#include <math.h>
#include <stdio.h>

namespace RTOS {

    using ProcedimientoMedible = void (*)();

    struct PerfilTareaAnalizada {
        const char* nombreTarea;
        const char* nombreProcA;
        ProcedimientoMedible procA;
        const char* nombreProcB;
        ProcedimientoMedible procB;
        uint32_t tiempoAsignadoDesarrolladorMs;
        uint32_t retencionRecursoCompartidoMs; // Cuánto tiempo retiene un mutex/núcleo (Z bloqueo)
        uint8_t prioridad;
        uint8_t nucleo;
        uint8_t idSemaforoPrimero;             // 0 = ninguno, 1..N = ID de semáforo que toma primero
        uint8_t idSemaforoSegundo;             // Para detectar espera circular (Deadlock A->B vs B->A)
    };

    struct DiagnosticoTareaResultado {
        float promedioProcAMs;
        float promedioProcBMs;
        float costoTotalMs;
        uint32_t tiempoYMinimoMs;
        uint32_t tiempoHRecomendadoMs;
        uint32_t tiempoZSobradoMs;
        bool tiempoImposible;
        bool produceStopRepentino;
        bool entraEnDeadlock;
        const char* tareaAfectadaPorStop;
        char resumenCortoOled[22];
        char reporteDetallado[280];
    };

    class AnalizadorPlanificacionRTOS {
    public:
        static constexpr uint8_t MUESTRAS_CALIBRACION = 3;

        // Mide cuánto tarda realmente un procedimiento en el ESP32 promediando N ejecuciones
        static float medirProcedimientoMs(ProcedimientoMedible fn) {
            if (!fn) return 0.0f;
            int64_t acumuladoUs = 0;
            for (uint8_t i = 0; i < MUESTRAS_CALIBRACION; ++i) {
                int64_t t0 = esp_timer_get_time();
                fn();
                acumuladoUs += (esp_timer_get_time() - t0);
            }
            return static_cast<float>(acumuladoUs) / (1000.0f * MUESTRAS_CALIBRACION);
        }

        // Analiza todas las tareas registradas antes de arrancarlas y predice colisiones, tiempos imposibles y deadlocks
        static bool inicializarFreeRTOS(
            const PerfilTareaAnalizada* tareas,
            size_t cantidadTareas,
            DiagnosticoTareaResultado* resultadosOut
        ) {
            if (!tareas || cantidadTareas == 0 || !resultadosOut) return false;
            bool sistemaSeguro = true;

            // Paso 1: Medición empírica de Proc A y Proc B + Cálculo de Y_min, H_rec y Z_sobrado
            for (size_t i = 0; i < cantidadTareas; ++i) {
                DiagnosticoTareaResultado& r = resultadosOut[i];
                memset(&r, 0, sizeof(DiagnosticoTareaResultado));

                r.promedioProcAMs = medirProcedimientoMs(tareas[i].procA);
                r.promedioProcBMs = medirProcedimientoMs(tareas[i].procB);
                r.costoTotalMs    = r.promedioProcAMs + r.promedioProcBMs;

                // Y_min: Costo físico + 15% de margen de cambio de contexto de FreeRTOS
                r.tiempoYMinimoMs      = static_cast<uint32_t>(ceilf(r.costoTotalMs * 1.15f));
                if (r.tiempoYMinimoMs < 1) r.tiempoYMinimoMs = 1;

                // H_rec: Cota de Liu & Layland (~65% de uso máximo para garantizar planificabilidad armónica)
                r.tiempoHRecomendadoMs = static_cast<uint32_t>(ceilf(r.costoTotalMs / 0.65f));
                if (r.tiempoHRecomendadoMs <= r.tiempoYMinimoMs) {
                    r.tiempoHRecomendadoMs = r.tiempoYMinimoMs + 2;
                }

                // Z_sobrado: Holgura 3x (< 33% de carga de CPU)
                r.tiempoZSobradoMs     = static_cast<uint32_t>(ceilf(r.costoTotalMs * 3.0f));

                // Verificación 1: ¿El tiempo asignado por el desarrollador es imposible?
                if (tareas[i].tiempoAsignadoDesarrolladorMs < r.tiempoYMinimoMs) {
                    r.tiempoImposible = true;
                    sistemaSeguro = false;
                }
            }

            // Paso 2: Predicción cruzada entre tareas (Deadlock de Coffman y Stop Repentino por interferencia)
            for (size_t i = 0; i < cantidadTareas; ++i) {
                DiagnosticoTareaResultado& r = resultadosOut[i];

                for (size_t j = 0; j < cantidadTareas; ++j) {
                    if (i == j) continue;

                    // Verificación 2: Deadlock por orden cruzado de semáforos (Tarea i: M1->M2 vs Tarea j: M2->M1)
                    // o auto-bloqueo (M1->M1)
                    if (tareas[i].idSemaforoPrimero != 0 &&
                        tareas[i].idSemaforoPrimero == tareas[i].idSemaforoSegundo) {
                        r.entraEnDeadlock = true;
                        sistemaSeguro = false;
                    }
                    if (tareas[i].idSemaforoPrimero != 0 && tareas[i].idSemaforoSegundo != 0 &&
                        tareas[i].idSemaforoPrimero == tareas[j].idSemaforoSegundo &&
                        tareas[i].idSemaforoSegundo == tareas[j].idSemaforoPrimero) {
                        r.entraEnDeadlock = true;
                        sistemaSeguro = false;
                    }

                    // Verificación 3: ¿La retención Z o el costo de la tarea i produce un STOP REPENTINO de la tarea j?
                    // Ocurre si i retiene un recurso compartido (o el mismo Core con mayor prioridad) más tiempo
                    // que todo el período disponible de la tarea j.
                    bool comparteRecurso = (tareas[i].idSemaforoPrimero != 0 &&
                                            tareas[i].idSemaforoPrimero == tareas[j].idSemaforoPrimero);
                    bool monopolizaCore  = (tareas[i].nucleo == tareas[j].nucleo &&
                                            tareas[i].prioridad >= tareas[j].prioridad);

                    uint32_t tiempoBloqueo = tareas[i].retencionRecursoCompartidoMs > static_cast<uint32_t>(r.costoTotalMs)
                                             ? tareas[i].retencionRecursoCompartidoMs
                                             : static_cast<uint32_t>(r.costoTotalMs);

                    if ((comparteRecurso || monopolizaCore) &&
                        tiempoBloqueo >= tareas[j].tiempoAsignadoDesarrolladorMs) {
                        r.produceStopRepentino = true;
                        r.tareaAfectadaPorStop = tareas[j].nombreTarea;
                        sistemaSeguro = false;
                    }
                }

                // Formateo del diagnóstico humano para el desarrollador
                if (r.entraEnDeadlock) {
                    snprintf(r.resumenCortoOled, sizeof(r.resumenCortoOled), "!DEADLOCK EN %.8s!",
                             tareas[i].nombreTarea ? tareas[i].nombreTarea : "TASK");
                } else if (r.tiempoImposible) {
                    snprintf(r.resumenCortoOled, sizeof(r.resumenCortoOled), "!T<%lums IMPOSIBLE!",
                             static_cast<unsigned long>(r.tiempoYMinimoMs));
                } else if (r.produceStopRepentino) {
                    snprintf(r.resumenCortoOled, sizeof(r.resumenCortoOled), "!STOP A %.7s!",
                             r.tareaAfectadaPorStop ? r.tareaAfectadaPorStop : "OTRA");
                } else {
                    snprintf(r.resumenCortoOled, sizeof(r.resumenCortoOled), "OK Y:%lu H:%lu Z:%lums",
                             static_cast<unsigned long>(r.tiempoYMinimoMs),
                             static_cast<unsigned long>(r.tiempoHRecomendadoMs),
                             static_cast<unsigned long>(r.tiempoZSobradoMs));
                }

                snprintf(
                    r.reporteDetallado,
                    sizeof(r.reporteDetallado),
                    "[RTOS-ANALIZADOR] Tarea '%s': hacer '%s' lleva %.1f ms y '%s' lleva %.1f ms (Total=%.1f ms). "
                    "Debe llevar Y_min=%lu ms, H_recomendado=%lu ms, Z_sobrado=%lu ms (Asignaste=%lu ms). %s%s%s",
                    tareas[i].nombreTarea,
                    tareas[i].nombreProcA, r.promedioProcAMs,
                    tareas[i].nombreProcB, r.promedioProcBMs,
                    r.costoTotalMs,
                    static_cast<unsigned long>(r.tiempoYMinimoMs),
                    static_cast<unsigned long>(r.tiempoHRecomendadoMs),
                    static_cast<unsigned long>(r.tiempoZSobradoMs),
                    static_cast<unsigned long>(tareas[i].tiempoAsignadoDesarrolladorMs),
                    r.tiempoImposible ? "[ERROR: El tiempo asignado es IMPOSIBLE] " : "",
                    r.produceStopRepentino ? "[CUIDADO: Tu tiempo Z produce un STOP REPENTINO de otra tarea] " : "",
                    r.entraEnDeadlock ? "[PELIGRO: Esta tarea entra en DEADLOCK]" : "[PLANIFICACION SEGURA]"
                );
            }

            return sistemaSeguro;
        }
    };

} // namespace RTOS