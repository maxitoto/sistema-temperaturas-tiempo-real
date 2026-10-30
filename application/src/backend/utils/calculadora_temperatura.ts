interface WaterTemperatures {
  altitudeMeters: number;
  atmosphericPressureHpa: number;
  boilingTemperatureCelsius: number;
  freezingTemperatureCelsius: number;
}

/**
 * Estima la presión atmosférica según la altura.
 *
 * Modelo de atmósfera estándar.
 * Válido aproximadamente para alturas entre 0 y 11.000 metros.
 *
 * @param altitudeMeters Altura sobre el nivel del mar, en metros.
 * @returns Presión atmosférica estimada en hPa.
 */
function calculateAtmosphericPressure(
  altitudeMeters: number
): number {
  if (altitudeMeters < 0 || altitudeMeters > 11000) {
    throw new Error(
      "La altura debe estar entre 0 y 11.000 metros."
    );
  }

  const seaLevelPressure = 101325; // Pa
  const temperatureLapseRate = 0.0065; // K/m
  const seaLevelTemperature = 288.15; // K
  const gravity = 9.80665; // m/s²
  const molarMassAir = 0.0289644; // kg/mol
  const gasConstant = 8.3144598; // J/(mol·K)

  const exponent =
    (gravity * molarMassAir) /
    (gasConstant * temperatureLapseRate);

  const pressurePa =
    seaLevelPressure *
    Math.pow(
      1 -
        (temperatureLapseRate * altitudeMeters) /
          seaLevelTemperature,
      exponent
    );

  return pressurePa / 100; // Pa -> hPa
}

/**
 * Calcula la temperatura de ebullición del agua
 * mediante la ecuación de Antoine.
 *
 * Válida aproximadamente entre 1 y 100 °C
 * para agua y presiones cercanas a la atmosférica.
 *
 * @param pressureHpa Presión atmosférica en hPa.
 * @returns Temperatura de ebullición en °C.
 */
function calculateBoilingTemperature(
  pressureHpa: number
): number {
  if (pressureHpa <= 0) {
    throw new Error("La presión debe ser mayor que cero.");
  }

  // Conversión de hPa a mmHg.
  const pressureMmHg = pressureHpa / 1.3332239;

  // Constantes de Antoine para el agua.
  const A = 8.07131;
  const B = 1730.63;
  const C = 233.426;

  const temperatureCelsius =
    B / (A - Math.log10(pressureMmHg)) - C;

  return temperatureCelsius;
}

/**
 * Calcula las temperaturas características del agua.
 *
 * La congelación se aproxima a 0 °C para agua pura
 * a presiones atmosféricas habituales.
 */
function calculateWaterTemperatures(
  altitudeMeters: number
): WaterTemperatures {
  const atmosphericPressureHpa =
    calculateAtmosphericPressure(altitudeMeters);

  const boilingTemperatureCelsius =
    calculateBoilingTemperature(atmosphericPressureHpa);

  // Aproximación para agua pura a presión atmosférica.
  const freezingTemperatureCelsius = 0;

  return {
    altitudeMeters,
    atmosphericPressureHpa,
    boilingTemperatureCelsius,
    freezingTemperatureCelsius,
  };
}

// ==========================================
// EJEMPLO DE USO
// ==========================================

// Cambiá este valor por tu altura sobre el nivel del mar.
const altitudeMeters = 0;

const result = calculateWaterTemperatures(altitudeMeters);

console.log("=== TEMPERATURAS DEL AGUA ===");

console.log(
  `Altura: ${result.altitudeMeters} metros sobre el nivel del mar`
);

console.log(
  `Presión atmosférica estimada: ${result.atmosphericPressureHpa.toFixed(2)} hPa`
);

console.log(
  `Temperatura de ebullición: ${result.boilingTemperatureCelsius.toFixed(2)} °C`
);

console.log(
  `Temperatura de congelación: ${result.freezingTemperatureCelsius.toFixed(2)} °C`
);