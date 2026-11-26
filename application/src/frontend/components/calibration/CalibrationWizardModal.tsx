import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  X,
  Sliders,
  Snowflake,
  Flame,
  CheckCircle2,
  AlertTriangle,
  VolumeX,
  Clock,
  RefreshCw,
  ArrowRight,
  ArrowLeft,
  Loader2,
  Trash2,
  Check,
  Globe,
  Play,
  Pause,
} from "lucide-react";
import { toast } from "react-toastify";
import { client, safeRpc } from "../../lib/service";
import {
  formatearEpochSegundos,
  redondearDecimal,
} from "../../lib/format";
import {
  calcularRectaCalibracion,
  estimarEbullicionDesdePresion,
  estimarPresionDesdeAltura,
  formatearEcuacionRecta,
  obtenerReferenciasActivas,
  reconstruirTempCrudaDesdeCorregida,
} from "../../lib/calibracionModels";
import { FormulaTooltip } from "../ui/FormulaTooltip";
import { NumericInput } from "../ui/NumericInput";
import {
  CalibracionAmbienteDTO,
  LecturaSensorCliente,
  PerfilSensorCliente,
  ResumenSensorCliente,
} from "#types/rpc.types";

type AlcanceCalibracion = "UNO" | "LOTE";
type PasoWizard = "CONFIG" | "HIELO" | "EBULLICION" | "RESUMEN";

const DURACION_ESTABILIZACION_SEG = 180; // 3 minutos automáticos

interface LecturaEnVivoSensor {
  tempCorregida: number;
  tempCruda: number;
  timestampEsp: number;
  deltaEstabilidad: number;
  estable: boolean;
}

interface CapturasSensor {
  errorInf: number | null;
  errorSup: number | null;
}

