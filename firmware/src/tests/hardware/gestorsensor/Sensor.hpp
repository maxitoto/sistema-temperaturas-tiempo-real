#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <DallasTemperature.h>
#include "EstadoSensor.hpp"

class Sensor {
public:
    static constexpr int8_t UMBRAL_MAX_DEFAULT = 126;
    static constexpr int8_t UMBRAL_MIN_DEFAULT = -126;
    static constexpr float  OFFSET_DEFAULT     = 0.0f;

private:
    bool          activo               = false;
    uint8_t       indiceSlot           = 0;
    DeviceAddress rom                  = {0};
    char          mac[17]              = "0000000000000000";
    char          mac2[3]              = "00";
    int8_t        umbralSuperior       = UMBRAL_MAX_DEFAULT;
    int8_t        umbralInferior       = UMBRAL_MIN_DEFAULT;
    float         offset               = OFFSET_DEFAULT;
    EstadoSensor  estado               = EstadoSensor::SLOT_VACIO;
    float         tempCelsius          = DEVICE_DISCONNECTED_C;
    int32_t       tempRaw              = DEVICE_DISCONNECTED_RAW;
    uint8_t       resolucionBits       = 12;
    bool          alimentacionParasita = false;
    bool          enAlarma             = false;
    int16_t       datoUsuario          = 0;
    uint8_t       scratchpad[9]        = {0};

public:
    Sensor() = default;
    ~Sensor() = default;

    void limpiar(uint8_t slot = 0) {
        activo               = false;
        indiceSlot           = slot;
        memset(rom, 0, sizeof(rom));
        strncpy(mac, "0000000000000000", sizeof(mac));
        strncpy(mac2, "00", sizeof(mac2));
        umbralSuperior       = UMBRAL_MAX_DEFAULT;
        umbralInferior       = UMBRAL_MIN_DEFAULT;
        offset               = OFFSET_DEFAULT;
        estado               = EstadoSensor::SLOT_VACIO;
        tempCelsius          = DEVICE_DISCONNECTED_C;
        tempRaw              = DEVICE_DISCONNECTED_RAW;
        resolucionBits       = 12;
        alimentacionParasita = false;
        enAlarma             = false;
        datoUsuario          = 0;
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
        mac2[0] = mac[14];
        mac2[1] = mac[15];
        mac2[2] = '\0';
        umbralSuperior = UMBRAL_MAX_DEFAULT;
        umbralInferior = UMBRAL_MIN_DEFAULT;
        offset         = OFFSET_DEFAULT;
        estado         = EstadoSensor::OK;
    }

    bool coincideROM(const DeviceAddress otraRom) const {
        return activo && (memcmp(rom, otraRom, sizeof(DeviceAddress)) == 0);
    }

    void setActivo(bool v)                     { activo = v; }
    void setIndiceSlot(uint8_t s)              { indiceSlot = s; }
    void setUmbralSuperior(int8_t v)           { umbralSuperior = v; }
    void setUmbralInferior(int8_t v)           { umbralInferior = v; }
    void setUmbrales(int8_t sup, int8_t inf)   { umbralSuperior = sup; umbralInferior = inf; }
    void setOffset(float v)                    { offset = v; }
    void setEstado(EstadoSensor e)             { estado = e; }
    void setTempCelsius(float t)               { tempCelsius = t; }
    void setTempRaw(int32_t r)                 { tempRaw = r; }
    void setResolucionBits(uint8_t b)          { resolucionBits = b; }
    void setAlimentacionParasita(bool p)       { alimentacionParasita = p; }
    void setEnAlarma(bool a)                   { enAlarma = a; }
    void setDatoUsuario(int16_t d)             { datoUsuario = d; }

    bool           estaActivo()             const { return activo; }
    uint8_t        obtenerSlot()            const { return indiceSlot; }
    const uint8_t* obtenerROM()             const { return rom; }
    uint8_t        obtenerFamilia()         const { return rom[0]; }
    const char*    obtenerMAC()             const { return mac; }
    const char*    obtenerMAC2()            const { return mac2; }
    int8_t         obtenerUmbralSuperior()  const { return umbralSuperior; }
    int8_t         obtenerUmbralInferior()  const { return umbralInferior; }
    float          obtenerOffset()          const { return offset; }
    EstadoSensor   obtenerEstado()          const { return estado; }
    float          obtenerTempCelsius()     const { return tempCelsius; }
    int32_t        obtenerTempRaw()         const { return tempRaw; }
    uint8_t        obtenerResolucionBits()  const { return resolucionBits; }
    bool           esAlimentacionParasita() const { return alimentacionParasita; }
    bool           estaEnAlarma()           const { return enAlarma; }
    int16_t        obtenerDatoUsuario()     const { return datoUsuario; }
    uint8_t*       obtenerScratchpad()            { return scratchpad; }
    const uint8_t* obtenerScratchpad()      const { return scratchpad; }
};