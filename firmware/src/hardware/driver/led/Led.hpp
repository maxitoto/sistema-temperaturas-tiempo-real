#pragma once

#include <atomic>
#include "../../placa/esp32S/pines_nodemcu32s.hpp"

class Led {
private:
    std::atomic<bool> state{false};

    Led();
    ~Led() = default;

public:
    static Led& getInstance() {
        static Led instance;
        return instance;
    }

    Led(const Led&)            = delete;
    Led& operator=(const Led&) = delete;
    Led(Led&&)                 = delete;
    Led& operator=(Led&&)      = delete;

    void encenderLed();
    void apagarLed();
    bool ledEstaEncendido() const;
};

inline Led& led = Led::getInstance();