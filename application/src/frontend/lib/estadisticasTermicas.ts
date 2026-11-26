import { redondearDecimal } from "./format";

export interface MuestraTermica {
  timestamp: number;
  valor: number;
}

export interface ResumenEstadisticoSerie {
  n: number;
  min: number;
  max: number;
  rango: number;
  promedio: number;
  mediana: number;
  p05: number;
  p95: number;
  /** Desviación estándar muestral insesgada s (divisor n - 1, corrección de Bessel) */
  desviacionStd: number;
  /** Pendiente β1 de regresión lineal MCO en °C/hora */
  pendienteCporHora: number;
  /** Ordenada al origen β0 en t = t0 */
  tendenciaIntercepto: number;
  /** Coeficiente de determinación R² ∈ [0, 1] del ajuste lineal */
  rCuadrado: number;
  cumplimientoPct: number | null;
  sobreTh: number;
  bajoTl: number;
  t0Seg: number;
}

function normalizarEpochSeg(ts: number): number {
  if (!Number.isFinite(ts) || ts <= 0) return 0;
  return ts >= 10_000_000_000 ? ts / 1000 : ts;
}

function percentilOrdenado(ordenados: number[], p: number): number {
  if (ordenados.length === 0) return 0;
  if (ordenados.length === 1) return ordenados[0];
  const idx = (ordenados.length - 1) * p;
  const lo = Math.floor(idx);
  const hi = Math.ceil(idx);
  if (lo === hi) return ordenados[lo];
  return ordenados[lo] + (ordenados[hi] - ordenados[lo]) * (idx - lo);
}

export function calcularEstadisticasSerie(
  muestras: MuestraTermica[],
  umbralInf?: number,
  umbralSup?: number
): ResumenEstadisticoSerie | null {
  const validas = muestras.filter((m) => Number.isFinite(m.valor));
  const n = validas.length;
  if (n === 0) return null;

  const valores = validas.map((m) => m.valor);
  const ordenados = [...valores].sort((a, b) => a - b);

  const min = ordenados[0];
  const max = ordenados[n - 1];
  const promedio = valores.reduce((acc, v) => acc + v, 0) / n;

  // Suma de cuadrados totales (SST) y desviación estándar muestral insesgada (n - 1)
  const sst = valores.reduce((acc, v) => acc + (v - promedio) ** 2, 0);
  const desviacionStd = n > 1 ? Math.sqrt(sst / (n - 1)) : 0;

  // Regresión lineal por Mínimos Cuadrados Ordinarios (MCO) respecto a horas desde t0
  const tiemposSeg = validas.map((m) => normalizarEpochSeg(m.timestamp));
  const t0Seg = tiemposSeg[0];
  const xsHoras = tiemposSeg.map((t, idx) =>
    t > 0 && t0Seg > 0 ? (t - t0Seg) / 3600 : idx / 60
  );
  const spanHoras = xsHoras[n - 1] - xsHoras[0];

  let pendienteCporHora = 0;
  let tendenciaIntercepto = promedio;
  let rCuadrado = 0;

  if (n >= 2 && spanHoras > 5 / 3600) {
    const meanX = xsHoras.reduce((a, b) => a + b, 0) / n;
    let sxy = 0;
    let sxx = 0;
    for (let i = 0; i < n; i++) {
      const dx = xsHoras[i] - meanX;
      sxy += dx * (valores[i] - promedio);
      sxx += dx * dx;
    }
    if (sxx > 1e-12) {
      pendienteCporHora = sxy / sxx;
      tendenciaIntercepto = promedio - pendienteCporHora * meanX;
      if (sst > 1e-12) {
        let sse = 0;
        for (let i = 0; i < n; i++) {
          const yHat = tendenciaIntercepto + pendienteCporHora * xsHoras[i];
          sse += (valores[i] - yHat) ** 2;
        }
        rCuadrado = Math.max(0, Math.min(1, 1 - sse / sst));
      }
    }
  }

  let sobreTh = 0;
  let bajoTl = 0;
  let cumplimientoPct: number | null = null;
  if (
    umbralInf !== undefined &&
    umbralSup !== undefined &&
    Number.isFinite(umbralInf) &&
    Number.isFinite(umbralSup)
  ) {
    for (const v of valores) {
      if (v > umbralSup) sobreTh++;
      else if (v < umbralInf) bajoTl++;
    }
    cumplimientoPct = redondearDecimal(((n - sobreTh - bajoTl) / n) * 100, 1);
  }

  return {
    n,
    min: redondearDecimal(min, 2),
    max: redondearDecimal(max, 2),
    rango: redondearDecimal(max - min, 2),
    promedio: redondearDecimal(promedio, 2),
    mediana: redondearDecimal(percentilOrdenado(ordenados, 0.5), 2),
    p05: redondearDecimal(percentilOrdenado(ordenados, 0.05), 2),
    p95: redondearDecimal(percentilOrdenado(ordenados, 0.95), 2),
    desviacionStd: redondearDecimal(desviacionStd, 2),
    pendienteCporHora: redondearDecimal(pendienteCporHora, 2),
    tendenciaIntercepto,
    rCuadrado: redondearDecimal(rCuadrado, 3),
    cumplimientoPct,
    sobreTh,
    bajoTl,
    t0Seg,
  };
}

export function calcularMediaMovil(
  valores: number[],
  ventana: number
): number[] {
  const w = Math.max(2, Math.round(ventana));
  return valores.map((_, idx) => {
    const inicio = Math.max(0, idx - w + 1);
    let suma = 0;
    for (let j = inicio; j <= idx; j++) suma += valores[j];
    return redondearDecimal(suma / (idx - inicio + 1), 2);
  });
}

export function evaluarTendenciaEn(
  stats: ResumenEstadisticoSerie,
  timestamp: number,
  indexFallback: number
): number {
  const tSeg = normalizarEpochSeg(timestamp);
  const xHoras =
    tSeg > 0 && stats.t0Seg > 0 ? (tSeg - stats.t0Seg) / 3600 : indexFallback / 60;
  return redondearDecimal(
    stats.tendenciaIntercepto + stats.pendienteCporHora * xHoras,
    2
  );
}