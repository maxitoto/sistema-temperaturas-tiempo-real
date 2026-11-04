#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../database/orm/ORM.hpp"
#include "AlarmController.hpp"

namespace Controllers {

class SensorDbController : public Rtos::RtosTask {
public:
    static constexpr uint32_t SAMPLE_INTERVAL_MS  = 2000;               // Lectura física cada 2 s
    static constexpr uint32_t DB_SAVE_INTERVAL_MS = 2UL * 60UL * 1000UL; // Guardado en BD cada 2 minutos

    static SensorDbController& instance() {
        static SensorDbController inst;
        return inst;
    }

    void forzarGuardadoInmediato() {
        Rtos::sysBus.eventos.setBits(Rtos::namespaceEvt::EVT_FORZAR_GUARDADO_DB);
        notify();
    }

protected:
    void onSetup() override {
        gestorSensores.escanearYAsignarSensores(12, false);
        sincronizarCatalogoSensoresEnDB();
        ultimoGuardadoDbMs_ = millis();
    }

    void onStep(bool /*wokenByNotification*/) override {
        // 1. Verificar hot-plug y actualizar temperaturas de los 6 slots
        if (gestorSensores.verificarCambiosEnCaliente()) {
            gestorSensores.escanearYAsignarSensores(12, false);
            sincronizarCatalogoSensoresEnDB();
        } else {
            gestorSensores.actualizarTodosLosSlots();
        }

        // 2. Despertar al controlador de Alarmas y notificar a la tarea OLED
        alarmController.notificarEvaluacionSensores();
        Rtos::sysBus.eventos.setBits(Rtos::namespaceEvt::EVT_SENSORES_ACTUALIZADOS);

        // 3. Guardar en la tabla "Senso" cada 2 minutos (o si se solicitó forzar guardado)
        uint32_t ahora = millis();
        EventBits_t bits = Rtos::sysBus.eventos.waitBits(
            Rtos::namespaceEvt::EVT_FORZAR_GUARDADO_DB, true, false, 0
        );
        if ((bits & Rtos::namespaceEvt::EVT_FORZAR_GUARDADO_DB) != 0 ||
            (ahora - ultimoGuardadoDbMs_ >= DB_SAVE_INTERVAL_MS)) {
            guardarSensosEnDB();
            ultimoGuardadoDbMs_ = ahora;
        }
    }

private:
    uint32_t ultimoGuardadoDbMs_ = 0;

    SensorDbController()
        : Rtos::RtosTask("SensorDbTask", 6144, 2, 1, Rtos::TaskMode::HYBRID, SAMPLE_INTERVAL_MS) {}

    // Registra en la tabla "Sensor" cualquier sonda DS18B20 nueva detectada en el bus 1-Wire
    void sincronizarCatalogoSensoresEnDB() {
        Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 2000);
        if (!lock.isLocked()) return;

        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            if (!s.estaActivo()) continue;

            String hexMac = String(s.obtenerMAC());
            auto existentes = db.from("Sensor").select("id_sensor").eq("hexadecimal", hexMac).execute();
            if (existentes.empty()) {
                db.from("Sensor").insert({
                    {"id_sensor", String(i + 1)},
                    {"hexadecimal", hexMac},
                    {"mac2", String(s.obtenerMAC2())},
                    {"nombre", "Sonda " + String(i + 1)},
                    {"ubicacion", "Heladera"},
                    {"offset_calibracion", String(s.obtenerOffset(), 2)},
                    {"umbral_superior", String(s.obtenerUmbralSuperior())},
                    {"umbral_inferior", String(s.obtenerUmbralInferior())},
                    {"resolucion_bits", String(s.obtenerResolucionBits())},
                    {"alimentacion_parasita", s.esAlimentacionParasita() ? "1" : "0"},
                    {"estado_actual", String(static_cast<int>(s.obtenerEstado()))},
                    {"fecha_creacion", String(millis() / 1000)},
                    {"fecha_modificacion", String(millis() / 1000)}
                });
            }
        }
    }

    // Inserta el senso periódico de todos los sensores activos cada 2 minutos
    void guardarSensosEnDB() {
        Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 3000);
        if (!lock.isLocked()) return;

        uint32_t tsSeg = millis() / 1000;
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            if (!s.estaActivo()) continue;

            float tempCorregida = s.obtenerTempCelsius();
            float tempCruda = tempCorregida - s.obtenerOffset();

            db.from("Senso").insert({
                {"id_sensor", String(i + 1)},
                {"temperatura_cruda", String(tempCruda, 2)},
                {"temperatura_corregida", String(tempCorregida, 2)},
                {"estado_sensor", String(static_cast<int>(s.obtenerEstado()))},
                {"activa", "1"},
                {"fecha_creacion", String(tsSeg)}
            });
        }
    }
};

inline SensorDbController& sensorDbController = SensorDbController::instance();

} // namespace Controllers