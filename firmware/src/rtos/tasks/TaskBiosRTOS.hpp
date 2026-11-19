#pragma once
#include <Arduino.h>
#include <SD.h>
#include <esp_attr.h>
#include <esp_system.h>
#include "../PrimitivasRTOS.hpp"
#include "../RelojSistema.hpp"
#include "../../hardware/driver/led/Led.hpp"
#include "../../hardware/driver/buzz/Buzz.hpp"
#include "../../hardware/driver/oled/Oled.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"

namespace RTOS::Tasks {

    struct CajaNegraBiosRTC {
        uint32_t magic;
        uint32_t timestampUnix;
        uint32_t uptimeSegundos;
        uint32_t heapLibreBytes;
        int8_t   nucleoAfectado;
        char     tareaAfectada[16];
        char     accionTomada[32];
        char     resultado[24];
        char     conclusion[40];
        bool     pendienteVolcarSd;
    };

    // Memoria RTC que sobrevive a reinicios por Watchdog o ESP.restart()
    RTC_NOINIT_ATTR inline CajaNegraBiosRTC cajaNegraRtc;

    class TaskBiosRTOS {
    public:
        static constexpr uint32_t MAGIC_RTC_BIOS = 0xB105F00DU;
        static constexpr const char* RUTA_HELP_LOG = "/db/help.log";

        struct VigilanciaTarea {
            TareaSegura* refTarea;
            uint32_t     timeoutMaxMs;
        };

    private:
        inline static TareaSegura    tareaBios;
        inline static VigilanciaTarea vigiladas[4] = {};
        inline static size_t         totalVigiladas = 0;
        inline static uint32_t       incidentesTotales = 0;

        static void emitirCodigoPitidosBios(uint8_t cantidadPitidos) {
            led.encenderLed();
            for (uint8_t i = 0; i < cantidadPitidos; ++i) {
                buzz.sonarBuzz(1850, 0.14f);
                vTaskDelay(pdMS_TO_TICKS(220));
            }
        }

        // Intenta anexar en /db/help.log con timeout máximo de 150 ms (NUNCA bloquea a la BIOS)
        static bool escribirLineaHelpLogNoBloqueante(const char* linea) {
            SemaphoreHandle_t mtx = TarjetaSD::getInstance().obtenerMutex();
            if (!mtx || !TarjetaSD::getInstance().estaInicializada()) return false;

            if (xSemaphoreTakeRecursive(mtx, pdMS_TO_TICKS(150)) != pdTRUE) {
                return false;
            }

            bool ok = false;
            File f = SD.open(RUTA_HELP_LOG, FILE_APPEND);
            if (!f) {
                f = SD.open(RUTA_HELP_LOG, FILE_WRITE);
            }
            if (f) {
                size_t len = strlen(linea);
                ok = (f.write(reinterpret_cast<const uint8_t*>(linea), len) == len);
                f.flush();
                f.close();
            }
            xSemaphoreGiveRecursive(mtx);
            return ok;
        }

        static void formatearYGuardarIncidente(
            uint32_t tsUnix,
            uint32_t uptimeS,
            uint32_t heapLibre,
            int8_t nucleo,
            const char* nombreTarea,
            const char* accion,
            const char* resultado,
            const char* conclusion
        ) {
            PgDateTime fecha = RelojSistema::desdeTimestampUnix(tsUnix);
            char linea[240];
            snprintf(
                linea,
                sizeof(linea),
                "[%s | up:%lus | heap:%luB] NUCLEO:%d | TAREA:%s | ACCION:%s | RESULTADO:%s | CONCLUSION:%s\n",
                fecha.c_str(),
                static_cast<unsigned long>(uptimeS),
                static_cast<unsigned long>(heapLibre),
                static_cast<int>(nucleo),
                nombreTarea ? nombreTarea : "NONE",
                accion ? accion : "NONE",
                resultado ? resultado : "NONE",
                conclusion ? conclusion : "NONE"
            );

            if (!escribirLineaHelpLogNoBloqueante(linea)) {
                // Si la MicroSD está bloqueada, guarda en CajaNegraBiosRTC para escribirlo al reiniciar
                cajaNegraRtc.magic             = MAGIC_RTC_BIOS;
                cajaNegraRtc.timestampUnix     = tsUnix;
                cajaNegraRtc.uptimeSegundos    = uptimeS;
                cajaNegraRtc.heapLibreBytes    = heapLibre;
                cajaNegraRtc.nucleoAfectado    = nucleo;
                strncpy(cajaNegraRtc.tareaAfectada, nombreTarea ? nombreTarea : "TASK", sizeof(cajaNegraRtc.tareaAfectada) - 1);
                strncpy(cajaNegraRtc.accionTomada,  accion ? accion : "KILL",           sizeof(cajaNegraRtc.accionTomada) - 1);
                strncpy(cajaNegraRtc.resultado,     resultado ? resultado : "FALLO_SD", sizeof(cajaNegraRtc.resultado) - 1);
                strncpy(cajaNegraRtc.conclusion,    conclusion ? conclusion : "REBOOT", sizeof(cajaNegraRtc.conclusion) - 1);
                cajaNegraRtc.pendienteVolcarSd = true;
            }
        }

