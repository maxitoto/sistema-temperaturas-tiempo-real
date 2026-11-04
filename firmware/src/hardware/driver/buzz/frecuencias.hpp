#pragma once
#include "Buzz.hpp"
#include <cmath>

inline float sirena(float t) {
    return 1000.0f + 400.0f * sinf(3.14159f * t);
}

inline float barrido(float t) {
    return 200.0f + 933.0f * t;
}

inline float pitpit(float t) {
    return (static_cast<int>(t * 4) % 2) ? 1200.0f : 800.0f;
}