#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "EstadoSensor.hpp"

// Copia segura por valor (Thread-Safe Snapshot) para usar desde cualquier tarea o RPC
struct SnapshotSensor {
    bool         activo;
    uint8_t      slot;
    int32_t      idSensorDB;
    uint8_t      rom[8];
    char         mac[17];
    char         mac3[4];
    char         mac2[3];
    float        tempCruda;
    float        tempCorregida;
    float        umbral_sup;
    float        umbral_inf;
    float        offset;
    float        medicionAguaHirviendo;
    float        medicionAguaHelada;
    EstadoSensor estado;
    EstadoSensor estadoPrevio;
    bool         enAlarma;
    uint8_t      resolucionBits;
    bool         alimentacionParasita;
    uint32_t     timestampLecturaMs;
};

// Evento emitido automáticamente cuando un sensor cambia de estado (ej. de OK a ALARMA o DESCONECTADO)
struct EventoCambioSensor {
    int32_t      idSensorDB;
    uint8_t      slot;
    char         mac[17];
    EstadoSensor estadoPrevio;
    EstadoSensor estadoNuevo;
    float        tempCruda;
    float        tempCorregida;
    uint32_t     timestampMs;
};

struct ResumenSistemaSensores {
    EstadoBus    estadoBus;
    EventoGestor ultimoEvento;
    uint8_t      totalConectados;
    uint8_t      totalDetectadosBus;
    uint8_t      totalEnAlarma;
    bool         modoParasitoActivo;
    uint32_t     timestampUltimaConversionMs;
};

class Sensor {
public:
    static constexpr float UMBRAL_MAX_DEFAULT = 126.0f;
    static constexpr float UMBRAL_MIN_DEFAULT = -55.0f;
    static constexpr float OFFSET_DEFAULT     = 0.0f;
    static constexpr float TEMP_MIN_C = -55.0f, TEMP_MAX_C = 126.0f, OFFSET_MAX_ABS_C = 20.0f;

private:
    bool          activo                = false;
    bool          hidratadoDB           = false;
    uint8_t       indiceSlot            = 0;
    int32_t       idSensorDB            = -1;
    DeviceAddress rom                   = {0};
    char          mac[17]               = "0000000000000000";
    char          mac3[4]               = "000";
    char          mac2[3]               = "00";
    float         umbral_sup            = UMBRAL_MAX_DEFAULT;
    float         umbral_inf            = UMBRAL_MIN_DEFAULT;
    float         offset                = OFFSET_DEFAULT;
    float         error_sup    = 0.0f; // Temperatura medida en agua hirviendo (3 min)
    float         error_inf    = 0.0f; // Temperatura medida en agua con hielo (3 min)
    float         escalaM               = 1.0f; // Pendiente precalculada O(1)
    float         ordenadaB             = 0.0f; // Ordenada precalculada O(1)
    EstadoSensor  estado                = EstadoSensor::SLOT_VACIO;
    EstadoSensor  estadoPrevio          = EstadoSensor::SLOT_VACIO;
    bool          cambioEstadoPendiente = false;
    float         tempCruda             = DEVICE_DISCONNECTED_C;
    float         tempCelsius           = DEVICE_DISCONNECTED_C; // Temperatura corregida (fuente de verdad)
    int32_t       tempRaw               = DEVICE_DISCONNECTED_RAW;
    uint8_t       resolucionBits        = 12;
    bool          alimentacionParasita  = false;
    bool          enAlarma              = false;
    uint32_t      ultimoTimestampMs     = 0;
    uint8_t       scratchpad[9]         = {0};

public:
    Sensor() = default;
    ~Sensor() = default;