        static void volcarCajaNegraPreviaSiExiste() {
            if (cajaNegraRtc.magic == MAGIC_RTC_BIOS && cajaNegraRtc.pendienteVolcarSd) {
                cajaNegraRtc.pendienteVolcarSd = false;
                formatearYGuardarIncidente(
                    cajaNegraRtc.timestampUnix,
                    cajaNegraRtc.uptimeSegundos,
                    cajaNegraRtc.heapLibreBytes,
                    cajaNegraRtc.nucleoAfectado,
                    cajaNegraRtc.tareaAfectada,
                    cajaNegraRtc.accionTomada,
                    cajaNegraRtc.resultado,
                    cajaNegraRtc.conclusion
                );
            } else if (cajaNegraRtc.magic != MAGIC_RTC_BIOS) {
                memset(&cajaNegraRtc, 0, sizeof(CajaNegraBiosRTC));
                cajaNegraRtc.magic = MAGIC_RTC_BIOS;
            }
        }

        static void ejecutarCicloBios(void*) {
            volcarCajaNegraPreviaSiExiste();

            const uint32_t ahoraMs = millis();
            SemaphoreHandle_t mtxSd = TarjetaSD::getInstance().obtenerMutex();
            TaskHandle_t duenoSd    = mtxSd ? xSemaphoreGetMutexHolder(mtxSd) : nullptr;

            for (size_t i = 0; i < totalVigiladas; ++i) {
                TareaSegura* t = vigiladas[i].refTarea;
                if (!t || !t->estaActiva()) continue;

                const uint32_t sinLatidoMs = ahoraMs - t->obtenerUltimoLatidoMs();
                if (sinLatidoMs <= vigiladas[i].timeoutMaxMs) {
                    continue;
                }

                // ¡TAREA COLGADA DETECTADA!
                incidentesTotales++;
                const int8_t nucleo       = t->obtenerNucleo();
                const char*  nombre       = t->obtenerNombre();
                TaskHandle_t handleColgado= t->obtenerHandle();
                const uint32_t tsUnix     = RelojSistema::obtenerTimestampActual();
                const uint32_t uptimeS    = ahoraMs / 1000UL;
                const uint32_t heapLibre  = ESP.getFreeHeap();

                // 3 pitidos si el bloqueo es en Core 0 (BD/Alertas), 4 pitidos si es en Core 1 (Comms/Sensores/OLED)
                emitirCodigoPitidosBios(nucleo == 0 ? 3 : 4);

                char f1[22], f2[22], f3[22], f4[22];
                snprintf(f1, sizeof(f1), "!BIOS CORE %d LOCK!", static_cast<int>(nucleo));
                snprintf(f2, sizeof(f2), "Tarea: %.13s", nombre);
                snprintf(f3, sizeof(f3), "Sin latido: %lus", static_cast<unsigned long>(sinLatidoMs / 1000UL));

                // Caso A: La tarea murió reteniendo el mutex físico de la MicroSD
                if (duenoSd != nullptr && duenoSd == handleColgado) {
                    snprintf(f4, sizeof(f4), "SD LOCK -> REBOOT");
                    pizarraOled.publicarPantalla("BIOS-OS", f1, f2, f3, f4);
                    pizarraOled.volcarAlHardwareI2C();

                    formatearYGuardarIncidente(
                        tsUnix, uptimeS, heapLibre, nucleo, nombre,
                        "MATAR_TAREA_CON_MUTEX_SD",
                        "MUTEX_SD_RETENIDO",
                        "REINICIO_BUS_HARDWARE_CONTROLADO"
                    );
                    vTaskDelay(pdMS_TO_TICKS(400));
                    ESP.restart();
                    return;
                }

                // Caso B: La tarea no retiene la MicroSD -> Matar y Revivir en caliente
                bool revivida = t->reiniciarForzadoPorBios();
                snprintf(f4, sizeof(f4), revivida ? "REVIVIDA EN CALIENTE" : "FALLO REVIVIR->RST");
                pizarraOled.publicarPantalla("BIOS-OS", f1, f2, f3, f4);
                pizarraOled.volcarAlHardwareI2C();

                formatearYGuardarIncidente(
                    tsUnix, uptimeS, heapLibre, nucleo, nombre,
                    "MATAR_Y_REVIVIR_TAREA",
                    revivida ? "TAREA_REINICIADA" : "SIN_HEAP_PARA_TAREA",
                    revivida ? "SISTEMA_RECUPERADO_EN_CALIENTE" : "REINICIO_FORZADO_SISTEMA"
                );

                if (!revivida) {
                    vTaskDelay(pdMS_TO_TICKS(400));
                    ESP.restart();
                }
            }
        }

    public:
        static void registrarTareaVigilada(TareaSegura& tarea, uint32_t timeoutMaxMs) {
            if (totalVigiladas < 4) {
                vigiladas[totalVigiladas++] = VigilanciaTarea{ &tarea, timeoutMaxMs };
            }
        }

        static bool iniciar() {
            // Prioridad 5 (máxima del sistema), corre cada 500 ms con stack ligero
            return tareaBios.iniciarEnNucleo("TaskBiosOS", &ejecutarCicloBios, 500, 4096, 5, 1);
        }

        static TareaSegura& obtenerTarea()       { return tareaBios; }
        static uint32_t obtenerTotalIncidentes() { return incidentesTotales; }
        static size_t obtenerCantidadVigiladas() { return totalVigiladas; }
        static const TareaSegura* obtenerTareaVigilada(size_t idx) {
            return (idx < totalVigiladas) ? vigiladas[idx].refTarea : nullptr;
        }
    };

} // namespace RTOS::Tasks