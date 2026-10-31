#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Led.hpp"
#include <iostream>

void setup() {
  Led& led = Led::getInstance();

  led.encenderLed();
  delay(1000);
  led.apagarLed();
  delay(1000);
  led.encenderLed();
  std::cout << led.ledEstaEncendido() << std::endl;
  delay(1000);
  led.apagarLed();
  std::cout << led.ledEstaEncendido() << std::endl;
  delay(1000);
  led.encenderLed();
  delay(1000);
  led.apagarLed();

  vTaskDelete(nullptr);
}

void loop() {
  // Inaccesible después de eliminar esta tarea.
}   