#pragma once
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../hardware/driver/led/Led.hpp"
#include "../hardware/driver/buzz/Buzz.hpp"
#include "../hardware/driver/buzz/frecuencias.hpp"
#include "../hardware/placa/esp32S/pines_nodemcu32s.hpp"
#include "../database/AuditService.hpp"

namespace Controllers {

enum class SonidoEvento : uint8_t {
    NINGUNO = 0,
    SISTEMA_BOOT,
    CLIENTE_CONECTADO,
    CLIENTE_DESCONECTADO,
    SENSOR_CONECTADO,
    SENSOR_DESCONECTADO,
    MUTE_ACK
};

class AlarmController : public Rtos::RtosTask {
public:
    // Cantidad de lecturas consecutivas anómalas (~2s c/u) requeridas para confirmar alarma
    static constexpr uint8_t CONFIRM_LECTURAS_TERMICAS = 3; // ~6 s fuera de umbral confirma alarma crítica
    static constexpr uint8_t CONFIRM_LECTURAS_TECNICAS = 3; // ~6 s de fallo confirma alerta media

    static AlarmController& instance() {
        static AlarmController inst;
        return inst;
    }

    // Reproduce un sonido corto de evento (sistema, conexión o hot-plug)
    void dispararSonidoEvento(SonidoEvento ev) {
        eventoSonoroPendiente_ = ev;
        notify();
    }

    // Llamado por RPC o botón físico para silenciar alarmas activas confirmadas
    void mutearAlarmas(MedioConexion medio = MedioConexion::MEDIO_INTERNO) {
        medioUltimoMute_ = medio;
        Rtos::sysBus.semMuteAlarma.give();
        notify();
    }

    // Llamado por SensorController cada vez que hay una nueva muestra real de sensores
    void notificarEvaluacionSensores() {
        nuevaMuestraSensores_ = true;
        notify();
    }

    uint8_t obtenerMascaraActivas() const        { return maskConfirmadasCriticas_ | maskConfirmadasMedias_; }
    uint8_t obtenerMascaraCriticas() const       { return maskConfirmadasCriticas_; }
    uint8_t obtenerMascaraTecnicasMedias() const { return maskConfirmadasMedias_; }
    uint8_t obtenerMascaraMuteadas() const       { return maskMuteadas_; }

protected:
    void onSetup() override {
        pinMode(Pin::PIN_BOTON_BOOT, INPUT_PULLUP);
        dispararSonidoEvento(SonidoEvento::SISTEMA_BOOT);
    }

