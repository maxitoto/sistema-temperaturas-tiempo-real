import { useMemo, useState } from "react";
import {
  ResponsiveContainer,
  ComposedChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  ReferenceLine,
  ReferenceArea,
  Brush,
} from "recharts";
import {
  TrendingUp,
  TrendingDown,
  Minus,
  Loader2,
  HelpCircle,
} from "lucide-react";
import { formatearEpochSegundos } from "../../lib/format";
import {
  calcularEstadisticasSerie,
  calcularMediaMovil,
  evaluarTendenciaEn,
} from "../../lib/estadisticasTermicas";
import { FormulaTooltip } from "../ui/FormulaTooltip";
import type { LecturaSensorCliente } from "#types/rpc.types";

interface PuntoAnalitico {
  hora: string;
  horaCorta: string;
  temp: number;
  sma: number;
  tendencia: number;
  estadoZona: "OK" | "ALTA" | "BAJA";
}

function BotonAyuda({
  titulo,
  ecuacion,
  sustitucion,
  descripcion,
}: {
  titulo: string;
  ecuacion: string;
  sustitucion?: string;
  descripcion: string;
}) {
  return (
    <FormulaTooltip
      titulo={titulo}
      ecuacion={ecuacion}
      sustitucion={sustitucion}
      descripcion={descripcion}
    >
      <span className="inline-flex h-4 w-4 items-center justify-center rounded-full border border-info/50 bg-info-light text-info transition-colors hover:bg-info hover:text-info-foreground">
        <HelpCircle className="h-3 w-3" />
      </span>
    </FormulaTooltip>
  );
}

