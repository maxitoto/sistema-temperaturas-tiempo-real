#pragma once
#include <Arduino.h>
#include "Sensor.hpp"
#include "../../../database/repositories/repoCalibracion.hpp"

class SensorInicializador {
private:
    inline static float refCongelacionGlobal = 0.0f;
    inline static float refEbullicionGlobal  = 100.0f;
    inline static bool  calibracionGlobalLeida = false;

    static void cargarReferenciasGlobalesBajoDemanda() {
        if (calibracionGlobalLeida) return;

        // consulta DB
        

        calibracionGlobalLeida = true;
    }

public:
    static float obtenerRefCongelacion() { return refCongelacionGlobal; }
    static float obtenerRefEbullicion()  { return refEbullicionGlobal; }

    static bool hidratarSensorBajoDemanda(Sensor& s) {
        if (!s.estaActivo()) return false;
        if (s.estaHidratadoDB()) return true;

        cargarReferenciasGlobalesBajoDemanda();

        // consulta DB
        s.setIdSensorDB(static_cast<int32_t>(s.obtenerSlot() + 1));

        s.setHidratadoDB(true);
        return true;
    }

    static bool persistirConfiguracionSensor(const Sensor& s) {
        if (!s.estaActivo()) return false;

        // consulta DB

        return true;
    }
};