    void limpiar(uint8_t slot = 0) {
        activo                = false;
        hidratadoDB           = false;
        indiceSlot            = slot;
        idSensorDB            = -1;
        memset(rom, 0, sizeof(rom));
        strncpy(mac, "0000000000000000", sizeof(mac));
        strncpy(mac3, "000", sizeof(mac3));
        strncpy(mac2, "00", sizeof(mac2));
        umbral_sup        = UMBRAL_MAX_DEFAULT;
        umbral_inf        = UMBRAL_MIN_DEFAULT;
        offset                = OFFSET_DEFAULT;
        error_sup    = 0.0f;
        error_inf    = 0.0f;
        escalaM               = 1.0f;
        ordenadaB             = 0.0f;
        estado                = EstadoSensor::SLOT_VACIO;
        estadoPrevio          = EstadoSensor::SLOT_VACIO;
        cambioEstadoPendiente = false;
        tempCruda             = DEVICE_DISCONNECTED_C;
        tempCelsius           = DEVICE_DISCONNECTED_C;
        tempRaw               = DEVICE_DISCONNECTED_RAW;
        resolucionBits        = 12;
        alimentacionParasita  = false;
        enAlarma              = false;
        ultimoTimestampMs     = 0;
        memset(scratchpad, 0, sizeof(scratchpad));
    }

    void inicializarNuevo(uint8_t slot, const DeviceAddress nuevaRom) {
        limpiar(slot);
        activo = true;
        memcpy(rom, nuevaRom, sizeof(DeviceAddress));
        for (uint8_t i = 0; i < 8; i++) {
            snprintf(mac + (i * 2), 3, "%02X", rom[i]);
        }
        mac[16] = '\0';
        mac3[0] = mac[13];
        mac3[1] = mac[14];
        mac3[2] = mac[15];
        mac3[3] = '\0';
        mac2[0] = mac[14];
        mac2[1] = mac[15];
        mac2[2] = '\0';
        estado       = EstadoSensor::OK;
        estadoPrevio = EstadoSensor::OK;
        recalcularRectaCalibracion();
    }

    // Precalcula m y b una sola vez para que cada lectura sea O(1): T_corr = m * T_cruda + b
    void recalcularRectaCalibracion(float refCongelacion = 0.0f, float refEbullicion = 100.0f) {
        float deltaMedido = error_sup - error_inf;
        if (fabsf(deltaMedido) > 1.0f && fabsf(refEbullicion - refCongelacion) > 1.0f) {
            escalaM   = (refEbullicion - refCongelacion) / deltaMedido;
            ordenadaB = refCongelacion - (escalaM * error_inf) + offset;
        } else {
            escalaM   = 1.0f;
            ordenadaB = offset;
        }
    }

    float aplicarCalibracion(float lecturaCruda) const {
        return (escalaM * lecturaCruda) + ordenadaB;
    }

    bool coincideROM(const DeviceAddress otraRom) const {
        return activo && (memcmp(rom, otraRom, sizeof(DeviceAddress)) == 0);
    }

    void setActivo(bool v)                     { activo = v; }
    void setHidratadoDB(bool h)                { hidratadoDB = h; }
    void setIdSensorDB(int32_t id)             { idSensorDB = id; }
    void setIndiceSlot(uint8_t s)              { indiceSlot = s; }
    static float limitarTemp(float v)   { return fminf(fmaxf(v, TEMP_MIN_C), TEMP_MAX_C); }
    static float limitarOffset(float v) { return fminf(fmaxf(v, -OFFSET_MAX_ABS_C), OFFSET_MAX_ABS_C); }
    void setUmbralSuperior(float v) { umbral_sup = limitarTemp(v); }
    void setUmbralInferior(float v) { umbral_inf = limitarTemp(v); }
    void setUmbrales(float sup, float inf) { umbral_sup = limitarTemp(sup); umbral_inf = limitarTemp(inf); }
    void setOffset(float v, float refCong = 0.0f, float refEbull = 100.0f) {
        offset = limitarOffset(v);
        recalcularRectaCalibracion(refCong, refEbull);
    }
    void setCorrecciones(float medSupHirviendo, float medInfHielo, float refCong = 0.0f, float refEbull = 100.0f) {
        error_sup = limitarTemp(medSupHirviendo);
        error_inf = limitarTemp(medInfHielo);
        recalcularRectaCalibracion(refCong, refEbull);
    }
    void setEstado(EstadoSensor nuevoEstado) {
        if (estado != nuevoEstado) {
            if (estado != EstadoSensor::SLOT_VACIO) {
                estadoPrevio = estado;
                cambioEstadoPendiente = true;
            } else {
                estadoPrevio = nuevoEstado;
            }
            estado = nuevoEstado;
        }
    }
    bool consumirCambioEstado(EventoCambioSensor& outEvt) {
        if (!cambioEstadoPendiente) return false;
        cambioEstadoPendiente = false;
        outEvt.idSensorDB    = idSensorDB;
        outEvt.slot          = indiceSlot;
        strncpy(outEvt.mac, mac, sizeof(outEvt.mac));
        outEvt.estadoPrevio  = estadoPrevio;
        outEvt.estadoNuevo   = estado;
        outEvt.tempCruda     = tempCruda;
        outEvt.tempCorregida = tempCelsius;
        outEvt.timestampMs   = ultimoTimestampMs;
        return true;
    }

