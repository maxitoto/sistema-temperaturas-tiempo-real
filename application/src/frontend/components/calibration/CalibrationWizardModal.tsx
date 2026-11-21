import { useEffect, useMemo, useRef, useState } from "react";
import {
  X,
  Sliders,
  Snowflake,
  Flame,
  Thermometer,
  CheckCircle2,
  AlertTriangle,
  VolumeX,
  Clock,
  RefreshCw,
  ArrowRight,
  ArrowLeft,
  Loader2,
  Trash2,
  Sparkles,
  Check,
  Globe,
} from "lucide-react";
import { toast } from "react-toastify";
import { client, safeRpc } from "../../lib/service";
import {
  formatearEpochSegundos,
  redondearDecimal,
} from "../../lib/format";
import { NumericInput } from "../ui/NumericInput";
import {
  CalibracionAmbienteDTO,
  LecturaSensorCliente,
  PerfilSensorCliente,
  ResumenSensorCliente,
} from "#types/rpc.types";

type AlcanceCalibracion = "UNO" | "LOTE";
type MetodoCalibracion = "DOS_PUNTOS" | "UN_PUNTO";
type PasoWizard =
  | "CONFIG"
  | "HIELO"
  | "EBULLICION"
  | "PATRON"
  | "RESUMEN";

interface LecturaEnVivoSensor {
  tempCorregida: number;
  tempCruda: number;
  timestampEsp: number;
  muestrasCrudas: number[];
  deltaEstabilidad: number;
  estable: boolean;
}

interface CapturasSensor {
  errorInf: number | null;
  errorSup: number | null;
  offsetUnPunto: number | null;
}

/**
 * Invierte la recta de calibración exacta de Sensor.hpp:
 *   T_corr = m * T_cruda + b   =>   T_cruda = (T_corr - b) / m
 */
function reconstruirTemperaturaCruda(
  tempCorregida: number,
  perfil: PerfilSensorCliente | undefined,
  ambiente: CalibracionAmbienteDTO | null
): number {
  if (!perfil) return redondearDecimal(tempCorregida, 2);

  const refInf = ambiente?.activa ? ambiente.tempCongelacionRef : 0;
  const refSup = ambiente?.activa ? ambiente.tempEbullicionRef : 100;
  const deltaMedido = perfil.errorSup - perfil.errorInf;

  let m = 1;
  let b = perfil.offset;

  if (Math.abs(deltaMedido) > 1.0 && Math.abs(refSup - refInf) > 1.0) {
    m = (refSup - refInf) / deltaMedido;
    b = refInf - m * perfil.errorInf + perfil.offset;
  }

  if (!Number.isFinite(m) || Math.abs(m) < 0.001) {
    return redondearDecimal(tempCorregida, 2);
  }
  return redondearDecimal((tempCorregida - b) / m, 2);
}

