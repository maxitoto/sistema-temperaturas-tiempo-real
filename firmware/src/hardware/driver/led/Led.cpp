#include "Led.hpp"
#include <Arduino.h>

Led::Led() {
    pinMode(Pin::PIN_LED, OUTPUT);
    digitalWrite(Pin::PIN_LED, LOW);
}

void Led::encenderLed() {
    state.store(true, std::memory_order_relaxed);
    digitalWrite(Pin::PIN_LED, HIGH);
}

void Led::apagarLed() {
    state.store(false, std::memory_order_relaxed);
    digitalWrite(Pin::PIN_LED, LOW);
}

bool Led::ledEstaEncendido() const {
    return state.load(std::memory_order_relaxed);
}