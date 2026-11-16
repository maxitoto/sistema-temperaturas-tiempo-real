#pragma once
#include <Arduino.h>
#include "Sensor.hpp"
#include "../../../database/repositories/repoSensor.hpp"

class SensorInicializador {
private:
    inline static float refCongelacionGlobal = 0.0f;
    inline static float refEbullicionGlobal  = 100.0f;
    inline static bool  calibracionGlobalLeida = false;

    static void cargarReferenciasGlobalesBajoDemanda() {
        if (calibracionGlobalLeida) return;

        // try { CalibracionDTO c; RepoCalibracion::obtenerOInicializar(c);
        //   si c.activa -> refCongelacionGlobal = c.tempCongelacionRef; refEbullicionGlobal = c.tempEbullicionRef;
        //   calibracionGlobalLeida = true; } catch (...) { dejar en false para reintentar }
        

        calibracionGlobalLeida = true;
    }

public:
    static float obtenerRefCongelacion() { return refCongelacionGlobal; }
    static float obtenerRefEbullicion()  { return refEbullicionGlobal; }

    static bool hidratarSensorBajoDemanda(Sensor& s) {
        if (!s.estaActivo()) return false;
        if (s.estaHidratadoDB()) return true;

        cargarReferenciasGlobalesBajoDemanda();

        // try { SensorDTO d = RepoSensor::registrarOAsegurarPorMac(s.obtenerMAC());  // crea la fila si el sensor es nuevo
        //   s.setIdSensorDB(d.idSensor);
        //   s.setUmbrales(d.umbralSup, d.umbralInf);
        //   s.setOffset(d.offset, refCongelacionGlobal, refEbullicionGlobal);
        //   s.setCorrecciones(d.errorSup, d.errorInf, refCongelacionGlobal, refEbullicionGlobal); // recalcula m y b
        // } catch (...) { return false; }   // sin marcar hidratado, se reintenta en el próximo escaneo


        s.setHidratadoDB(true);
        return true;
    }

    static bool persistirConfiguracionSensor(const Sensor& s) {
        if (!s.estaActivo()) return false;

        // try { RepoSensor::actualizarCalibracionYUmbralesPorMac(s.obtenerMAC(), s.obtenerUmbralSuperior(),
        //   s.obtenerUmbralInferior(), s.obtenerCorreccionSuperior(), s.obtenerCorreccionInferior(), s.obtenerOffset());
        //   return true; } catch (...) { return false; }

        return true;
    }
};