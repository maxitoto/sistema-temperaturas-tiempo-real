#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../hardware/driver/oled/Oled.hpp"
#include "../hardware/driver/oled/icons/icons.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"


#include "../connections/CommunicationContainer.hpp"
#include <WiFi.h>

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
        char fechaTxt[16];
        Time::timeSrv.formatearFechaActual(fechaTxt, sizeof(fechaTxt));
        oled.cabeceraIzq(fechaTxt);
    }

    void onStep(bool /*wokenByNotification*/) override {

        Rtos::sysBus.eventos.waitBits(
            Rtos::Evt::EVT_SENSORES_ACTUALIZADOS | Rtos::Evt::EVT_CONEXION_CAMBIO,
            true, false, 2000
        );

        auto lock = Rtos::sysBus.lockI2c();
        if (!lock.isLocked() || !oled.estaInicializada()) return;

        char fechaTxt[16];
        Time::timeSrv.formatearFechaActual(fechaTxt, sizeof(fechaTxt));
        oled.cabeceraIzq(fechaTxt);

        auto& proxy = Connections::comm.proxy;
        actualizarSlot(cacheDer1_, proxy.isConnected() ? person_icon16x16 : nullptr,
                       &Oled::slotDer1, &Oled::limpiarSlotDer1);
        actualizarSlot(cacheDer2_,
                       proxy.isMediumConnected(Connections::MediumType::UART_S) ? usb_icon16x16 : nullptr,
                       &Oled::slotDer2, &Oled::limpiarSlotDer2);
        actualizarSlot(cacheDer3_, iconoWifiSta(), &Oled::slotDer3, &Oled::limpiarSlotDer3);

        // Solo leemos el último valor en caché de RAM (SensorDbController se encarga del bus 1-Wire)
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            if (s.estaActivo()) {
                oled.sensorTemperatura(i, s.obtenerMAC2(), s.obtenerTempCelsius(), true);
            } else {
                oled.sensorTemperatura(i, "--", 0.0f, false);   // fuerza "--:ERR"
            }
        }
    }

private:

    const uint8_t* cacheDer1_ = nullptr;
    const uint8_t* cacheDer2_ = nullptr;
    const uint8_t* cacheDer3_ = nullptr;

    void actualizarSlot(const uint8_t*& cache, const uint8_t* nuevo,
                        void (Oled::*dibujar)(const unsigned char*),
                        void (Oled::*limpiar)()) {
        if (cache == nuevo) return;
        cache = nuevo;
        if (nuevo) (oled.*dibujar)(nuevo);
        else       (oled.*limpiar)();
    }

    // wifi1 = 3 arcos (mejor señal) ... wifi4 = solo punto (señal mínima)
    static const uint8_t* iconoWifiSta() {
        if (WiFi.status() != WL_CONNECTED) return nullptr;
        int rssi = WiFi.RSSI();
        if (rssi >= -60) return wifi1_icon16x16;
        if (rssi >= -70) return wifi2_icon16x16;
        if (rssi >= -80) return wifi3_icon16x16;
        return wifi4_icon16x16;
    }

    OledController()
        : Rtos::RtosTask("OledTask", 4096, 1, 1, Rtos::TaskMode::HYBRID, 50) {}
};

inline OledController& oledController = OledController::instance();

} // namespace Controllers