    void onStep(bool /*wokenByNotification*/) override {
        const bool botonActual = digitalRead(Pin::PIN_BOTON_BOOT);
        const uint32_t ahora = millis();

        // 1. Lectura del botón físico BOOT con debounce de 250 ms
        if (botonMuteAnterior_ == HIGH &&
            botonActual == LOW &&
            (ahora - ultimoBotonMuteMs_ > 250)) {
            ultimoBotonMuteMs_ = ahora;
            medioUltimoMute_ = MedioConexion::MEDIO_INTERNO;
            Rtos::sysBus.semMuteAlarma.give();
        }
        botonMuteAnterior_ = botonActual;

        // 2. Procesar solicitud de Mute
        if (Rtos::sysBus.semMuteAlarma.take(0)) {
            if (ahora - ultimoMuteMs_ > 250) {
                ultimoMuteMs_ = ahora;
                uint8_t activas = obtenerMascaraActivas();
                uint8_t prevMute = maskMuteadas_;
                maskMuteadas_ |= activas;
                Buzz::getInstance().sonarBuzz(sonidoMuteAck, 0.10f);
                finSonidoEventoMs_ = ahora + 120;
                patronContinuoActual_ = PatronContinuo::NINGUNO;

                Audit::registrar(
                    "ALARM_MUTE", "Sensor",
                    String(prevMute), String(maskMuteadas_),
                    "Silenciamiento manual de alarmas activas",
                    CriticidadEvento::CRITICIDAD_BAJA,
                    ResultadoAuditoria::RESULTADO_EXITO,
                    medioUltimoMute_
                );
            }
        }

        // 3. Reproducir sonidos cortos de eventos (Boot, Cliente In/Out, Sensor In/Out)
        SonidoEvento ev = eventoSonoroPendiente_;
        if (ev != SonidoEvento::NINGUNO) {
            eventoSonoroPendiente_ = SonidoEvento::NINGUNO;
            reproducirEfectoEvento(ev, ahora);
        }

        // 4. Evaluar sensores con etapa de confirmación SOLO cuando hay nueva muestra (cada 2s)
        if (nuevaMuestraSensores_) {
            nuevaMuestraSensores_ = false;
            evaluarConfirmacionPorSensor();
        }

        // 5. Indicador visual LED (parpadeo en fallo medio, fijo en alarma crítica)
        uint8_t todasConfirmadas = obtenerMascaraActivas();
        if (maskConfirmadasCriticas_ != 0) {
            Led::getInstance().encenderLed();
        } else if (maskConfirmadasMedias_ != 0) {
            if ((ahora / 400) % 2 == 0) Led::getInstance().encenderLed();
            else Led::getInstance().apagarLed();
        } else if (todasConfirmadas == 0) {
            Led::getInstance().apagarLed();
        }

        // 6. Si todavía está sonando un chime corto de evento, respetarlo hasta que termine
        if (ahora < finSonidoEventoMs_) {
            return;
        }

        // 7. Actuar sobre el Buzzer según la máxima criticidad confirmada no muteada
        uint8_t criticasSonoras = maskConfirmadasCriticas_ & (~maskMuteadas_);
        uint8_t mediasSonoras   = maskConfirmadasMedias_   & (~maskMuteadas_);

        if (criticasSonoras != 0) {
            if (patronContinuoActual_ != PatronContinuo::CRITICA || !Buzz::getInstance().buzzEstaEncendido()) {
                Buzz::getInstance().sonarBuzz(sirenaCritica, -1.0f);
                patronContinuoActual_ = PatronContinuo::CRITICA;
            }
        } else if (mediasSonoras != 0) {
            if (patronContinuoActual_ != PatronContinuo::MEDIA || !Buzz::getInstance().buzzEstaEncendido()) {
                Buzz::getInstance().sonarBuzz(alertaTecnicaMedia, -1.0f);
                patronContinuoActual_ = PatronContinuo::MEDIA;
            }
        } else {
            if (patronContinuoActual_ != PatronContinuo::NINGUNO || Buzz::getInstance().buzzEstaEncendido()) {
                Buzz::getInstance().silenciarBuzz();
                patronContinuoActual_ = PatronContinuo::NINGUNO;
            }
        }
    }

private:
    enum class PatronContinuo : uint8_t { NINGUNO = 0, MEDIA, CRITICA };

    uint8_t maskConfirmadasCriticas_ = 0;
    uint8_t maskConfirmadasMedias_   = 0;
    uint8_t maskMuteadas_            = 0;

    uint8_t contadorCritico_[GestorSensores::MAX_SENSORES] = {0};
    uint8_t contadorTecnico_[GestorSensores::MAX_SENSORES] = {0};
    uint8_t contadorOk_[GestorSensores::MAX_SENSORES]      = {0};

    volatile SonidoEvento eventoSonoroPendiente_ = SonidoEvento::NINGUNO;
    volatile bool nuevaMuestraSensores_          = false;
    uint32_t finSonidoEventoMs_                  = 0;
    PatronContinuo patronContinuoActual_         = PatronContinuo::NINGUNO;

    uint32_t ultimoMuteMs_                       = 0;
    bool botonMuteAnterior_                      = HIGH;
    uint32_t ultimoBotonMuteMs_                  = 0;
    MedioConexion medioUltimoMute_               = MedioConexion::MEDIO_INTERNO;

    AlarmController()
        : Rtos::RtosTask("AlarmTask", 4096, 2, 1, Rtos::TaskMode::HYBRID, 30) {}

    void reproducirEfectoEvento(SonidoEvento ev, uint32_t ahora) {
        patronContinuoActual_ = PatronContinuo::NINGUNO;
        switch (ev) {
            case SonidoEvento::SISTEMA_BOOT:
                Buzz::getInstance().sonarBuzz(sonidoBoot, 0.45f);
                finSonidoEventoMs_ = ahora + 480;
                break;
            case SonidoEvento::CLIENTE_CONECTADO:
                Buzz::getInstance().sonarBuzz(sonidoClienteConectado, 0.26f);
                finSonidoEventoMs_ = ahora + 290;
                break;
            case SonidoEvento::CLIENTE_DESCONECTADO:
                Buzz::getInstance().sonarBuzz(sonidoClienteDesconectado, 0.26f);
                finSonidoEventoMs_ = ahora + 290;
                break;
            case SonidoEvento::SENSOR_CONECTADO:
                Buzz::getInstance().sonarBuzz(sonidoSensorConectado, 0.22f);
                finSonidoEventoMs_ = ahora + 250;
                break;
            case SonidoEvento::SENSOR_DESCONECTADO:
                Buzz::getInstance().sonarBuzz(sonidoSensorDesconectado, 0.30f);
                finSonidoEventoMs_ = ahora + 330;
                break;
            case SonidoEvento::MUTE_ACK:
                Buzz::getInstance().sonarBuzz(sonidoMuteAck, 0.10f);
                finSonidoEventoMs_ = ahora + 120;
                break;
            default:
                break;
        }
    }