    void setTempCruda(float t)                 { tempCruda = t; }
    void setTempCelsius(float t)               { tempCelsius = t; }
    void setTempRaw(int32_t r)                 { tempRaw = r; }
    void setTimestampLectura(uint32_t ts)      { ultimoTimestampMs = ts; }
    void setResolucionBits(uint8_t b)          { resolucionBits = b; }
    void setAlimentacionParasita(bool p)       { alimentacionParasita = p; }
    void setEnAlarma(bool a)                   { enAlarma = a; }

    bool           estaActivo()                const { return activo; }
    bool           estaHidratadoDB()           const { return hidratadoDB; }
    int32_t        obtenerIdSensorDB()         const { return idSensorDB; }
    uint8_t        obtenerSlot()               const { return indiceSlot; }
    const uint8_t* obtenerROM()                const { return rom; }
    uint8_t        obtenerFamilia()            const { return rom[0]; }
    const char*    obtenerMAC()                const { return mac; }
    const char*    obtenerMAC3()               const { return mac3; }
    const char*    obtenerMAC2()               const { return mac2; }
    float          obtenerUmbralSuperior()     const { return umbral_sup; }
    float          obtenerUmbralInferior()     const { return umbral_inf; }
    float          obtenerOffset()             const { return offset; }
    float          obtenerErrorSuperior() const { return error_sup; }
    float          obtenerErrorInferior() const { return error_inf; }
    EstadoSensor   obtenerEstado()             const { return estado; }
    EstadoSensor   obtenerEstadoPrevio()       const { return estadoPrevio; }
    float          obtenerTempCruda()          const { return tempCruda; }
    float          obtenerTempCelsius()        const { return tempCelsius; }
    int32_t        obtenerTempRaw()            const { return tempRaw; }
    uint8_t        obtenerResolucionBits()     const { return resolucionBits; }
    bool           esAlimentacionParasita()    const { return alimentacionParasita; }
    bool           estaEnAlarma()              const { return enAlarma; }
    uint32_t       obtenerTimestampLectura()   const { return ultimoTimestampMs; }
    uint8_t*       obtenerScratchpad()               { return scratchpad; }
    const uint8_t* obtenerScratchpad()         const { return scratchpad; }

    SnapshotSensor crearSnapshot() const {
        SnapshotSensor snap{};
        snap.activo               = activo;
        snap.slot                 = indiceSlot;
        snap.idSensorDB           = idSensorDB;
        memcpy(snap.rom, rom, sizeof(rom));
        strncpy(snap.mac, mac, sizeof(snap.mac));
        strncpy(snap.mac3, mac3, sizeof(snap.mac3));
        strncpy(snap.mac2, mac2, sizeof(snap.mac2));
        snap.tempCruda            = tempCruda;
        snap.tempCorregida        = tempCelsius;
        snap.umbral_sup       = umbral_sup;
        snap.umbral_inf       = umbral_inf;
        snap.offset               = offset;
        snap.medicionAguaHirviendo= error_sup;
        snap.medicionAguaHelada   = error_inf;
        snap.estado               = estado;
        snap.estadoPrevio         = estadoPrevio;
        snap.enAlarma             = enAlarma;
        snap.resolucionBits       = resolucionBits;
        snap.alimentacionParasita = alimentacionParasita;
        snap.timestampLecturaMs   = ultimoTimestampMs;
        return snap;
    }
};