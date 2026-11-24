#pragma once
#include <math.h>

namespace CalibracionModelos {

    struct ParametrosRectaCalibracion {
        float pendienteM   = 1.0f;  // Ganancia (m)
        float ordenadaB    = 0.0f;  // Desplazamiento en 0 °C (b)
        bool  usaDosPuntos = false; // true: recta completa 2P | false: desplazamiento simple
    };

    // Modelo metrológico oficial (Opción A · Recta Lineal de 2 Puntos):
    //   m = (T_ref,sup - T_ref,inf) / (T_ebull - T_hielo)
    //   b = T_ref,inf - m * T_hielo
    //   T_cal = m * T_cruda + b
    // Si aún no existen 2 puntos válidos (|T_ebull - T_hielo| <= 1 °C), aplica m = 1, b = offsetFallback.
    class ModeloCalibracionDosPuntos {
    public:
        static constexpr float DELTA_MINIMO_C = 1.0f;

        static ParametrosRectaCalibracion calcular(
            float medicionHieloC,
            float medicionEbullicionC,
            float refCongelacionC,
            float refEbullicionC,
            float offsetFallbackC = 0.0f
        ) {
            ParametrosRectaCalibracion out{};
            const float deltaMedido = medicionEbullicionC - medicionHieloC;
            const float deltaRef    = refEbullicionC - refCongelacionC;

            if (isfinite(deltaMedido) && isfinite(deltaRef) &&
                fabsf(deltaMedido) > DELTA_MINIMO_C &&
                fabsf(deltaRef) > DELTA_MINIMO_C) {
                out.pendienteM   = deltaRef / deltaMedido;
                out.ordenadaB    = refCongelacionC - (out.pendienteM * medicionHieloC);
                out.usaDosPuntos = true;
            } else {
                out.pendienteM   = 1.0f;
                out.ordenadaB    = isfinite(offsetFallbackC) ? offsetFallbackC : 0.0f;
                out.usaDosPuntos = false;
            }
            return out;
        }

        static float aplicar(const ParametrosRectaCalibracion& p, float tempCrudaC) {
            return (p.pendienteM * tempCrudaC) + p.ordenadaB;
        }

        static float reconstruirCruda(const ParametrosRectaCalibracion& p, float tempCalibradaC) {
            if (fabsf(p.pendienteM) < 0.001f) return tempCalibradaC;
            return (tempCalibradaC - p.ordenadaB) / p.pendienteM;
        }
    };

} // namespace CalibracionModelos