    void evaluarConfirmacionPorSensor() {
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; ++i) {
            const Sensor& s = gestorSensores.obtenerSensor(i);
            const uint8_t bit = (1 << i);

            if (!s.estaActivo()) {
                contadorCritico_[i] = 0;
                contadorTecnico_[i] = 0;
                contadorOk_[i]      = 0;
                maskConfirmadasCriticas_ &= ~bit;
                maskConfirmadasMedias_   &= ~bit;
                maskMuteadas_            &= ~bit;
                continue;
            }

            EstadoSensor est = s.obtenerEstado();
            bool esTermicaCritica = (est == EstadoSensor::ALARMA_SUPERIOR ||
                                     est == EstadoSensor::ALARMA_INFERIOR);
            bool esFalloTecnico   = (est == EstadoSensor::DESCONECTADO ||
                                     est == EstadoSensor::ERROR_CRC_SCRATCHPAD ||
                                     est == EstadoSensor::CORTOCIRCUITO_GND ||
                                     est == EstadoSensor::CORTOCIRCUITO_VDD ||
                                     est == EstadoSensor::ALIMENTACION_INSUFICIENTE ||
                                     est == EstadoSensor::CIRCUITO_ABIERTO);

            if (esTermicaCritica) {
                contadorTecnico_[i] = 0;
                contadorOk_[i]      = 0;
                if (contadorCritico_[i] < CONFIRM_LECTURAS_TERMICAS) {
                    contadorCritico_[i]++;
                }
                if (contadorCritico_[i] >= CONFIRM_LECTURAS_TERMICAS && !(maskConfirmadasCriticas_ & bit)) {
                    maskConfirmadasCriticas_ |= bit;
                    maskConfirmadasMedias_   &= ~bit;
                    Audit::registrar(
                        "ALARM_CRITICA_CONFIRMADA", "Sensor",
                        "OK", GestorSensores::nombreEstadoSensor(est),
                        String("Alarma termica confirmada en sonda ") + s.obtenerMAC() +
                        " T=" + String(s.obtenerTempCelsius(), 2) + "C",
                        CriticidadEvento::CRITICIDAD_CRITICA
                    );
                }
            } else if (esFalloTecnico) {
                contadorCritico_[i] = 0;
                contadorOk_[i]      = 0;
                if (contadorTecnico_[i] < CONFIRM_LECTURAS_TECNICAS) {
                    contadorTecnico_[i]++;
                }
                if (contadorTecnico_[i] >= CONFIRM_LECTURAS_TECNICAS && !(maskConfirmadasMedias_ & bit)) {
                    maskConfirmadasMedias_   |= bit;
                    maskConfirmadasCriticas_ &= ~bit;
                    Audit::registrar(
                        "ALERTA_TECNICA_CONFIRMADA", "Sensor",
                        "OK", GestorSensores::nombreEstadoSensor(est),
                        String("Fallo fisico persistente en sonda ") + s.obtenerMAC() +
                        " (" + GestorSensores::nombreEstadoSensor(est) + ")",
                        CriticidadEvento::CRITICIDAD_MEDIA
                    );
                }
            } else {
                // El sensor leyó OK: limpiamos contadores de fallo inmediatamente
                contadorCritico_[i] = 0;
                contadorTecnico_[i] = 0;
                if ((maskConfirmadasCriticas_ & bit) || (maskConfirmadasMedias_ & bit)) {
                    contadorOk_[i]++;
                    // 2 lecturas OK seguidas confirman recuperación y limpian alarma y mute
                    if (contadorOk_[i] >= 2) {
                        maskConfirmadasCriticas_ &= ~bit;
                        maskConfirmadasMedias_   &= ~bit;
                        maskMuteadas_            &= ~bit;
                        contadorOk_[i] = 0;
                        Audit::registrar(
                            "ALARM_RECUPERADA", "Sensor",
                            "ALARMA", "OK",
                            String("Sonda ") + s.obtenerMAC() + " normalizada",
                            CriticidadEvento::CRITICIDAD_BAJA
                        );
                    }
                }
            }
        }
    }
};

inline AlarmController& alarmController = AlarmController::instance();

} // namespace Controllers