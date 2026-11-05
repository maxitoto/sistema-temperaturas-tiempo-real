#pragma once
#include <stdint.h>
#include <string>
#include "../../hardware/driver/gestorsensores/EstadoSensor.hpp"

namespace Apis::Sensores {

struct SensorStreamRequestDto {
    uint8_t sensorId;
    uint8_t sampleCount;
};

struct TemperatureReadingDto {
    uint8_t sensorId;
    float celsius;
    uint32_t timestampMs;
};

struct SetUmbralesDto {
    uint8_t sensorId;
    float umbralSuperior;
    float umbralInferior;
};

// Exactamente 15 campos (máximo soportado por Detail::toTuple en RpcDto.hpp)
struct SensorInfoDto {
    int32_t     sensorId;
    std::string hexadecimal;
    std::string mac2;
    std::string nombre;
    std::string descripcion;
    std::string ubicacion;
    float       offsetCalibracion;
    float       umbralSuperior;
    float       umbralInferior;
    float       tempActualC;
    uint8_t     resolucionBits;
    bool        alimentacionParasita;
    bool        activo;
    int16_t     estado;
    uint32_t    fechaModificacion;
};

struct SensorCalibracionDto {
    int32_t sensorId;
    float   offsetCalibracion;
    float   correccionSuperior;
    float   correccionInferior;
};

} // namespace Apis::Sensores