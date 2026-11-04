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
        Rtos::LockGuard lock(Rtos::sysBus.i2cMutex);
        iniciarOled();
        oled.fullScreen(icon_untdf, 128, 48);
    }

    void onStep(bool /*wokenByNotification*/) override {
        // Espera reactiva hasta 2000 ms o hasta que cambien sensores / conexión
        Rtos::sysBus.eventos.waitBits(
            Rtos::Evt::EVT_SENSORES_ACTUALIZADOS | Rtos::Evt::EVT_CONEXION_CAMBIO,
            true, false, 2000
        );

        auto lock = Rtos::sysBus.lockI2c();
        if (!lock.isLocked() || !oled.estaInicializada()) return;
        
        gestorSensores.actualizarTodosLosSlots();
        for (int i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& sensor = gestorSensores.obtenerSensor(i);
            if (!sensor.estaActivo()) continue;
            oled.sensorTemperatura(i, sensor.obtenerTempCelsius(), sensor.estaActivo());
        }
        
    }

private:
    OledController()
        : Rtos::RtosTask("OledTask", 4096, 1, 1, Rtos::TaskMode::HYBRID, 50) {}
};

inline OledController& oledController = OledController::instance();

} // namespace Controllers