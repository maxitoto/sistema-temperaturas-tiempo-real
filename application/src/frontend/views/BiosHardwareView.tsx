import { useCallback, useEffect, useState } from "react";
import {
  Cpu,
  RefreshCw,
  Activity,
  FileWarning,
  Lightbulb,
  Volume2,
  VolumeX,
  Monitor,
  Radio,
  RotateCcw,
  CheckCircle2,
  XCircle,
  Power
} from "lucide-react";
import {
  ResponsiveContainer,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
} from "recharts";
import { client, safeRpc } from "../lib/service";
import { formatearBytes, formatearDuracionSegundos } from "../lib/format";
import { LABEL_ESTADO_BUS } from "../lib/labels";
import { NumericInput } from "../components/ui/NumericInput";
import {
  AccionHardwareDirecto,
  EstadoHardwareDirectoDTO,
  InfoCompletaEspDTO,
  ReporteHelpLogBiosDTO,
  TelemetriaTareaRtosDTO,
} from "#types/rpc.types";

export function BiosHardwareView() {
  const [infoEsp, setInfoEsp] = useState<InfoCompletaEspDTO | null>(null);
  const [tareasRtos, setTareasRtos] = useState<TelemetriaTareaRtosDTO[]>([]);
  const [helpLog, setHelpLog] = useState<ReporteHelpLogBiosDTO | null>(null);
  const [hwDirecto, setHwDirecto] = useState<EstadoHardwareDirectoDTO | null>(
    null
  );

  const [autoRtos, setAutoRtos] = useState(true);
  const [ejecutandoHw, setEjecutandoHw] = useState(false);

  // Controles de pruebas temporizadas: LED, Buzzer y OLED
  const [duracionLedMs, setDuracionLedMs] = useState(2000);
  const [frecuenciaHz, setFrecuenciaHz] = useState(1200);
  const [duracionBuzzMs, setDuracionBuzzMs] = useState(250);
  const [lineaOled1, setLineaOled1] = useState("UNTDF LAB ESP32");
  const [lineaOled2, setLineaOled2] = useState("TEST PANTALLA OK");
  const [duracionOledMs, setDuracionOledMs] = useState(4000);

  const cargarTodo = useCallback(async () => {
    const [esp, rtos, log, hw] = await Promise.all([
      safeRpc(
        () => client.obtenerInfoCompletaSistemaESP(),
        "obtenerInfoCompletaSistemaESP",
        { silent: true }
      ),
      safeRpc(
        () => client.obtenerEstadoTareasRTOS(),
        "obtenerEstadoTareasRTOS",
        { silent: true }
      ),
      safeRpc(() => client.leerHelpLogBios(), "leerHelpLogBios", {
        silent: true,
      }),
      safeRpc(
        () => client.obtenerEstadoHardwareDirecto(),
        "obtenerEstadoHardwareDirecto",
        { silent: true }
      ),
    ]);
    if (esp) setInfoEsp(esp);
    if (rtos) setTareasRtos(rtos);
    if (log) setHelpLog(log);
    if (hw) setHwDirecto(hw);
  }, []);

  useEffect(() => {
    void cargarTodo();
  }, [cargarTodo]);

  useEffect(() => {
    if (!autoRtos) return;
    const id = window.setInterval(async () => {
      const rtos = await safeRpc(
        () => client.obtenerEstadoTareasRTOS(),
        "obtenerEstadoTareasRTOS",
        { silent: true }
      );
      if (rtos) setTareasRtos(rtos);
    }, 2000);
    return () => window.clearInterval(id);
  }, [autoRtos]);

  const enviarComandoHw = async (
    accion: AccionHardwareDirecto,
    mensajeOk: string,
    duracionPersonalizadaMs?: number
  ) => {
    setEjecutandoHw(true);
    const duracionEnviar = Math.round(
      duracionPersonalizadaMs ?? duracionBuzzMs
    );
    const nuevoEstado = await safeRpc(
      () =>
        client.controlarHardwareDirecto({
          accion,
          frecuenciaHz: Math.round(frecuenciaHz),
          duracionMs: duracionEnviar,
          lineaOled1: lineaOled1.slice(0, 21),
          lineaOled2: lineaOled2.slice(0, 21),
        }),
      "controlarHardwareDirecto",
      { mensajeExito: mensajeOk }
    );
    setEjecutandoHw(false);
    if (nuevoEstado) {
      setHwDirecto(nuevoEstado);
      if (accion === AccionHardwareDirecto.LED_ENCENDER) {
        window.setTimeout(async () => {
          const refresco = await safeRpc(
            () => client.obtenerEstadoHardwareDirecto(),
            "obtenerEstadoHardwareDirecto",
            { silent: true }
          );
          if (refresco) setHwDirecto(refresco);
        }, duracionEnviar + 250);
      }
    }
  };

  const handleReiniciarEsp32 = async () => {
    if (
      !window.confirm(
        "¿Confirma el reinicio remoto por software del microcontrolador ESP32? Se cerrará la conexión activa."
      )
    ) {
      return;
    }
    await safeRpc(() => client.reiniciarHardware(), "reiniciarHardware", {
      mensajeExito: "Orden de reinicio enviada al ESP32.",
    });
  };

  const handleApagadoSeguro = async () => {
    if (
      !window.confirm(
        "¿Confirma el APAGADO SEGURO del ESP32? Se silenciará la alarma, se desmontará la MicroSD (FAT32 seguro) y el equipo entrará en deep sleep hasta presionar Reset físico."
      )
    ) {
      return;
    }
    const res = await safeRpc(
      () => client.solicitarApagadoSeguro(),
      "solicitarApagadoSeguro",
      {
        mensajeExito:
          "Apagado seguro iniciado. Espere ~2 s a que se apague la OLED antes de retirar energía o MicroSD.",
      }
    );
    if (res) {
      setAutoRtos(false);
    }
  };

  const heapChartData = infoEsp
    ? [
        {
          metrica: "Heap Total",
          kb: Number((infoEsp.heapTotalBytes / 1024).toFixed(1)),
        },
        {
          metrica: "Heap Libre",
          kb: Number((infoEsp.heapLibreBytes / 1024).toFixed(1)),
        },
        {
          metrica: "Max Bloque",
          kb: Number((infoEsp.heapMaxBloqueBytes / 1024).toFixed(1)),
        },
        {
          metrica: "Mín. Histórico",
          kb: Number((infoEsp.heapMinimoHistoricoBytes / 1024).toFixed(1)),
        },
      ]
    : [];

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado */}
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-light text-primary">
            <Cpu className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-lg font-bold">
              Panel Supervisor BIOS / FreeRTOS y Control Directo de Hardware
            </h1>
            <p className="text-xs text-foreground-secondary">
              Telemetría nativa Xtensa ESP32, watchdog de tareas RTOS, Caja Negra
              RTC y pruebas temporizadas de periféricos
            </p>
          </div>
        </div>

        <button
          type="button"
          className="btn btn-outline px-3 py-1.5 text-xs"
          onClick={() => void cargarTodo()}
        >
          <RefreshCw className="h-3.5 w-3.5" />
          Actualizar Todo
        </button>
      </div>

      {/* Panel 1: Telemetría Nativa del Chip ESP32 + Gráfico Heap */}
      <div
        data-tour="bios-esp"
        className="grid grid-cols-1 gap-4 xl:grid-cols-12"
      >
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-7">
          <h2 className="mb-3 flex items-center gap-2 text-sm font-bold">
            <Cpu className="h-4 w-4 text-primary" />
            Telemetría Nativa del Silicio ESP32
          </h2>

          {infoEsp ? (
            <div className="grid grid-cols-2 gap-3 text-xs sm:grid-cols-3">
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Chip / Revisión</div>
                <div className="mt-0.5 font-mono font-bold">
                  {infoEsp.modeloChip} (Rev {infoEsp.revisionChip})
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">CPU / Flash</div>
                <div className="mt-0.5 font-mono font-bold">
                  {infoEsp.frecuenciaCpuMhz} MHz ·{" "}
                  {formatearBytes(infoEsp.flashTamanoBytes)}
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">
                  Motivo Último Reset
                </div>
                <div className="mt-0.5 font-mono font-bold text-info">
                  {infoEsp.motivoUltimoReset || "—"}
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Uptime Núcleo</div>
                <div className="mt-0.5 font-mono font-bold">
                  {formatearDuracionSegundos(infoEsp.uptimeSegundos)}
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">
                  Incidentes BIOS RTC
                </div>
                <div
                  className={`mt-0.5 font-mono font-bold ${
                    infoEsp.incidentesBios > 0 ? "text-error" : "text-success"
                  }`}
                >
                  {infoEsp.incidentesBios} incidentes
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">
                  Heap Libre / Total
                </div>
                <div className="mt-0.5 font-mono font-bold text-primary">
                  {formatearBytes(infoEsp.heapLibreBytes)} /{" "}
                  {formatearBytes(infoEsp.heapTotalBytes)}
                </div>
              </div>
            </div>
          ) : (
            <p className="text-xs text-foreground-secondary">
              Consultando registros del chip ESP32...
            </p>
          )}
        </div>

        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-5">
          <h2 className="mb-2 text-sm font-bold">
            Distribución de Memoria RAM Heap (KB)
          </h2>
          <div className="h-44 w-full rounded-lg border border-border bg-background p-2">
            {heapChartData.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={heapChartData}>
                  <CartesianGrid
                    strokeDasharray="3 3"
                    stroke="currentColor"
                    className="opacity-15"
                  />
                  <XAxis dataKey="metrica" tick={{ fontSize: 10 }} />
                  <YAxis tick={{ fontSize: 10 }} unit=" KB" width={52} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "var(--card)",
                      borderColor: "var(--border)",
                      color: "var(--card-foreground)",
                      fontSize: "12px",
                    }}
                  />
                  <Bar
                    dataKey="kb"
                    fill="var(--primary)"
                    radius={[4, 4, 0, 0]}
                    name="Memoria (KB)"
                  />
                </BarChart>
              </ResponsiveContainer>
            ) : (
              <div className="flex h-full items-center justify-center text-xs text-foreground-secondary">
                Sin métricas de Heap.
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Panel 2: Orquestador de Tareas FreeRTOS en Vivo */}
      <div
        data-tour="bios-rtos"
        className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
      >
        <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
          <div className="flex items-center gap-2">
            <Activity className="h-4 w-4 text-primary" />
            <h2 className="text-sm font-bold">
              Orquestador de Tareas FreeRTOS en Vivo ({tareasRtos.length} tareas)
            </h2>
          </div>

          <div className="flex items-center gap-3 text-xs">
            <label className="flex cursor-pointer items-center gap-1.5">
              <input
                type="checkbox"
                checked={autoRtos}
                onChange={(e) => setAutoRtos(e.target.checked)}
              />
              Polling cada 2 s
            </label>
            <button
              type="button"
              className="btn btn-outline px-2.5 py-1 text-xs"
              onClick={() => void cargarTodo()}
            >
              <RefreshCw className="h-3.5 w-3.5" />
            </button>
          </div>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-xs">
            <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
              <tr>
                <th className="px-3 py-2">Tarea</th>
                <th className="px-3 py-2">Core / Prio / Período</th>
                <th className="px-3 py-2">Salud de Stack (Libre / Total)</th>
                <th className="px-3 py-2">Ciclos / Max Exec</th>
                <th className="px-3 py-2">Último Latido</th>
                <th className="px-3 py-2">Resurrecciones</th>
                <th className="px-3 py-2">Estado</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-border">
              {tareasRtos.map((t) => {
                const pctStackLibre =
                  t.stackReservadoBytes > 0
                    ? Math.min(
                        100,
                        (t.stackLibreMinBytes / t.stackReservadoBytes) * 100
                      )
                    : 0;
                return (
                  <tr key={t.nombre} className="hover:bg-secondary/40">
                    <td className="px-3 py-2 font-mono font-bold text-primary">
                      {t.nombre}
                    </td>
                    <td className="px-3 py-2 font-mono text-[11px]">
                      Core {t.nucleo} · P{t.prioridad} · {t.periodoMs} ms
                    </td>
                    <td className="px-3 py-2">
                      <div className="flex items-center justify-between font-mono text-[11px]">
                        <span>
                          {t.stackLibreMinBytes} / {t.stackReservadoBytes} B
                        </span>
                        <span>{pctStackLibre.toFixed(0)}%</span>
                      </div>
                      <div className="mt-1 h-1.5 w-36 overflow-hidden rounded-full bg-secondary">
                        <div
                          className={`h-full ${
                            pctStackLibre < 20
                              ? "bg-error"
                              : pctStackLibre < 40
                                ? "bg-warning"
                                : "bg-success"
                          }`}
                          style={{ width: `${pctStackLibre}%` }}
                        />
                      </div>
                    </td>
                    <td className="px-3 py-2 font-mono text-[11px]">
                      <div>{t.ciclosEjecutados.toLocaleString("es-AR")}</div>
                      <div className="text-foreground-secondary">
                        máx {t.maxTiempoEjecucionUs} µs
                      </div>
                    </td>
                    <td className="px-3 py-2 font-mono">
                      {t.msDesdeUltimoLatido} ms
                    </td>
                    <td className="px-3 py-2 font-mono">
                      <span
                        className={
                          t.reiniciosBios > 0
                            ? "font-bold text-error"
                            : "text-foreground-secondary"
                        }
                      >
                        {t.reiniciosBios}
                      </span>
                    </td>
                    <td className="px-3 py-2">
                      <span
                        className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                          t.activa
                            ? "bg-alert-success-bg text-alert-success-icon"
                            : "bg-alert-err-bg text-alert-err-icon"
                        }`}
                      >
                        {t.enEjecucionPaso
                          ? "EN PASO"
                          : t.activa
                            ? "ACTIVA"
                            : "DETENIDA"}
                      </span>
                    </td>
                  </tr>
                );
              })}
              {tareasRtos.length === 0 && (
                <tr>
                  <td
                    colSpan={7}
                    className="px-3 py-6 text-center text-foreground-secondary"
                  >
                    Sin telemetría de tareas FreeRTOS disponible.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Panel 3 y Panel 4: Caja Negra BIOS + Banco de Control Hardware */}
      <div className="grid grid-cols-1 gap-4 xl:grid-cols-12">
        {/* Panel 3: Caja Negra /db/help.log */}
        <div
          data-tour="bios-log"
          className="flex flex-col rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-5"
        >
          <div className="mb-2 flex items-center justify-between">
            <div className="flex items-center gap-2 text-sm font-bold">
              <FileWarning className="h-4 w-4 text-warning" />
              Caja Negra BIOS (<code>/db/help.log</code>)
            </div>
            {helpLog && (
              <span className="rounded bg-secondary px-2 py-0.5 font-mono text-xs">
                Total incidentes: {helpLog.incidentesTotalesBios}
              </span>
            )}
          </div>
          <pre className="min-h-64 flex-1 overflow-auto rounded-lg border border-border bg-zinc-950 p-3 font-mono text-[11px] leading-relaxed text-amber-300 whitespace-pre-wrap">
            {helpLog?.ultimosRegistrosLog ||
              "Sin incidentes registrados en /db/help.log."}
          </pre>
        </div>

        {/* Panel 4: Banco de Pruebas Temporizadas de Periféricos */}
        <div
          data-tour="bios-hw"
          className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-7"
        >
          <div className="flex flex-wrap items-center justify-between gap-2">
            <h2 className="text-sm font-bold">
              Banco de Pruebas y Control Directo de Periféricos
            </h2>
            <div className="flex flex-wrap items-center gap-2">
              <button
                type="button"
                onClick={() => void handleReiniciarEsp32()}
                className="btn btn-outline btn-error px-3 py-1.5 text-xs font-bold"
              >
                <RotateCcw className="h-3.5 w-3.5" />
                Reiniciar ESP32
              </button>
              <button
                type="button"
                onClick={() => void handleApagadoSeguro()}
                className="btn btn-solid btn-error px-3 py-1.5 text-xs font-bold"
                title="Desmonta la MicroSD de forma segura y entra en deep sleep"
              >
                <Power className="h-3.5 w-3.5" />
                Apagado Seguro
              </button>
            </div>
          </div>

          {/* Tarjetas de estado en vivo de drivers */}
          {hwDirecto && (
            <div className="grid grid-cols-2 gap-2 text-xs sm:grid-cols-5">
              <DriverBadge
                label="MicroSD"
                ok={hwDirecto.sdInicializada}
                textOk="Lista"
                textErr="Error"
              />
              <DriverBadge
                label="OLED I2C"
                ok={hwDirecto.oledInicializada}
                textOk="Lista"
                textErr="Error"
              />
              <DriverBadge
                label="LED Alarma"
                ok={hwDirecto.ledEncendido}
                textOk="ON"
                textErr="OFF"
              />
              <DriverBadge
                label="Buzzer PWM"
                ok={hwDirecto.buzzerEncendido}
                textOk="Activo"
                textErr="Silencio"
              />
              <div className="rounded-lg border border-border bg-background p-2">
                <div className="text-[10px] text-foreground-secondary">
                  Bus 1-Wire
                </div>
                <div className="font-mono text-[11px] font-bold">
                  {LABEL_ESTADO_BUS[hwDirecto.estadoBus1Wire] ??
                    hwDirecto.estadoBus1Wire}
                </div>
                <div className="text-[10px] text-foreground-secondary">
                  {hwDirecto.sensoresDetectadosBus} det. /{" "}
                  {hwDirecto.sensoresAsignados} asig.
                </div>
              </div>
            </div>
          )}

          {/* Acciones Rápidas: Silenciar Alarma y Bus 1-Wire */}
          <div className="flex flex-wrap gap-2 text-xs">
            <button
              type="button"
              disabled={ejecutandoHw}
              onClick={() =>
                void enviarComandoHw(
                  AccionHardwareDirecto.BUZZ_SILENCIAR_ALARMA,
                  "Alarma sonora silenciada."
                )
              }
              className="btn btn-outline btn-error px-3 py-1.5 text-xs"
            >
              <VolumeX className="h-3.5 w-3.5" />
              Silenciar Alarma
            </button>
            <button
              type="button"
              disabled={ejecutandoHw}
              onClick={() =>
                void enviarComandoHw(
                  AccionHardwareDirecto.SENSORES_REESCANEAR,
                  "Bus 1-Wire re-escaneado en caliente."
                )
              }
              className="btn btn-solid btn-info px-3 py-1.5 text-xs font-semibold"
            >
              <Radio className="h-3.5 w-3.5" />
              Re-escanear Bus 1-Wire
            </button>
          </div>

          {/* Prueba Temporizada de LED + Prueba Acústica PWM en Buzzer */}
          <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
            {/* LED Temporizado */}
            <div className="rounded-lg border border-border bg-background p-3 text-xs">
              <div className="mb-2 font-semibold">
                Prueba Visual Temporizada de LED
              </div>
              <div className="flex flex-wrap items-end gap-2.5">
                <div>
                  <label className="mb-1 block">Duración (100–15000 ms)</label>
                  <NumericInput
                    value={duracionLedMs}
                    onChange={setDuracionLedMs}
                    decimals={0}
                    allowNegative={false}
                    min={100}
                    max={15000}
                    className="w-28 rounded border border-border bg-card px-2.5 py-1 font-mono"
                  />
                </div>
                <button
                  type="button"
                  disabled={ejecutandoHw}
                  onClick={() =>
                    void enviarComandoHw(
                      AccionHardwareDirecto.LED_ENCENDER,
                      `LED encendido en prueba durante ${duracionLedMs} ms.`,
                      duracionLedMs
                    )
                  }
                  className="btn btn-solid btn-warning px-3 py-1.5 text-xs font-semibold"
                >
                  <Lightbulb className="h-3.5 w-3.5" />
                  Probar LED
                </button>
              </div>
            </div>

            {/* Buzzer PWM Temporizado */}
            <div className="rounded-lg border border-border bg-background p-3 text-xs">
              <div className="mb-2 font-semibold">
                Prueba Acústica PWM en Buzzer
              </div>
              <div className="flex flex-wrap items-end gap-2.5">
                <div>
                  <label className="mb-1 block">Frecuencia (Hz)</label>
                  <NumericInput
                    value={frecuenciaHz}
                    onChange={setFrecuenciaHz}
                    decimals={0}
                    allowNegative={false}
                    min={100}
                    max={5000}
                    className="w-24 rounded border border-border bg-card px-2.5 py-1 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Duración (ms)</label>
                  <NumericInput
                    value={duracionBuzzMs}
                    onChange={setDuracionBuzzMs}
                    decimals={0}
                    allowNegative={false}
                    min={50}
                    max={5000}
                    className="w-24 rounded border border-border bg-card px-2.5 py-1 font-mono"
                  />
                </div>
                <button
                  type="button"
                  disabled={ejecutandoHw}
                  onClick={() =>
                    void enviarComandoHw(
                      AccionHardwareDirecto.BUZZ_TONO_PRUEBA,
                      `Tono de prueba emitido (${frecuenciaHz} Hz · ${duracionBuzzMs} ms).`,
                      duracionBuzzMs
                    )
                  }
                  className="btn btn-solid btn-primary px-3 py-1.5 text-xs font-semibold"
                >
                  <Volume2 className="h-3.5 w-3.5" />
                  Probar Tono
                </button>
              </div>
            </div>
          </div>

          {/* Publicar Mensaje Temporizado en Pantalla OLED I2C */}
          <div className="rounded-lg border border-border bg-background p-3 text-xs">
            <div className="mb-2 font-semibold">
              Pantalla OLED Física I2C (2 líneas × 21 chars · retorno automático)
            </div>
            <div className="grid grid-cols-1 gap-2 sm:grid-cols-12">
              <div className="sm:col-span-5">
                <label className="mb-1 block">Línea 1 (máx. 21)</label>
                <input
                  type="text"
                  maxLength={21}
                  value={lineaOled1}
                  onChange={(e) => setLineaOled1(e.target.value)}
                  className="w-full rounded border border-border bg-card px-2.5 py-1 font-mono"
                />
              </div>
              <div className="sm:col-span-5">
                <label className="mb-1 block">Línea 2 (máx. 21)</label>
                <input
                  type="text"
                  maxLength={21}
                  value={lineaOled2}
                  onChange={(e) => setLineaOled2(e.target.value)}
                  className="w-full rounded border border-border bg-card px-2.5 py-1 font-mono"
                />
              </div>
              <div className="sm:col-span-2">
                <label className="mb-1 block">Duración (ms)</label>
                <NumericInput
                  value={duracionOledMs}
                  onChange={setDuracionOledMs}
                  decimals={0}
                  allowNegative={false}
                  min={500}
                  max={15000}
                  className="w-full rounded border border-border bg-card px-2 py-1 font-mono"
                />
              </div>
            </div>
            <div className="mt-2.5 flex flex-wrap items-center justify-between gap-2">
              <span className="text-[11px] text-foreground-secondary">
                Al finalizar el tiempo, la OLED vuelve sola a la grilla de
                sensores.
              </span>
              <button
                type="button"
                disabled={ejecutandoHw}
                onClick={() =>
                  void enviarComandoHw(
                    AccionHardwareDirecto.OLED_PUBLICAR_MENSAJE,
                    `Mensaje publicado en OLED durante ${duracionOledMs} ms.`,
                    duracionOledMs
                  )
                }
                className="btn btn-solid btn-success px-3 py-1.5 text-xs font-semibold"
              >
                <Monitor className="h-3.5 w-3.5" />
                Publicar en OLED
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function DriverBadge({
  label,
  ok,
  textOk,
  textErr,
}: {
  label: string;
  ok: boolean;
  textOk: string;
  textErr: string;
}) {
  return (
    <div className="rounded-lg border border-border bg-background p-2">
      <div className="text-[10px] text-foreground-secondary">{label}</div>
      <div
        className={`mt-0.5 inline-flex items-center gap-1 font-semibold ${
          ok ? "text-success" : "text-foreground-secondary"
        }`}
      >
        {ok ? (
          <CheckCircle2 className="h-3.5 w-3.5" />
        ) : (
          <XCircle className="h-3.5 w-3.5" />
        )}
        {ok ? textOk : textErr}
      </div>
    </div>
  );
}