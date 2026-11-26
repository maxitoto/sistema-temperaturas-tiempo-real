#pragma once

#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class Led {
private:
    bool state = false;

    Led();
    ~Led() = default;

public:
    // Definido inline en el header (cuerpo aquí) → NO se repite en .cpp
    static Led& getInstance() {
        static Led instance;  // thread-safe (C++11+)
        return instance;
    }

    // anulamos los constructores y operadores transitivos
    Led(const Led&)            = delete;
    Led& operator=(const Led&) = delete;
    Led(Led&&)                 = delete;
    Led& operator=(Led&&)      = delete;

    // Solo se DECLARAN aquí (sin cuerpo) → se definen en .cpp
    void encenderLed();
    void apagarLed();
    bool ledEstaEncendido();
};   