#pragma once
#include <map>
#include <Arduino.h>
#include "../rtos/RtosTask.hpp"
#include "../rtos/SystemBus.hpp"
#include "../database/orm/ORM.hpp"
#include "../database/EsquemaFisico.hpp"
#include "../hardware/driver/gestorsensores/GestorSensores.hpp"

namespace Controllers {

class SensorController : public Rtos::RtosTask {
public:
    static constexpr uint32_t SAMPLE_INTERVAL_MS        = 2000;
    static constexpr uint32_t SENSO_PERSIST_INTERVAL_MS = 2UL * 60UL * 1000UL;

    static SensorController& instance() {
        static SensorController inst;
        return inst;
    }

protected:
    void onSetup() override;
    void onStep(bool wokenByNotification) override;

private:
    // ------- pipeline -------
    void sincronizarCatalogoDB();
    void detectarTransicionesYGuardar();
    void guardarSensoPeriodico();
    void guardarEventoSistema(EventoGestor ev, const String& detalle);  // requiere dbMutex tomado
    void notificarTareas();
    void actualizarSnapshot();

    // ------- helpers -------
    int  estadoParaSenso(EstadoSensor e);

    struct Snapshot {
        bool         activo = false;
        EstadoSensor estado = EstadoSensor::SLOT_VACIO;
        char         mac[17] = {0};
        uint8_t estabilidad = 0;
    };
    Snapshot snapshot_[GestorSensores::MAX_SENSORES];
    uint32_t ultimoSensoMs_ = 0;

    SensorController()
        : Rtos::RtosTask("SensorTask", 8192, 1, 1,
                         Rtos::TaskMode::HYBRID, SAMPLE_INTERVAL_MS) {}
};

inline SensorController& sensorController = SensorController::instance();

} // namespace Controllers