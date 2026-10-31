#pragma once

#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class Buzz {
private:

    bool state = false;
    float (*frec)(float) = nullptr;

    int hz = 0;
    float inicio = 0;
    float duracion = -1;

    Buzz();
    ~Buzz() = default;

    void _tick();
    static void _task(void* pvParameters);

public:
    static Buzz& getInstance() {
        static Buzz instance;
        return instance;
    }

    Buzz(const Buzz&) = delete;
    Buzz& operator=(const Buzz&) = delete;
    Buzz(Buzz&&) = delete;
    Buzz& operator=(Buzz&&) = delete;

    void silenciarBuzz();

    void sonarBuzz(int frecuencia, float duracion);
    void sonarBuzz(float (*frec)(float), float duracion);

    bool buzzEstaEncendido();
};