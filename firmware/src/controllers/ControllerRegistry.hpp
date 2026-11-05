#pragma once
#include "CommController.hpp"
#include "SensorController.hpp"
#include "AlarmController.hpp"
#include "OledController.hpp"

namespace Controllers {

inline void iniciarTodosLosControladores() {
    oledController.start();
    alarmController.start();
    sensorController.start();
    commController.start();
}

} // namespace Controllers