function calcularPendienteResultante(
  errorInf: number,
  errorSup: number,
  refInf: number,
  refSup: number
): number | null {
  const deltaMedido = errorSup - errorInf;
  if (Math.abs(deltaMedido) <= 1.0 || Math.abs(refSup - refInf) <= 1.0) {
    return null;
  }
  return (refSup - refInf) / deltaMedido;
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
  const [metodo, setMetodo] = useState<MetodoCalibracion>("DOS_PUNTOS");
  const [macsSeleccionadas, setMacsSeleccionadas] = useState<string[]>([]);

  // Datos cargados del ESP32
  const [ambiente, setAmbiente] = useState<CalibracionAmbienteDTO | null>(null);
  const [perfilesPorMac, setPerfilesPorMac] = useState<
    Record<string, PerfilSensorCliente>
  >({});
  const [cargandoInicio, setCargandoInicio] = useState(false);

  // Lecturas en vivo del bus 1-Wire (mac UPPER -> info)
  const [enVivoPorMac, setEnVivoPorMac] = useState<
    Record<string, LecturaEnVivoSensor>
  >({});
  const historialCrudoRef = useRef<Record<string, number[]>>({});
  const ultimoTsEspRef = useRef<number>(0);

  // Ventana de tiempo del ESP32 para purga quirúrgica de sensos falsos
  const [tsInicioCalibEsp, setTsInicioCalibEsp] = useState<number>(0);
  const [borrarSensosFalsos, setBorrarSensosFalsos] = useState<boolean>(true);

  // Capturas por sensor (mac UPPER)
  const [capturas, setCapturas] = useState<Record<string, CapturasSensor>>({});
  const [tempPatronRef, setTempPatronRef] = useState<number>(25.0);

  // Cronómetro no bloqueante de estabilización (3 min = 180 s)
  const [timerSegundos, setTimerSegundos] = useState<number>(0);
  const [timerActivo, setTimerActivo] = useState<boolean>(false);

  // Estado de guardado / mute
  const [aplicando, setAplicando] = useState<boolean>(false);
  const [progresoTexto, setProgresoTexto] = useState<string>("");
  const [muteando, setMuteando] = useState<boolean>(false);

  const perfilesRef = useRef<Record<string, PerfilSensorCliente>>({});
  const ambienteRef = useRef<CalibracionAmbienteDTO | null>(null);
  perfilesRef.current = perfilesPorMac;
  ambienteRef.current = ambiente;

  // Reiniciar estado al abrir el modal y suscribir al stream en vivo
  useEffect(() => {
    if (!open) return;

    setPaso("CONFIG");
    setAlcance("UNO");
    setMetodo("DOS_PUNTOS");
    setCapturas({});
    setTsInicioCalibEsp(0);
    setBorrarSensosFalsos(true);
    setTimerActivo(false);
    setTimerSegundos(0);
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

  const actualizarDesdeLote = (lote: LecturaSensorCliente[]) => {
    if (!lote || lote.length === 0) return;
    if (lote[0].timestampMs > 0) {
      ultimoTsEspRef.current = lote[0].timestampMs;
    }

    setEnVivoPorMac((prev) => {
      const next = { ...prev };
      for (const l of lote) {
        const mac = l.macHex.toUpperCase();
        const perfil = perfilesRef.current[mac];
        const cruda = reconstruirTemperaturaCruda(
          l.temperaturaCorregida,
          perfil,
          ambienteRef.current
        );
        const hist = [...(historialCrudoRef.current[mac] ?? []), cruda].slice(
          -6
        );
        historialCrudoRef.current[mac] = hist;

        const minV = Math.min(...hist);
        const maxV = Math.max(...hist);
        const delta = redondearDecimal(maxV - minV, 2);

        next[mac] = {
          tempCorregida: l.temperaturaCorregida,
          tempCruda: cruda,
          timestampEsp: l.timestampMs,
          muestrasCrudas: hist,
          deltaEstabilidad: delta,
          estable: hist.length >= 4 && delta <= 0.15,
        };
      }
      return next;
    });
  };

  // Polling de telemetría viva mientras el modal está abierto
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
  }, [open]);

  // Cronómetro de estabilización (3:00 min recomendados)
  useEffect(() => {
    if (!open || !timerActivo) return;
    const id = window.setInterval(() => {
      setTimerSegundos((s) => s + 1);
    }, 1000);
    return () => window.clearInterval(id);
  }, [open, timerActivo]);

  const refCongelacion = ambiente?.activa ? ambiente.tempCongelacionRef : 0;
  const refEbullicion = ambiente?.activa ? ambiente.tempEbullicionRef : 100;

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
    if (conectados.length > 0) {
      setMacsSeleccionadas(conectados);
    } else {
      setMacsSeleccionadas(sensores.map((s) => s.mac.toUpperCase()));
    }
  };

  // Paso 1 -> Paso 2: carga los perfiles completos (errorInf/errorSup/umbrales) de los sensores elegidos
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

    // Marcar inicio de ventana en reloj del ESP32 (con margen de 10 s previos)
    const tsBase =
      ultimoTsEspRef.current > 0
        ? ultimoTsEspRef.current
        : Math.floor(Date.now() / 1000);
    setTsInicioCalibEsp(Math.max(0, tsBase - 10));

    const capInit: Record<string, CapturasSensor> = {};
    for (const mac of macsSeleccionadas) {
      capInit[mac] = { errorInf: null, errorSup: null, offsetUnPunto: null };
    }
    setCapturas(capInit);

    setTimerSegundos(0);
    setTimerActivo(false);
    setCargandoInicio(false);
    setPaso(metodo === "DOS_PUNTOS" ? "HIELO" : "PATRON");
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

  const capturarHieloTodos = () => {
    setCapturas((prev) => {
      const next = { ...prev };
      let capturados = 0;
      for (const mac of macsSeleccionadas) {
        const live = enVivoPorMac[mac];
        if (live) {
          next[mac] = {
            ...(next[mac] ?? {
              errorInf: null,
              errorSup: null,
              offsetUnPunto: null,
            }),
            errorInf: live.tempCruda,
          };
          capturados++;
        }
      }
      if (capturados === 0) {
        toast.warning("Ningún sensor seleccionado reporta lecturas en vivo.");
      }
      return next;
    });
  };

  const capturarEbullicionTodos = () => {
    setCapturas((prev) => {
      const next = { ...prev };
      let capturados = 0;
      for (const mac of macsSeleccionadas) {
        const live = enVivoPorMac[mac];
        if (live) {
          next[mac] = {
            ...(next[mac] ?? {
              errorInf: null,
              errorSup: null,
              offsetUnPunto: null,
            }),
            errorSup: live.tempCruda,
          };
          capturados++;
        }
      }
      if (capturados === 0) {
        toast.warning("Ningún sensor seleccionado reporta lecturas en vivo.");
      }
      return next;
    });
  };

  const capturarPatronTodos = () => {
    setCapturas((prev) => {
      const next = { ...prev };
      let capturados = 0;
      for (const mac of macsSeleccionadas) {
        const live = enVivoPorMac[mac];
        if (live) {
          const offsetCalc = redondearDecimal(
            Math.max(-20, Math.min(20, tempPatronRef - live.tempCruda)),
            2
          );
          next[mac] = {
            ...(next[mac] ?? {
              errorInf: null,
              errorSup: null,
              offsetUnPunto: null,
            }),
            offsetUnPunto: offsetCalc,
          };
          capturados++;
        }
      }
      if (capturados === 0) {
        toast.warning("Ningún sensor seleccionado reporta lecturas en vivo.");
      }
      return next;
    });
  };

  const todosHieloListos = macsSeleccionadas.every(
    (m) => capturas[m]?.errorInf !== null && capturas[m]?.errorInf !== undefined
  );
  const todosEbullicionListos = macsSeleccionadas.every(
    (m) => capturas[m]?.errorSup !== null && capturas[m]?.errorSup !== undefined
  );
  const todosPatronListos = macsSeleccionadas.every(
    (m) =>
      capturas[m]?.offsetUnPunto !== null &&
      capturas[m]?.offsetUnPunto !== undefined
  );

  const validarResumen = useMemo(() => {
    const advertencias: string[] = [];
    const errores: string[] = [];

    for (const s of sensoresSeleccionados) {
      const mac = s.mac.toUpperCase();
      const cap = capturas[mac];
      if (!cap) {
        errores.push(`${s.nombre || mac}: faltan capturas.`);
        continue;
      }
      if (metodo === "DOS_PUNTOS") {
        if (cap.errorInf === null || cap.errorSup === null) {
          errores.push(`${s.nombre || mac}: faltan puntos de hielo/ebullición.`);
        } else if (cap.errorSup - cap.errorInf <= 1.0) {
          errores.push(
            `${s.nombre || mac}: la medición de ebullición (${cap.errorSup}°C) debe ser mayor que la de hielo (${cap.errorInf}°C).`
          );
        } else {
          const m = calcularPendienteResultante(
            cap.errorInf,
            cap.errorSup,
            refCongelacion,
            refEbullicion
          );
          if (m !== null && (m < 0.85 || m > 1.15)) {
            advertencias.push(
              `${s.nombre || mac}: pendiente inusual (m=${m.toFixed(3)}). Verifique que la sonda se haya estabilizado bien.`
            );
          }
        }
      } else {
        if (cap.offsetUnPunto === null) {
          errores.push(`${s.nombre || mac}: falta capturar el offset.`);
        } else if (Math.abs(cap.offsetUnPunto) > 10) {
          advertencias.push(
            `${s.nombre || mac}: offset elevado (${cap.offsetUnPunto > 0 ? "+" : ""}${cap.offsetUnPunto}°C).`
          );
        }
      }
    }
    return { errores, advertencias };
  }, [
    sensoresSeleccionados,
    capturas,
    metodo,
    refCongelacion,
    refEbullicion,
  ]);

  const aplicarCalibracionYLimpiar = async () => {
    if (validarResumen.errores.length > 0) return;

    setAplicando(true);
    let aplicadosOk = 0;

    for (let i = 0; i < sensoresSeleccionados.length; i++) {
      const s = sensoresSeleccionados[i];
      const mac = s.mac.toUpperCase();
      const perfil = perfilesPorMac[mac];
      const cap = capturas[mac];
      if (!cap) continue;

      setProgresoTexto(
        `Guardando calibración ${i + 1}/${sensoresSeleccionados.length} (${s.nombre || mac})...`
      );

      const umbralSup = perfil ? perfil.umbralSup : s.umbralSup;
      const umbralInf = perfil ? perfil.umbralInf : s.umbralInf;
      const errorInf =
        metodo === "DOS_PUNTOS" ? redondearDecimal(cap.errorInf ?? 0, 2) : 0;
      const errorSup =
        metodo === "DOS_PUNTOS" ? redondearDecimal(cap.errorSup ?? 0, 2) : 0;
      const offset =
        metodo === "DOS_PUNTOS"
          ? 0
          : redondearDecimal(cap.offsetUnPunto ?? 0, 2);

      const ok = await safeRpc(
        () =>
          client.actualizarCalibracionSensor(
            s.mac,
            umbralSup,
            umbralInf,
            errorSup,
            errorInf,
            offset
          ),
        `wizardCalibrar(${s.mac})`
      );
      if (ok) aplicadosOk++;
    }

    // Limpieza quirúrgica de sensos y alertas generados durante la ventana de calibración
    let sensosBorrados = 0;
    let eventosBorrados = 0;

    if (borrarSensosFalsos && tsInicioCalibEsp > 0 && aplicadosOk > 0) {
      const tsFin =
        (ultimoTsEspRef.current > 0
          ? ultimoTsEspRef.current
          : Math.floor(Date.now() / 1000)) + 15;

      for (let i = 0; i < sensoresSeleccionados.length; i++) {
        const s = sensoresSeleccionados[i];
        setProgresoTexto(
          `Limpiando sensos de calibración ${i + 1}/${sensoresSeleccionados.length} (${s.nombre || s.mac})...`
        );

        // 1. Identificar sensos generados en [tsInicioCalibEsp, tsFin] para borrar sus alertas vinculadas
        const recientes = await safeRpc(
          () =>
            client.listarSensosPaginados({
              idSensor: s.idSensor,
              limiteRegistros: 32,
              offsetRegistros: 0,
            }),
          "listarSensosParaLimpieza",
          { silent: true }
        );
        const idsSensoEnVentana = new Set(
          (recientes ?? [])
            .filter(
              (r) =>
                r.fechaCreacion >= tsInicioCalibEsp &&
                r.fechaCreacion <= tsFin
            )
            .map((r) => r.idSenso)
        );

        if (idsSensoEnVentana.size > 0) {
          const auds = await safeRpc(
            () => client.listarAuditoriaSensorPorMac(s.mac, 32),
            "listarAuditoriaParaLimpieza",
            { silent: true }
          );
          for (const ev of auds ?? []) {
            if (idsSensoEnVentana.has(ev.idSenso)) {
              await safeRpc(
                () => client.eliminarAuditoriaSensorPorId(ev.idAuditoriaSensor),
                "eliminarAuditoriaCalib",
                { silent: true }
              );
              eventosBorrados++;
            }
          }
        }

        // 2. Eliminar los registros de Senso del intervalo con 1 sola sentencia SQL rápida
        const delRes = await safeRpc(
          () =>
            client.ejecutarSqlTerminal(
              `DELETE FROM Senso WHERE id_sensor = ${s.idSensor} AND fecha_creacion >= ${tsInicioCalibEsp} AND fecha_creacion <= ${tsFin};`
            ),
          "limpiarSensosCalibracionSql",
          { silent: true }
        );

        if (delRes?.exito) {
          sensosBorrados += delRes.filasAfectadas;
        } else if (idsSensoEnVentana.size > 0) {
          for (const idSenso of idsSensoEnVentana) {
            await safeRpc(
              () => client.eliminarSensoPorId(idSenso),
              "eliminarSensoFallback",
              { silent: true }
            );
            sensosBorrados++;
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
      const detalleLimpieza = borrarSensosFalsos
        ? ` · ${sensosBorrados} sensos y ${eventosBorrados} alertas de prueba limpiados.`
        : ".";
      toast.success(
        `Calibración aplicada en ${aplicadosOk} sensor(es)${detalleLimpieza}`
      );
      onCompletado();
      onClose();
    }
  };

  if (!open) return null;

  const mmTimer = String(Math.floor(timerSegundos / 60)).padStart(2, "0");
  const ssTimer = String(timerSegundos % 60).padStart(2, "0");

  const pasosOrden: Array<{ id: PasoWizard; label: string }> =
    metodo === "DOS_PUNTOS"
      ? [
          { id: "CONFIG", label: "1. Sensores y Método" },
          { id: "HIELO", label: "2. Baño de Hielo" },
          { id: "EBULLICION", label: "3. Agua Hirviendo" },
          { id: "RESUMEN", label: "4. Revisión y Limpieza" },
        ]
      : [
          { id: "CONFIG", label: "1. Sensores y Método" },
          { id: "PATRON", label: "2. Termómetro Patrón" },
          { id: "RESUMEN", label: "3. Revisión y Limpieza" },
        ];

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4 backdrop-blur-[2px]">
      <div className="flex max-h-[90vh] w-full max-w-4xl flex-col overflow-hidden rounded-xl border border-border bg-card text-card-foreground shadow-2xl">
        {/* Cabecera del Wizard */}
        <div className="flex items-center justify-between border-b border-border px-5 py-3.5">
          <div className="flex items-center gap-2.5">
            <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-warning-light text-warning">
              <Sliders className="h-5 w-5" />
            </div>
            <div>
              <h2 className="text-base font-bold">
                Asistente de Calibración Metrológica DS18B20
              </h2>
              <p className="text-xs text-foreground-secondary">
                Reconstruye la lectura cruda real del sensor y protege los
                umbrales de alarma existentes
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

        {/* Cuerpo scrolleable */}
        <div className="flex-1 space-y-4 overflow-y-auto p-5 text-xs">
          {/* ==================== PASO 1: CONFIG ==================== */}
          {paso === "CONFIG" && (
            <div className="space-y-4">
              {/* Alcance: 1 sensor vs Lote */}
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
                  <div className="font-bold text-sm">Calibrar 1 sensor</div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Ideal al incorporar una sonda nueva o reajustar un único
                    sensor del grupo.
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
                  <div className="font-bold text-sm">
                    Calibración en lote (múltiples sensores)
                  </div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Sumerja todos los sensores juntos en hielo y agua hirviendo
                    y capture el lote completo de una sola vez.
                  </p>
                </button>
              </div>

              {/* Selección de sensores */}
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
                      className="text-primary underline"
                    >
                      Seleccionar conectados en vivo
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
                        <div className="flex items-center gap-2 min-w-0">
                          <input
                            type={alcance === "UNO" ? "radio" : "checkbox"}
                            name="wizard-sensor"
                            checked={marcado}
                            onChange={() => {
                              if (alcance === "UNO") setMacsSeleccionadas([mac]);
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
                          {live ? `${live.tempCorregida.toFixed(2)} °C` : "Offline"}
                        </span>
                      </label>
                    );
                  })}
                </div>
              </div>

              {/* Método de calibración */}
              <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
                <button
                  type="button"
                  onClick={() => setMetodo("DOS_PUNTOS")}
                  className={`rounded-lg border p-3.5 text-left transition-all ${
                    metodo === "DOS_PUNTOS"
                      ? "border-warning bg-warning-light/50 ring-1 ring-warning/40"
                      : "border-border bg-background hover:border-warning/50"
                  }`}
                >
                  <div className="flex items-center gap-1.5 font-bold text-sm">
                    <Sparkles className="h-4 w-4 text-warning" />
                    2 Puntos (Hielo + Agua Hirviendo)
                  </div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Estándar metrológico completo: corrige ganancia (pendiente) y
                    desplazamiento usando referencias ambientales.
                  </p>
                </button>

                <button
                  type="button"
                  onClick={() => setMetodo("UN_PUNTO")}
                  className={`rounded-lg border p-3.5 text-left transition-all ${
                    metodo === "UN_PUNTO"
                      ? "border-info bg-info-light/50 ring-1 ring-info/40"
                      : "border-border bg-background hover:border-info/50"
                  }`}
                >
                  <div className="flex items-center gap-1.5 font-bold text-sm">
                    <Thermometer className="h-4 w-4 text-info" />
                    1 Punto (Termómetro Patrón / Offset)
                  </div>
                  <p className="mt-1 text-xs text-foreground-secondary">
                    Ajuste rápido de desplazamiento constante comparando contra
                    un termómetro certificado a temperatura ambiente o de cámara.
                  </p>
                </button>
              </div>

              {/* Referencias ambientales actuales */}
              {metodo === "DOS_PUNTOS" && (
                <div className="flex flex-wrap items-center justify-between gap-2 rounded-lg border border-border bg-secondary/40 px-3.5 py-2.5">
                  <div className="flex items-center gap-2">
                    <Globe className="h-4 w-4 text-info shrink-0" />
                    <div>
                      <span className="font-semibold">
                        Referencias ambientales activas:{" "}
                      </span>
                      <span className="font-mono">
                        Hielo = {refCongelacion.toFixed(2)} °C · Ebullición ={" "}
                        {refEbullicion.toFixed(2)} °C
                      </span>
                      {ambiente && (
                        <span className="text-foreground-secondary">
                          {" "}
                          ({ambiente.altura} m · {ambiente.presionAtmosferica}{" "}
                          hPa)
                        </span>
                      )}
                    </div>
                  </div>
                  {onIrAConfigAmbiente && (
                    <button
                      type="button"
                      onClick={() => {
                        onClose();
                        onIrAConfigAmbiente();
                      }}
                      className="text-info underline font-semibold"
                    >
                      Ajustar en Configuración
                    </button>
                  )}
                </div>
              )}
            </div>
          )}

          {/* ==================== PASO 2: HIELO ==================== */}
          {paso === "HIELO" && (
            <PasoCapturaEnVivo
              titulo={`Punto Inferior: Baño de Hielo (Ref. ${refCongelacion.toFixed(2)} °C)`}
              icono={<Snowflake className="h-5 w-5 text-info" />}
              instrucciones={[
                "Llene un vaso térmico con hielo picado y agua fría (proporción 50/50) y mezcle 30 segundos.",
                "Sumerja la punta metálica de la(s) sonda(s) en el centro sin tocar paredes ni fondo.",
                "Espere a que el indicador marque ESTABLE (recomendado ~3 minutos) y pulse Capturar.",
              ]}
              sensores={sensoresSeleccionados}
              enVivoPorMac={enVivoPorMac}
              valoresCapturados={Object.fromEntries(
                macsSeleccionadas.map((m) => [m, capturas[m]?.errorInf ?? null])
              )}
              etiquetaValor="Cruda capturada (E_inf °C)"
              onCapturarTodos={capturarHieloTodos}
              onCambiarValor={(mac, val) =>
                setCapturas((prev) => ({
                  ...prev,
                  [mac]: {
                    ...(prev[mac] ?? {
                      errorInf: null,
                      errorSup: null,
                      offsetUnPunto: null,
                    }),
                    errorInf: val,
                  },
                }))
              }
              timerTexto={`${mmTimer}:${ssTimer}`}
              timerActivo={timerActivo}
              onToggleTimer={() => setTimerActivo((a) => !a)}
              onResetTimer={() => {
                setTimerActivo(false);
                setTimerSegundos(0);
              }}
              muteando={muteando}
              onMute={silenciarAlarma}
            />
          )}

          {/* ==================== PASO 3: EBULLICIÓN ==================== */}
          {paso === "EBULLICION" && (
            <PasoCapturaEnVivo
              titulo={`Punto Superior: Agua en Ebullición (Ref. ${refEbullicion.toFixed(2)} °C)`}
              icono={<Flame className="h-5 w-5 text-error" />}
              instrucciones={[
                "Lleve agua a ebullición franca (burbujeo continuo).",
                "Sumerja la(s) sonda(s) en el centro del agua sin tocar el fondo caliente ni los bordes del recipiente.",
                "Espere estabilización (chip ESTABLE) y pulse Capturar. Puede silenciar la bocina si se dispara la alarma de temperatura alta.",
              ]}
              sensores={sensoresSeleccionados}
              enVivoPorMac={enVivoPorMac}
              valoresCapturados={Object.fromEntries(
                macsSeleccionadas.map((m) => [m, capturas[m]?.errorSup ?? null])
              )}
              etiquetaValor="Cruda capturada (E_sup °C)"
              onCapturarTodos={capturarEbullicionTodos}
              onCambiarValor={(mac, val) =>
                setCapturas((prev) => ({
                  ...prev,
                  [mac]: {
                    ...(prev[mac] ?? {
                      errorInf: null,
                      errorSup: null,
                      offsetUnPunto: null,
                    }),
                    errorSup: val,
                  },
                }))
              }
              timerTexto={`${mmTimer}:${ssTimer}`}
              timerActivo={timerActivo}
              onToggleTimer={() => setTimerActivo((a) => !a)}
              onResetTimer={() => {
                setTimerActivo(false);
                setTimerSegundos(0);
              }}
              muteando={muteando}
              onMute={silenciarAlarma}
            />
          )}

          {/* ==================== PASO 2 (ALT): 1 PUNTO PATRÓN ==================== */}
          {paso === "PATRON" && (
            <div className="space-y-3">
              <div className="flex flex-wrap items-end justify-between gap-3 rounded-lg border border-info/40 bg-info-light/30 p-3.5">
                <div>
                  <div className="font-bold text-sm">
                    Temperatura del Termómetro Patrón Certificado
                  </div>
                  <p className="text-xs text-foreground-secondary">
                    Coloque la(s) sonda(s) junto al termómetro patrón, ingrese
                    la temperatura real y capture el offset calculado.
                  </p>
                </div>
                <div className="flex items-center gap-2">
                  <label className="font-semibold">T_patrón (°C):</label>
                  <NumericInput
                    value={tempPatronRef}
                    onChange={setTempPatronRef}
                    decimals={2}
                    min={-55}
                    max={126}
                    className="w-24 rounded border border-border bg-card px-2.5 py-1.5 font-mono font-bold"
                  />
                </div>
              </div>

              <PasoCapturaEnVivo
                titulo="Captura de Offset (Δ = T_patrón − T_cruda)"
                icono={<Thermometer className="h-5 w-5 text-info" />}
                instrucciones={[
                  "Verifique que la lectura cruda reconstruida esté estable antes de capturar.",
                  "Al capturar, el offset se calcula automáticamente como (T_patrón − T_cruda) y borra una recta de 2 puntos anterior.",
                ]}
                sensores={sensoresSeleccionados}
                enVivoPorMac={enVivoPorMac}
                valoresCapturados={Object.fromEntries(
                  macsSeleccionadas.map((m) => [
                    m,
                    capturas[m]?.offsetUnPunto ?? null,
                  ])
                )}
                etiquetaValor="Offset a aplicar (°C)"
                minValor={-20}
                maxValor={20}
                onCapturarTodos={capturarPatronTodos}
                onCambiarValor={(mac, val) =>
                  setCapturas((prev) => ({
                    ...prev,
                    [mac]: {
                      ...(prev[mac] ?? {
                        errorInf: null,
                        errorSup: null,
                        offsetUnPunto: null,
                      }),
                      offsetUnPunto: val,
                    },
                  }))
                }
                timerTexto={`${mmTimer}:${ssTimer}`}
                timerActivo={timerActivo}
                onToggleTimer={() => setTimerActivo((a) => !a)}
                onResetTimer={() => {
                  setTimerActivo(false);
                  setTimerSegundos(0);
                }}
                muteando={muteando}
                onMute={silenciarAlarma}
              />
            </div>
          )}

          {/* ==================== PASO FINAL: RESUMEN Y LIMPIEZA ==================== */}
          {paso === "RESUMEN" && (
            <div className="space-y-4">
              <div className="rounded-lg border border-alert-success-border bg-alert-success-bg px-3.5 py-2.5 text-alert-success-icon">
                <strong>Protección de alarmas:</strong> los umbrales TL / TH de
                cada sensor se mantienen exactamente como estaban configurados.
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
                      <th className="px-3 py-2">Umbrales (intactos)</th>
                      <th className="px-3 py-2">E_inf / E_sup</th>
                      <th className="px-3 py-2">Offset</th>
                      <th className="px-3 py-2">Pendiente (m)</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-border">
                    {sensoresSeleccionados.map((s) => {
                      const mac = s.mac.toUpperCase();
                      const perf = perfilesPorMac[mac];
                      const cap = capturas[mac];
                      const eInf =
                        metodo === "DOS_PUNTOS" ? (cap?.errorInf ?? 0) : 0;
                      const eSup =
                        metodo === "DOS_PUNTOS" ? (cap?.errorSup ?? 0) : 0;
                      const off =
                        metodo === "DOS_PUNTOS"
                          ? 0
                          : (cap?.offsetUnPunto ?? 0);
                      const m =
                        metodo === "DOS_PUNTOS"
                          ? calcularPendienteResultante(
                              eInf,
                              eSup,
                              refCongelacion,
                              refEbullicion
                            )
                          : 1;
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
                            {(perf?.umbralInf ?? s.umbralInf).toFixed(1)} /{" "}
                            {(perf?.umbralSup ?? s.umbralSup).toFixed(1)} °C
                          </td>
                          <td className="px-3 py-2 font-mono font-semibold">
                            {eInf.toFixed(2)} / {eSup.toFixed(2)} °C
                          </td>
                          <td className="px-3 py-2 font-mono font-semibold">
                            {off >= 0 ? `+${off.toFixed(2)}` : off.toFixed(2)}{" "}
                            °C
                          </td>
                          <td className="px-3 py-2 font-mono">
                            {m !== null ? m.toFixed(4) : "—"}
                          </td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>

              {/* Opción de limpieza de sensos falsos */}
              <div className="rounded-lg border border-border bg-background p-3.5">
                <label className="flex cursor-pointer items-start gap-2.5">
                  <input
                    type="checkbox"
                    checked={borrarSensosFalsos}
                    onChange={(e) => setBorrarSensosFalsos(e.target.checked)}
                    className="mt-0.5"
                  />
                  <div>
                    <div className="flex items-center gap-1.5 font-bold">
                      <Trash2 className="h-3.5 w-3.5 text-error" />
                      Descartar sensos y alertas falsas generadas durante la
                      calibración (recomendado)
                    </div>
                    <p className="mt-0.5 text-[11px] text-foreground-secondary">
                      Elimina únicamente los registros de <code>Senso</code> y{" "}
                      <code>AuditoriaSensores</code> de los sensores
                      seleccionados creados desde{" "}
                      <span className="font-mono text-foreground">
                        {formatearEpochSegundos(tsInicioCalibEsp)}
                      </span>{" "}
                      hasta ahora, evitando contaminar promedios, mínimas y
                      máximas históricas con el agua hirviendo o el hielo.
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

        {/* Pie con botones de navegación */}
        <div className="flex items-center justify-between border-t border-border bg-secondary/30 px-5 py-3">
          <div>
            {paso !== "CONFIG" && (
              <button
                type="button"
                disabled={aplicando}
                onClick={() => {
                  setTimerActivo(false);
                  setTimerSegundos(0);
                  if (paso === "HIELO" || paso === "PATRON") setPaso("CONFIG");
                  else if (paso === "EBULLICION") setPaso("HIELO");
                  else if (paso === "RESUMEN") {
                    setPaso(metodo === "DOS_PUNTOS" ? "EBULLICION" : "PATRON");
                  }
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
                Comenzar Calibración
              </button>
            )}

            {paso === "HIELO" && (
              <button
                type="button"
                disabled={!todosHieloListos}
                onClick={() => {
                  setTimerActivo(false);
                  setTimerSegundos(0);
                  setPaso("EBULLICION");
                }}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                Siguiente: Agua Hirviendo
                <ArrowRight className="h-3.5 w-3.5" />
              </button>
            )}

            {paso === "EBULLICION" && (
              <button
                type="button"
                disabled={!todosEbullicionListos}
                onClick={() => {
                  setTimerActivo(false);
                  setPaso("RESUMEN");
                }}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                Revisar y Aplicar
                <ArrowRight className="h-3.5 w-3.5" />
              </button>
            )}

            {paso === "PATRON" && (
              <button
                type="button"
                disabled={!todosPatronListos}
                onClick={() => {
                  setTimerActivo(false);
                  setPaso("RESUMEN");
                }}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                Revisar y Aplicar
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
                Aplicar Calibración ({sensoresSeleccionados.length})
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

// Subcomponente reutilizable para los pasos de captura en vivo (Hielo / Ebullición / Patrón)
function PasoCapturaEnVivo({
  titulo,
  icono,
  instrucciones,
  sensores,
  enVivoPorMac,
  valoresCapturados,
  etiquetaValor,
  minValor = -55,
  maxValor = 126,
  onCapturarTodos,
  onCambiarValor,
  timerTexto,
  timerActivo,
  onToggleTimer,
  onResetTimer,
  muteando,
  onMute,
}: {
  titulo: string;
  icono: React.ReactNode;
  instrucciones: string[];
  sensores: ResumenSensorCliente[];
  enVivoPorMac: Record<string, LecturaEnVivoSensor>;
  valoresCapturados: Record<string, number | null>;
  etiquetaValor: string;
  minValor?: number;
  maxValor?: number;
  onCapturarTodos: () => void;
  onCambiarValor: (macUpper: string, val: number) => void;
  timerTexto: string;
  timerActivo: boolean;
  onToggleTimer: () => void;
  onResetTimer: () => void;
  muteando: boolean;
  onMute: () => void;
}) {
  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-start justify-between gap-3 rounded-lg border border-border bg-background p-3.5">
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

        <div className="flex flex-wrap items-center gap-2">
          <div className="flex items-center gap-1.5 rounded-lg border border-border bg-card px-2.5 py-1.5 font-mono">
            <Clock className="h-3.5 w-3.5 text-primary" />
            <span className="font-bold">{timerTexto}</span>
            <span className="text-[10px] text-foreground-secondary">/ 03:00</span>
            <button
              type="button"
              onClick={onToggleTimer}
              className="ml-1 rounded bg-secondary px-1.5 py-0.5 text-[10px] font-semibold hover:bg-hover"
            >
              {timerActivo ? "Pausar" : "Iniciar"}
            </button>
            <button
              type="button"
              onClick={onResetTimer}
              className="rounded p-0.5 hover:bg-secondary"
              title="Reiniciar cronómetro"
            >
              <RefreshCw className="h-3 w-3" />
            </button>
          </div>

          <button
            type="button"
            disabled={muteando}
            onClick={onMute}
            className="btn btn-outline btn-error px-2.5 py-1.5 text-xs"
            title="Silenciar bocina física si saltó alarma de umbral"
          >
            <VolumeX className="h-3.5 w-3.5" />
            Mute Bocina
          </button>
        </div>
      </div>

      <div className="flex items-center justify-between">
        <span className="font-semibold text-foreground-secondary">
          Telemetría en vivo (reconstrucción automática de temperatura cruda):
        </span>
        <button
          type="button"
          onClick={onCapturarTodos}
          className="btn btn-solid btn-warning px-3 py-1.5 text-xs font-bold"
        >
          Capturar Todos Ahora
        </button>
      </div>

      <div className="overflow-x-auto rounded-lg border border-border">
        <table className="w-full text-left text-xs">
          <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
            <tr>
              <th className="px-3 py-2">Sensor</th>
              <th className="px-3 py-2">Temp. Actual (Corr.)</th>
              <th className="px-3 py-2">Temp. Cruda Real</th>
              <th className="px-3 py-2">Estabilidad</th>
              <th className="px-3 py-2">{etiquetaValor}</th>
              <th className="px-3 py-2 text-right">Acción</th>
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
                      min={minValor}
                      max={maxValor}
                      placeholder="Sin capturar"
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