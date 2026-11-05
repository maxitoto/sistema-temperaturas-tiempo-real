#pragma once
#include <Arduino.h>
#include "sensoresDTO.hpp"
#include "../../connections/CommunicationContainer.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../controllers/AlarmController.hpp"
#include "../../database/orm/ORM.hpp"
#include "../../rtos/SystemBus.hpp"

namespace Apis::Sensores {

inline void registrarSensoresApi() {
    auto& comm = Connections::comm;

    // 1. Streaming de lecturas de temperatura
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

    // 2. Obtener información completa de un sensor (combinando RAM + DB)
    comm.srv.bind("sensors.get_info", [](uint8_t sensorId) -> SensorInfoDto {
        uint8_t slot = (sensorId > 0 && sensorId <= GestorSensores::MAX_SENSORES)
                       ? (sensorId - 1) : 0;
        const Sensor& s = gestorSensores.obtenerSensor(slot);

        SensorInfoDto dto{
            static_cast<int32_t>(slot + 1),
            s.obtenerMAC(),
            s.obtenerMAC2(),
            "Sensor " + std::to_string(slot + 1),
            "",
            "Heladera",
            s.obtenerOffset(),
            s.obtenerUmbralSuperior(),
            s.obtenerUmbralInferior(),
            s.obtenerTempCelsius(),
            s.obtenerResolucionBits(),
            s.esAlimentacionParasita(),
            s.estaActivo(),
            static_cast<int16_t>(s.obtenerEstado()),
            0
        };

        if (s.estaActivo()) {
            auto lock = Rtos::sysBus.lockDb(2000);
            if (lock.isLocked()) {
                auto filas = db.from("Sensor").select("*").eq("hexadecimal", s.obtenerMAC()).execute();
                if (!filas.empty()) {
                    const auto& f = filas[0];
                    dto.sensorId          = f.at("id_sensor").toInt();
                    dto.nombre            = f.at("nombre").c_str();
                    dto.descripcion       = f.at("descripcion").c_str();
                    dto.ubicacion         = f.at("ubicacion").c_str();
                    dto.fechaModificacion = static_cast<uint32_t>(f.at("fecha_modificacion").toInt());
                }
            }
        }
        return dto;
    });

    // 3. Configurar umbrales en punto flotante (actualiza BD y GestorSensores en RAM)
    comm.srv.bind("sensors.set_umbrales", [](uint8_t sensorId, float umbralSup, float umbralInf) -> bool {
        if (sensorId == 0 || sensorId > GestorSensores::MAX_SENSORES) return false;
        if (umbralInf >= umbralSup) return false;

        uint8_t slot = sensorId - 1;
        const Sensor& s = gestorSensores.obtenerSensor(slot);
        if (!s.estaActivo()) return false;

        auto lock = Rtos::sysBus.lockDb(3000);
        if (!lock.isLocked()) return false;

        db.setSession("system_admin", ROL_ADMIN);
        bool okDb = db.from("Sensor")
                      .eq("hexadecimal", s.obtenerMAC())
                      .update({
                          {"umbral_superior",    String(umbralSup, 2)},
                          {"umbral_inferior",    String(umbralInf, 2)},
                          {"fecha_modificacion", String(millis() / 1000)}
                      });

        String prevVal = String(s.obtenerUmbralInferior(), 2) + ".." + String(s.obtenerUmbralSuperior(), 2);
        String nuevoVal = String(umbralInf, 2) + ".." + String(umbralSup, 2);

        if (okDb) {
            gestorSensores.configurarAlarmasSlot(slot, umbralSup, umbralInf, false);
            Controllers::alarmController.notificarEvaluacionSensores();
        }

        lock.release();
        Audit::registrar(
            "SENSOR_SET_UMBRALES", "Sensor",
            prevVal, nuevoVal,
            String("Cambio de umbrales en sonda ") + s.obtenerMAC(),
            CriticidadEvento::CRITICIDAD_MEDIA,
            okDb ? ResultadoAuditoria::RESULTADO_EXITO : ResultadoAuditoria::RESULTADO_ERROR_VALIDACION,
            Audit::detectarMedioActivo(), "RPC"
        );
        return okDb;
    });

    // 4. Silenciar alarma activa por semáforo binario desde Electron
    comm.srv.bind("alarm.mute", []() -> bool {
        Controllers::alarmController.mutearAlarmas(Audit::detectarMedioActivo());
        return true;
    });

    comm.srv.bind("sensors.get_calibracion", [](uint8_t sensorId) -> SensorCalibracionDto {
        uint8_t slot = (sensorId > 0 && sensorId <= GestorSensores::MAX_SENSORES) ? (sensorId - 1) : 0;
        const Sensor& s = gestorSensores.obtenerSensor(slot);
        return SensorCalibracionDto{ static_cast<int32_t>(slot + 1), s.obtenerOffset(),
                                     s.obtenerCorreccionSuperior(), s.obtenerCorreccionInferior() };
    });

    comm.srv.bind("sensors.set_calibracion",
                  [](uint8_t sensorId, float offset, float corrSup, float corrInf) -> bool {
        if (sensorId == 0 || sensorId > GestorSensores::MAX_SENSORES) return false;
        auto fuera = [](float v) { return v < -10.0f || v > 10.0f; };   // ajusta el rango si hace falta
        if (fuera(offset) || fuera(corrSup) || fuera(corrInf)) return false;

        uint8_t slot = sensorId - 1;
        Sensor& s = gestorSensores.obtenerSensor(slot);
        if (!s.estaActivo()) return false;

        auto lock = Rtos::sysBus.lockDb(3000);
        if (!lock.isLocked()) return false;

        db.setSession("system_admin", ROL_ADMIN);
        bool okDb = db.from("Sensor")
                      .eq("hexadecimal", s.obtenerMAC())
                      .update({
                          {"offset_calibracion", String(offset, 2)},
                          {"correcion_superior", String(corrSup, 2)},
                          {"correcion_inferior", String(corrInf, 2)},
                          {"fecha_modificacion", String(millis() / 1000)}
                      });

        String prevVal  = String(s.obtenerOffset(), 2) + "/" + String(s.obtenerCorreccionSuperior(), 2)
                          + "/" + String(s.obtenerCorreccionInferior(), 2);
        String nuevoVal = String(offset, 2) + "/" + String(corrSup, 2) + "/" + String(corrInf, 2);

        if (okDb) {
            gestorSensores.configurarOffsetSlot(slot, offset);
            s.setCorrecciones(corrSup, corrInf);
        }
        Audit::registrar(
            "SENSOR_SET_CALIBRACION", "Sensor", prevVal, nuevoVal,
            String("Cambio de calibración en sonda ") + s.obtenerMAC(),
            CriticidadEvento::CRITICIDAD_MEDIA,
            okDb ? ResultadoAuditoria::RESULTADO_EXITO : ResultadoAuditoria::RESULTADO_ERROR_VALIDACION,
            Audit::detectarMedioActivo(), "RPC"
        );
        return okDb;
    });
}

} // namespace Apis::Sensores