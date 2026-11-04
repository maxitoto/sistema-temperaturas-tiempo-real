#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../connections/CommunicationContainer.hpp"

namespace Controllers {

class CommController : public Rtos::RtosTask {
public:
    // Frecuencia rápida cuando hay sesión o tráfico (5 ms), ahorro cuando espera conexión (40 ms)
    static constexpr uint32_t ACTIVE_STEP_MS = 5;
    static constexpr uint32_t IDLE_STEP_MS   = 40;

    static CommController& instance() {
        static CommController inst;
        return inst;
    }

    // Permite despertar inmediatamente la tarea cuando un controlador o API encola un mensaje saliente
    void wakeUpForTx() {
        notify();
    }

protected:
    void onSetup() override {
        auto& comm = Connections::comm;

        // Despertar inmediatamente por interrupción/callback de UART cuando entran bytes por Serial
        Serial.onReceive([this]() {
            this->notify();
        });

        // Notificar a la pantalla OLED cada vez que cambie el estado de autenticación en UART-S o WiFi
        auto onStateChange = [](Connections::MediumType, Connections::TransportState, Connections::DisconnectReason) {
            Rtos::sysBus.eventos.setBits(Rtos::namespaceEvt::EVT_CONEXION_CAMBIO);
        };
        comm.uartS.setStateChangeCallback(onStateChange);
        comm.wifi.setStateChangeCallback(onStateChange);
    }

    void onStep(bool /*wokenByNotification*/) override {
        auto& comm = Connections::comm;

        // Ejecuta el scheduler del Proxy y despacha/reintenta mensajes RPC
        comm.tick();

        // Ajusta dinámicamente el tiempo de bloqueo según haya clientes conectados o bytes pendientes
        bool hayClienteActivo = comm.proxy.isConnected() ||
                                comm.uartS.getState() == Connections::TransportState::HANDSHAKING ||
                                comm.wifi.getState() == Connections::TransportState::HANDSHAKING ||
                                Serial.available() > 0;

        setIntervalMs(hayClienteActivo ? ACTIVE_STEP_MS : IDLE_STEP_MS);
    }

private:
    // Corre en Core 0 junto a la pila WiFi/LwIP con prioridad alta (3)
    CommController()
        : Rtos::RtosTask("CommTask", 8192, 3, 0, Rtos::TaskMode::HYBRID, IDLE_STEP_MS) {}
};

inline CommController& commController = CommController::instance();

} // namespace Controllers