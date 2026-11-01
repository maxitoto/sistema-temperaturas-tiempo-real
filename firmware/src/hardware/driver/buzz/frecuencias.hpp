#include "Buzz.hpp"
#include <cmath>

// --- Funciones de frecuencia (float t → Hz) ---

// Sirena: oscila entre 600 y 1400 Hz, período 2 s
float sirena(float t) {
    return 1000.0f + 400.0f * sinf(3.14159f * t);
}

// Barrido ascendente: 200 → 3000 Hz en 3 s
float barrido(float t) {
    return 200.0f + 933.0f * t;  // 200 + (3000-200)/3 * t
}

// Pit-pit: alterna 800/1200 Hz cada 0.25 s
float pitpit(float t) {
    return (int)(t * 4) % 2 ? 1200.0f : 800.0f;
}