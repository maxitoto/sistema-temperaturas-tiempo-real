#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../connections/CommunicationContainer.hpp"
#include "../database/AuditService.hpp"
#include "../utils/TimeService.hpp"
#include "AlarmController.hpp"

namespace Controllers {

class CommController : public Rtos::RtosTask {
public:
    static constexpr uint32_t ACTIVE_STEP_MS       = 5;
    static constexpr uint32_t IDLE_STEP_MS         = 40;
    static constexpr uint32_t NTP_RETRY_INTERVAL_MS = 60000UL; // Reintenta NTP cada 60s si hay WiFi y no tiene hora

    static CommController& instance() {
        static CommController inst;
        return inst;
    }

    void wakeUpForTx() {
        notify();
    }

protected:
    void onSetup() override {
        auto& comm = Connections::comm;

        Serial.onReceive([this]() {
            this->notify();
        });

        auto onStateChange = [this](Connections::MediumType med,
                                    Connections::TransportState st,
                                    Connections::DisconnectReason reason) {
            Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_CONEXION_CAMBIO);

            bool& estabaAuth = (med == Connections::MediumType::UART_S) ? uartAutenticado_ : wifiAutenticado_;
            if (st == Connections::TransportState::AUTHENTICATED && !estabaAuth) {
                estabaAuth = true;
                alarmController.dispararSonidoEvento(SonidoEvento::CLIENTE_CONECTADO);
                eventoAuditoriaPendiente_ = EventoConexion{true, true, med, reason};
            } else if (st != Connections::TransportState::AUTHENTICATED && estabaAuth) {
                estabaAuth = false;
                alarmController.dispararSonidoEvento(SonidoEvento::CLIENTE_DESCONECTADO);
                eventoAuditoriaPendiente_ = EventoConexion{true, false, med, reason};
            }
        };

        comm.uartS.setStateChangeCallback(onStateChange);
        comm.wifi.setStateChangeCallback(onStateChange);
    }

    void onStep(bool /*wokenByNotification*/) override {
        auto& comm = Connections::comm;
        const uint32_t ahora = millis();

        comm.tick();

        // 1. Procesar auditoría de conexión/desconexión
        if (eventoAuditoriaPendiente_.pendiente) {
            EventoConexion ev = eventoAuditoriaPendiente_;
            eventoAuditoriaPendiente_.pendiente = false;
            auditarCambioSesion(ev.conectado, ev.medio, ev.motivo);
        }

        // 2. Si hay WiFi STA conectado y aún no hay hora válida, probar servidores NTP en cascada (1 -> 2 -> 3)
        if (!Time::timeSrv.estaSincronizado() && WiFi.status() == WL_CONNECTED) {
            if (ultimoIntentoNtpMs_ == 0 || (ahora - ultimoIntentoNtpMs_ >= NTP_RETRY_INTERVAL_MS)) {
                ultimoIntentoNtpMs_ = ahora;
                if (Time::timeSrv.sincronizarConFailoverNtp()) {
                    Audit::registrar(
                        "NTP_SYNC_OK", "ServidoresNtp",
                        "0", String(Time::nowUnix()),
                        "Reloj sincronizado por servidor NTP",
                        CriticidadEvento::CRITICIDAD_BAJA,
                        ResultadoAuditoria::RESULTADO_EXITO,
                        MedioConexion::MEDIO_WIFI_STA, "NTP"
                    );
                }
            }
        }

        bool hayClienteActivo = comm.proxy.isConnected() ||
                                comm.uartS.getState() == Connections::TransportState::HANDSHAKING ||
                                comm.wifi.getState() == Connections::TransportState::HANDSHAKING ||
                                Serial.available() > 0;

        setIntervalMs(hayClienteActivo ? ACTIVE_STEP_MS : IDLE_STEP_MS);
    }

private:
    struct EventoConexion {
        bool pendiente = false;
        bool conectado = false;
        Connections::MediumType medio = Connections::MediumType::NONE;
        Connections::DisconnectReason motivo = Connections::DisconnectReason::NONE;
    };

    bool uartAutenticado_ = false;
    bool wifiAutenticado_ = false;
    uint32_t ultimoIntentoNtpMs_ = 0;
    EventoConexion eventoAuditoriaPendiente_;

    CommController()
        : Rtos::RtosTask("CommTask", 8192, 3, 0, Rtos::TaskMode::HYBRID, IDLE_STEP_MS) {}

    static void auditarCambioSesion(bool conectado,
                                    Connections::MediumType med,
                                    Connections::DisconnectReason reason) {
        MedioConexion medioDb = (med == Connections::MediumType::UART_S)
                                ? MedioConexion::MEDIO_USB
                                : ((WiFi.status() == WL_CONNECTED) ? MedioConexion::MEDIO_WIFI_STA : MedioConexion::MEDIO_WIFI_AP);
        const char* nombreMedio = (med == Connections::MediumType::UART_S) ? "UART-S" : "WIFI";

        if (conectado) {
            Audit::registrar(
                "CLIENT_CONNECT", "Lts",
                "DISCONNECTED", "AUTHENTICATED",
                String("Cliente autenticado por ") + nombreMedio,
                CriticidadEvento::CRITICIDAD_BAJA,
                ResultadoAuditoria::RESULTADO_EXITO,
                medioDb, nombreMedio
            );
        } else {
            Audit::registrar(
                "CLIENT_DISCONNECT", "Lts",
                "AUTHENTICATED", String(static_cast<int>(reason)),
                String("Cliente desconectado de ") + nombreMedio + " (motivo=" + String(static_cast<int>(reason)) + ")",
                CriticidadEvento::CRITICIDAD_BAJA,
                ResultadoAuditoria::RESULTADO_EXITO,
                medioDb, nombreMedio
            );
        }
    }
};

inline CommController& commController = CommController::instance();

} // namespace Controllers