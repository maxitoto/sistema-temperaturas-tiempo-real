#pragma once
#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
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
        static constexpr uint32_t    MAGIC_RTC_BIOS    = 0xB105F00DU;
        static constexpr const char* RUTA_HELP_LOG     = "/db/help.log";
        static constexpr const char* NVS_NAMESPACE     = "bios_bb";
        static constexpr uint8_t     FALLOS_CONFIRMAR  = 2; // 2 ciclos consecutivos (1 s)

        struct VigilanciaTarea {
            TareaSegura* refTarea;
            uint32_t     timeoutMaxMs;
        };

    private:
        inline static TareaSegura     tareaBios;
        inline static VigilanciaTarea vigiladas[4]         = {};
        inline static uint8_t         fallosConsecutivos[4]= {};
        inline static size_t          totalVigiladas       = 0;
        inline static uint32_t        incidentesTotales    = 0;
        inline static bool            arranqueSincronizado = false;
        inline static bool            pendienteVolcarNvs   = false;
        inline static uint32_t        ultimoIntentoVolcarMs= 0;

        static void emitirCodigoPitidosBios(uint8_t cantidadPitidos) {
            led.encenderLed();
            for (uint8_t i = 0; i < cantidadPitidos; ++i) {
                buzz.sonarBuzz(1850, 0.14f);
                vTaskDelay(pdMS_TO_TICKS(220));
            }
        }

        static void formatearLineaIncidente(
            char* outBuf,
            size_t outCap,
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
            snprintf(
                outBuf,
                outCap,
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
        }

        // Guarda el incidente en la Flash interna (NVS) del ESP32: sobrevive a cortes de energía y formateo de SD
        static void guardarEnFlashNvs(const char* linea, uint32_t totalIncidentes, bool pendienteSd) {
            Preferences prefs;
            if (prefs.begin(NVS_NAMESPACE, false)) {
                prefs.putUInt("cnt", totalIncidentes);
                prefs.putBool("pend", pendienteSd);
                if (linea && linea[0] != '\0') {
                    prefs.putString("last", linea);
                }
                prefs.end();
            }
        }

        static void marcarVolcadoNvsCompletado() {
            Preferences prefs;
            if (prefs.begin(NVS_NAMESPACE, false)) {
                prefs.putBool("pend", false);
                prefs.end();
            }
        }

        // Anexa una línea en /db/help.log asegurando que /db exista (solo si se obtiene el mutex SD limpiamente)
        static bool escribirLineaHelpLogSeguro(const char* linea, uint32_t timeoutMutexMs, bool desmontarSdAlTerminar = false) {
            if (!linea || linea[0] == '\0') return false;
            TarjetaSD& sd = TarjetaSD::getInstance();
            SemaphoreHandle_t mtx = sd.obtenerMutex();
            if (!mtx || !sd.estaInicializada()) return false;

            if (xSemaphoreTakeRecursive(mtx, pdMS_TO_TICKS(timeoutMutexMs)) != pdTRUE) {
                return false;
            }

            bool ok = false;
            if (!SD.exists("/db")) {
                SD.mkdir("/db");
            }
            File f = SD.open(RUTA_HELP_LOG, FILE_APPEND);
            if (!f) {
                f = SD.open(RUTA_HELP_LOG, FILE_WRITE);
            }
            if (f) {
                const size_t len = strlen(linea);
                ok = (f.write(reinterpret_cast<const uint8_t*>(linea), len) == len);
                f.flush();
                f.close();
            }

            if (desmontarSdAlTerminar) {
                SD.end();
            }
            xSemaphoreGiveRecursive(mtx);
            return ok;
        }

        // Sincroniza al arranque el contador NVS y vuelca a /db/help.log cualquier incidente pendiente
        static void sincronizarCajaNegraEnArranque() {
            if (!arranqueSincronizado) {
                arranqueSincronizado = true;

                Preferences prefs;
                if (prefs.begin(NVS_NAMESPACE, true)) {
                    incidentesTotales  = prefs.getUInt("cnt", 0);
                    pendienteVolcarNvs = prefs.getBool("pend", false);
                    prefs.end();
                }

                if (cajaNegraRtc.magic != MAGIC_RTC_BIOS) {
                    memset(&cajaNegraRtc, 0, sizeof(CajaNegraBiosRTC));
                    cajaNegraRtc.magic = MAGIC_RTC_BIOS;
                }
            }

            const bool hayRtc = (cajaNegraRtc.magic == MAGIC_RTC_BIOS && cajaNegraRtc.pendienteVolcarSd);
            if (!hayRtc && !pendienteVolcarNvs) {
                return;
            }

            const uint32_t ahoraMs = millis();
            if (ultimoIntentoVolcarMs != 0 && static_cast<int32_t>(ahoraMs - ultimoIntentoVolcarMs) < 5000) {
                return;
            }
            ultimoIntentoVolcarMs = ahoraMs;

            char linea[260]{};
            if (pendienteVolcarNvs) {
                Preferences prefs;
                if (prefs.begin(NVS_NAMESPACE, true)) {
                    String s = prefs.getString("last", "");
                    prefs.end();
                    if (!s.isEmpty()) {
                        strncpy(linea, s.c_str(), sizeof(linea) - 1);
                    }
                }
            }

            if (linea[0] == '\0' && hayRtc) {
                formatearLineaIncidente(
                    linea, sizeof(linea),
                    cajaNegraRtc.timestampUnix,
                    cajaNegraRtc.uptimeSegundos,
                    cajaNegraRtc.heapLibreBytes,
                    cajaNegraRtc.nucleoAfectado,
                    cajaNegraRtc.tareaAfectada,
                    cajaNegraRtc.accionTomada,
                    cajaNegraRtc.resultado,
                    cajaNegraRtc.conclusion
                );
            }

            if (linea[0] == '\0') {
                pendienteVolcarNvs = false;
                cajaNegraRtc.pendienteVolcarSd = false;
                marcarVolcadoNvsCompletado();
                return;
            }

            if (escribirLineaHelpLogSeguro(linea, 150, false)) {
                pendienteVolcarNvs = false;
                cajaNegraRtc.pendienteVolcarSd = false;
                marcarVolcadoNvsCompletado();
            }
        }

        static void ejecutarCicloBios(void*) {
            sincronizarCajaNegraEnArranque();

            for (size_t i = 0; i < totalVigiladas; ++i) {
                TareaSegura* t = vigiladas[i].refTarea;
                if (!t || !t->estaActiva()) {
                    fallosConsecutivos[i] = 0;
                    continue;
                }

                // Cálculo inmune a underflow entre Core 0 y Core 1
                const uint32_t sinLatidoMs = t->msDesdeUltimoLatido();
                if (sinLatidoMs <= vigiladas[i].timeoutMaxMs) {
                    fallosConsecutivos[i] = 0;
                    continue;
                }

                // Histéresis de 2 ciclos consecutivos (1 s) para descartar picos transitorios
                if (fallosConsecutivos[i] < 255) {
                    fallosConsecutivos[i]++;
                }
                if (fallosConsecutivos[i] < FALLOS_CONFIRMAR) {
                    continue;
                }

                // ¡BLOQUEO REAL CONFIRMADO! Nunca se mata la tarea en caliente para proteger FAT32 y mutexes.
                incidentesTotales++;
                const int8_t   nucleo      = t->obtenerNucleo();
                const char*    nombre      = t->obtenerNombre();
                const uint32_t tsUnix      = RelojSistema::obtenerTimestampActual();
                const uint32_t uptimeS     = millis() / 1000UL;
                const uint32_t heapLibre   = ESP.getFreeHeap();
                TaskHandle_t   hColgado    = t->obtenerHandle();
                SemaphoreHandle_t mtxSd    = TarjetaSD::getInstance().obtenerMutex();
                TaskHandle_t   duenoSd     = mtxSd ? xSemaphoreGetMutexHolder(mtxSd) : nullptr;
                const bool     retieneSd   = (duenoSd != nullptr && duenoSd == hColgado);

                const char* accion     = retieneSd ? "REBOOT_CONTROLADO_SD_LOCK" : "REBOOT_CONTROLADO_WATCHDOG";
                const char* resultado  = retieneSd ? "MUTEX_SD_RETENIDO"         : "TAREA_SIN_LATIDO";
                const char* conclusion = "CAJA_NEGRA_NVS_Y_REINICIO_LIMPIO";

                char linea[260]{};
                formatearLineaIncidente(
                    linea, sizeof(linea),
                    tsUnix, uptimeS, heapLibre, nucleo, nombre,
                    accion, resultado, conclusion
                );

                // 1. Respaldo inmediato en Flash NVS + RTC (sobrevive a corte de energía o fallo de SD)
                guardarEnFlashNvs(linea, incidentesTotales, true);
                cajaNegraRtc.magic             = MAGIC_RTC_BIOS;
                cajaNegraRtc.timestampUnix     = tsUnix;
                cajaNegraRtc.uptimeSegundos    = uptimeS;
                cajaNegraRtc.heapLibreBytes    = heapLibre;
                cajaNegraRtc.nucleoAfectado    = nucleo;
                strncpy(cajaNegraRtc.tareaAfectada, nombre ? nombre : "TASK", sizeof(cajaNegraRtc.tareaAfectada) - 1);
                strncpy(cajaNegraRtc.accionTomada,  accion,                   sizeof(cajaNegraRtc.accionTomada) - 1);
                strncpy(cajaNegraRtc.resultado,     resultado,                sizeof(cajaNegraRtc.resultado) - 1);
                strncpy(cajaNegraRtc.conclusion,    conclusion,               sizeof(cajaNegraRtc.conclusion) - 1);
                cajaNegraRtc.pendienteVolcarSd = true;

                // 2. Publicar aviso en PizarraOled (lo dibuja TaskOled sin colisionar en I2C)
                char f1[28], f2[28], f3[28], f4[28];
                snprintf(f1, sizeof(f1), "!BIOS CORE %d LOCK!", static_cast<int>(nucleo));
                snprintf(f2, sizeof(f2), "Tarea: %.13s", nombre);
                snprintf(f3, sizeof(f3), "Sin latido: %lus", static_cast<unsigned long>(sinLatidoMs / 1000UL));
                snprintf(f4, sizeof(f4), retieneSd ? "SD BUSY -> REBOOT" : "GUARDADO -> REBOOT");
                pizarraOled.publicarMensajeTemporal("BIOS-OS", f1, f2, f3, f4, 6000);

                // 3. Emitir alerta sonora (3 pitidos Core 0, 4 pitidos Core 1)
                emitirCodigoPitidosBios(nucleo == 0 ? 3 : 4);

                // 4. Si la tarea colgada NO tiene la SD, volcar a /db/help.log y desmontar FAT32 limpiamente
                if (!retieneSd) {
                    if (escribirLineaHelpLogSeguro(linea, 1500, true)) {
                        cajaNegraRtc.pendienteVolcarSd = false;
                        marcarVolcadoNvsCompletado();
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(350));
                ESP.restart();
                return;
            }
        }

    public:
        static void registrarTareaVigilada(TareaSegura& tarea, uint32_t timeoutMaxMs) {
            if (totalVigiladas < 4) {
                vigiladas[totalVigiladas]          = VigilanciaTarea{ &tarea, timeoutMaxMs };
                fallosConsecutivos[totalVigiladas] = 0;
                totalVigiladas++;
            }
        }

        static bool iniciar() {
            // Prioridad 5 (máxima), chequeo cada 500 ms, stack 4 KB con holgura para NVS + SD
            return tareaBios.iniciarEnNucleo("TaskBiosOS", &ejecutarCicloBios, 500, 4096, 5, 1);
        }

        // Permite a RPSistema leer el respaldo en Flash NVS incluso si /db/help.log fue borrado o no existe aún
        static bool leerRespaldoNvs(char* outBuf, size_t outCap) {
            if (!outBuf || outCap == 0) return false;
            outBuf[0] = '\0';
            Preferences prefs;
            if (!prefs.begin(NVS_NAMESPACE, true)) return false;
            String s = prefs.getString("last", "");
            prefs.end();
            if (s.isEmpty()) return false;
            strncpy(outBuf, s.c_str(), outCap - 1);
            outBuf[outCap - 1] = '\0';
            return true;
        }

        static TareaSegura& obtenerTarea()       { return tareaBios; }
        static uint32_t obtenerTotalIncidentes() { return incidentesTotales; }
        static size_t obtenerCantidadVigiladas() { return totalVigiladas; }
        static const TareaSegura* obtenerTareaVigilada(size_t idx) {
            return (idx < totalVigiladas) ? vigiladas[idx].refTarea : nullptr;
        }
    };

} // namespace RTOS::Tasks