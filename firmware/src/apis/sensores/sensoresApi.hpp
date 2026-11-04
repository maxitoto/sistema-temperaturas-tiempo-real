#pragma once
#include <Arduino.h>
#include "sensoresDTO.hpp"
#include "../../connections/CommunicationContainer.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../controllers/AlarmController.hpp"

namespace Apis::Sensores {

inline void registrarSensoresApi() {
    auto& comm = Connections::comm;

    // 1. Streaming de lecturas de temperatura (usa el sensor físico si está activo en el bus 1-Wire)
    comm.srv.bind_stream<SensorStreamRequestDto, TemperatureReadingDto>(
        "sensors.watch_temperature",
        [](auto& ctx) {
            SensorStreamRequestDto req = ctx.request();
            uint8_t count = (req.sampleCount == 0 || req.sampleCount > 10) ? 5 : req.sampleCount;
            uint8_t slot = (req.sensorId > 0 && req.sensorId <= GestorSensores::MAX_SENSORES)
                           ? (req.sensorId - 1) : 0;

            const Sensor& s = gestorSensores.obtenerSensor(slot);
            for (uint8_t i = 0; i < count; ++i) {
                float tempC = s.estaActivo() ? s.obtenerTempCelsius() : (22.5f + (i * 0.4f));
                ctx.emit(TemperatureReadingDto{
                    req.sensorId,
                    tempC,
                    millis()
                }, true);
            }
            ctx.close();
        }
    );

    // 2. Silenciar alarma activa por semáforo binario desde Electron
    comm.srv.bind("alarm.mute", []() -> bool {
        Controllers::alarmController.mutearAlarmas();
        return true;
    });
}

} // namespace Apis::Sensores