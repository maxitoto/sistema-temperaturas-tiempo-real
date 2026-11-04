#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../hardware/driver/oled/Oled.hpp"
#include "../hardware/driver/oled/icons/icons.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../connections/CommunicationContainer.hpp"

namespace Controllers {

class OledController : public Rtos::RtosTask {
public:
    static OledController& instance() {
        static OledController inst;
        return inst;
    }

protected:
    void onSetup() override {
        auto lock = Rtos::sysBus.lockI2c();
        if (!iniciarOled()) return;

        // Mostrar logo UNTDF durante 1.5s al inicio y luego preparar la grilla
        oled.fullScreen(icon_untdf, 128, 48);
        vTaskDelay(pdMS_TO_TICKS(1500));
        oled.limpiarTodo();
        oled.cabeceraIzq("HELADERA");
    }

    void onStep(bool /*wokenByNotification*/) override {
        Rtos::sysBus.eventos.waitBits(
            Rtos::Evt::EVT_SENSORES_ACTUALIZADOS | Rtos::Evt::EVT_CONEXION_CAMBIO,
            true, false, 2000
        );

        auto lock = Rtos::sysBus.lockI2c();
        if (!lock.isLocked() || !oled.estaInicializada()) return;

        // Solo leemos el último valor en caché de RAM (SensorDbController se encarga del bus 1-Wire)
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            if (!s.estaActivo()) {
                oled.sensor(i, "S--: ---");
            } else {
                EstadoSensor est = s.obtenerEstado();
                bool ok = (est == EstadoSensor::OK ||
                           est == EstadoSensor::ALARMA_SUPERIOR ||
                           est == EstadoSensor::ALARMA_INFERIOR);
                oled.sensorTemperatura(i, s.obtenerTempCelsius(), ok);
            }
        }
    }

private:
    OledController()
        : Rtos::RtosTask("OledTask", 4096, 1, 1, Rtos::TaskMode::HYBRID, 50) {}
};

inline OledController& oledController = OledController::instance();

} // namespace Controllers