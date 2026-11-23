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
    pruebaActiva.store(false, std::memory_order_relaxed);
    state.store(false, std::memory_order_relaxed);
    digitalWrite(Pin::PIN_LED, LOW);
}

void Led::encenderPruebaMs(uint32_t duracionMs) {
    if (duracionMs < 100) duracionMs = 100;
    if (duracionMs > 15000) duracionMs = 15000;
    finPruebaMs.store(millis() + duracionMs, std::memory_order_relaxed);
    pruebaActiva.store(true, std::memory_order_relaxed);
    encenderLed();
}

bool Led::enPruebaActiva() {
    if (!pruebaActiva.load(std::memory_order_relaxed)) {
        return false;
    }
    const uint32_t fin = finPruebaMs.load(std::memory_order_relaxed);
    if (static_cast<int32_t>(millis() - fin) >= 0) {
        pruebaActiva.store(false, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool Led::ledEstaEncendido() const {
    return state.load(std::memory_order_relaxed);
}