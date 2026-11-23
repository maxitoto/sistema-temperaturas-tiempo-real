#pragma once

#include <atomic>
#include <stdint.h>
#include "../../placa/esp32S/pines_nodemcu32s.hpp"

class Led {
private:
    std::atomic<bool>     state{false};
    std::atomic<bool>     pruebaActiva{false};
    std::atomic<uint32_t> finPruebaMs{0};

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
    void encenderPruebaMs(uint32_t duracionMs);
    bool enPruebaActiva();
    bool ledEstaEncendido() const;
};

inline Led& led = Led::getInstance();