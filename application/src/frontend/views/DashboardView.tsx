import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  HardDrive,
  Wifi,
  Thermometer,
  RefreshCw,
  ArrowUpRight,
  AlertTriangle,
  Pause,
  Play,
} from "lucide-react";
import {
  ResponsiveContainer,
  ComposedChart,
  Line,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  ReferenceLine,
  ReferenceArea,
} from "recharts";
import { client, safeRpc } from "../lib/service";
import { useAuth } from "../contexts/AuthContext";
import {
  formatearBytes,
  formatearDuracionSegundos,
  formatearEpochSegundos,
} from "../lib/format";
import { LABEL_ESTADO_SENSOR } from "../lib/labels";
import {
  DiagnosticoSistemaCliente,
  EspacioMicroSdDTO,
  EstadoSensor,
  EstadisticasSensoresCliente,
  InfoRedPublicaDTO,
  LecturaSensorCliente,
  ResumenSensorCliente,
  RolUsuario,
  SensorEnAlarmaItemDTO,
} from "#types/rpc.types";
import type { DestinoNavegacion } from "../pages/App";

interface ChartPoint {
  hora: string;
  [macHex: string]: string | number;
}

const PALETA_SENSORES = [
  "#10b981",
  "#3b82f6",
  "#ef4444",
  "#f59e0b",
  "#8b5cf6",
  "#ec4899",
  "#14b8a6",
  "#f97316",
];

