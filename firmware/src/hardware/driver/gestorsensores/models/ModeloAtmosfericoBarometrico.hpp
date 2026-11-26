#pragma once
#include <math.h>

namespace CalibracionModelos {

    struct ReferenciasTermicasGlobales {
        float alturaMetros        = 115.0f;
        float presionHpa          = 1013.25f;
        float tempCongelacionRefC = 0.0f;
        float tempEbullicionRefC  = 100.0f;
    };

    // Modelo físico atmosférico estándar (ISA + Clausius-Clapeyron)
    class ModeloAtmosfericoBarometrico {
    public:
        static constexpr float PRESION_NIVEL_MAR_HPA  = 1013.25f;
        static constexpr float TEMP_EBULLICION_STD_K  = 373.15f;   // 100.00 °C a 1013.25 hPa
        static constexpr float CERO_CELSIUS_K         = 273.15f;
        static constexpr float CONSTANTE_R_GAS        = 8.31446f;  // J/(mol·K)
        static constexpr float ENTALPIA_VAPOR_AGUA    = 40660.0f;  // J/mol a 100 °C
        static constexpr float GRADIENTE_ISA_INV_M    = 2.25577e-5f;
        static constexpr float EXPONENTE_BAROMETRICO  = 5.25588f;

        // Ecuación 1 (Atmósfera Estándar ISA):
        // P(h) = 1013.25 * (1 - 2.25577e-5 * h)^5.25588  [hPa]
        static float estimarPresionDesdeAltura(float alturaMetros) {
            if (!isfinite(alturaMetros)) alturaMetros = 0.0f;
            if (alturaMetros < -500.0f) alturaMetros = -500.0f;
            if (alturaMetros > 9000.0f) alturaMetros = 9000.0f;

            const float base = 1.0f - (GRADIENTE_ISA_INV_M * alturaMetros);
            const float p = PRESION_NIVEL_MAR_HPA * powf(base, EXPONENTE_BAROMETRICO);
            return fminf(fmaxf(p, 300.0f), 1100.0f);
        }

        // Ecuación 2 (Clausius-Clapeyron para punto de ebullición del agua):
        // 1/T_ebull(K) = 1/373.15 - (R / ΔH_vap) * ln(P / 1013.25)
        static float estimarEbullicionDesdePresion(float presionHpa) {
            if (!isfinite(presionHpa) || presionHpa <= 0.0f) {
                presionHpa = PRESION_NIVEL_MAR_HPA;
            }
            if (presionHpa < 300.0f)  presionHpa = 300.0f;
            if (presionHpa > 1100.0f) presionHpa = 1100.0f;

            const float invT = (1.0f / TEMP_EBULLICION_STD_K)
                             - ((CONSTANTE_R_GAS / ENTALPIA_VAPOR_AGUA) * logf(presionHpa / PRESION_NIVEL_MAR_HPA));
            return (1.0f / invT) - CERO_CELSIUS_K;
        }

        // Punto de fusión del agua pura a presión atmosférica estándar (0.00 °C)
        static constexpr float estimarCongelacionRef() {
            return 0.0f;
        }

        static ReferenciasTermicasGlobales calcularDesdeAltura(float alturaMetros) {
            ReferenciasTermicasGlobales ref{};
            ref.alturaMetros        = alturaMetros;
            ref.presionHpa          = estimarPresionDesdeAltura(alturaMetros);
            ref.tempCongelacionRefC = estimarCongelacionRef();
            ref.tempEbullicionRefC  = estimarEbullicionDesdePresion(ref.presionHpa);
            return ref;
        }
    };

} // namespace CalibracionModelos