export function SensorAnalyticsChart({
  serie,
  umbralInf,
  umbralSup,
  cargando = false,
}: {
  serie: LecturaSensorCliente[];
  umbralInf?: number;
  umbralSup?: number;
  cargando?: boolean;
}) {
  const [verUmbrales, setVerUmbrales] = useState(true);
  const [verPromedio, setVerPromedio] = useState(true);
  const [verBandaSigma, setVerBandaSigma] = useState(false);
  const [verSma, setVerSma] = useState(false);
  const [verTendencia, setVerTendencia] = useState(false);
  const [ventanaSma, setVentanaSma] = useState(5);
  const [escalaIncluyeUmbrales, setEscalaIncluyeUmbrales] = useState(false);

  const stats = useMemo(
    () =>
      calcularEstadisticasSerie(
        serie.map((s) => ({
          timestamp: s.timestampMs,
          valor: s.temperaturaCorregida,
        })),
        umbralInf,
        umbralSup
      ),
    [serie, umbralInf, umbralSup]
  );

  const puntos = useMemo<PuntoAnalitico[]>(() => {
    if (!stats || serie.length === 0) return [];
    const valores = serie.map((s) => Number(s.temperaturaCorregida.toFixed(2)));
    const smas = calcularMediaMovil(valores, ventanaSma);

    return serie.map((item, idx) => {
      const temp = valores[idx];
      const fechaCompleta = formatearEpochSegundos(item.timestampMs);
      const estadoZona =
        umbralSup !== undefined && temp > umbralSup
          ? "ALTA"
          : umbralInf !== undefined && temp < umbralInf
            ? "BAJA"
            : "OK";
      return {
        hora: fechaCompleta,
        horaCorta: fechaCompleta.slice(-8),
        temp,
        sma: smas[idx],
        tendencia: evaluarTendenciaEn(stats, item.timestampMs, idx),
        estadoZona,
      };
    });
  }, [serie, stats, ventanaSma, umbralInf, umbralSup]);

  // Si la historia supera 600 puntos, reduce solo el trazado SVG preservando
  // mínimos, máximos y lecturas fuera de umbral; la estadística usa el 100% real.
  const MAX_PUNTOS_SVG = 600;
  const { puntosRender, trazadoOptimizado } = useMemo(() => {
    if (puntos.length <= MAX_PUNTOS_SVG) {
      return { puntosRender: puntos, trazadoOptimizado: false };
    }

    const cubetas = Math.floor(MAX_PUNTOS_SVG / 2);
    const tamCubeta = puntos.length / cubetas;
    const reducidos: PuntoAnalitico[] = [];

    for (let b = 0; b < cubetas; b++) {
      const ini = Math.floor(b * tamCubeta);
      const fin = Math.min(puntos.length, Math.floor((b + 1) * tamCubeta));
      if (ini >= fin) continue;

      let idxMin = ini;
      let idxMax = ini;
      for (let i = ini + 1; i < fin; i++) {
        if (puntos[i].temp < puntos[idxMin].temp) idxMin = i;
        if (puntos[i].temp > puntos[idxMax].temp) idxMax = i;
      }

      if (idxMin === idxMax) {
        reducidos.push(puntos[idxMin]);
      } else if (idxMin < idxMax) {
        reducidos.push(puntos[idxMin], puntos[idxMax]);
      } else {
        reducidos.push(puntos[idxMax], puntos[idxMin]);
      }
    }

    return { puntosRender: reducidos, trazadoOptimizado: true };
  }, [puntos]);

  const dominioY = useMemo<[number | "auto", number | "auto"]>(() => {
    if (!stats) return ["auto", "auto"];
    let lo = stats.min;
    let hi = stats.max;
    if (verBandaSigma) {
      lo = Math.min(lo, stats.promedio - stats.desviacionStd);
      hi = Math.max(hi, stats.promedio + stats.desviacionStd);
    }
    if (
      escalaIncluyeUmbrales &&
      umbralInf !== undefined &&
      umbralSup !== undefined
    ) {
      lo = Math.min(lo, umbralInf);
      hi = Math.max(hi, umbralSup);
    }
    const pad = Math.max(0.5, (hi - lo) * 0.14);
    return [Number((lo - pad).toFixed(2)), Number((hi + pad).toFixed(2))];
  }, [stats, verBandaSigma, escalaIncluyeUmbrales, umbralInf, umbralSup]);

  const mostrarUmbralesValidos =
    verUmbrales &&
    umbralInf !== undefined &&
    umbralSup !== undefined &&
    umbralInf <= umbralSup;

  return (
    <div className="space-y-2.5">
      {/* Barra de capas matemáticas del gráfico con ayuda '?' en cada elemento */}
      <div className="flex flex-wrap items-center gap-x-3 gap-y-1.5 rounded-lg border border-border bg-background/70 px-3 py-2 text-[11px]">
        <span className="font-semibold text-foreground-secondary">
          Capas del modelo:
        </span>

        <span className="inline-flex items-center gap-1">
          <label className="flex cursor-pointer items-center gap-1">
            <input
              type="checkbox"
              checked={verUmbrales}
              onChange={(e) => setVerUmbrales(e.target.checked)}
            />
            TL / TH
          </label>
          <BotonAyuda
            titulo="Umbrales Operativos (TL / TH)"
            ecuacion={"TL ≤ T(t) ≤ TH"}
            sustitucion={
              umbralInf !== undefined && umbralSup !== undefined
                ? `Rango seguro configurado: [${umbralInf.toFixed(1)} °C, ${umbralSup.toFixed(1)} °C]`
                : undefined
            }
            descripcion="Dibuja la banda verde permitida y las rectas límite inferior (TL) y superior (TH) que disparan las alarmas."
          />
        </span>

        <span className="inline-flex items-center gap-1">
          <label className="flex cursor-pointer items-center gap-1">
            <input
              type="checkbox"
              checked={verPromedio}
              onChange={(e) => setVerPromedio(e.target.checked)}
            />
            Media (x̄)
          </label>
          <BotonAyuda
            titulo="Media Aritmética de la Ventana"
            ecuacion={"x̄ = (1 / n) · Σ Tᵢ"}
            sustitucion={
              stats
                ? `Con n = ${stats.n} lecturas visibles → x̄ = ${stats.promedio.toFixed(2)} °C`
                : undefined
            }
            descripcion="Recta horizontal con el promedio calculado solo sobre las n lecturas cargadas en el gráfico (puede diferir del promedio histórico total en BD)."
          />
        </span>

        <span className="inline-flex items-center gap-1">
          <label className="flex cursor-pointer items-center gap-1">
            <input
              type="checkbox"
              checked={verBandaSigma}
              onChange={(e) => setVerBandaSigma(e.target.checked)}
            />
            Banda x̄ ± s
          </label>
          <BotonAyuda
            titulo="Banda de Dispersión Muestral (x̄ ± s)"
            ecuacion={"s = √[ (1 / (n − 1)) · Σ (Tᵢ − x̄)² ]\nBanda = [x̄ − s , x̄ + s]"}
            sustitucion={
              stats
                ? `[${(stats.promedio - stats.desviacionStd).toFixed(2)} °C , ${(stats.promedio + stats.desviacionStd).toFixed(2)} °C] (s = ${stats.desviacionStd.toFixed(2)} °C)`
                : undefined
            }
            descripcion="Sombrea una desviación estándar muestral insesgada (corrección de Bessel) alrededor de la media para visualizar la estabilidad térmica."
          />
        </span>

        <span className="inline-flex items-center gap-1">
          <label className="flex cursor-pointer items-center gap-1">
            <input
              type="checkbox"
              checked={verSma}
              onChange={(e) => setVerSma(e.target.checked)}
            />
            Media Móvil
          </label>
          {verSma && (
            <select
              value={ventanaSma}
              onChange={(e) => setVentanaSma(Number(e.target.value))}
              className="rounded border border-border bg-card px-1.5 py-0.5 font-mono text-[10px]"
              title="Cantidad de muestras promediadas (k)"
            >
              <option value={3}>k=3</option>
              <option value={5}>k=5</option>
              <option value={9}>k=9</option>
            </select>
          )}
          <BotonAyuda
            titulo="Media Móvil Simple (Filtro Suavizador)"
            ecuacion={"SMAₖ(i) = (1 / k) · Σⱼ₌₀ᵏ⁻¹ Tᵢ₋ⱼ"}
            sustitucion={`Ventana actual: k = ${ventanaSma} muestras consecutivas`}
            descripcion="Filtro pasa-bajos discreto que promedia cada lectura con las k−1 anteriores para atenuar el ruido de cuantización del DS18B20 y ver la dinámica térmica real."
          />
        </span>

        <span className="inline-flex items-center gap-1">
          <label className="flex cursor-pointer items-center gap-1">
            <input
              type="checkbox"
              checked={verTendencia}
              onChange={(e) => setVerTendencia(e.target.checked)}
            />
            Tendencia (MCO)
          </label>
          <BotonAyuda
            titulo="Regresión Lineal por Mínimos Cuadrados (MCO)"
            ecuacion={
              "T̂(t) = β₀ + β₁ · t\nβ₁ = Σ(tᵢ − t̄)(Tᵢ − x̄) / Σ(tᵢ − t̄)²\nR² = 1 − Σ(Tᵢ − T̂ᵢ)² / Σ(Tᵢ − x̄)²"
            }
            sustitucion={
              stats
                ? `β₁ = ${stats.pendienteCporHora >= 0 ? "+" : ""}${stats.pendienteCporHora.toFixed(2)} °C/h · R² = ${stats.rCuadrado.toFixed(3)}`
                : undefined
            }
            descripcion="Ajusta la recta que minimiza el error cuadrático respecto al tiempo real transcurrido (en horas). Un R² cercano a 1 indica tendencia lineal sostenida; cercano a 0 indica estabilidad o ruido."
          />
        </span>

        <label
          className="ml-auto flex cursor-pointer items-center gap-1 text-foreground-secondary hover:text-foreground"
          title="Amplía el eje Y para incluir siempre TL y TH en pantalla"
        >
          <input
            type="checkbox"
            checked={escalaIncluyeUmbrales}
            onChange={(e) => setEscalaIncluyeUmbrales(e.target.checked)}
          />
          Encuadrar TL/TH
        </label>
      </div>

      {/* Cinta de KPIs estadísticos (explícitamente sobre la ventana visible n) */}
      {stats && (
        <div className="space-y-1">
          <div className="flex items-center justify-between px-0.5 text-[10px] text-foreground-secondary">
            <span>
              Estadísticos exactos sobre <strong>todas las lecturas cargadas</strong>{" "}
              (<code>n = {stats.n.toLocaleString("es-AR")}</code>)
              {trazadoOptimizado && (
                <span className="ml-1 text-info">
                  · trazado optimizado preservando picos min/max
                </span>
              )}
            </span>
            <span className="font-mono">
              Mín {stats.min.toFixed(2)}° · Máx {stats.max.toFixed(2)}°
            </span>
          </div>

          <div className="grid grid-cols-2 gap-2 text-[11px] sm:grid-cols-4">
            <div className="rounded-md border border-border bg-background px-2.5 py-1.5">
              <div className="flex items-center justify-between text-[10px] text-foreground-secondary">
                <span>Media ± Desv. (x̄ ± s)</span>
                <BotonAyuda
                  titulo="Media y Desviación Estándar Muestral"
                  ecuacion={"x̄ = (1/n) Σ Tᵢ\ns = √[ Σ(Tᵢ − x̄)² / (n − 1) ]"}
                  sustitucion={`n=${stats.n} → x̄=${stats.promedio.toFixed(2)} °C, s=${stats.desviacionStd.toFixed(2)} °C`}
                  descripcion="Resume el centro y la dispersión de las temperaturas dentro del período graficado usando estimador insesgado (n−1)."
                />
              </div>
              <div className="font-mono font-bold text-primary">
                {stats.promedio.toFixed(2)} ± {stats.desviacionStd.toFixed(2)} °C
              </div>
            </div>

            <div className="rounded-md border border-border bg-background px-2.5 py-1.5">
              <div className="flex items-center justify-between text-[10px] text-foreground-secondary">
                <span>Mediana · P05–P95</span>
                <BotonAyuda
                  titulo="Estadísticos de Orden (Robustos)"
                  ecuacion={"Mediana = Q₂ (P₅₀)\nIntervalo 90% central = [P₀₅ , P₉₅]"}
                  sustitucion={`P₅₀ = ${stats.mediana.toFixed(2)} °C · 90% de lecturas entre ${stats.p05.toFixed(2)} y ${stats.p95.toFixed(2)} °C`}
                  descripcion="A diferencia de la media, la mediana y los percentiles P05/P95 no se distorsionan por picos aislados o lecturas atípicas."
                />
              </div>
              <div className="font-mono font-semibold">
                {stats.mediana.toFixed(2)}°{" "}
                <span className="text-foreground-secondary">
                  ({stats.p05.toFixed(1)}..{stats.p95.toFixed(1)})
                </span>
              </div>
            </div>

            <div className="rounded-md border border-border bg-background px-2.5 py-1.5">
              <div className="flex items-center justify-between text-[10px] text-foreground-secondary">
                <span>Pendiente MCO · R²</span>
                <BotonAyuda
                  titulo="Tasa de Cambio Térmico y Bondad de Ajuste"
                  ecuacion={"β₁ = dT̂/dt (°C/h)\nR² = 1 − SSE / SST"}
                  sustitucion={`β₁ = ${stats.pendienteCporHora >= 0 ? "+" : ""}${stats.pendienteCporHora.toFixed(2)} °C/h · R² = ${stats.rCuadrado.toFixed(3)} · Δ = ${stats.rango.toFixed(2)} °C`}
                  descripcion="Indica cuántos °C sube o baja el sistema por hora según el ajuste lineal y qué fracción de la varianza explica esa recta (R²)."
                />
              </div>
              <div className="inline-flex items-center gap-1 font-mono font-semibold">
                {stats.pendienteCporHora > 0.05 ? (
                  <TrendingUp className="h-3 w-3 text-error" />
                ) : stats.pendienteCporHora < -0.05 ? (
                  <TrendingDown className="h-3 w-3 text-info" />
                ) : (
                  <Minus className="h-3 w-3 text-foreground-secondary" />
                )}
                <span>
                  {stats.pendienteCporHora >= 0 ? "+" : ""}
                  {stats.pendienteCporHora.toFixed(2)} °C/h
                </span>
                <span className="text-foreground-secondary">
                  (R² {stats.rCuadrado.toFixed(2)})
                </span>
              </div>
            </div>

            <div className="rounded-md border border-border bg-background px-2.5 py-1.5">
              <div className="flex items-center justify-between text-[10px] text-foreground-secondary">
                <span>Cumplimiento [TL, TH]</span>
                <BotonAyuda
                  titulo="Tasa de Cumplimiento Térmico"
                  ecuacion={"C% = 100 · (n_dentro / n)"}
                  sustitucion={
                    stats.cumplimientoPct !== null
                      ? `${stats.n - stats.sobreTh - stats.bajoTl}/${stats.n} en rango (${stats.sobreTh} sobre TH, ${stats.bajoTl} bajo TL)`
                      : "Sin umbrales configurados"
                  }
                  descripcion="Porcentaje de muestras de la ventana analizada que permanecieron dentro del intervalo seguro [TL, TH]."
                />
              </div>
              <div className="font-mono font-bold">
                {stats.cumplimientoPct !== null ? (
                  <span
                    className={
                      stats.cumplimientoPct < 95
                        ? "text-alert-err-icon"
                        : "text-success"
                    }
                  >
                    {stats.cumplimientoPct.toFixed(1)}%
                    {stats.sobreTh + stats.bajoTl > 0 && (
                      <span className="ml-1 font-normal text-foreground-secondary">
                        ({stats.sobreTh}↑ / {stats.bajoTl}↓)
                      </span>
                    )}
                  </span>
                ) : (
                  "—"
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Lienzo del gráfico: altura y márgenes corregidos para XAxis y Brush */}
      <div className="relative h-80 w-full rounded-lg border border-border bg-background px-2 pt-2 pb-3">
        {cargando && (
          <div className="absolute inset-0 z-10 flex items-center justify-center gap-2 rounded-lg bg-background/70 text-xs backdrop-blur-[1px]">
            <Loader2 className="h-4 w-4 animate-spin text-primary" />
            Consultando historial en MicroSD...
          </div>
        )}

        {puntos.length === 0 ? (
          <div className="flex h-full items-center justify-center text-xs text-foreground-secondary">
            Sin lecturas históricas en el rango consultado.
          </div>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            <ComposedChart
              data={puntosRender}
              margin={{ top: 10, right: 20, left: 4, bottom: 20 }}
            >
              <CartesianGrid
                strokeDasharray="3 3"
                stroke="currentColor"
                className="opacity-15"
              />
              <XAxis
                dataKey="horaCorta"
                tick={{ fontSize: 10, fill: "var(--foreground-secondary)" }}
                tickMargin={6}
                height={28}
                minTickGap={30}
              />
              <YAxis
                domain={dominioY}
                tick={{ fontSize: 11, fill: "var(--foreground-secondary)" }}
                unit=" °C"
                width={60}
              />

              {mostrarUmbralesValidos && (
                <>
                  <ReferenceArea
                    y1={umbralInf}
                    y2={umbralSup}
                    fill="#10b981"
                    fillOpacity={0.07}
                    ifOverflow="hidden"
                  />
                  <ReferenceLine
                    y={umbralSup}
                    stroke="#ef4444"
                    strokeDasharray="4 4"
                    ifOverflow="extendDomain"
                    label={{
                      value: `TH ${umbralSup.toFixed(1)}°`,
                      position: "insideTopRight",
                      fill: "#ef4444",
                      fontSize: 10,
                    }}
                  />
                  <ReferenceLine
                    y={umbralInf}
                    stroke="#3b82f6"
                    strokeDasharray="4 4"
                    ifOverflow="extendDomain"
                    label={{
                      value: `TL ${umbralInf.toFixed(1)}°`,
                      position: "insideBottomRight",
                      fill: "#3b82f6",
                      fontSize: 10,
                    }}
                  />
                </>
              )}

              {verBandaSigma && stats && stats.desviacionStd > 0 && (
                <ReferenceArea
                  y1={stats.promedio - stats.desviacionStd}
                  y2={stats.promedio + stats.desviacionStd}
                  fill="#f59e0b"
                  fillOpacity={0.12}
                  ifOverflow="hidden"
                />
              )}

              {verPromedio && stats && (
                <ReferenceLine
                  y={stats.promedio}
                  stroke="#f59e0b"
                  strokeDasharray="2 4"
                  label={{
                    value: `x̄ ${stats.promedio.toFixed(2)}°`,
                    position: "insideTopLeft",
                    fill: "#f59e0b",
                    fontSize: 10,
                  }}
                />
              )}

              <Tooltip
                content={({ active, payload }) => {
                  if (!active || !payload?.length) return null;
                  const p = payload[0].payload as PuntoAnalitico;
                  const deltaMedia = stats ? p.temp - stats.promedio : 0;
                  return (
                    <div className="rounded-lg border border-border bg-card px-3 py-2 text-xs text-card-foreground shadow-md">
                      <div className="font-mono text-[10px] text-foreground-secondary">
                        {p.hora}
                      </div>
                      <div className="mt-1 flex items-center justify-between gap-3">
                        <span>Temp. Corregida:</span>
                        <strong
                          className={`font-mono ${
                            p.estadoZona === "ALTA"
                              ? "text-error"
                              : p.estadoZona === "BAJA"
                                ? "text-info"
                                : "text-primary"
                          }`}
                        >
                          {p.temp.toFixed(2)} °C
                        </strong>
                      </div>
                      {verSma && (
                        <div className="flex items-center justify-between gap-3 text-[11px]">
                          <span className="text-foreground-secondary">
                            Media móvil (k={ventanaSma}):
                          </span>
                          <span className="font-mono text-amber-500">
                            {p.sma.toFixed(2)} °C
                          </span>
                        </div>
                      )}
                      {verTendencia && (
                        <div className="flex items-center justify-between gap-3 text-[11px]">
                          <span className="text-foreground-secondary">
                            Ajuste MCO (T̂):
                          </span>
                          <span className="font-mono text-violet-400">
                            {p.tendencia.toFixed(2)} °C
                          </span>
                        </div>
                      )}
                      {stats && (
                        <div className="mt-0.5 flex items-center justify-between gap-3 text-[11px] text-foreground-secondary">
                          <span>Residuo (T − x̄):</span>
                          <span className="font-mono">
                            {deltaMedia >= 0 ? "+" : ""}
                            {deltaMedia.toFixed(2)} °C
                          </span>
                        </div>
                      )}
                    </div>
                  );
                }}
              />

              <Line
                type="monotone"
                dataKey="temp"
                stroke="#10b981"
                strokeWidth={2}
                isAnimationActive={false}
                dot={(props) => {
                  const p = props.payload as PuntoAnalitico | undefined;
                  if (!p || !props.cx || !props.cy) return <g key={props.key} />;
                  if (p.estadoZona !== "OK") {
                    return (
                      <circle
                        key={props.key}
                        cx={props.cx}
                        cy={props.cy}
                        r={3.5}
                        fill={p.estadoZona === "ALTA" ? "#ef4444" : "#3b82f6"}
                        stroke="var(--background)"
                        strokeWidth={1}
                      />
                    );
                  }
                  if (puntosRender.length <= 36) {
                    return (
                      <circle
                        key={props.key}
                        cx={props.cx}
                        cy={props.cy}
                        r={2}
                        fill="#10b981"
                      />
                    );
                  }
                  return <g key={props.key} />;
                }}
              />

              {verSma && (
                <Line
                  type="monotone"
                  dataKey="sma"
                  stroke="#f59e0b"
                  strokeWidth={1.6}
                  dot={false}
                  isAnimationActive={false}
                />
              )}

              {verTendencia && (
                <Line
                  type="linear"
                  dataKey="tendencia"
                  stroke="#8b5cf6"
                  strokeWidth={1.6}
                  strokeDasharray="6 3"
                  dot={false}
                  isAnimationActive={false}
                />
              )}

              {puntos.length > 14 && (
                <Brush
                  dataKey="horaCorta"
                  height={22}
                  stroke="var(--primary)"
                  fill="var(--secondary)"
                  travellerWidth={8}
                  tickFormatter={(v) => String(v)}
                />
              )}
            </ComposedChart>
          </ResponsiveContainer>
        )}
      </div>
    </div>
  );
}