export function DashboardView({
  onNavegar,
}: {
  onNavegar?: (destino: DestinoNavegacion) => void;
}) {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [diag, setDiag] = useState<DiagnosticoSistemaCliente | null>(null);
  const [sd, setSd] = useState<EspacioMicroSdDTO | null>(null);
  const [red, setRed] = useState<InfoRedPublicaDTO | null>(null);
  const [stats, setStats] = useState<EstadisticasSensoresCliente | null>(null);
  const [catalogoSensores, setCatalogoSensores] = useState<
    Record<string, ResumenSensorCliente>
  >({});
  const [alarmasPorMac, setAlarmasPorMac] = useState<
    Record<string, SensorEnAlarmaItemDTO>
  >({});

  const [lecturas, setLecturas] = useState<LecturaSensorCliente[]>([]);
  const [chartData, setChartData] = useState<ChartPoint[]>([]);
  const [macs, setMacs] = useState<string[]>([]);
  const [suscripto, setSuscripto] = useState(false);
  const [refrescando, setRefrescando] = useState(false);
  const [ventanaMuestras, setVentanaMuestras] = useState<30 | 60 | 120>(60);
  const [chartPausado, setChartPausado] = useState(false);
  const [estiloCurva, setEstiloCurva] = useState<"LINEA" | "AREA">("LINEA");
  const [verUmbralesVivo, setVerUmbralesVivo] = useState(true);
  const [macsOcultas, setMacsOcultas] = useState<Record<string, boolean>>({});

  const chartPausadoRef = useRef(false);
  chartPausadoRef.current = chartPausado;

  const coloresRef = useRef<Record<string, string>>({});

  const procesarLote = useCallback((lote: LecturaSensorCliente[]) => {
    if (!lote || lote.length === 0) return;
    setLecturas(lote);

    setMacs((prev) => {
      const conjunto = new Set(prev);
      lote.forEach((l) => conjunto.add(l.macHex));
      return Array.from(conjunto);
    });

    lote.forEach((l) => {
      if (!coloresRef.current[l.macHex]) {
        const idx =
          Object.keys(coloresRef.current).length % PALETA_SENSORES.length;
        coloresRef.current[l.macHex] = PALETA_SENSORES[idx];
      }
    });

    if (chartPausadoRef.current) return;

    const horaStr = formatearEpochSegundos(lote[0].timestampMs).slice(-8);
    const punto: ChartPoint = { hora: horaStr };
    lote.forEach((l) => {
      punto[l.macHex] = Number(l.temperaturaCorregida.toFixed(2));
    });

    setChartData((prev) => [...prev.slice(-119), punto]);
  }, []);

  const cargarResumen = useCallback(async (forzarSinCache = false) => {
    if (forzarSinCache) {
      setRefrescando(true);
      await safeRpc(() => client.invalidarCache(), "invalidarCache", {
        silent: true,
      });
    }
    const [d, s, r, st, lista, alarmas] = await Promise.all([
      safeRpc(() => client.ejecutarDiagnosticoSistema(), "diag", {
        silent: true,
      }),
      safeRpc(() => client.obtenerEspacioMicroSD(), "sd", { silent: true }),
      safeRpc(() => client.obtenerInfoRedPublica(), "red", { silent: true }),
      safeRpc(() => client.obtenerEstadisticasSensores(), "stats", {
        silent: true,
      }),
      safeRpc(() => client.listarSensores(12, 0, false), "listarSensores", {
        silent: true,
      }),
      safeRpc(() => client.obtenerEstadoAlarmasActivas(), "alarmas", {
        silent: true,
      }),
    ]);
    if (d) setDiag(d);
    if (s) setSd(s);
    if (r) setRed(r);
    if (st) setStats(st);
    if (lista) {
      const mapa: Record<string, ResumenSensorCliente> = {};
      lista.forEach((item) => {
        mapa[item.mac.toUpperCase()] = item;
      });
      setCatalogoSensores(mapa);
    }
    if (alarmas) {
      const mapaAl: Record<string, SensorEnAlarmaItemDTO> = {};
      alarmas.sensoresAfectados.forEach((item) => {
        mapaAl[item.mac.toUpperCase()] = item;
      });
      setAlarmasPorMac(mapaAl);
    }
    if (forzarSinCache) setRefrescando(false);
  }, []);

  useEffect(() => {
    void cargarResumen(false);
    const id = window.setInterval(() => void cargarResumen(false), 5000);
    return () => window.clearInterval(id);
  }, [cargarResumen]);

  useEffect(() => {
    let montado = true;
    (async () => {
      const inicial = await safeRpc(
        () => client.suscribirSensoresTiempoReal(),
        "suscribirSensoresTiempoReal"
      );
      if (!montado) return;
      if (inicial) {
        setSuscripto(true);
        if (inicial.length > 0) {
          procesarLote(inicial);
        }
      }
    })();
    return () => {
      montado = false;
    };
  }, [procesarLote]);

  useEffect(() => {
    if (!suscripto) return;
    const id = window.setInterval(async () => {
      const lote = await safeRpc(
        () => client.obtenerUltimoLoteTiempoReal(),
        "obtenerUltimoLoteTiempoReal",
        { silent: true }
      );
      if (lote && lote.length > 0) {
        procesarLote(lote);
      }
    }, 500);
    return () => window.clearInterval(id);
  }, [suscripto, procesarLote]);

  const porcentajeSd = sd && sd.totalKb > 0 ? Math.min(100, (sd.usadoKb / sd.totalKb) * 100) : 0;

  const macsVisibles = useMemo(
    () => macs.filter((m) => !macsOcultas[m]),
    [macs, macsOcultas]
  );

  const chartDataVisible = useMemo(
    () => chartData.slice(-ventanaMuestras),
    [chartData, ventanaMuestras]
  );

  const metaUnicoSensor =
    macsVisibles.length === 1
      ? catalogoSensores[macsVisibles[0].toUpperCase()]
      : undefined;

  const statsVentanaViva = useMemo(() => {
    if (chartDataVisible.length === 0 || macsVisibles.length === 0) return null;
    let min = Infinity;
    let max = -Infinity;
    let suma = 0;
    let count = 0;

    for (const p of chartDataVisible) {
      for (const m of macsVisibles) {
        const v = p[m];
        if (typeof v === "number" && Number.isFinite(v)) {
          if (v < min) min = v;
          if (v > max) max = v;
          suma += v;
          count++;
        }
      }
    }
    if (count === 0) return null;

    // Dispersión instantánea entre sensores en la última muestra
    const ultimo = chartDataVisible[chartDataVisible.length - 1];
    const ultimosVals = macsVisibles
      .map((m) => ultimo[m])
      .filter((v): v is number => typeof v === "number" && Number.isFinite(v));
    const spreadActual =
      ultimosVals.length >= 2
        ? Math.max(...ultimosVals) - Math.min(...ultimosVals)
        : 0;

    return {
      min,
      max,
      promedio: suma / count,
      amplitud: max - min,
      spreadActual,
    };
  }, [chartDataVisible, macsVisibles]);

  return (
    <div className="space-y-4 p-4">
      <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4">
        <MetricCard
          icon={<Activity className="h-5 w-5 text-primary" />}
          title="Heap Libre & Uptime"
          value={diag ? formatearBytes(diag.heapLibreBytes) : "—"}
          sub={
            diag
              ? `Uptime: ${formatearDuracionSegundos(diag.uptimeSegundos)}`
              : "Consultando núcleo..."
          }
          onClick={
            esAdmin && onNavegar
              ? () => onNavegar({ vista: "bios" })
              : undefined
          }
          actionLabel={esAdmin ? "Abrir BIOS / FreeRTOS" : undefined}
        />

        <MetricCard
          icon={<HardDrive className="h-5 w-5 text-info" />}
          title="Almacenamiento MicroSD"
          value={sd ? `${sd.libreKb.toLocaleString("es-AR")} KB libres` : "—"}
          sub={
            sd
              ? `Usado ${sd.usadoKb.toLocaleString("es-AR")} / ${sd.totalKb.toLocaleString("es-AR")} KB (${porcentajeSd.toFixed(1)}%)`
              : "Consultando FAT32..."
          }
          progress={sd ? porcentajeSd : undefined}
          onClick={
            onNavegar ? () => onNavegar({ vista: "database" }) : undefined
          }
          actionLabel="Ver Base de Datos"
        />

        <MetricCard
          icon={<Wifi className="h-5 w-5 text-success" />}
          title="Estado de Red"
          value={
            red
              ? red.conectadoARouter
                ? `STA ${red.ipSta}`
                : `SoftAP ${red.ipAp}`
              : "—"
          }
          sub={
            red
              ? `AP ${red.ipAp || "—"} · RSSI ${red.rssiDbm} dBm · ${red.clientesAutenticados} cli`
              : "Consultando WiFi..."
          }
          onClick={
            onNavegar
              ? () =>
                  onNavegar({
                    vista: "configuracion",
                    configTab: esAdmin ? "WIFI" : "SISTEMA",
                  })
              : undefined
          }
          actionLabel={esAdmin ? "Configurar Red WiFi" : "Ver Configuración"}
        />

        <MetricCard
          icon={<Thermometer className="h-5 w-5 text-warning" />}
          title="Catálogo de Sensores"
          value={stats ? `${stats.sensoresActivos} activos` : "—"}
          sub={
            stats
              ? `Total: ${stats.totalSensores} · Inactivos: ${stats.sensoresInactivos}`
              : "Consultando catálogo..."
          }
          onClick={
            onNavegar ? () => onNavegar({ vista: "sensores" }) : undefined
          }
          actionLabel="Administrar Sensores"
        />
      </div>

      <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="mb-4 flex flex-wrap items-center justify-between gap-2">
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-lg font-semibold">
                Monitoreo Térmico en Tiempo Real
              </h2>
              <span
                className={`inline-flex items-center gap-1.5 rounded-full px-2.5 py-0.5 text-xs font-medium ${
                  suscripto
                    ? "bg-alert-success-bg text-alert-success-icon border border-alert-success-border"
                    : "bg-alert-warn-bg text-alert-warn-icon border border-alert-warn-border"
                }`}
              >
                <span
                  className={`h-2 w-2 rounded-full ${
                    suscripto ? "bg-success animate-pulse" : "bg-warning"
                  }`}
                />
                {suscripto ? "Stream activo (500 ms)" : "Conectando stream..."}
              </span>
            </div>
            <p className="text-xs text-foreground-secondary">
              Lecturas en vivo del bus 1-Wire (DS18B20) · Haga clic en un sensor
              para abrir su ficha y calibración
            </p>
          </div>

          <div className="flex items-center gap-2">
            <button
              type="button"
              className="btn btn-outline text-xs py-1.5 px-3"
              onClick={() => setChartData([])}
              disabled={chartData.length === 0}
            >
              Limpiar curva
            </button>
            <button
              type="button"
              className="btn btn-outline text-xs py-1.5 px-3"
              disabled={refrescando}
              onClick={() => void cargarResumen(true)}
            >
              <RefreshCw
                className={`h-3.5 w-3.5 ${refrescando ? "animate-spin" : ""}`}
              />
              Refrescar estado
            </button>
          </div>
        </div>

        {/* Tarjetas individuales clickeables por sensor DS18B20 conectado */}
        <div
          data-tour="dash-stream-cards"
          className="mb-5 grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4"
        >
          {lecturas.map((l) => {
            const macUpper = l.macHex.toUpperCase();
            const colorSensor = coloresRef.current[l.macHex] ?? "#10b981";
            const meta = catalogoSensores[macUpper];
            const alarmaFw = alarmasPorMac[macUpper];

            const alertaAltaVivo =
              meta !== undefined && l.temperaturaCorregida >= meta.umbralSup;
            const alertaBajaVivo =
              meta !== undefined && l.temperaturaCorregida <= meta.umbralInf;
            const estadoMostrar: EstadoSensor =
              alarmaFw?.estado ??
              (alertaAltaVivo
                ? EstadoSensor.ALARMA_SUPERIOR
                : alertaBajaVivo
                  ? EstadoSensor.ALARMA_INFERIOR
                  : EstadoSensor.OK);
            const enAlerta = estadoMostrar !== EstadoSensor.OK;
            const lecturaValida =
              estadoMostrar === EstadoSensor.OK ||
              estadoMostrar === EstadoSensor.ALARMA_SUPERIOR ||
              estadoMostrar === EstadoSensor.ALARMA_INFERIOR;

            return (
              <button
                key={l.macHex}
                type="button"
                onClick={() =>
                  onNavegar?.({
                    vista: "sensores",
                    sensorMac: macUpper,
                  })
                }
                title={`Abrir detalle y calibración de ${meta?.nombre || l.macHex}`}
                className={`group cursor-pointer rounded-lg border p-3.5 text-left transition-all duration-150 ${
                  enAlerta
                    ? "border-red-800/60 bg-alert-err-bg/75 dark:border-red-900 hover:border-red-500 hover:bg-alert-err-bg hover:ring-2 hover:ring-red-500/45 hover:shadow-[0_0_14px_rgba(239,68,68,0.35)] dark:hover:border-red-400"
                    : "border-border bg-background hover:border-primary hover:shadow-sm"
                }`}
              >
                <div className="flex items-center justify-between gap-2 text-xs text-foreground-secondary">
                  <span className="flex items-center gap-1.5 font-mono font-medium text-foreground">
                    <span
                      className={`h-2.5 w-2.5 shrink-0 rounded-full ${
                        enAlerta ? "animate-pulse bg-error" : ""
                      }`}
                      style={enAlerta ? undefined : { backgroundColor: colorSensor }}
                    />
                    {l.macHex}
                  </span>
                  <span
                    className={`inline-flex items-center gap-1 rounded px-1.5 py-0.5 font-mono text-[11px] ${
                      enAlerta
                        ? "bg-error text-error-foreground font-bold"
                        : "bg-secondary"
                    }`}
                  >
                    ID #{l.idSensor}
                    <ArrowUpRight className="h-3 w-3 opacity-70 transition-opacity group-hover:opacity-100" />
                  </span>
                </div>

                {meta && (meta.nombre || meta.ubicacion) && (
                  <div className="mt-1 truncate text-xs font-medium text-foreground-secondary">
                    {meta.nombre || "Sin nombre"}
                    {meta.ubicacion ? ` · ${meta.ubicacion}` : ""}
                  </div>
                )}

                <div className="mt-2 flex items-baseline justify-between gap-2">
                  <span
                    className={`text-2xl font-bold tracking-tight tabular-nums ${
                      enAlerta ? "text-alert-err-icon" : ""
                    }`}
                  >
                    {lecturaValida
                      ? `${l.temperaturaCorregida.toFixed(2)} °C`
                      : "FALLO"}
                  </span>

                  {enAlerta && (
                    <span className="inline-flex items-center gap-1 rounded border border-alert-err-border bg-background px-1.5 py-0.5 text-[10px] font-bold text-alert-err-icon">
                      <AlertTriangle className="h-3 w-3 shrink-0" />
                      {LABEL_ESTADO_SENSOR[estadoMostrar] ?? `E${estadoMostrar}`}
                    </span>
                  )}
                </div>

                <div className="mt-1.5 flex items-center justify-between gap-2 text-[11px] text-foreground-secondary">
                  <span>Muestra: {formatearEpochSegundos(l.timestampMs)}</span>
                  {meta && (
                    <span className="font-mono text-[10px]">
                      {meta.umbralInf.toFixed(1)}..{meta.umbralSup.toFixed(1)}°C
                    </span>
                  )}
                </div>
              </button>
            );
          })}

          {lecturas.length === 0 && (
            <div className="col-span-full rounded-lg border border-dashed border-border bg-background/50 p-6 text-center text-sm text-foreground-secondary">
              Esperando primer lote de telemetría desde el bus 1-Wire del ESP32…
            </div>
          )}
        </div>

        {/* Controles y estadísticas del gráfico en tiempo real */}
        <div
          data-tour="dash-chart"
          className="space-y-2.5"
        >
          <div className="flex flex-wrap items-center justify-between gap-2 rounded-lg border border-border bg-background/70 px-3 py-2 text-xs">
            {/* Chips para mostrar/ocultar cada sensor */}
            <div className="flex flex-wrap items-center gap-1.5">
              {macs.map((mac) => {
                const oculto = Boolean(macsOcultas[mac]);
                const color = coloresRef.current[mac] ?? "#10b981";
                const nombre = catalogoSensores[mac.toUpperCase()]?.nombre || mac;
                return (
                  <button
                    key={mac}
                    type="button"
                    onClick={() =>
                      setMacsOcultas((prev) => ({ ...prev, [mac]: !prev[mac] }))
                    }
                    className={`inline-flex cursor-pointer items-center gap-1.5 rounded-full border px-2.5 py-0.5 text-[11px] font-medium transition-opacity ${
                      oculto
                        ? "border-border bg-secondary/40 text-foreground-secondary opacity-45 line-through"
                        : "border-border bg-card text-foreground"
                    }`}
                    title={oculto ? "Mostrar serie" : "Ocultar serie"}
                  >
                    <span
                      className="h-2 w-2 rounded-full"
                      style={{ backgroundColor: color }}
                    />
                    {nombre}
                  </button>
                );
              })}
            </div>

            {/* Controles de ventana, estilo y pausa */}
            <div className="flex flex-wrap items-center gap-2 text-[11px]">
              <div className="flex rounded bg-secondary p-0.5">
                {([30, 60, 120] as const).map((n) => (
                  <button
                    key={n}
                    type="button"
                    onClick={() => setVentanaMuestras(n)}
                    className={`rounded px-2 py-0.5 font-mono font-semibold ${
                      ventanaMuestras === n
                        ? "bg-card text-primary shadow-xs"
                        : "text-foreground-secondary hover:text-foreground"
                    }`}
                  >
                    {n} pts
                  </button>
                ))}
              </div>

              <div className="flex rounded bg-secondary p-0.5">
                {(["LINEA", "AREA"] as const).map((est) => (
                  <button
                    key={est}
                    type="button"
                    onClick={() => setEstiloCurva(est)}
                    className={`rounded px-2 py-0.5 font-semibold ${
                      estiloCurva === est
                        ? "bg-card text-primary shadow-xs"
                        : "text-foreground-secondary hover:text-foreground"
                    }`}
                  >
                    {est === "LINEA" ? "Línea" : "Área"}
                  </button>
                ))}
              </div>

              {metaUnicoSensor && (
                <label className="flex cursor-pointer items-center gap-1">
                  <input
                    type="checkbox"
                    checked={verUmbralesVivo}
                    onChange={(e) => setVerUmbralesVivo(e.target.checked)}
                  />
                  TL/TH
                </label>
              )}

              <button
                type="button"
                onClick={() => setChartPausado((p) => !p)}
                className={`btn px-2.5 py-1 text-[11px] ${
                  chartPausado ? "btn-solid btn-warning" : "btn-outline"
                }`}
              >
                {chartPausado ? (
                  <>
                    <Play className="h-3 w-3" />
                    Reanudar
                  </>
                ) : (
                  <>
                    <Pause className="h-3 w-3" />
                    Pausar
                  </>
                )}
              </button>
            </div>
          </div>

          {statsVentanaViva && (
            <div className="flex flex-wrap items-center gap-3 px-1 font-mono text-[11px] text-foreground-secondary">
              <span>
                Ventana ({chartDataVisible.length} pts):{" "}
                <strong className="text-info">
                  Mín {statsVentanaViva.min.toFixed(2)}°C
                </strong>{" "}
                ·{" "}
                <strong className="text-primary">
                  Prom {statsVentanaViva.promedio.toFixed(2)}°C
                </strong>{" "}
                ·{" "}
                <strong className="text-error">
                  Máx {statsVentanaViva.max.toFixed(2)}°C
                </strong>
              </span>
              <span>· Amplitud Δ {statsVentanaViva.amplitud.toFixed(2)}°C</span>
              {macsVisibles.length >= 2 && (
                <span>
                  · Dispersión entre sensores:{" "}
                  <strong className="text-foreground">
                    {statsVentanaViva.spreadActual.toFixed(2)}°C
                  </strong>
                </span>
              )}
            </div>
          )}

          <div className="h-80 w-full rounded-lg border border-border bg-background p-3">
            {chartDataVisible.length > 0 && macsVisibles.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <ComposedChart
                  data={chartDataVisible}
                  margin={{ top: 8, right: 16, left: 0, bottom: 4 }}
                >
                  <CartesianGrid
                    strokeDasharray="3 3"
                    stroke="currentColor"
                    className="opacity-15"
                  />
                  <XAxis dataKey="hora" tick={{ fontSize: 11 }} minTickGap={24} />
                  <YAxis
                    domain={["auto", "auto"]}
                    tick={{ fontSize: 11 }}
                    unit=" °C"
                    width={56}
                  />

                  {verUmbralesVivo && metaUnicoSensor && (
                    <>
                      <ReferenceArea
                        y1={metaUnicoSensor.umbralInf}
                        y2={metaUnicoSensor.umbralSup}
                        fill="#10b981"
                        fillOpacity={0.07}
                        ifOverflow="hidden"
                      />
                      <ReferenceLine
                        y={metaUnicoSensor.umbralSup}
                        stroke="#ef4444"
                        strokeDasharray="4 4"
                        ifOverflow="extendDomain"
                        label={{
                          value: `TH ${metaUnicoSensor.umbralSup.toFixed(1)}°`,
                          position: "insideTopRight",
                          fill: "#ef4444",
                          fontSize: 10,
                        }}
                      />
                      <ReferenceLine
                        y={metaUnicoSensor.umbralInf}
                        stroke="#3b82f6"
                        strokeDasharray="4 4"
                        ifOverflow="extendDomain"
                        label={{
                          value: `TL ${metaUnicoSensor.umbralInf.toFixed(1)}°`,
                          position: "insideBottomRight",
                          fill: "#3b82f6",
                          fontSize: 10,
                        }}
                      />
                    </>
                  )}

                  <Tooltip
                    content={({ active, payload, label }) => {
                      if (!active || !payload?.length) return null;
                      return (
                        <div className="rounded-lg border border-border bg-card px-3 py-2 text-xs text-card-foreground shadow-md">
                          <div className="mb-1 font-mono text-[10px] text-foreground-secondary">
                            {label}
                          </div>
                          {payload.map((entry) => (
                            <div
                              key={String(entry.dataKey)}
                              className="flex items-center justify-between gap-3 py-0.5"
                            >
                              <span className="inline-flex items-center gap-1.5">
                                <span
                                  className="h-2 w-2 rounded-full"
                                  style={{ backgroundColor: entry.color }}
                                />
                                {entry.name}
                              </span>
                              <strong className="font-mono">
                                {Number(entry.value ?? 0).toFixed(2)} °C
                              </strong>
                            </div>
                          ))}
                        </div>
                      );
                    }}
                  />

                  {macsVisibles.map((mac) => {
                    const stroke = coloresRef.current[mac] ?? "#10b981";
                    const name = catalogoSensores[mac.toUpperCase()]?.nombre || mac;
                    return estiloCurva === "AREA" ? (
                      <Area
                        key={mac}
                        type="monotone"
                        dataKey={mac}
                        stroke={stroke}
                        fill={stroke}
                        fillOpacity={0.14}
                        strokeWidth={2}
                        dot={false}
                        isAnimationActive={false}
                        name={name}
                      />
                    ) : (
                      <Line
                        key={mac}
                        type="monotone"
                        dataKey={mac}
                        stroke={stroke}
                        strokeWidth={2}
                        dot={false}
                        isAnimationActive={false}
                        name={name}
                      />
                    );
                  })}
                </ComposedChart>
              </ResponsiveContainer>
            ) : (
              <div className="flex h-full items-center justify-center text-xs text-foreground-secondary">
                {macsVisibles.length === 0 && macs.length > 0
                  ? "Activá al menos un sensor arriba para visualizar su curva."
                  : "Sin muestras acumuladas en la ventana gráfica."}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

function MetricCard({
  icon,
  title,
  value,
  sub,
  progress,
  onClick,
  actionLabel,
}: {
  icon: React.ReactNode;
  title: string;
  value: string;
  sub?: string;
  progress?: number;
  onClick?: () => void;
  actionLabel?: string;
}) {
  const Tag = onClick ? "button" : "div";
  return (
    <Tag
      type={onClick ? "button" : undefined}
      onClick={onClick}
      title={actionLabel}
      className={`group rounded-xl border border-border bg-card p-4 text-left text-card-foreground shadow-xs transition-all ${
        onClick
          ? "cursor-pointer hover:border-primary/60 hover:shadow-sm"
          : ""
      }`}
    >
      <div className="mb-2 flex items-center justify-between text-sm font-medium text-foreground-secondary">
        <span className="inline-flex items-center gap-1.5">
          {title}
          {onClick && (
            <ArrowUpRight className="h-3.5 w-3.5 opacity-40 transition-all group-hover:translate-x-0.5 group-hover:-translate-y-0.5 group-hover:text-primary group-hover:opacity-100" />
          )}
        </span>
        <span className="text-primary">{icon}</span>
      </div>
      <div className="text-xl font-bold tracking-tight">{value}</div>
      {sub && (
        <div className="mt-1 text-xs text-foreground-secondary">{sub}</div>
      )}
      {progress !== undefined && (
        <div className="mt-2.5 h-2 w-full overflow-hidden rounded-full bg-secondary">
          <div
            className="h-full bg-primary transition-all duration-300"
            style={{ width: `${Math.min(100, Math.max(0, progress))}%` }}
          />
        </div>
      )}
    </Tag>
  );
}