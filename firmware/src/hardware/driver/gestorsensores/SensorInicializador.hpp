#pragma once
#include <Arduino.h>
#include "Sensor.hpp"

class SensorInicializador {
private:
    struct EntradaCacheLazy {
        bool    ocupado = false;
        char    mac[17] = {0};
        int32_t idSensorDB = -1;
        float   offset = 0.0f;
        float   corrSup = 0.0f;
        float   corrInf = 0.0f;
        float   umbralSup = 125.0f;
        float   umbralInf = -55.0f;
        uint8_t resolucion = 12;
    };

    inline static EntradaCacheLazy cacheLazy[6] = {};
    inline static float refCongelacionGlobal = 0.0f;
    inline static float refEbullicionGlobal  = 100.0f;
    inline static bool  calibracionGlobalLeida = false;

    static void cargarReferenciasGlobalesBajoDemanda() {
        if (calibracionGlobalLeida) return;

        //consulta DB

        calibracionGlobalLeida = true;
    }

    static EntradaCacheLazy* buscarEnCacheLazy(const char* macHex) {
        for (uint8_t i = 0; i < 6; ++i) {
            if (cacheLazy[i].ocupado && strncmp(cacheLazy[i].mac, macHex, 16) == 0) {
                return &cacheLazy[i];
            }
        }
        return nullptr;
    }

    static void guardarEnCacheLazy(const Sensor& s) {
        EntradaCacheLazy* entrada = buscarEnCacheLazy(s.obtenerMAC());
        if (!entrada) {
            for (uint8_t i = 0; i < 6; ++i) {
                if (!cacheLazy[i].ocupado) {
                    entrada = &cacheLazy[i];
                    break;
                }
            }
            if (!entrada) entrada = &cacheLazy[0];
        }
        entrada->ocupado    = true;
        strncpy(entrada->mac, s.obtenerMAC(), sizeof(entrada->mac));
        entrada->idSensorDB = s.obtenerIdSensorDB();
        entrada->offset     = s.obtenerOffset();
        entrada->corrSup    = s.obtenerCorreccionSuperior();
        entrada->corrInf    = s.obtenerCorreccionInferior();
        entrada->umbralSup  = s.obtenerUmbralSuperior();
        entrada->umbralInf  = s.obtenerUmbralInferior();
        entrada->resolucion = s.obtenerResolucionBits();
    }

public:
    static float obtenerRefCongelacion() { return refCongelacionGlobal; }
    static float obtenerRefEbullicion()  { return refEbullicionGlobal; }

    // Hidrata un sensor individual a demanda (Lazy Loading): primero mira la caché lazy en RAM, luego la tabla Sensor
    static bool hidratarSensorBajoDemanda(Sensor& s) {
        if (!s.estaActivo()) return false;
        if (s.estaHidratadoDB()) return true;

        // 1. Consulta rápida en caché lazy si ya había sido cargado antes en esta sesión
        if (EntradaCacheLazy* hit = buscarEnCacheLazy(s.obtenerMAC())) {
            s.setIdSensorDB(hit->idSensorDB);
            s.setUmbrales(hit->umbralSup, hit->umbralInf);
            s.setResolucionBits(hit->resolucion);
            s.setOffset(hit->offset, refCongelacionGlobal, refEbullicionGlobal);
            s.setCorrecciones(hit->corrSup, hit->corrInf, refCongelacionGlobal, refEbullicionGlobal);
            s.setHidratadoDB(true);
            return true;
        }

        // 2. Si el catálogo físico de base de datos está montado, busca por MAC o registra el sensor nuevo

        //
        
        cargarReferenciasGlobalesBajoDemanda();

        // consulta DB
        s.setIdSensorDB(static_cast<int32_t>(s.obtenerSlot() + 1));

        s.setHidratadoDB(true);
        guardarEnCacheLazy(s);
        return true;
    }

    // Persiste en la tabla Sensor cualquier cambio de umbrales, offset o calibración hecho en caliente
    static bool persistirConfiguracionSensor(const Sensor& s) {
        if (!s.estaActivo()) return false;
        guardarEnCacheLazy(s);

        // consulta DB

        return true;
    }
};