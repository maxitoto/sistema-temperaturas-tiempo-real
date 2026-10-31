// led.cpp
#include "Led.hpp"
#include <Arduino.h>

Led::Led() {
    pinMode(Pin::PIN_LED, OUTPUT);
    digitalWrite(Pin::PIN_LED, LOW);
}

// Se definen SOLO los métodos que quedaron sin cuerpo en el .hpp
void Led::encenderLed() {
    state = true;
    digitalWrite(Pin::PIN_LED, HIGH);
}

void Led::apagarLed() {
    state = false;
    digitalWrite(Pin::PIN_LED, LOW);
}   

bool Led::ledEstaEncendido() {
    return state;
}