#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../hardware/driver/led/Led.hpp"
#include "../hardware/driver/buzz/Buzz.hpp"
#include "../hardware/driver/buzz/frecuencias.hpp"
#include "../hardware/placa/esp32S/pines_nodemcu32s.hpp"

namespace Controllers {

class AlarmController : public Rtos::RtosTask {
public:
    static AlarmController& instance() {
        static AlarmController inst;
        return inst;
    }

    // Llamado por RPC o código para mutear las alarmas actuales usando el semáforo binario
    void mutearAlarmas() {
        Rtos::sysBus.semMuteAlarma.give();
        notify();
    }

    // Llamado por SensorDbController cuando cambia el estado de los sensores
    void notificarEvaluacionSensores() {
        notify();
    }

    uint8_t obtenerMascaraActivas() const  { return maskEnAlarma_; }
    uint8_t obtenerMascaraMuteadas() const { return maskMuteadas_; }

protected:
    void onSetup() override {
        pinMode(Pin::PIN_BOTON_BOOT, INPUT_PULLUP);
    }

    void onStep(bool /*wokenByNotification*/) override {
        const bool botonActual = digitalRead(Pin::PIN_BOTON_BOOT);
        const uint32_t ahora = millis();

        // Detectar flanco descendente del botón físico BOOT con anti-rebote
        if (botonMuteAnterior_ == HIGH &&
            botonActual == LOW &&
            (ahora - ultimoBotonMuteMs_ > 250)) {
            ultimoBotonMuteMs_ = ahora;
            Rtos::sysBus.semMuteAlarma.give();
        }
        botonMuteAnterior_ = botonActual;

        // 1. Procesar solicitud de mute (tanto desde botón físico como desde RPC)
        if (Rtos::sysBus.semMuteAlarma.take(0)) {
            if (ahora - ultimoMuteMs_ > 250) {
                ultimoMuteMs_ = ahora;
                maskMuteadas_ |= maskEnAlarma_;
                Buzz::getInstance().silenciarBuzz();
            }
        }

        // 2. Evaluar el estado individual de los 6 sensores
        uint8_t nuevasAlarmas = 0;
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            if (!s.estaActivo()) continue;

            EstadoSensor est = s.obtenerEstado();
            bool esFalloOAlarma = (est == EstadoSensor::ALARMA_SUPERIOR ||
                                   est == EstadoSensor::ALARMA_INFERIOR ||
                                   est == EstadoSensor::DESCONECTADO ||
                                   est == EstadoSensor::ERROR_CRC_SCRATCHPAD ||
                                   est == EstadoSensor::CORTOCIRCUITO_GND ||
                                   est == EstadoSensor::CORTOCIRCUITO_VDD);

            if (esFalloOAlarma) {
                nuevasAlarmas |= (1 << i);
            } else {
                // Si el sensor volvió a OK, se limpia su flag de mute para futuros eventos
                maskMuteadas_ &= ~(1 << i);
            }
        }

        maskEnAlarma_ = nuevasAlarmas;
        uint8_t alarmasSonorasActivas = maskEnAlarma_ & (~maskMuteadas_);

        // 3. Actuar sobre LED (indicador visual de cualquier alarma activa) y Buzzer (solo no muteadas)
        if (maskEnAlarma_ != 0) {
            Led::getInstance().encenderLed();
        } else {
            Led::getInstance().apagarLed();
        }

        if (alarmasSonorasActivas != 0) {
            if (!Buzz::getInstance().buzzEstaEncendido()) {
                Buzz::getInstance().sonarBuzz(sirena, -1.0f);
            }
        } else {
            if (Buzz::getInstance().buzzEstaEncendido()) {
                Buzz::getInstance().silenciarBuzz();
            }
        }
    }

private:
    uint8_t maskEnAlarma_ = 0;
    uint8_t maskMuteadas_ = 0;
    uint32_t ultimoMuteMs_ = 0;
    bool botonMuteAnterior_ = HIGH;
    uint32_t ultimoBotonMuteMs_ = 0;

    AlarmController()
        : Rtos::RtosTask("AlarmTask", 3072, 2, 1, Rtos::TaskMode::HYBRID, 20) {}
};

inline AlarmController& alarmController = AlarmController::instance();

} // namespace Controllers