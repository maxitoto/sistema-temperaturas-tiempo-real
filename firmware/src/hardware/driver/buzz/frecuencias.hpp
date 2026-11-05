#pragma once
#include "Buzz.hpp"
#include <cmath>

// ============================================================================
// 1. PATRONES DE ALARMA CONTINUA POR CRITICIDAD (duración = -1.0f)
// ============================================================================

// ALERTA CRÍTICA (Violación confirmada de temperatura alta/baja): Sirena rápida
inline float sirenaCritica(float t) {
    return 1000.0f + 400.0f * sinf(3.14159f * 2.0f * t);
}

// Alias retrocompatible
inline float sirena(float t) {
    return sirenaCritica(t);
}

// ALERTA MEDIA (Fallo técnico confirmado: sensor desconectado / CRC / bus):
// Dos pulsos cortos de 950 Hz cada 2 segundos y silencio el resto del ciclo
inline float alertaTecnicaMedia(float t) {
    float ciclo = fmodf(t, 2.0f);
    if (ciclo < 0.12f || (ciclo >= 0.24f && ciclo < 0.36f)) {
        return 950.0f;
    }
    return 0.0f; // Silencio en el resto del período
}

// ============================================================================
// 2. SONIDOS DE EVENTOS DEL SISTEMA (One-Shot no bloqueantes)
// ============================================================================

// Arranque del sistema (0.45 s): Arpegio ascendente Do6 -> Mi6 -> Sol6
inline float sonidoBoot(float t) {
    if (t < 0.14f) return 1046.5f;
    if (t < 0.28f) return 1318.5f;
    return 1568.0f;
}

// Cliente autenticado conectado (0.26 s): Doble tono ascendente amigable
inline float sonidoClienteConectado(float t) {
    return (t < 0.12f) ? 1174.7f : 1760.0f;
}

// Cliente desconectado (0.26 s): Doble tono descendente suave
inline float sonidoClienteDesconectado(float t) {
    return (t < 0.12f) ? 1480.0f : 987.8f;
}

// Sensor físico conectado en caliente (0.22 s): Chirp ascendente rápido
inline float sonidoSensorConectado(float t) {
    return (t < 0.10f) ? 1318.5f : 1975.5f;
}

// Sensor físico desconectado (0.30 s): Doble pulso grave de aviso técnico
inline float sonidoSensorDesconectado(float t) {
    if (t < 0.12f) return 784.0f;
    if (t < 0.17f) return 0.0f;
    return 587.3f;
}

// Confirmación de botón Mute presionado (0.10 s)
inline float sonidoMuteAck(float /*t*/) {
    return 1500.0f;
}

inline float barrido(float t) {
    return 200.0f + 933.0f * t;
}

inline float pitpit(float t) {
    return (static_cast<int>(t * 4) % 2) ? 1200.0f : 800.0f;
}