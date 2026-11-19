#pragma once
#include <Arduino.h>
#include "../PrimitivasRTOS.hpp"
#include "../RelojSistema.hpp"
#include "../EventosYColas.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../connections/middlewares/middleware_message.hpp"


namespace RTOS::Tasks {

    class TaskSensores {
    public:
        static TareaSegura& obtenerTarea() { return tarea; }
        static constexpr uint32_t INTERVALO_STREAM_ACTIVO_MS  = 1000;   // 1s cuando hay streaming
        static constexpr uint32_t INTERVALO_REPOSO_MS         = 3000;   // 3s sin streaming
        static constexpr uint32_t INTERVALO_GUARDADO_DB_MS    = 120000; // 2 minutos fijos para DB

    private:
        inline static TareaSegura tarea;
        inline static MiddlewareMessage* refMiddlewareL3 = nullptr;
        inline static uint32_t ultimoRefrescoSensoresMs  = 0;
        inline static uint32_t ultimoGuardadoLoteDbMs    = 0;

        static void ejecutarCiclo(void*) {
            const uint32_t ahoraMs = millis();
            const bool streamingActivo = (refMiddlewareL3 != nullptr) &&
                                         (refMiddlewareL3->obtenerMascaraSuscriptores() != 0);

            const uint32_t intervaloDeseado = streamingActivo
                ? INTERVALO_STREAM_ACTIVO_MS
                : INTERVALO_REPOSO_MS;

            if (ahoraMs - ultimoRefrescoSensoresMs < intervaloDeseado) {
                return;
            }
            ultimoRefrescoSensoresMs = ahoraMs;

            // 1. Verificar si se conectó o desconectó físicamente un sensor en el bus 1-Wire
            if (gestorSensores.verificarCambiosEnCaliente()) {
                EventoGestor evBus = gestorSensores.obtenerUltimoEvento();
                if (evBus == EventoGestor::NUEVO_SENSOR_DETECTADO) {
                    encolarSonido(TipoSonidoEvento::SENSOR_CONECTADO);
                } else if (evBus == EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE) {
                    encolarSonido(TipoSonidoEvento::SENSOR_DESCONECTADO);
                }

                gestorSensores.escanearYAsignarSensores();

                RegistroPersistenciaPOD regBus{};
                regBus.tipo          = TipoRegistroDB::EVENTO_SISTEMA_BUS;
                regBus.timestampUnix = RelojSistema::obtenerTimestampActual();
                regBus.estadoNuevo   = static_cast<int16_t>(gestorSensores.obtenerEstadoBus());
                regBus.estadoPrevio  = static_cast<int16_t>(evBus);
                strncpy(regBus.detalle, GestorSensores::nombreEventoGestor(evBus), sizeof(regBus.detalle) - 1);
                encolarPersistencia(regBus);
                gestorSensores.consumirEvento();
            }

            // 2. Solicitar conversión y actualizar lecturas de todos los sensores activos
            gestorSensores.actualizarTodosLosSlots();

            // 3. Si algún sensor cambió de estado (ej. OK -> ALARMA_SUPERIOR o DESCONECTADO), encolar evento inmediato
            static EventoCambioSensor cambios[GestorSensores::MAX_SENSORES];
            uint8_t cantCambios = gestorSensores.extraerEventosCambioEstado(cambios);
            for (uint8_t i = 0; i < cantCambios; ++i) {
                RegistroPersistenciaPOD ev{};
                ev.tipo          = TipoRegistroDB::EVENTO_CAMBIO_SENSOR;
                ev.timestampUnix = RelojSistema::obtenerTimestampActual();
                ev.idRelacionado = cambios[i].idSensorDB;
                ev.estadoPrevio  = static_cast<int16_t>(cambios[i].estadoPrevio);
                ev.estadoNuevo   = static_cast<int16_t>(cambios[i].estadoNuevo);
                ev.tempCruda     = cambios[i].tempCruda;
                ev.tempCorregida = cambios[i].tempCorregida;
                strncpy(ev.origenOClave, cambios[i].mac, sizeof(ev.origenOClave) - 1);
                strncpy(ev.detalle, GestorSensores::nombreEstadoSensor(cambios[i].estadoNuevo), sizeof(ev.detalle) - 1);
                encolarPersistencia(ev);
            }

            // 4. Cada 2 minutos exactos: ordenar el guardado en lote de todos los sensores conectados en la tabla Senso
            if (ahoraMs - ultimoGuardadoLoteDbMs >= INTERVALO_GUARDADO_DB_MS) {
                ultimoGuardadoLoteDbMs = ahoraMs;
                RegistroPersistenciaPOD lote{};
                lote.tipo          = TipoRegistroDB::LOTE_SENSOS_2_MINUTOS;
                lote.timestampUnix = RelojSistema::obtenerTimestampActual();
                strncpy(lote.detalle, "Censo periodico 2 min", sizeof(lote.detalle) - 1);
                encolarPersistencia(lote);
            }
        }

    public:
        static bool iniciar(MiddlewareMessage& l3) {
            refMiddlewareL3 = &l3;
            if (gestorSensores.obtenerTotalEnBus() == 0) {
                if (gestorSensores.verificarEstadoElectricoBus() == EstadoBus::OK) {
                    gestorSensores.escanearYAsignarSensores();
                }
            }
            ultimoRefrescoSensoresMs = millis();
            ultimoGuardadoLoteDbMs   = millis();

            // Corre en Core 1 con chequeo ligero cada 200ms
            return tarea.iniciarEnNucleo("TaskSensores", &ejecutarCiclo, 200, 6144, 2, 1);
        }
    };

} // namespace RTOS::Tasks