export function CalibrationWizardModal({
  open,
  sensores,
  macInicial,
  onClose,
  onCompletado,
  onIrAConfigAmbiente,
}: {
  open: boolean;
  sensores: ResumenSensorCliente[];
  macInicial?: string;
  onClose: () => void;
  onCompletado: () => void;
  onIrAConfigAmbiente?: () => void;
}) {
  const [paso, setPaso] = useState<PasoWizard>("CONFIG");
  const [alcance, setAlcance] = useState<AlcanceCalibracion>("UNO");
  const [macsSeleccionadas, setMacsSeleccionadas] = useState<string[]>([]);

  // Datos del ESP32
  const [ambiente, setAmbiente] = useState<CalibracionAmbienteDTO | null>(null);
  const [perfilesPorMac, setPerfilesPorMac] = useState<
    Record<string, PerfilSensorCliente>
  >({});
  const [cargandoInicio, setCargandoInicio] = useState(false);

  // Telemetría viva (mac UPPER -> lectura)
  const [enVivoPorMac, setEnVivoPorMac] = useState<
    Record<string, LecturaEnVivoSensor>
  >({});
  const enVivoRef = useRef<Record<string, LecturaEnVivoSensor>>({});
  const historialCrudoRef = useRef<Record<string, number[]>>({});
  const ultimoTsEspRef = useRef<number>(0);

  // Ventana de limpieza de censos falsos
  const [tsInicioCalibEsp, setTsInicioCalibEsp] = useState<number>(0);
  const [borrarCensosFalsos, setBorrarCensosFalsos] = useState<boolean>(true);

  // Capturas de los 2 extremos
  const [capturas, setCapturas] = useState<Record<string, CapturasSensor>>({});
  const [autoCapturadoPaso, setAutoCapturadoPaso] = useState<boolean>(false);

  // Cuenta regresiva automática de 3:00 min (180 -> 0 s)
  const [segundosRestantes, setSegundosRestantes] = useState<number>(
    DURACION_ESTABILIZACION_SEG
  );
  const [timerCorriendo, setTimerCorriendo] = useState<boolean>(false);

  const [aplicando, setAplicando] = useState<boolean>(false);
  const [progresoTexto, setProgresoTexto] = useState<string>("");
  const [muteando, setMuteando] = useState<boolean>(false);

  const perfilesRef = useRef<Record<string, PerfilSensorCliente>>({});
  const ambienteRef = useRef<CalibracionAmbienteDTO | null>(null);
  perfilesRef.current = perfilesPorMac;
  ambienteRef.current = ambiente;
  enVivoRef.current = enVivoPorMac;

  const actualizarDesdeLote = useCallback((lote: LecturaSensorCliente[]) => {
    if (!lote || lote.length === 0) return;
    if (lote[0].timestampMs > 0) {
      ultimoTsEspRef.current = lote[0].timestampMs;
    }

    setEnVivoPorMac((prev) => {
      const next = { ...prev };
      for (const l of lote) {
        const mac = l.macHex.toUpperCase();
        const perfil = perfilesRef.current[mac];
        const cruda = perfil
          ? reconstruirTempCrudaDesdeCorregida(
              l.temperaturaCorregida,
              perfil.errorInf,
              perfil.errorSup,
              perfil.offset,
              ambienteRef.current
            )
          : redondearDecimal(l.temperaturaCorregida, 2);

        const hist = [...(historialCrudoRef.current[mac] ?? []), cruda].slice(-6);
        historialCrudoRef.current[mac] = hist;

        const delta = redondearDecimal(Math.max(...hist) - Math.min(...hist), 2);
        next[mac] = {
          tempCorregida: l.temperaturaCorregida,
          tempCruda: cruda,
          timestampEsp: l.timestampMs,
          deltaEstabilidad: delta,
          estable: hist.length >= 4 && delta <= 0.15,
        };
      }
      return next;
    });
  }, []);

  // Reiniciar al abrir
  useEffect(() => {
    if (!open) return;

    setPaso("CONFIG");
    setAlcance("UNO");
    setCapturas({});
    setTsInicioCalibEsp(0);
    setBorrarCensosFalsos(true);
    setTimerCorriendo(false);
    setSegundosRestantes(DURACION_ESTABILIZACION_SEG);
    setAutoCapturadoPaso(false);
    setAplicando(false);
    setProgresoTexto("");
    historialCrudoRef.current = {};

    const inicial = macInicial
      ? macInicial.toUpperCase()
      : sensores[0]?.mac.toUpperCase() || "";
    setMacsSeleccionadas(inicial ? [inicial] : []);

    let montado = true;
    (async () => {
      const [amb, loteInicial] = await Promise.all([
        safeRpc(
          () => client.obtenerCalibracionAmbiente(),
          "obtenerCalibracionAmbiente",
          { silent: true }
        ),
        safeRpc(
          () => client.suscribirSensoresTiempoReal(),
          "suscribirSensoresTiempoReal",
          { silent: true }
        ),
      ]);
      if (!montado) return;
      if (amb) setAmbiente(amb);
      if (loteInicial && loteInicial.length > 0) {
        actualizarDesdeLote(loteInicial);
      }
    })();

    return () => {
      montado = false;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open, macInicial]);

  // Stream en vivo cada 1 s
  useEffect(() => {
    if (!open) return;
    const id = window.setInterval(async () => {
      const lote = await safeRpc(
        () => client.obtenerUltimoLoteTiempoReal(),
        "obtenerUltimoLoteTiempoReal",
        { silent: true }
      );
      if (lote && lote.length > 0) {
        actualizarDesdeLote(lote);
      }
    }, 1000);
    return () => window.clearInterval(id);
  }, [open, actualizarDesdeLote]);

  const capturarTodosParaPaso = useCallback(
    (campo: "errorInf" | "errorSup", fueAutomatico = false) => {
      const mapaVivo = enVivoRef.current;
      let capturados = 0;

      setCapturas((prev) => {
        const next = { ...prev };
        for (const mac of macsSeleccionadas) {
          const live = mapaVivo[mac];
          if (live) {
            next[mac] = {
              ...(next[mac] ?? { errorInf: null, errorSup: null }),
              [campo]: live.tempCruda,
            };
            capturados++;
          }
        }
        return next;
      });

      if (capturados > 0) {
        if (fueAutomatico) {
          setAutoCapturadoPaso(true);
          toast.success(
            campo === "errorInf"
              ? `¡03:00 completados! Se capturó el punto de hielo en ${capturados} sensor(es).`
              : `¡03:00 completados! Se capturó el punto de ebullición en ${capturados} sensor(es).`
          );
        }
      } else {
        toast.warning(
          "Ningún sensor seleccionado reporta lecturas en vivo en este momento."
        );
      }
    },
    [macsSeleccionadas]
  );

  // Cuenta regresiva automática 03:00 -> 00:00 con autocaptura al llegar a cero
  useEffect(() => {
    if (!open || !timerCorriendo) return;
    if (paso !== "HIELO" && paso !== "EBULLICION") return;

    const id = window.setInterval(() => {
      setSegundosRestantes((prev) => {
        if (prev <= 1) {
          window.clearInterval(id);
          setTimerCorriendo(false);
          capturarTodosParaPaso(
            paso === "HIELO" ? "errorInf" : "errorSup",
            true
          );
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => window.clearInterval(id);
  }, [open, timerCorriendo, paso, capturarTodosParaPaso]);

  const { refCong, refEbull } = obtenerReferenciasActivas(ambiente);

  const sensoresSeleccionados = useMemo(() => {
    const set = new Set(macsSeleccionadas.map((m) => m.toUpperCase()));
    return sensores.filter((s) => set.has(s.mac.toUpperCase()));
  }, [sensores, macsSeleccionadas]);

  const toggleMacEnLote = (macUpper: string) => {
    setMacsSeleccionadas((prev) =>
      prev.includes(macUpper)
        ? prev.filter((m) => m !== macUpper)
        : [...prev, macUpper]
    );
  };

  const seleccionarConectados = () => {
    const conectados = sensores
      .map((s) => s.mac.toUpperCase())
      .filter((mac) => Boolean(enVivoPorMac[mac]));
    setMacsSeleccionadas(
      conectados.length > 0
        ? conectados
        : sensores.map((s) => s.mac.toUpperCase())
    );
  };

  const reiniciarTimerPaso = () => {
    setTimerCorriendo(false);
    setSegundosRestantes(DURACION_ESTABILIZACION_SEG);
    setAutoCapturadoPaso(false);
  };

  const iniciarProcedimiento = async () => {
    if (macsSeleccionadas.length === 0) {
      toast.warning("Seleccione al menos un sensor para calibrar.");
      return;
    }

    setCargandoInicio(true);
    const nuevosPerfiles: Record<string, PerfilSensorCliente> = {};
    for (const mac of macsSeleccionadas) {
      const p = await safeRpc(
        () => client.obtenerSensorPorMac(mac),
        `obtenerSensorPorMac(${mac})`,
        { silent: true }
      );
      if (p) nuevosPerfiles[mac.toUpperCase()] = p;
    }

    setPerfilesPorMac(nuevosPerfiles);
    perfilesRef.current = nuevosPerfiles;
    historialCrudoRef.current = {};

    const tsBase =
      ultimoTsEspRef.current > 0
        ? ultimoTsEspRef.current
        : Math.floor(Date.now() / 1000);
    setTsInicioCalibEsp(Math.max(0, tsBase - 10));

    const capInit: Record<string, CapturasSensor> = {};
    for (const mac of macsSeleccionadas) {
      capInit[mac] = { errorInf: null, errorSup: null };
    }
    setCapturas(capInit);

    reiniciarTimerPaso();
    setCargandoInicio(false);
    setPaso("HIELO");
  };

  const silenciarAlarma = async () => {
    setMuteando(true);
    const res = await safeRpc(
      () => client.silenciarAlarmaActiva(),
      "silenciarAlarmaActiva"
    );
    setMuteando(false);
    if (res) toast.info("Bocina silenciada temporalmente.");
  };

  const todosHieloListos = macsSeleccionadas.every(
    (m) => capturas[m]?.errorInf !== null && capturas[m]?.errorInf !== undefined
  );
  const todosEbullicionListos = macsSeleccionadas.every(
    (m) => capturas[m]?.errorSup !== null && capturas[m]?.errorSup !== undefined
  );

  const validarResumen = useMemo(() => {
    const advertencias: string[] = [];
    const errores: string[] = [];

    for (const s of sensoresSeleccionados) {
      const mac = s.mac.toUpperCase();
      const cap = capturas[mac];
      if (!cap || cap.errorInf === null || cap.errorSup === null) {
        errores.push(`${s.nombre || mac}: faltan capturas de hielo/ebullición.`);
        continue;
      }
      if (cap.errorSup - cap.errorInf <= 1.0) {
        errores.push(
          `${s.nombre || mac}: T_ebull (${cap.errorSup}°C) debe ser mayor que T_hielo (${cap.errorInf}°C).`
        );
        continue;
      }
      const recta = calcularRectaCalibracion(
        cap.errorInf,
        cap.errorSup,
        refCong,
        refEbull,
        0
      );
      if (recta.pendienteM < 0.85 || recta.pendienteM > 1.15) {
        advertencias.push(
          `${s.nombre || mac}: pendiente inusual (m = ${recta.pendienteM.toFixed(4)}). Verifique la estabilización térmica.`
        );
      }
    }
    return { errores, advertencias };
  }, [sensoresSeleccionados, capturas, refCong, refEbull]);

  const aplicarCalibracionYLimpiar = async () => {
    if (validarResumen.errores.length > 0) return;

    setAplicando(true);
    let aplicadosOk = 0;

    for (let i = 0; i < sensoresSeleccionados.length; i++) {
      const s = sensoresSeleccionados[i];
      const mac = s.mac.toUpperCase();
      const perfil = perfilesPorMac[mac];
      const cap = capturas[mac];
      if (!cap || cap.errorInf === null || cap.errorSup === null) continue;

      setProgresoTexto(
        `Aplicando calibración ${i + 1}/${sensoresSeleccionados.length} (${s.nombre || mac})...`
      );

      const errorInf = redondearDecimal(cap.errorInf, 2);
      const errorSup = redondearDecimal(cap.errorSup, 2);
      const recta = calcularRectaCalibracion(
        errorInf,
        errorSup,
        refCong,
        refEbull,
        0
      );

      const ok = await safeRpc(
        () =>
          client.actualizarCalibracionSensor(
            s.mac,
            perfil ? perfil.umbralSup : s.umbralSup,
            perfil ? perfil.umbralInf : s.umbralInf,
            errorSup,
            errorInf,
            redondearDecimal(recta.ordenadaB, 2)
          ),
        `wizardCalibrar(${s.mac})`
      );
      if (ok) aplicadosOk++;
    }

    let censosBorrados = 0;
    let eventosBorrados = 0;

    if (borrarCensosFalsos && tsInicioCalibEsp > 0 && aplicadosOk > 0) {
      const tsFin =
        (ultimoTsEspRef.current > 0
          ? ultimoTsEspRef.current
          : Math.floor(Date.now() / 1000)) + 15;

      for (let i = 0; i < sensoresSeleccionados.length; i++) {
        const s = sensoresSeleccionados[i];
        setProgresoTexto(
          `Limpiando censos de calibración ${i + 1}/${sensoresSeleccionados.length} (${s.nombre || s.mac})...`
        );

        const recientes = await safeRpc(
          () =>
            client.listarCensosPaginados({
              idSensor: s.idSensor,
              limiteRegistros: 32,
              offsetRegistros: 0,
            }),
          "listarCensosParaLimpieza",
          { silent: true }
        );
        const idsCensoEnVentana = new Set(
          (recientes ?? [])
            .filter(
              (r) =>
                r.fechaCreacion >= tsInicioCalibEsp && r.fechaCreacion <= tsFin
            )
            .map((r) => r.idCenso)
        );

        if (idsCensoEnVentana.size > 0) {
          const auds = await safeRpc(
            () => client.listarAuditoriaSensorPorMac(s.mac, 32),
            "listarAuditoriaParaLimpieza",
            { silent: true }
          );
          for (const ev of auds ?? []) {
            if (idsCensoEnVentana.has(ev.idCenso)) {
              await safeRpc(
                () => client.eliminarAuditoriaSensorPorId(ev.idAuditoriaSensor),
                "eliminarAuditoriaCalib",
                { silent: true }
              );
              eventosBorrados++;
            }
          }
        }

        const delRes = await safeRpc(
          () =>
            client.ejecutarSqlTerminal(
              `DELETE FROM Censo WHERE id_sensor = ${s.idSensor} AND fecha_creacion >= ${tsInicioCalibEsp} AND fecha_creacion <= ${tsFin};`
            ),
          "limpiarCensosCalibracionSql",
          { silent: true }
        );

        if (delRes?.exito) {
          censosBorrados += delRes.filasAfectadas;
        } else if (idsCensoEnVentana.size > 0) {
          for (const idCenso of idsCensoEnVentana) {
            await safeRpc(
              () => client.eliminarCensoPorId(idCenso),
              "eliminarCensoFallback",
              { silent: true }
            );
            censosBorrados++;
          }
        }
      }
    }

    await safeRpc(() => client.invalidarCache(), "invalidarCache", {
      silent: true,
    });

    setAplicando(false);
    setProgresoTexto("");

    if (aplicadosOk > 0) {
      const detalleLimpieza = borrarCensosFalsos
        ? ` · ${censosBorrados} censos y ${eventosBorrados} alertas de prueba descartados.`
        : ".";
      toast.success(
        `Calibración 2 puntos aplicada en caliente y en BD para ${aplicadosOk} sensor(es)${detalleLimpieza}`
      );
      onCompletado();
      onClose();
    }
  };

  if (!open) return null;

  const mmTimer = String(Math.floor(segundosRestantes / 60)).padStart(2, "0");
  const ssTimer = String(segundosRestantes % 60).padStart(2, "0");
  const progresoTimerPct =
    ((DURACION_ESTABILIZACION_SEG - segundosRestantes) /
      DURACION_ESTABILIZACION_SEG) *
    100;

  const pasosOrden: Array<{ id: PasoWizard; label: string }> = [
    { id: "CONFIG", label: "1. Sensores" },
    { id: "HIELO", label: "2. Frío Extremo (3 min)" },
    { id: "EBULLICION", label: "3. Calor Extremo (3 min)" },
    { id: "RESUMEN", label: "4. Ecuación y Aplicación" },
  ];

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4 backdrop-blur-[2px]">
      <div className="flex max-h-[90vh] w-full max-w-4xl flex-col overflow-hidden rounded-xl border border-border bg-card text-card-foreground shadow-2xl">
        {/* Cabecera */}
        <div className="flex items-center justify-between border-b border-border px-5 py-3.5">
          <div className="flex items-center gap-2.5">
            <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-warning-light text-warning">
              <Sliders className="h-5 w-5" />
            </div>
            <div>
              <h2 className="text-base font-bold">
                Asistente de Calibración Automática (2 Puntos · 3 + 3 min)
              </h2>
              <p className="text-xs text-foreground-secondary">
                Captura automática en frío y calor extremo para 1 o múltiples
                sensores · Conserva intactos los umbrales TL / TH
              </p>
            </div>
          </div>

          <button
            type="button"
            disabled={aplicando}
            onClick={onClose}
            className="btn btn-icon btn-secondary"
            title="Cerrar asistente"
          >
            <X className="h-4 w-4" />
          </button>
        </div>

        {/* Barra de pasos */}
        <div className="flex flex-wrap items-center gap-2 border-b border-border bg-secondary/40 px-5 py-2.5 text-xs">
          {pasosOrden.map((p, idx) => {
            const idxActual = pasosOrden.findIndex((x) => x.id === paso);
            const completado = idx < idxActual;
            const activo = p.id === paso;
            return (
              <div
                key={p.id}
                className={`inline-flex items-center gap-1.5 rounded-full px-3 py-1 font-semibold ${
                  activo
                    ? "bg-primary text-primary-foreground"
                    : completado
                      ? "bg-alert-success-bg text-alert-success-icon"
                      : "bg-card text-foreground-secondary"
                }`}
              >
                {completado && <Check className="h-3 w-3" />}
                <span>{p.label}</span>
              </div>
            );
          })}
        </div>

        {/* Contenido */}
        <div className="flex-1 space-y-4 overflow-y-auto p-5 text-xs">
          {/* PASO 1: SELECCIÓN DE SENSORES Y REFERENCIAS */}
          {paso === "CONFIG" && (
            <div className="space-y-4">
              <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
                <button
                  type="button"
                  onClick={() => {
                    setAlcance("UNO");
                    setMacsSeleccionadas((prev) =>
                      prev.length > 0
                        ? [prev[0]]
                        : sensores[0]
                          ? [sensores[0].mac.toUpperCase()]
                          : []
                    );
                  }}
                  className={`rounded-lg border p-3.5 text-left transition-all ${
                    alcance === "UNO"
                      ? "border-primary bg-primary-light ring-1 ring-primary/40"
                      : "border-border bg-background hover:border-primary/50"
                  }`}
                >
                  <div className="text-sm font-bold">Calibrar 1 sensor</div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Para calibrar una sonda individual o incorporar un sensor
                    nuevo al grupo.
                  </p>
                </button>

                <button
                  type="button"
                  onClick={() => {
                    setAlcance("LOTE");
                    seleccionarConectados();
                  }}
                  className={`rounded-lg border p-3.5 text-left transition-all ${
                    alcance === "LOTE"
                      ? "border-primary bg-primary-light ring-1 ring-primary/40"
                      : "border-border bg-background hover:border-primary/50"
                  }`}
                >
                  <div className="text-sm font-bold">
                    Calibrar múltiples sensores a la vez
                  </div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Sumerja todas las sondas juntas en hielo y agua hirviendo:
                    al finalizar cada contador de 3 minutos se capturan todas en
                    simultáneo.
                  </p>
                </button>
              </div>

              {/* Lista de sensores */}
              <div className="rounded-lg border border-border bg-background p-3.5">
                <div className="mb-2 flex items-center justify-between">
                  <span className="font-bold">
                    {alcance === "UNO"
                      ? "Seleccione el sensor a calibrar:"
                      : `Sensores incluidos en el lote (${macsSeleccionadas.length} seleccionados):`}
                  </span>
                  {alcance === "LOTE" && (
                    <button
                      type="button"
                      onClick={seleccionarConectados}
                      className="font-semibold text-primary underline"
                    >
                      Seleccionar todos los conectados en vivo
                    </button>
                  )}
                </div>

                <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                  {sensores.map((s) => {
                    const mac = s.mac.toUpperCase();
                    const marcado = macsSeleccionadas.includes(mac);
                    const live = enVivoPorMac[mac];
                    return (
                      <label
                        key={mac}
                        className={`flex cursor-pointer items-center justify-between gap-2 rounded-md border p-2.5 transition-colors ${
                          marcado
                            ? "border-primary bg-primary-light/60"
                            : "border-border bg-card hover:bg-secondary/40"
                        }`}
                      >
                        <div className="flex min-w-0 items-center gap-2">
                          <input
                            type={alcance === "UNO" ? "radio" : "checkbox"}
                            name="wizard-sensor"
                            checked={marcado}
                            onChange={() => {
                              if (alcance === "UNO")
                                setMacsSeleccionadas([mac]);
                              else toggleMacEnLote(mac);
                            }}
                          />
                          <div className="min-w-0">
                            <div className="truncate font-semibold">
                              #{s.idSensor} · {s.nombre || "Sensor DS18B20"}
                            </div>
                            <div className="font-mono text-[10px] text-foreground-secondary">
                              {mac}
                            </div>
                          </div>
                        </div>
                        <span
                          className={`shrink-0 rounded px-2 py-0.5 font-mono text-[10px] font-semibold ${
                            live
                              ? "bg-alert-success-bg text-alert-success-icon"
                              : "bg-secondary text-foreground-secondary"
                          }`}
                        >
                          {live
                            ? `${live.tempCorregida.toFixed(2)} °C`
                            : "Offline"}
                        </span>
                      </label>
                    );
                  })}
                </div>
              </div>

              {/* Referencias ambientales globales con globo de ecuación */}
              <div className="flex flex-wrap items-center justify-between gap-2 rounded-lg border border-border bg-secondary/40 px-3.5 py-2.5">
                <div className="flex flex-wrap items-center gap-2">
                  <Globe className="h-4 w-4 shrink-0 text-info" />
                  <span className="font-semibold">Referencias globales:</span>
                  <span className="font-mono">
                    Hielo = {refCong.toFixed(2)} °C · Ebullición ={" "}
                    {refEbull.toFixed(2)} °C
                  </span>
                  {ambiente && (
                    <FormulaTooltip
                      titulo="Modelo Atmosférico Global"
                      archivoModelo="ModeloAtmosfericoBarometrico.hpp"
                      ecuacion={
                        "P(h) = 1013.25 · (1 − 2.25577×10⁻⁵ · h)^5.25588\nT_ebull = [1/373.15 − (R/ΔH)·ln(P/1013.25)]⁻¹ − 273.15"
                      }
                      sustitucion={`h = ${ambiente.altura} m → P ≈ ${estimarPresionDesdeAltura(ambiente.altura).toFixed(2)} hPa | P_cfg = ${ambiente.presionAtmosferica} hPa → T_ebull ≈ ${estimarEbullicionDesdePresion(ambiente.presionAtmosferica).toFixed(2)} °C`}
                      descripcion="Referencias teóricas contra las que se ajusta la recta de 2 puntos de todos los sensores."
                    />
                  )}
                </div>
                {onIrAConfigAmbiente && (
                  <button
                    type="button"
                    onClick={() => {
                      onClose();
                      onIrAConfigAmbiente();
                    }}
                    className="font-semibold text-info underline"
                  >
                    Ajustar en Configuración
                  </button>
                )}
              </div>
            </div>
          )}

          {/* PASO 2: FRÍO EXTREMO (HIELO · 3 MIN AUTOMÁTICO) */}
          {paso === "HIELO" && (
            <PasoBanoTermico
              titulo={`Paso 1 · Frío Extremo: Baño de Hielo (Ref. ${refCong.toFixed(2)} °C)`}
              icono={<Snowflake className="h-5 w-5 text-info" />}
              recomendacionExtra="Consejo práctico: apenas inicies el contador de 3 minutos, poné a hervir el agua en una pava o recipiente para tenerla lista justo al terminar este paso."
              instrucciones={[
                "Llená un vaso térmico con hielo picado y agua fría (mitad y mitad) y revolvé 30 segundos.",
                "Sumergí la(s) sonda(s) en el centro del vaso sin tocar las paredes ni el fondo.",
                "Pulsá 'Iniciar 03:00': al llegar a 00:00 se guardarán automáticamente las lecturas crudas de todos los sensores.",
              ]}
              sensores={sensoresSeleccionados}
              enVivoPorMac={enVivoPorMac}
              valoresCapturados={Object.fromEntries(
                macsSeleccionadas.map((m) => [m, capturas[m]?.errorInf ?? null])
              )}
              etiquetaValor="T_hielo capturada (°C)"
              timerTexto={`${mmTimer}:${ssTimer}`}
              progresoPct={progresoTimerPct}
              timerCorriendo={timerCorriendo}
              autoCapturado={autoCapturadoPaso}
              onToggleTimer={() => {
                if (segundosRestantes === 0) {
                  setSegundosRestantes(DURACION_ESTABILIZACION_SEG);
                  setAutoCapturadoPaso(false);
                }
                setTimerCorriendo((c) => !c);
              }}
              onResetTimer={reiniciarTimerPaso}
              onCapturarAhora={() => capturarTodosParaPaso("errorInf", false)}
              onCambiarValor={(mac, val) =>
                setCapturas((prev) => ({
                  ...prev,
                  [mac]: {
                    ...(prev[mac] ?? { errorInf: null, errorSup: null }),
                    errorInf: val,
                  },
                }))
              }
              muteando={muteando}
              onMute={silenciarAlarma}
            />
          )}

          {/* PASO 3: CALOR EXTREMO (AGUA HIRVIENDO · 3 MIN AUTOMÁTICO) */}
          {paso === "EBULLICION" && (
            <PasoBanoTermico
              titulo={`Paso 2 · Calor Extremo: Agua en Ebullición (Ref. ${refEbull.toFixed(2)} °C)`}
              icono={<Flame className="h-5 w-5 text-error" />}
              recomendacionExtra="Si salta la alarma de temperatura alta mientras las sondas están en el agua hirviendo, usá el botón 'Mute Bocina'. Las alertas y censos de prueba se limpiarán al finalizar."
              instrucciones={[
                "Mantené el agua en ebullición franca (burbujeo continuo).",
                "Sumergí la(s) sonda(s) en el centro del agua sin tocar el fondo caliente del recipiente.",
                "Pulsá 'Iniciar 03:00': al finalizar la cuenta regresiva se capturará automáticamente T_ebull de todos los sensores.",
              ]}
              sensores={sensoresSeleccionados}
              enVivoPorMac={enVivoPorMac}
              valoresCapturados={Object.fromEntries(
                macsSeleccionadas.map((m) => [m, capturas[m]?.errorSup ?? null])
              )}
              etiquetaValor="T_ebull capturada (°C)"
              timerTexto={`${mmTimer}:${ssTimer}`}
              progresoPct={progresoTimerPct}
              timerCorriendo={timerCorriendo}
              autoCapturado={autoCapturadoPaso}
              onToggleTimer={() => {
                if (segundosRestantes === 0) {
                  setSegundosRestantes(DURACION_ESTABILIZACION_SEG);
                  setAutoCapturadoPaso(false);
                }
                setTimerCorriendo((c) => !c);
              }}
              onResetTimer={reiniciarTimerPaso}
              onCapturarAhora={() => capturarTodosParaPaso("errorSup", false)}
              onCambiarValor={(mac, val) =>
                setCapturas((prev) => ({
                  ...prev,
                  [mac]: {
                    ...(prev[mac] ?? { errorInf: null, errorSup: null }),
                    errorSup: val,
                  },
                }))
              }
              muteando={muteando}
              onMute={silenciarAlarma}
            />
          )}

          {/* PASO 4: RESUMEN DE ECUACIÓN Y LIMPIEZA */}
          {paso === "RESUMEN" && (
            <div className="space-y-4">
              <div className="flex flex-wrap items-center justify-between gap-2 rounded-lg border border-alert-success-border bg-alert-success-bg px-3.5 py-2.5 text-alert-success-icon">
                <span>
                  <strong>Protección de alarmas:</strong> los umbrales TL / TH
                  de cada sensor permanecen intactos.
                </span>
                <FormulaTooltip
                  titulo="Modelo Recta Lineal de 2 Puntos"
                  archivoModelo="ModeloCalibracionDosPuntos.hpp"
                  ecuacion={
                    "m = (T_ref,sup − T_ref,inf) / (T_ebull − T_hielo)\nb = T_ref,inf − m · T_hielo\nT_cal = m · T_cruda + b"
                  }
                  sustitucion={`Referencias: T_ref,inf = ${refCong.toFixed(2)} °C | T_ref,sup = ${refEbull.toFixed(2)} °C`}
                  descripcion="Al aplicar, el ESP32 actualiza m y b en RAM al instante y guarda T_hielo, T_ebull y el desplazamiento b en la base de datos."
                />
              </div>

              {validarResumen.errores.length > 0 && (
                <div className="space-y-1 rounded-lg border border-alert-err-border bg-alert-err-bg p-3 text-alert-err-icon">
                  <div className="flex items-center gap-1.5 font-bold">
                    <AlertTriangle className="h-4 w-4" />
                    Corrija estos errores antes de aplicar:
                  </div>
                  {validarResumen.errores.map((e, i) => (
                    <div key={i}>• {e}</div>
                  ))}
                </div>
              )}

              {validarResumen.advertencias.length > 0 && (
                <div className="space-y-1 rounded-lg border border-alert-warn-border bg-alert-warn-bg p-3 text-alert-warn-icon">
                  <div className="flex items-center gap-1.5 font-bold">
                    <AlertTriangle className="h-4 w-4" />
                    Advertencias metrológicas:
                  </div>
                  {validarResumen.advertencias.map((w, i) => (
                    <div key={i}>• {w}</div>
                  ))}
                </div>
              )}

              <div className="overflow-x-auto rounded-lg border border-border">
                <table className="w-full text-left text-xs">
                  <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                    <tr>
                      <th className="px-3 py-2">Sensor</th>
                      <th className="px-3 py-2">T_hielo / T_ebull</th>
                      <th className="px-3 py-2">Pendiente (m)</th>
                      <th className="px-3 py-2">Desplazamiento (b)</th>
                      <th className="px-3 py-2">Ecuación Resultante</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-border">
                    {sensoresSeleccionados.map((s) => {
                      const mac = s.mac.toUpperCase();
                      const cap = capturas[mac];
                      const eInf = cap?.errorInf ?? 0;
                      const eSup = cap?.errorSup ?? 0;
                      const recta = calcularRectaCalibracion(
                        eInf,
                        eSup,
                        refCong,
                        refEbull,
                        0
                      );
                      return (
                        <tr key={mac}>
                          <td className="px-3 py-2">
                            <div className="font-semibold">
                              #{s.idSensor} · {s.nombre || "Sensor"}
                            </div>
                            <div className="font-mono text-[10px] text-foreground-secondary">
                              {mac}
                            </div>
                          </td>
                          <td className="px-3 py-2 font-mono font-semibold">
                            {eInf.toFixed(2)} / {eSup.toFixed(2)} °C
                          </td>
                          <td className="px-3 py-2 font-mono">
                            {recta.pendienteM.toFixed(4)}
                          </td>
                          <td className="px-3 py-2 font-mono font-bold text-primary">
                            {recta.ordenadaB >= 0
                              ? `+${recta.ordenadaB.toFixed(2)}`
                              : recta.ordenadaB.toFixed(2)}{" "}
                            °C
                          </td>
                          <td className="px-3 py-2 font-mono text-[11px]">
                            <FormulaTooltip
                              titulo={`Recta de #${s.idSensor} (${s.nombre || mac})`}
                              archivoModelo="ModeloCalibracionDosPuntos.hpp"
                              ecuacion={formatearEcuacionRecta(recta)}
                              sustitucion={`m = (${refEbull.toFixed(2)} − ${refCong.toFixed(2)}) / (${eSup.toFixed(2)} − ${eInf.toFixed(2)}) = ${recta.pendienteM.toFixed(4)} | b = ${refCong.toFixed(2)} − m·${eInf.toFixed(2)} = ${recta.ordenadaB.toFixed(2)}`}
                              descripcion="Verifica exactitud: al ingresar T_hielo devuelve T_ref,inf y al ingresar T_ebull devuelve T_ref,sup."
                            >
                              <span className="underline decoration-dotted">
                                {formatearEcuacionRecta(recta)}
                              </span>
                            </FormulaTooltip>
                          </td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>

              <div className="rounded-lg border border-border bg-background p-3.5">
                <label className="flex cursor-pointer items-start gap-2.5">
                  <input
                    type="checkbox"
                    checked={borrarCensosFalsos}
                    onChange={(e) => setBorrarCensosFalsos(e.target.checked)}
                    className="mt-0.5"
                  />
                  <div>
                    <div className="flex items-center gap-1.5 font-bold">
                      <Trash2 className="h-3.5 w-3.5 text-error" />
                      Descartar censos y alertas falsas generadas durante la
                      calibración (recomendado)
                    </div>
                    <p className="mt-0.5 text-[11px] text-foreground-secondary">
                      Borra únicamente los registros de <code>Censo</code> y{" "}
                      <code>AuditoriaSensores</code> de los sensores
                      seleccionados creados desde{" "}
                      <span className="font-mono text-foreground">
                        {formatearEpochSegundos(tsInicioCalibEsp)}
                      </span>{" "}
                      hasta ahora.
                    </p>
                  </div>
                </label>
              </div>

              {aplicando && (
                <div className="flex items-center gap-2 rounded-lg border border-primary/40 bg-primary-light px-3.5 py-2.5 font-semibold text-primary">
                  <Loader2 className="h-4 w-4 animate-spin" />
                  <span>{progresoTexto || "Aplicando calibración..."}</span>
                </div>
              )}
            </div>
          )}
        </div>

        {/* Pie */}
        <div className="flex items-center justify-between border-t border-border bg-secondary/30 px-5 py-3">
          <div>
            {paso !== "CONFIG" && (
              <button
                type="button"
                disabled={aplicando}
                onClick={() => {
                  reiniciarTimerPaso();
                  if (paso === "HIELO") setPaso("CONFIG");
                  else if (paso === "EBULLICION") setPaso("HIELO");
                  else if (paso === "RESUMEN") setPaso("EBULLICION");
                }}
                className="btn btn-outline px-3 py-1.5 text-xs"
              >
                <ArrowLeft className="h-3.5 w-3.5" />
                Volver
              </button>
            )}
          </div>

          <div className="flex items-center gap-2">
            <button
              type="button"
              disabled={aplicando}
              onClick={onClose}
              className="btn btn-outline px-3 py-1.5 text-xs"
            >
              Cancelar
            </button>

            {paso === "CONFIG" && (
              <button
                type="button"
                disabled={cargandoInicio || macsSeleccionadas.length === 0}
                onClick={() => void iniciarProcedimiento()}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                {cargandoInicio ? (
                  <Loader2 className="h-3.5 w-3.5 animate-spin" />
                ) : (
                  <ArrowRight className="h-3.5 w-3.5" />
                )}
                Comenzar en Frío Extremo
              </button>
            )}

            {paso === "HIELO" && (
              <button
                type="button"
                disabled={!todosHieloListos}
                onClick={() => {
                  reiniciarTimerPaso();
                  setPaso("EBULLICION");
                }}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                Siguiente: Calor Extremo (Agua Hirviendo)
                <ArrowRight className="h-3.5 w-3.5" />
              </button>
            )}

            {paso === "EBULLICION" && (
              <button
                type="button"
                disabled={!todosEbullicionListos}
                onClick={() => {
                  setTimerCorriendo(false);
                  setPaso("RESUMEN");
                }}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                Revisar Ecuación y Aplicar
                <ArrowRight className="h-3.5 w-3.5" />
              </button>
            )}

            {paso === "RESUMEN" && (
              <button
                type="button"
                disabled={aplicando || validarResumen.errores.length > 0}
                onClick={() => void aplicarCalibracionYLimpiar()}
                className="btn btn-solid btn-success px-4 py-1.5 text-xs font-bold"
              >
                {aplicando ? (
                  <Loader2 className="h-3.5 w-3.5 animate-spin" />
                ) : (
                  <CheckCircle2 className="h-3.5 w-3.5" />
                )}
                Calibrar y Guardar ({sensoresSeleccionados.length})
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

function PasoBanoTermico({
  titulo,
  icono,
  recomendacionExtra,
  instrucciones,
  sensores,
  enVivoPorMac,
  valoresCapturados,
  etiquetaValor,
  timerTexto,
  progresoPct,
  timerCorriendo,
  autoCapturado,
  onToggleTimer,
  onResetTimer,
  onCapturarAhora,
  onCambiarValor,
  muteando,
  onMute,
}: {
  titulo: string;
  icono: React.ReactNode;
  recomendacionExtra: string;
  instrucciones: string[];
  sensores: ResumenSensorCliente[];
  enVivoPorMac: Record<string, LecturaEnVivoSensor>;
  valoresCapturados: Record<string, number | null>;
  etiquetaValor: string;
  timerTexto: string;
  progresoPct: number;
  timerCorriendo: boolean;
  autoCapturado: boolean;
  onToggleTimer: () => void;
  onResetTimer: () => void;
  onCapturarAhora: () => void;
  onCambiarValor: (macUpper: string, val: number) => void;
  muteando: boolean;
  onMute: () => void;
}) {
  return (
    <div className="space-y-3">
      <div className="rounded-lg border border-border bg-background p-3.5 space-y-2.5">
        <div className="flex flex-wrap items-start justify-between gap-3">
          <div className="space-y-1.5">
            <div className="flex items-center gap-2 text-sm font-bold">
              {icono}
              <span>{titulo}</span>
            </div>
            <ol className="list-decimal space-y-0.5 pl-4 text-xs text-foreground-secondary">
              {instrucciones.map((txt, idx) => (
                <li key={idx}>{txt}</li>
              ))}
            </ol>
          </div>

          <button
            type="button"
            disabled={muteando}
            onClick={onMute}
            className="btn btn-outline btn-error px-2.5 py-1.5 text-xs shrink-0"
            title="Silenciar bocina física si saltó alarma de umbral"
          >
            <VolumeX className="h-3.5 w-3.5" />
            Mute Bocina
          </button>
        </div>

        <div className="rounded-md border border-info/40 bg-info-light/40 px-3 py-2 text-[11px] text-foreground">
          {recomendacionExtra}
        </div>

        {/* Contador regresivo automático 03:00 -> 00:00 */}
        <div className="rounded-lg border border-border bg-card p-3">
          <div className="flex flex-wrap items-center justify-between gap-2">
            <div className="flex items-center gap-2">
              <Clock className="h-4 w-4 text-primary" />
              <span className="font-mono text-lg font-bold tabular-nums">
                {timerTexto}
              </span>
              <span className="text-[11px] text-foreground-secondary">
                {autoCapturado
                  ? "Captura automática completada"
                  : timerCorriendo
                    ? "Estabilizando… capturará automáticamente al llegar a 00:00"
                    : "Listo para iniciar cuenta regresiva de 3 minutos"}
              </span>
            </div>

            <div className="flex items-center gap-1.5">
              <button
                type="button"
                onClick={onToggleTimer}
                className="btn btn-solid btn-primary px-3 py-1.5 text-xs font-bold"
              >
                {timerCorriendo ? (
                  <>
                    <Pause className="h-3.5 w-3.5" />
                    Pausar
                  </>
                ) : (
                  <>
                    <Play className="h-3.5 w-3.5" />
                    {progresoPct > 0 && progresoPct < 100
                      ? "Reanudar"
                      : "Iniciar 03:00"}
                  </>
                )}
              </button>
              <button
                type="button"
                onClick={onResetTimer}
                className="btn btn-outline px-2 py-1.5 text-xs"
                title="Reiniciar contador a 03:00"
              >
                <RefreshCw className="h-3.5 w-3.5" />
              </button>
              <button
                type="button"
                onClick={onCapturarAhora}
                className="btn btn-outline btn-warning px-2.5 py-1.5 text-xs font-semibold"
                title="Capturar manualmente antes de que termine el contador"
              >
                Capturar ahora
              </button>
            </div>
          </div>

          <div className="mt-2 h-2 w-full overflow-hidden rounded-full bg-secondary">
            <div
              className={`h-full transition-all duration-300 ${
                autoCapturado ? "bg-success" : "bg-primary"
              }`}
              style={{ width: `${Math.min(100, Math.max(0, progresoPct))}%` }}
            />
          </div>
        </div>
      </div>

      {/* Tabla en vivo de sensores seleccionados */}
      <div className="overflow-x-auto rounded-lg border border-border">
        <table className="w-full text-left text-xs">
          <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
            <tr>
              <th className="px-3 py-2">Sensor</th>
              <th className="px-3 py-2">Temp. Actual</th>
              <th className="px-3 py-2">Temp. Cruda Real</th>
              <th className="px-3 py-2">Estabilidad</th>
              <th className="px-3 py-2">{etiquetaValor}</th>
              <th className="px-3 py-2 text-right">Manual</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-border">
            {sensores.map((s) => {
              const mac = s.mac.toUpperCase();
              const live = enVivoPorMac[mac];
              const capturado = valoresCapturados[mac];
              return (
                <tr key={mac}>
                  <td className="px-3 py-2">
                    <div className="font-semibold">
                      #{s.idSensor} · {s.nombre || "Sensor"}
                    </div>
                    <div className="font-mono text-[10px] text-foreground-secondary">
                      {mac}
                    </div>
                  </td>
                  <td className="px-3 py-2 font-mono text-foreground-secondary">
                    {live ? `${live.tempCorregida.toFixed(2)} °C` : "—"}
                  </td>
                  <td className="px-3 py-2 font-mono text-sm font-bold text-primary">
                    {live ? `${live.tempCruda.toFixed(2)} °C` : "Desconectado"}
                  </td>
                  <td className="px-3 py-2">
                    {!live ? (
                      <span className="rounded bg-secondary px-2 py-0.5 text-[10px] text-foreground-secondary">
                        Sin señal
                      </span>
                    ) : live.estable ? (
                      <span className="rounded bg-alert-success-bg px-2 py-0.5 text-[10px] font-bold text-alert-success-icon">
                        ESTABLE (Δ{live.deltaEstabilidad.toFixed(2)}°C)
                      </span>
                    ) : (
                      <span className="rounded bg-alert-warn-bg px-2 py-0.5 text-[10px] font-semibold text-alert-warn-icon">
                        Estabilizando (Δ{live.deltaEstabilidad.toFixed(2)}°C)
                      </span>
                    )}
                  </td>
                  <td className="px-3 py-2">
                    <NumericInput
                      value={capturado ?? 0}
                      onChange={(v) => onCambiarValor(mac, v)}
                      decimals={2}
                      min={-55}
                      max={126}
                      placeholder="Pendiente"
                      className={`w-24 rounded border px-2 py-1 font-mono ${
                        capturado !== null
                          ? "border-success bg-alert-success-bg/30 font-bold"
                          : "border-border bg-background"
                      }`}
                    />
                  </td>
                  <td className="px-3 py-2 text-right">
                    <button
                      type="button"
                      disabled={!live}
                      onClick={() => {
                        if (live) onCambiarValor(mac, live.tempCruda);
                      }}
                      className="btn btn-outline px-2.5 py-1 text-xs"
                    >
                      Capturar
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}