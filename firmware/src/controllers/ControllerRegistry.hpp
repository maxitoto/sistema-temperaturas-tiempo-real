#pragma once
#include "CommController.hpp"
#include "SensorDbController.hpp"
#include "AlarmController.hpp"
#include "OledController.hpp"

namespace Controllers {

inline void iniciarTodosLosControladores() {
    oledController.start();
    alarmController.start();
    sensorDbController.start();
    commController.start();
}

} // namespace Controllers