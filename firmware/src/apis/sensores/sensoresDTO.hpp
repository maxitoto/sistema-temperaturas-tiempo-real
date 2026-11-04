#pragma once
#include <stdint.h>

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

} // namespace Apis::Sensores