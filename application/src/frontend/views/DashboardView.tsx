import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Activity,
  HardDrive,
  Wifi,
  Thermometer,
  RefreshCw,
  ArrowUpRight,
} from "lucide-react";
import {
  ResponsiveContainer,
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  Legend,
  CartesianGrid,
} from "recharts";
import { client, safeRpc } from "../lib/service";
import { useAuth } from "../contexts/AuthContext";
import {
  formatearBytes,
  formatearDuracionSegundos,
  formatearEpochSegundos,
} from "../lib/format";
import {
  DiagnosticoSistemaCliente,
  EspacioMicroSdDTO,
  EstadisticasSensoresCliente,
  InfoRedPublicaDTO,
  LecturaSensorCliente,
  ResumenSensorCliente,
  RolUsuario,
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

  const [lecturas, setLecturas] = useState<LecturaSensorCliente[]>([]);
  const [chartData, setChartData] = useState<ChartPoint[]>([]);
  const [macs, setMacs] = useState<string[]>([]);
  const [suscripto, setSuscripto] = useState(false);
  const [refrescando, setRefrescando] = useState(false);

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

    const horaStr = formatearEpochSegundos(lote[0].timestampMs).slice(-8);
    const punto: ChartPoint = { hora: horaStr };
    lote.forEach((l) => {
      punto[l.macHex] = Number(l.temperaturaCorregida.toFixed(2));
    });

    setChartData((prev) => [...prev.slice(-59), punto]);
  }, []);

  const cargarResumen = useCallback(async (forzarSinCache = false) => {
    if (forzarSinCache) {
      setRefrescando(true);
      await safeRpc(() => client.invalidarCache(), "invalidarCache", {
        silent: true,
      });
    }
    const [d, s, r, st, lista] = await Promise.all([
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

  const porcentajeSd =
    sd && sd.totalKb > 0 ? Math.min(100, (sd.usadoKb / sd.totalKb) * 100) : 0;

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
        <div className="mb-5 grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
          {lecturas.map((l) => {
            const colorSensor = coloresRef.current[l.macHex] ?? "#10b981";
            const meta = catalogoSensores[l.macHex.toUpperCase()];
            return (
              <button
                key={l.macHex}
                type="button"
                onClick={() =>
                  onNavegar?.({
                    vista: "sensores",
                    sensorMac: l.macHex.toUpperCase(),
                  })
                }
                title={`Abrir detalle y calibración de ${meta?.nombre || l.macHex}`}
                className="group rounded-lg border border-border bg-background p-3.5 text-left transition-all hover:border-primary hover:shadow-sm"
              >
                <div className="flex items-center justify-between text-xs text-foreground-secondary">
                  <span className="flex items-center gap-1.5 font-mono font-medium text-foreground">
                    <span
                      className="h-2.5 w-2.5 rounded-full shrink-0"
                      style={{ backgroundColor: colorSensor }}
                    />
                    {l.macHex}
                  </span>
                  <span className="inline-flex items-center gap-1 rounded bg-secondary px-1.5 py-0.5 font-mono text-[11px]">
                    ID #{l.idSensor}
                    <ArrowUpRight className="h-3 w-3 opacity-50 transition-opacity group-hover:text-primary group-hover:opacity-100" />
                  </span>
                </div>

                {meta && (meta.nombre || meta.ubicacion) && (
                  <div className="mt-1 truncate text-xs font-medium text-foreground-secondary">
                    {meta.nombre || "Sin nombre"}
                    {meta.ubicacion ? ` · ${meta.ubicacion}` : ""}
                  </div>
                )}

                <div className="mt-2 flex items-baseline justify-between">
                  <span className="text-2xl font-bold tracking-tight tabular-nums">
                    {l.temperaturaCorregida.toFixed(2)} °C
                  </span>
                </div>

                <div className="mt-1.5 text-[11px] text-foreground-secondary">
                  Muestra: {formatearEpochSegundos(l.timestampMs)}
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

        {/* Gráfico Recharts multilínea en tiempo real */}
        <div className="h-80 w-full rounded-lg border border-border bg-background p-3">
          {chartData.length > 0 ? (
            <ResponsiveContainer width="100%" height="100%">
              <LineChart
                data={chartData}
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
                <Tooltip
                  contentStyle={{
                    backgroundColor: "var(--card)",
                    borderColor: "var(--border)",
                    color: "var(--card-foreground)",
                    borderRadius: "0.5rem",
                    fontSize: "12px",
                  }}
                  formatter={(value) => [
                    `${Number(value ?? 0).toFixed(2)} °C`,
                    "Temp.",
                  ]}
                />
                <Legend wrapperStyle={{ fontSize: "12px" }} />
                {macs.map((mac) => (
                  <Line
                    key={mac}
                    type="monotone"
                    dataKey={mac}
                    stroke={coloresRef.current[mac] ?? "#10b981"}
                    strokeWidth={2}
                    dot={false}
                    isAnimationActive={false}
                    name={catalogoSensores[mac.toUpperCase()]?.nombre || mac}
                  />
                ))}
              </LineChart>
            </ResponsiveContainer>
          ) : (
            <div className="flex h-full items-center justify-center text-xs text-foreground-secondary">
              Sin muestras acumuladas en la ventana gráfica.
            </div>
          )}
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