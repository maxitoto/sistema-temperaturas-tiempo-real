#pragma once
#include <Arduino.h>
#include "Sensor.hpp"
#include "../../../database/repositories/repoSensor.hpp"
#include "../../../database/repositories/repoCalibracion.hpp"

class SensorInicializador {
private:
    inline static float refCongelacionGlobal  = 0.0f;
    inline static float refEbullicionGlobal   = 100.0f;
    inline static bool  calibracionGlobalLeida = false;

    static void cargarReferenciasGlobalesBajoDemanda() {
        if (calibracionGlobalLeida) return;

        try {
            DbRuntime::Repositories::CalibracionDTO c{};
            if (DbRuntime::Repositories::RepoCalibracion::obtenerOInicializar(c)) {
                if (c.activa) {
                    refCongelacionGlobal = c.tempCongelacionRef;
                    refEbullicionGlobal  = c.tempEbullicionRef;
                }
                calibracionGlobalLeida = true;
            }
        } catch (...) {
            calibracionGlobalLeida = false;
        }
    }

public:
    static void invalidarCacheCalibracionGlobal() {
        calibracionGlobalLeida = false;
    }

    static float obtenerRefCongelacion() {
        cargarReferenciasGlobalesBajoDemanda();
        return refCongelacionGlobal;
    }

    static float obtenerRefEbullicion() {
        cargarReferenciasGlobalesBajoDemanda();
        return refEbullicionGlobal;
    }

    static bool hidratarSensorBajoDemanda(Sensor& s) {
        if (!s.estaActivo()) return false;
        if (s.estaHidratadoDB()) return true;

        cargarReferenciasGlobalesBajoDemanda();

        try {
            DbRuntime::Repositories::SensorDTO d{};
            DbRuntime::Repositories::RepoSensor::asegurarPorMacEnDestino(s.obtenerMAC(), d);
            s.setIdSensorDB(d.idSensor);
            s.setUmbrales(d.umbralSup, d.umbralInf);
            s.configurarCalibracion(d.errorSup, d.errorInf, d.offset, refCongelacionGlobal, refEbullicionGlobal);
            s.setHidratadoDB(true);
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool persistirConfiguracionSensor(const Sensor& s) {
        if (!s.estaActivo()) return false;

        try {
            DbRuntime::Repositories::RepoSensor::actualizarCalibracionYUmbralesPorMac(
                s.obtenerMAC(),
                s.obtenerUmbralSuperior(),
                s.obtenerUmbralInferior(),
                s.obtenerErrorSuperior(),
                s.obtenerErrorInferior(),
                s.obtenerOffset()
            );
            return true;
        } catch (...) {
            return false;
        }
    }
};