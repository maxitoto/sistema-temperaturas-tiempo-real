import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Thermometer,
  RefreshCw,
  Sliders,
  BellRing,
  Edit3,
  Trash2,
  PlusCircle,
  Globe,
  History,
  Search,
  Loader2,
  Download,
  FlaskConical,
  Square,
  FileSpreadsheet,
  Activity,
  AlertTriangle,
  ShieldCheck,
  Pause,
  Play,
  Info,
} from "lucide-react";
import {
  ResponsiveContainer,
  AreaChart,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  ReferenceLine,
} from "recharts";
import { toast } from "react-toastify";
import {
  detectarExperimentosDesdeAuditoria,
  VentanaExperimento,
} from "../lib/experimentosSensor";
import {
  exportarCurvaTermicaExcel,
  exportarTablaExcel,
} from "../lib/exportExcel";
import {
  descargarCsv,
  formatearFechaLocalCsv,
  formatearIsoUtcDesdeEpoch,
  sufijoFechaArchivo,
} from "../lib/exportCsv";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import {
  fechaAInputLocal,
  formatearEpochSegundos,
  inputLocalAEpochSegundos,
} from "../lib/format";
import {
  describirEstadoSensor,
  LABEL_CRITICIDAD,
  LABEL_ESTADO_BUS,
  LABEL_ESTADO_ENVIO,
  LABEL_ESTADO_SENSOR,
  LABEL_TIPO_EVENTO,
  esTemperaturaFisicaDS18B20,
  resolverEstadoLecturaVivo
} from "../lib/labels";
import { NumericInput } from "../components/ui/NumericInput";
import { CalibrationWizardModal } from "../components/calibration/CalibrationWizardModal";
import {
  AuditoriaSensorRowDTO,
  CalibracionAmbienteDTO,
  CensoRowDTO,
  CriticidadEvento,
  EstadoEnvioCorreo,
  EstadoSensor,
  LecturaSensorCliente,
  PerfilSensorCliente,
  ResumenEstadisticoCensoCliente,
  ResumenSensorCliente,
  RolUsuario,
  SensorEnAlarmaItemDTO,
} from "#types/rpc.types";
import { FormulaTooltip } from "../components/ui/FormulaTooltip";
import {
  calcularRectaCalibracion,
  formatearEcuacionRecta,
  obtenerReferenciasActivas,
} from "../lib/calibracionModels";
import { SensorAnalyticsChart } from "../components/charts/SensorAnalyticsChart";
import {
  DataTable,
  DataTableColumn,
  DataTableFilter,
} from "../components/ui/DataTable";

const LOTE_RPC_CENSO = 32;
const LOTE_SCROLL_CENSO = 25;

const LIMITE_DEFECTO_CURVA = 50;
type ModoConsultaTemporal =
  | "ULTIMOS_50"
  | "TODO"
  | "ENTRE_FECHAS"
  | "DESDE_ADELANTE"
  | "HASTA_ATRAS"
  | "EXPERIMENTO";

interface PuntoVivoSensor {
  hora: string;
  temp: number;
}

function parsearFechaTextoAEpochSeg(texto: string): number {
  if (!texto) return 0;
  const iso = texto.includes("T") ? texto : texto.replace(" ", "T");
  const ms = Date.parse(iso);
  return Number.isFinite(ms) ? Math.floor(ms / 1000) : 0;
}

export function SensoresView({
  macInicial,
  navToken = 0,
  onIrAConfigAmbiente,
}: {
  macInicial?: string;
  navToken?: number;
  onIrAConfigAmbiente?: () => void;
}) {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  // Catálogo y selección
  const [sensores, setSensores] = useState<ResumenSensorCliente[]>([]);
  const [soloActivos, setSoloActivos] = useState(false);
  const [wizardAbierto, setWizardAbierto] = useState(false);
  const [macSeleccionada, setMacSeleccionada] = useState<string>(
    macInicial ? macInicial.toUpperCase() : ""
  );
  const [perfil, setPerfil] = useState<PerfilSensorCliente | null>(null);
  const [statsCenso, setStatsCenso] =
    useState<ResumenEstadisticoCensoCliente | null>(null);
  const [ambienteGlobal, setAmbienteGlobal] =
    useState<CalibracionAmbienteDTO | null>(null);

  // Monitor en vivo del sensor seleccionado (reutiliza stream compartido sin sobrecargar el ESP32)
  const [streamActivo, setStreamActivo] = useState(false);
  const [lecturaViva, setLecturaViva] = useState<LecturaSensorCliente | null>(
    null
  );
  const [alarmaViva, setAlarmaViva] = useState<SensorEnAlarmaItemDTO | null>(
    null
  );
  const [curvaViva, setCurvaViva] = useState<PuntoVivoSensor[]>([]);
  const [vivoPausado, setVivoPausado] = useState(false);
  const [mostrarGraficoVivo, setMostrarGraficoVivo] = useState(true);
  const vivoPausadoRef = useRef(false);
  vivoPausadoRef.current = vivoPausado;
  const macSeleccionadaRef = useRef(macSeleccionada);
  macSeleccionadaRef.current = macSeleccionada;

  // Estados de carga + tokens anti-race-condition
  const [cargandoCatalogo, setCargandoCatalogo] = useState(false);
  const [catalogoCargadoUnaVez, setCatalogoCargadoUnaVez] = useState(false);
  const [cargandoDetalle, setCargandoDetalle] = useState(false);
  const [cargandoHistorial, setCargandoHistorial] = useState(false);
  const [progresoHistorial, setProgresoHistorial] = useState<number>(0);
  const [cargandoTabla, setCargandoTabla] = useState(false);
  const [guardandoAccion, setGuardandoAccion] = useState<string | null>(null);

  const detalleReqRef = useRef(0);
  const historialReqRef = useRef(0);
  const tablaReqRef = useRef(0);

  // Fijación determinística a la card de la MAC
  const macCardRef = useRef<HTMLDivElement | null>(null);
  const chartContainerRef = useRef<HTMLDivElement | null>(null);
  const [exportandoExcel, setExportandoExcel] = useState(false);
  const focusPendienteRef = useRef<boolean>(navToken > 0);
  const ultimoNavTokenRef = useRef<number>(navToken);
  const [focusTrigger, setFocusTrigger] = useState<number>(navToken > 0 ? 1 : 0);
  const [resaltarCardMac, setResaltarCardMac] = useState(false);

  // Formularios separados (ADMIN): Metadatos | Umbrales TL/TH | Calibración 2P
  const [formMeta, setFormMeta] = useState({
    nombre: "",
    ubicacion: "",
    descripcion: "",
    activa: true,
  });
  const [formUmbrales, setFormUmbrales] = useState({
    umbralInf: -55,
    umbralSup: 126,
  });
  const [formCalib, setFormCalib] = useState({
    errorInf: 0,
    errorSup: 0,
    offset: 0,
  });

  // Consulta temporal sin tope de muestras (Todo el historial por defecto)
  const [modoTemporal, setModoTemporal] =
    useState<ModoConsultaTemporal>("ULTIMOS_50");
  const [fechaDesde, setFechaDesde] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 3600 * 1000))
  );
  const [fechaHasta, setFechaHasta] = useState(() =>
    fechaAInputLocal(new Date())
  );
  const [serieHistorica, setSerieHistorica] = useState<LecturaSensorCliente[]>(
    []
  );
  const [experimentos, setExperimentos] = useState<VentanaExperimento[]>([]);
  const [experimentoId, setExperimentoId] = useState<string>("");
  const [cargandoExperimentos, setCargandoExperimentos] = useState(false);

  // Pestaña inferior: Censos del sensor | Auditorías de eventos del sensor
  const [tabRegistros, setTabRegistros] = useState<"CENSOS" | "AUDITORIAS">(
    "CENSOS"
  );
  const [filasCenso, setFilasCenso] = useState<CensoRowDTO[]>([]);
  const [hayMasCensos, setHayMasCensos] = useState<boolean>(false);
  const [cargandoMasCensos, setCargandoMasCensos] = useState<boolean>(false);
  const filasCensoLenRef = useRef<number>(0);
  filasCensoLenRef.current = filasCenso.length;

  const [filasAuditoria, setFilasAuditoria] = useState<AuditoriaSensorRowDTO[]>(
    []
  );
  const [cargandoAuditoria, setCargandoAuditoria] = useState(false);

  // Censo manual (siempre del sensor seleccionado) y purga (ADMIN)
  const [nuevoCenso, setNuevoCenso] = useState({
    temperaturaCruda: 22.5,
    temperaturaCorregida: 22.5,
    estadoSensor: EstadoSensor.OK,
  });
  const [fechaPurga, setFechaPurga] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 7 * 86400 * 1000))
  );

  // Detectar navegación entrante desde Dashboard o AlarmBanner
  useEffect(() => {
    if (navToken > 0 && navToken !== ultimoNavTokenRef.current) {
      ultimoNavTokenRef.current = navToken;
      focusPendienteRef.current = true;
      setFocusTrigger((n) => n + 1);
    }
    if (macInicial) {
      setMacSeleccionada(macInicial.toUpperCase());
    }
  }, [macInicial, navToken]);

  // Ejecutar scroll determinístico cuando el catálogo y la card del sensor ya tienen altura final
  useEffect(() => {
    if (!focusPendienteRef.current) return;
    if (!catalogoCargadoUnaVez || cargandoCatalogo) return;
    if (!macSeleccionada) return;
    if (!perfil || perfil.mac.toUpperCase() !== macSeleccionada.toUpperCase()) {
      return;
    }

    focusPendienteRef.current = false;
    let raf2 = 0;
    const tScroll = window.setTimeout(() => {
      window.requestAnimationFrame(() => {
        raf2 = window.requestAnimationFrame(() => {
          const el = macCardRef.current;
          if (!el) return;
          el.scrollIntoView({ behavior: "smooth", block: "start" });
          setResaltarCardMac(true);
        });
      });
    }, 60);

    const tHighlight = window.setTimeout(() => setResaltarCardMac(false), 1800);

    return () => {
      window.clearTimeout(tScroll);
      window.cancelAnimationFrame(raf2);
      window.clearTimeout(tHighlight);
    };
  }, [
    focusTrigger,
    catalogoCargadoUnaVez,
    cargandoCatalogo,
    perfil,
    macSeleccionada,
  ]);

  // Reiniciar curva en vivo al cambiar de sensor seleccionado
  useEffect(() => {
    setLecturaViva(null);
    setAlarmaViva(null);
    setCurvaViva([]);
  }, [macSeleccionada]);

  // Reutiliza el stream compartido de Electron (0 carga extra al ESP32) filtrando por MAC
  const aplicarLoteVivo = useCallback((lote: LecturaSensorCliente[]) => {
    const macObj = macSeleccionadaRef.current.toUpperCase();
    if (!macObj || !lote || lote.length === 0) return;

    const match = lote.find((l) => l.macHex.toUpperCase() === macObj);
    if (!match) return;

    setLecturaViva(match);
    if (vivoPausadoRef.current) return;

    const hora = formatearEpochSegundos(match.timestampMs).slice(-8);
    const temp = Number(match.temperaturaCorregida.toFixed(2));
    setCurvaViva((prev) => {
      const ultimo = prev[prev.length - 1];
      if (ultimo && ultimo.hora === hora && ultimo.temp === temp) return prev;
      return [...prev.slice(-59), { hora, temp }];
    });
  }, []);

  useEffect(() => {
    let montado = true;
    (async () => {
      const inicial = await safeRpc(
        () => client.suscribirSensoresTiempoReal(),
        "suscribirSensoresTiempoReal",
        { silent: true }
      );
      if (!montado) return;
      if (inicial) {
        setStreamActivo(true);
        aplicarLoteVivo(inicial);
      }
    })();
    return () => {
      montado = false;
    };
  }, [aplicarLoteVivo]);

  useEffect(() => {
    if (!streamActivo || !macSeleccionada) return;
    const id = window.setInterval(async () => {
      const lote = await safeRpc(
        () => client.obtenerUltimoLoteTiempoReal(),
        "obtenerUltimoLoteTiempoReal",
        { silent: true }
      );
      if (lote && lote.length > 0) {
        aplicarLoteVivo(lote);
      }
    }, 750);
    return () => window.clearInterval(id);
  }, [streamActivo, macSeleccionada, aplicarLoteVivo]);

  // Sincroniza estado de alarma del sensor seleccionado (detecta DESCONECTADO / fallas 1-Wire)
  useEffect(() => {
    if (!macSeleccionada) return;
    let cancelado = false;
    const consultarAlarma = async () => {
      const res = await safeRpc(
        () => client.obtenerEstadoAlarmasActivas(),
        "obtenerEstadoAlarmasActivas",
        { silent: true }
      );
      if (cancelado || !res) return;
      const macObj = macSeleccionada.toUpperCase();
      const hit =
        res.sensoresAfectados.find((s) => s.mac.toUpperCase() === macObj) ??
        null;
      setAlarmaViva(hit);
    };
    void consultarAlarma();
    const id = window.setInterval(() => void consultarAlarma(), 5000);
    return () => {
      cancelado = true;
      window.clearInterval(id);
    };
  }, [macSeleccionada]);

  const cargarCatalogo = useCallback(
    async (forzarSinCache = false) => {
      setCargandoCatalogo(true);
      if (forzarSinCache) {
        await safeRpc(() => client.invalidarCache(), "invalidarCache", {
          silent: true,
        });
      }
      const [lista, amb] = await Promise.all([
        safeRpc(
          () => client.listarSensores(12, 0, soloActivos),
          "listarSensores"
        ),
        safeRpc(
          () => client.obtenerCalibracionAmbiente(),
          "obtenerCalibracionAmbiente",
          { silent: true }
        ),
      ]);
      if (amb) setAmbienteGlobal(amb);
      setCargandoCatalogo(false);
      setCatalogoCargadoUnaVez(true);
      if (lista) {
        setSensores(lista);
        setMacSeleccionada((prev) => {
          if (
            prev &&
            lista.some((s) => s.mac.toUpperCase() === prev.toUpperCase())
          ) {
            return prev;
          }
          return lista.length > 0 ? lista[0].mac.toUpperCase() : "";
        });
      }
    },
    [soloActivos]
  );

  // Recorre todos los bloques de la tabla Censo del sensor sin tope de cantidad
  const cargarHistoriaCompleta = useCallback(
    async (
      idSensorObjetivo: number,
      macObjetivo: string,
      desdeSeg: number,
      hastaSeg: number,
      limiteMaximo = 0
    ) => {
      const reqId = ++historialReqRef.current;
      setCargandoHistorial(true);
      setProgresoHistorial(0);

      const acumulados: LecturaSensorCliente[] = [];
      let offset = 0;

      while (reqId === historialReqRef.current) {
        const lote = await safeRpc(
          () =>
            client.listarCensosPaginados({
              idSensor: idSensorObjetivo,
              limiteRegistros: LOTE_RPC_CENSO,
              offsetRegistros: offset,
            }),
          "cargarHistoriaCompleta",
          { silent: true }
        );

        if (reqId !== historialReqRef.current) return;
        if (!lote || lote.length === 0) break;

        let alcanzoLimite = false;
        for (const r of lote) {
          const ts = r.fechaCreacion;
          if (desdeSeg > 0 && ts > 0 && ts < desdeSeg) {
            alcanzoLimite = true;
            break;
          }
          if (hastaSeg > 0 && ts > 0 && ts > hastaSeg) {
            continue;
          }
          // Excluye códigos centinela (-127, -250..-254) de la curva para no aplastar el eje Y
          if (!esTemperaturaFisicaDS18B20(r.temperaturaCorregida)) {
            continue;
          }
          acumulados.push({
            idSensor: r.idSensor,
            macHex: macObjetivo,
            temperaturaCorregida: r.temperaturaCorregida,
            timestampMs: ts,
          });
          if (limiteMaximo > 0 && acumulados.length >= limiteMaximo) {
            alcanzoLimite = true;
            break;
          }
        }

        setProgresoHistorial(acumulados.length);

        if (offset === 0 || (offset / LOTE_RPC_CENSO) % 4 === 0) {
          setSerieHistorica([...acumulados].reverse());
        }

        if (alcanzoLimite || lote.length < LOTE_RPC_CENSO) break;
        offset += LOTE_RPC_CENSO;
      }

      if (reqId !== historialReqRef.current) return;
      setSerieHistorica([...acumulados].reverse());
      setCargandoHistorial(false);
    },
    []
  );

  const detenerCargaHistorial = () => {
    historialReqRef.current += 1;
    setCargandoHistorial(false);
  };

  const cargarAuditoriasSensor = useCallback(
    async (macObjetivo?: string) => {
      const mac = macObjetivo ?? perfil?.mac ?? "";
      if (!mac) {
        setFilasAuditoria([]);
        return;
      }
      setCargandoAuditoria(true);
      const rows = await safeRpc(
        () => client.listarAuditoriaSensorPorMac(mac, 32),
        "listarAuditoriaSensorPorMac",
        { silent: true }
      );
      setCargandoAuditoria(false);
      if (rows) setFilasAuditoria(rows);
    },
    [perfil?.mac]
  );

  const cargarDetalleSensor = useCallback(
    async (mac: string) => {
      const reqId = ++detalleReqRef.current;
      historialReqRef.current += 1;
      setCargandoHistorial(false);

      if (!mac) {
        setPerfil(null);
        setStatsCenso(null);
        setSerieHistorica([]);
        setFilasAuditoria([]);
        setCargandoDetalle(false);
        return;
      }

      setCargandoDetalle(true);
      const [det, st, auds] = await Promise.all([
        safeRpc(() => client.obtenerSensorPorMac(mac), "obtenerSensorPorMac"),
        safeRpc(
          () => client.obtenerEstadisticasCensoPorMac(mac),
          "obtenerEstadisticasCensoPorMac",
          { silent: true }
        ),
        safeRpc(
          () => client.listarAuditoriaSensorPorMac(mac, 32),
          "listarAuditoriaSensorPorMac",
          { silent: true }
        ),
      ]);

      if (reqId !== detalleReqRef.current) return;
      setCargandoDetalle(false);

      if (st) setStatsCenso(st);
      if (auds) setFilasAuditoria(auds);
      if (det) {
        setPerfil(det);
        setFormMeta({
          nombre: det.nombre,
          ubicacion: det.ubicacion,
          descripcion: det.descripcion,
          activa: det.activa,
        });
        setFormUmbrales({
          umbralSup: det.umbralSup,
          umbralInf: det.umbralInf,
        });
        setFormCalib({
          errorSup: det.errorSup,
          errorInf: det.errorInf,
          offset: det.offset,
        });
        setModoTemporal("ULTIMOS_50");
        void cargarHistoriaCompleta(det.idSensor, det.mac, 0, 0, LIMITE_DEFECTO_CURVA);
      }
    },
    [cargarHistoriaCompleta]
  );

  const cargarTablaCenso = useCallback(
    async (reiniciar = true, forzarSinCache = false) => {
      if (!perfil?.idSensor) {
        setFilasCenso([]);
        setHayMasCensos(false);
        return;
      }

      const reqId = ++tablaReqRef.current;
      const offsetActual = reiniciar ? 0 : filasCensoLenRef.current;

      if (reiniciar) setCargandoTabla(true);
      else setCargandoMasCensos(true);

      if (forzarSinCache) {
        await safeRpc(() => client.invalidarCache(), "invalidarCache", {
          silent: true,
        });
      }

      const rows = await safeRpc(
        () =>
          client.listarCensosPaginados({
            idSensor: perfil.idSensor,
            limiteRegistros: LOTE_SCROLL_CENSO,
            offsetRegistros: offsetActual,
          }),
        "listarCensosPaginados"
      );

      if (reqId !== tablaReqRef.current) return;
      setCargandoTabla(false);
      setCargandoMasCensos(false);

      if (rows) {
        setHayMasCensos(rows.length === LOTE_SCROLL_CENSO);
        if (reiniciar) {
          setFilasCenso(rows);
        } else {
          setFilasCenso((prev) => {
            const ids = new Set(prev.map((r) => r.idCenso));
            const nuevos = rows.filter((r) => !ids.has(r.idCenso));
            return [...prev, ...nuevos];
          });
        }
      }
    },
    [perfil?.idSensor]
  );

  useEffect(() => {
    void cargarCatalogo(false);
  }, [cargarCatalogo]);

  useEffect(() => {
    if (macSeleccionada) {
      void cargarDetalleSensor(macSeleccionada);
    }
  }, [macSeleccionada, cargarDetalleSensor]);

  useEffect(() => {
    if (perfil?.idSensor) {
      void cargarTablaCenso(true, false);
    } else {
      setFilasCenso([]);
      setHayMasCensos(false);
    }
  }, [perfil?.idSensor, cargarTablaCenso]);

  // Refresco silencioso cada 15s en la pestaña de Auditorías del sensor (para ver correos pasar a ENVIADO)
  useEffect(() => {
    if (tabRegistros !== "AUDITORIAS" || !perfil?.mac) return;
    const id = window.setInterval(() => {
      void cargarAuditoriasSensor(perfil.mac);
    }, 15000);
    return () => window.clearInterval(id);
  }, [tabRegistros, perfil?.mac, cargarAuditoriasSensor]);

  // Recarga experimentos al cambiar de sensor cuando el modo EXPERIMENTO está activo
  const cargarExperimentosSensor = useCallback(async () => {
    if (!perfil) return;
    setCargandoExperimentos(true);
    const lista = await detectarExperimentosDesdeAuditoria(perfil, esAdmin);
    setCargandoExperimentos(false);
    setExperimentos(lista);
    if (lista.length > 0) {
      setExperimentoId(lista[0].id);
    } else {
      setExperimentoId("");
    }
  }, [perfil, esAdmin]);

  useEffect(() => {
    if (modoTemporal === "EXPERIMENTO" && perfil) {
      void cargarExperimentosSensor();
    }
  }, [modoTemporal, perfil, cargarExperimentosSensor]);

  const consultarHistorial = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;

    if (modoTemporal === "ULTIMOS_50") {
      void cargarHistoriaCompleta(perfil.idSensor, perfil.mac, 0, 0, LIMITE_DEFECTO_CURVA);
      return;
    }

    let desde = 0;
    let hasta = 0;

    if (modoTemporal === "ENTRE_FECHAS") {
      desde = inputLocalAEpochSegundos(fechaDesde, false);
      hasta = inputLocalAEpochSegundos(fechaHasta, true);
    } else if (modoTemporal === "DESDE_ADELANTE") {
      desde = inputLocalAEpochSegundos(fechaDesde, false);
      hasta = 0;
    } else if (modoTemporal === "HASTA_ATRAS") {
      desde = 0;
      hasta = inputLocalAEpochSegundos(fechaHasta, true);
    } else if (modoTemporal === "EXPERIMENTO") {
      const exp = experimentos.find((x) => x.id === experimentoId);
      if (!exp) {
        toast.info("Seleccione un experimento válido.");
        return;
      }
      desde = exp.inicioSeg;
      hasta = exp.finSeg;
    }

    void cargarHistoriaCompleta(perfil.idSensor, perfil.mac, desde, hasta);
  };

  const seleccionarSensorConFocus = (mac: string) => {
    focusPendienteRef.current = true;
    setMacSeleccionada(mac.toUpperCase());
    setFocusTrigger((n) => n + 1);
  };

  const guardarMetadatos = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    setGuardandoAccion("META");
    const actualizado = await safeRpc(
      () =>
        client.actualizarMetadatosSensor(
          perfil.mac,
          formMeta.nombre,
          formMeta.ubicacion,
          formMeta.descripcion,
          formMeta.activa
        ),
      "actualizarMetadatosSensor",
      { mensajeExito: "Metadatos del sensor actualizados." }
    );
    setGuardandoAccion(null);
    if (actualizado) {
      setPerfil(actualizado);
      void cargarCatalogo(false);
    }
  };

  const guardarSoloUmbrales = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    setGuardandoAccion("UMBRALES");
    const actualizado = await safeRpc(
      () =>
        client.actualizarCalibracionSensor(
          perfil.mac,
          formUmbrales.umbralSup,
          formUmbrales.umbralInf,
          perfil.errorSup,
          perfil.errorInf,
          perfil.offset
        ),
      "actualizarCalibracionSensor",
      {
        mensajeExito: `Umbrales de alarma actualizados (TL ${formUmbrales.umbralInf}°C / TH ${formUmbrales.umbralSup}°C).`,
      }
    );
    setGuardandoAccion(null);
    if (actualizado) {
      setPerfil(actualizado);
      void cargarCatalogo(false);
    }
  };

  const { refCong, refEbull } = obtenerReferenciasActivas(ambienteGlobal);
  const rectaPreview = calcularRectaCalibracion(
    formCalib.errorInf,
    formCalib.errorSup,
    refCong,
    refEbull,
    0
  );

  const guardarSoloCalibracion = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    if (formCalib.errorSup - formCalib.errorInf <= 1.0) {
      toast.error(
        "La medición en agua hirviendo debe superar al menos en 1 °C a la medición en hielo."
      );
      return;
    }
    setGuardandoAccion("CALIB");
    const actualizado = await safeRpc(
      () =>
        client.actualizarCalibracionSensor(
          perfil.mac,
          perfil.umbralSup,
          perfil.umbralInf,
          formCalib.errorSup,
          formCalib.errorInf,
          rectaPreview.ordenadaB
        ),
      "actualizarCalibracionSensor",
      {
        mensajeExito: `Recta de calibración aplicada (${formatearEcuacionRecta(rectaPreview)}).`,
      }
    );
    setGuardandoAccion(null);
    if (actualizado) {
      setPerfil(actualizado);
      void cargarCatalogo(false);
    }
  };

  const restablecerCalibracionFabrica = async () => {
    if (!perfil) return;
    if (
      !window.confirm(
        `¿Restablecer la calibración del sensor ${perfil.mac} a valores de fábrica (T_cal = T_cruda)?`
      )
    ) {
      return;
    }
    setGuardandoAccion("CALIB");
    const actualizado = await safeRpc(
      () =>
        client.actualizarCalibracionSensor(
          perfil.mac,
          perfil.umbralSup,
          perfil.umbralInf,
          0,
          0,
          0
        ),
      "restablecerCalibracion",
      { mensajeExito: "Calibración restablecida a fábrica (m=1, b=0)." }
    );
    setGuardandoAccion(null);
    if (actualizado) {
      setPerfil(actualizado);
      setFormCalib({ errorInf: 0, errorSup: 0, offset: 0 });
      void cargarCatalogo(false);
    }
  };

  const eliminarSensor = async (mac: string) => {
    if (
      !window.confirm(
        `¿Desactivar el sensor ${mac} en el catálogo del sistema?`
      )
    ) {
      return;
    }
    await safeRpc(
      () => client.eliminarSensorPorMac(mac),
      "eliminarSensorPorMac",
      { mensajeExito: `Sensor ${mac} desactivado.` }
    );
    if (macSeleccionada.toUpperCase() === mac.toUpperCase()) {
      setMacSeleccionada("");
    }
    void cargarCatalogo(true);
  };

  const registrarCensoManual = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    setGuardandoAccion("CENSO");
    const creado = await safeRpc(
      () =>
        client.crearCensoManual({
          idSensor: perfil.idSensor,
          temperaturaCruda: nuevoCenso.temperaturaCruda,
          temperaturaCorregida: nuevoCenso.temperaturaCorregida,
          estadoSensor: Number(nuevoCenso.estadoSensor) as EstadoSensor,
        }),
      "crearCensoManual",
      {
        mensajeExito: `Censo manual registrado para sensor #${perfil.idSensor}.`,
      }
    );
    setGuardandoAccion(null);
    if (creado) {
      void cargarTablaCenso(true, false);
      if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
    }
  };

  const eliminarCenso = async (idCenso: number) => {
    await safeRpc(
      () => client.eliminarCensoPorId(idCenso),
      "eliminarCensoPorId",
      { mensajeExito: `Registro Censo #${idCenso} eliminado.` }
    );
    void cargarTablaCenso(true, false);
    if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
  };

  const eliminarAuditoriaSensor = async (idAuditoriaSensor: number) => {
    await safeRpc(
      () => client.eliminarAuditoriaSensorPorId(idAuditoriaSensor),
      "eliminarAuditoriaSensorPorId",
      { mensajeExito: `Evento de auditoría #${idAuditoriaSensor} eliminado.` }
    );
    void cargarAuditoriasSensor();
  };

  const purgarCensos = async () => {
    const limiteSeg = inputLocalAEpochSegundos(fechaPurga, false);
    if (!limiteSeg) return;
    if (
      !window.confirm(
        `¿Purgar todos los registros de Censo anteriores a ${formatearEpochSegundos(limiteSeg)}?`
      )
    ) {
      return;
    }
    const eliminados = await safeRpc(
      () => client.purgarCensosAnterioresA(limiteSeg),
      "purgarCensosAnterioresA",
      { mensajeExito: "Purga histórica completada." }
    );
    if (eliminados !== null) {
      void cargarTablaCenso(true, false);
      if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
    }
  };

  const exportarCurvaCsv = () => {
    if (serieHistorica.length === 0) {
      toast.info("No hay lecturas cargadas en la curva histórica para exportar.");
      return;
    }

    const mac = perfil?.mac || macSeleccionada || "sensor";
    const nombre = perfil?.nombre || "";
    const ubicacion = perfil?.ubicacion || "";

    descargarCsv(
      `curva_${mac}_${sufijoFechaArchivo()}.csv`,
      [
        "id_sensor",
        "mac",
        "nombre_sensor",
        "ubicacion",
        "timestamp_unix",
        "fecha_utc_iso",
        "fecha_hora_local",
        "temperatura_corregida_c",
      ],
      serieHistorica.map((item) => [
        item.idSensor,
        item.macHex,
        nombre,
        ubicacion,
        item.timestampMs,
        formatearIsoUtcDesdeEpoch(item.timestampMs),
        formatearFechaLocalCsv(item.timestampMs),
        item.temperaturaCorregida.toFixed(2),
      ])
    );

    toast.success(`CSV de curva exportado (${serieHistorica.length} registros).`);
  };

  const exportarCatalogoExcel = async () => {
    if (sensores.length === 0) {
      toast.info("No hay sensores cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `catalogo_sensores_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Sensores",
      titulo: "Catálogo de Sensores DS18B20",
      subtitulo: `${sensores.length} sensores registrados`,
      columnas: [
        { header: "ID", width: 10, numFmt: "0" },
        { header: "Dirección MAC", width: 20 },
        { header: "Nombre", width: 24 },
        { header: "Ubicación", width: 22 },
        { header: "TL (°C)", width: 12, numFmt: "0.00" },
        { header: "TH (°C)", width: 12, numFmt: "0.00" },
        { header: "Offset (°C)", width: 14, numFmt: "0.00" },
        { header: "Estado", width: 12 },
      ],
      filas: sensores.map((s) => [
        s.idSensor,
        s.mac,
        s.nombre || "Sin nombre",
        s.ubicacion || "—",
        Number(s.umbralInf.toFixed(2)),
        Number(s.umbralSup.toFixed(2)),
        Number(s.offset.toFixed(2)),
        s.activa ? "ACTIVO" : "INACTIVO",
      ]),
    });
    toast.success(`Excel de sensores exportado (${sensores.length} filas).`);
  };

  const exportarCurvaExcel = async () => {
    if (serieHistorica.length === 0) {
      toast.info("No hay lecturas cargadas en la curva para exportar.");
      return;
    }
    setExportandoExcel(true);
    try {
      await exportarCurvaTermicaExcel({
        nombreArchivo: `analisis_${perfil?.mac || macSeleccionada}_${sufijoFechaArchivo()}.xlsx`,
        nombreSensor: perfil?.nombre || "Sensor DS18B20",
        mac: perfil?.mac || macSeleccionada,
        ubicacion: perfil?.ubicacion || "",
        subtitulo: `Modo: ${modoTemporal} · n = ${serieHistorica.length} lecturas`,
        umbralInf: perfil?.umbralInf,
        umbralSup: perfil?.umbralSup,
        serie: serieHistorica,
        graficoContenedor: chartContainerRef.current,
      });
      toast.success(
        `Excel con fórmulas y gráfico exportado (${serieHistorica.length} puntos).`
      );
    } finally {
      setExportandoExcel(false);
    }
  };

  const exportarTablaCensoExcel = async () => {
    if (!perfil) return;
    setExportandoExcel(true);
    try {
      const registros: CensoRowDTO[] = [];
      let off = 0;
      while (true) {
        const lote = await safeRpc(
          () =>
            client.listarCensosPaginados({
              idSensor: perfil.idSensor,
              limiteRegistros: LOTE_RPC_CENSO,
              offsetRegistros: off,
            }),
          "exportarCensosExcelLote",
          { silent: true }
        );
        if (!lote || lote.length === 0) break;
        registros.push(...lote);
        if (lote.length < LOTE_RPC_CENSO) break;
        off += LOTE_RPC_CENSO;
      }
      if (registros.length === 0) {
        toast.info("No hay censos para exportar para este sensor.");
        return;
      }
      await exportarTablaExcel({
        nombreArchivo: `censos_sensor_${perfil.idSensor}_${sufijoFechaArchivo()}.xlsx`,
        nombreHoja: "Censos",
        titulo: `Historial Completo de Censos · ${perfil.nombre || perfil.mac}`,
        subtitulo: `Sensor #${perfil.idSensor} (${perfil.mac}) · ${registros.length} lecturas`,
        resumen: statsCenso
          ? [
              { etiqueta: "Total Lecturas BD", valor: statsCenso.totalLecturas, numFmt: "0" },
              { etiqueta: "Media Global (°C)", valor: Number(statsCenso.temperaturaPromedio.toFixed(2)), numFmt: "0.00" },
              { etiqueta: "Mínima Global (°C)", valor: Number(statsCenso.temperaturaMinima.toFixed(2)), numFmt: "0.00" },
              { etiqueta: "Máxima Global (°C)", valor: Number(statsCenso.temperaturaMaxima.toFixed(2)), numFmt: "0.00" },
            ]
          : undefined,
        columnas: [
          { header: "ID Censo", width: 12, numFmt: "0" },
          { header: "Temp. Cruda (°C)", width: 18, numFmt: "0.00" },
          { header: "Temp. Corregida (°C)", width: 20, numFmt: "0.00" },
          { header: "Estado DS18B20", width: 28 },
          { header: "Fecha / Hora Local", width: 22 },
          { header: "Timestamp Unix", width: 16, numFmt: "0" },
        ],
        filas: registros.map((r) => [
          r.idCenso,
          Number(r.temperaturaCruda.toFixed(2)),
          Number(r.temperaturaCorregida.toFixed(2)),
          LABEL_ESTADO_SENSOR[r.estadoSensor] ?? `Estado ${r.estadoSensor}`,
          formatearFechaLocalCsv(r.fechaCreacion),
          r.fechaCreacion,
        ]),
      });
      toast.success(`Excel completo exportado (${registros.length} censos).`);
    } finally {
      setExportandoExcel(false);
    }
  };

  const exportarAuditoriasSensorExcel = async () => {
    if (!perfil || filasAuditoria.length === 0) {
      toast.info("No hay auditorías registradas para este sensor.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `auditoria_sensor_${perfil.mac}_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Auditoria Sensor",
      titulo: `Auditoría de Eventos · ${perfil.nombre || perfil.mac}`,
      subtitulo: `Sensor #${perfil.idSensor} (${perfil.mac}) · ${filasAuditoria.length} eventos`,
      columnas: [
        { header: "ID Evento", width: 12, numFmt: "0" },
        { header: "ID Censo", width: 12, numFmt: "0" },
        { header: "Estado Previo", width: 26 },
        { header: "Estado Nuevo", width: 26 },
        { header: "Bus 1-Wire", width: 22 },
        { header: "Tipo", width: 16 },
        { header: "Criticidad", width: 14 },
        { header: "Correo Autónomo", width: 18 },
        { header: "Fecha / Hora", width: 22 },
      ],
      filas: filasAuditoria.map((ev) => [
        ev.idAuditoriaSensor,
        ev.idCenso,
        LABEL_ESTADO_SENSOR[ev.estadoPrevioSensor] ?? String(ev.estadoPrevioSensor),
        LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ?? String(ev.estadoNuevoSensor),
        LABEL_ESTADO_BUS[ev.estadoBus] ?? String(ev.estadoBus),
        LABEL_TIPO_EVENTO[ev.tipoEventoSensor] ?? String(ev.tipoEventoSensor),
        LABEL_CRITICIDAD[ev.criticidadSensor] ?? String(ev.criticidadSensor),
        LABEL_ESTADO_ENVIO[ev.estadoReporte] ?? String(ev.estadoReporte),
        ev.fechaCreacion || "—",
      ]),
    });
    toast.success(`Excel de auditorías exportado (${filasAuditoria.length} filas).`);
  };

  // Cálculo del estado vivo actual del sensor seleccionado
  const estadoVivo = resolverEstadoLecturaVivo(
    lecturaViva?.temperaturaCorregida,
    perfil?.umbralInf,
    perfil?.umbralSup,
    alarmaViva?.estado
  );
  const enAlertaViva = estadoVivo !== EstadoSensor.OK;
  const lecturaVivaValida =
    lecturaViva !== null &&
    esTemperaturaFisicaDS18B20(lecturaViva.temperaturaCorregida) &&
    (estadoVivo === EstadoSensor.OK ||
      estadoVivo === EstadoSensor.ALARMA_SUPERIOR ||
      estadoVivo === EstadoSensor.ALARMA_INFERIOR);

  const badgeCriticidad = (c: CriticidadEvento) => {
    if (c === CriticidadEvento.ALTA)
      return "bg-alert-err-bg text-alert-err-icon";
    if (c === CriticidadEvento.MEDIA)
      return "bg-alert-warn-bg text-alert-warn-icon";
    if (c === CriticidadEvento.BAJA) return "bg-info-light text-info";
    return "bg-secondary text-foreground-secondary";
  };

  const badgeEstadoCorreo = (est: EstadoEnvioCorreo) => {
    if (est === EstadoEnvioCorreo.ENVIADO)
      return "bg-alert-success-bg text-alert-success-icon";
    if (est === EstadoEnvioCorreo.FALLIDO)
      return "bg-alert-err-bg text-alert-err-icon";
    return "bg-alert-warn-bg text-alert-warn-icon";
  };

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado y Catálogo de Sensores */}
      <div
        data-tour="sens-catalogo"
        className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
      >
        <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
          <div className="flex items-center gap-2">
            <Thermometer className="h-5 w-5 text-primary" />
            <h1 className="text-lg font-bold">
              Catálogo de Sensores DS18B20 ({sensores.length})
            </h1>
            {cargandoCatalogo && (
              <Loader2 className="h-4 w-4 animate-spin text-primary" />
            )}
          </div>
          <div className="flex flex-wrap items-center gap-2.5">
            {esAdmin && sensores.length > 0 && (
              <button
                type="button"
                data-tour="sens-wizard"
                className="btn btn-solid btn-warning px-3 py-1.5 text-xs font-bold"
                onClick={() => setWizardAbierto(true)}
              >
                <Sliders className="h-3.5 w-3.5" />
                Asistente de Calibración (Wizard)
              </button>
            )}
            {onIrAConfigAmbiente && (
              <button
                type="button"
                className="btn btn-outline px-2.5 py-1.5 text-xs"
                onClick={onIrAConfigAmbiente}
                title="La Calibración Ambiental Global vive en Configuración"
              >
                <Globe className="h-3.5 w-3.5 text-info" />
                Calibración Ambiental Global
              </button>
            )}
            <label className="flex cursor-pointer items-center gap-1.5 text-xs">
              <input
                type="checkbox"
                checked={soloActivos}
                onChange={(e) => setSoloActivos(e.target.checked)}
              />
              Solo activos
            </label>
            <button
              type="button"
              className="btn btn-outline btn-success px-2.5 py-1.5 text-xs"
              disabled={sensores.length === 0}
              onClick={() => void exportarCatalogoExcel()}
              title="Exportar catálogo de sensores a Excel (.xlsx)"
            >
              <FileSpreadsheet className="h-3.5 w-3.5" />
              Excel
            </button>
            <button
              type="button"
              className="btn btn-outline px-3 py-1.5 text-xs"
              disabled={cargandoCatalogo}
              onClick={() => {
                void cargarCatalogo(true);
                if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
              }}
            >
              <RefreshCw
                className={`h-3.5 w-3.5 ${cargandoCatalogo ? "animate-spin" : ""}`}
              />
              Actualizar
            </button>
          </div>
        </div>

        <DataTable
          data={sensores}
          rowKey={(s) => s.mac}
          onRowClick={(s) => seleccionarSensorConFocus(s.mac)}
          searchPlaceholder="Buscar ID, MAC, nombre o ubicación..."
          searchValue={(s) =>
            `#${s.idSensor} ${s.mac} ${s.nombre} ${s.ubicacion}`
          }
          filters={
            [
              {
                key: "activa",
                label: "Estado",
                getValue: (s) => (s.activa ? "1" : "0"),
                options: [
                  { value: "1", label: "ACTIVO" },
                  { value: "0", label: "INACTIVO" },
                ],
              },
            ] as DataTableFilter<ResumenSensorCliente>[]
          }
          rowClassName={(s) =>
            s.mac.toUpperCase() === macSeleccionada.toUpperCase()
              ? "bg-primary-light hover:bg-primary-light"
              : "hover:bg-secondary/40"
          }
          columns={[
            {
              key: "id",
              header: "ID",
              sortValue: (s) => s.idSensor,
              cellClassName: "font-mono font-semibold",
              cell: (s) => `#${s.idSensor}`,
            },
            {
              key: "mac",
              header: "Dirección MAC",
              sortValue: (s) => s.mac,
              cellClassName: "font-mono",
              cell: (s) => s.mac,
            },
            {
              key: "nombre",
              header: "Nombre",
              sortValue: (s) => s.nombre || "Sin nombre",
              cellClassName: "font-medium",
              cell: (s) => s.nombre || "Sin nombre",
            },
            {
              key: "ubicacion",
              header: "Ubicación",
              sortValue: (s) => s.ubicacion,
              cellClassName: "text-foreground-secondary",
              cell: (s) => s.ubicacion || "—",
            },
            {
              key: "umbrales",
              header: "TL / TH (°C)",
              sortValue: (s) => s.umbralSup,
              cellClassName: "font-mono",
              cell: (s) =>
                `${s.umbralInf.toFixed(1)} / ${s.umbralSup.toFixed(1)}`,
            },
            {
              key: "offset",
              header: "Offset",
              sortValue: (s) => s.offset,
              cellClassName: "font-mono",
              cell: (s) =>
                s.offset >= 0
                  ? `+${s.offset.toFixed(2)}`
                  : s.offset.toFixed(2),
            },
            {
              key: "estado",
              header: "Estado",
              sortValue: (s) => s.activa,
              cell: (s) => (
                <span
                  className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                    s.activa
                      ? "bg-alert-success-bg text-alert-success-icon"
                      : "bg-alert-inactive-bg text-alert-inactive-icon"
                  }`}
                >
                  {s.activa ? "ACTIVO" : "INACTIVO"}
                </span>
              ),
            },
            ...(esAdmin
              ? ([
                  {
                    key: "acciones",
                    header: "Acciones",
                    headerClassName: "text-right",
                    cellClassName: "text-right",
                    stopRowClick: true,
                    cell: (s) => (
                      <button
                        className="btn btn-icon btn-error"
                        onClick={() => void eliminarSensor(s.mac)}
                        title="Desactivar sensor"
                      >
                        <Trash2 className="h-3.5 w-3.5" />
                      </button>
                    ),
                  },
                ] as DataTableColumn<ResumenSensorCliente>[])
              : []),
          ]}
          emptyMessage={
            cargandoCatalogo
              ? "Cargando catálogo de sensores..."
              : "No hay sensores registrados en la base de datos."
          }
        />
      </div>

      {/* Perfil, Monitor en Vivo e Historial Gráfico del Sensor Seleccionado */}
      {macSeleccionada && (
        <div
          className={`grid grid-cols-1 gap-4 transition-opacity xl:grid-cols-3 ${
            cargandoDetalle ? "opacity-75" : "opacity-100"
          }`}
        >
          {/* Columna izquierda: Card fijada por MAC + Visor en Vivo + Estadísticas Históricas */}
          <div
            ref={macCardRef}
            className={`scroll-mt-4 space-y-4 rounded-xl border bg-card p-4 text-card-foreground shadow-xs transition-all duration-300 ${
              resaltarCardMac
                ? "border-primary ring-2 ring-primary/60"
                : enAlertaViva
                  ? "border-alert-err-border"
                  : "border-border"
            }`}
          >
            {perfil ? (
              <>
                <div>
                  <div className="flex items-center justify-between">
                    <span className="inline-flex items-center gap-1.5 font-mono text-xs font-bold text-primary">
                      {perfil.mac}
                      {cargandoDetalle && (
                        <Loader2 className="h-3.5 w-3.5 animate-spin" />
                      )}
                    </span>
                    <span className="rounded bg-secondary px-2 py-0.5 font-mono text-xs">
                      ID #{perfil.idSensor}
                    </span>
                  </div>
                  <h2 className="mt-1 text-base font-bold">
                    {perfil.nombre || "Sensor DS18B20"}
                  </h2>
                  <p className="text-xs text-foreground-secondary">
                    {perfil.ubicacion || "Sin ubicación"} ·{" "}
                    {perfil.descripcion || "Sin descripción"}
                  </p>
                  <div className="mt-2 space-y-0.5 text-[11px] text-foreground-secondary">
                    <div>Creado: {perfil.fechaCreacion || "—"}</div>
                    <div>Modificado: {perfil.fechaModificacion || "—"}</div>
                    <div>
                      Umbrales: TL={perfil.umbralInf.toFixed(1)}°C · TH=
                      {perfil.umbralSup.toFixed(1)}°C
                    </div>
                    <div>
                      Calibración: E_inf={perfil.errorInf.toFixed(2)}°C · E_sup=
                      {perfil.errorSup.toFixed(2)}°C · Offset=
                      {perfil.offset.toFixed(2)}°C
                    </div>
                  </div>
                </div>

                {/* Visor en Tiempo Real exclusivo de este sensor (reutiliza stream compartido) */}
                <div
                  className={`space-y-2.5 rounded-lg border p-3 text-xs ${
                    enAlertaViva
                      ? "border-alert-err-border bg-alert-err-bg/60"
                      : "border-border bg-background"
                  }`}
                >
                  <div className="flex items-center justify-between gap-2">
                    <span className="inline-flex items-center gap-1.5 font-semibold">
                      <Activity
                        className={`h-3.5 w-3.5 ${
                          enAlertaViva
                            ? "text-alert-err-icon"
                            : streamActivo
                              ? "text-success"
                              : "text-foreground-secondary"
                        }`}
                      />
                      En Tiempo Real
                    </span>
                    <div className="flex items-center gap-1">
                      <button
                        type="button"
                        onClick={() => setMostrarGraficoVivo((v) => !v)}
                        className="rounded border border-border bg-card px-1.5 py-0.5 text-[10px] font-medium hover:bg-secondary"
                      >
                        {mostrarGraficoVivo ? "Ocultar curva" : "Ver curva"}
                      </button>
                      <button
                        type="button"
                        onClick={() => setVivoPausado((p) => !p)}
                        className="rounded border border-border bg-card p-1 hover:bg-secondary"
                        title={
                          vivoPausado
                            ? "Reanudar curva en vivo"
                            : "Pausar curva en vivo"
                        }
                      >
                        {vivoPausado ? (
                          <Play className="h-3 w-3 text-warning" />
                        ) : (
                          <Pause className="h-3 w-3" />
                        )}
                      </button>
                    </div>
                  </div>

                  <div className="flex items-baseline justify-between gap-2">
                    <span
                      className={`text-2xl font-bold tracking-tight tabular-nums ${
                        enAlertaViva ? "text-alert-err-icon" : "text-foreground"
                      }`}
                    >
                      {lecturaVivaValida && lecturaViva
                        ? `${lecturaViva.temperaturaCorregida.toFixed(2)} °C`
                        : enAlertaViva
                          ? "SIN LECTURA"
                          : "—"}
                    </span>
                    <span
                      className={`inline-flex items-center gap-1 rounded px-2 py-0.5 text-[10px] font-bold ${
                        enAlertaViva
                          ? "bg-error text-error-foreground"
                          : lecturaViva
                            ? "bg-alert-success-bg text-alert-success-icon"
                            : "bg-secondary text-foreground-secondary"
                      }`}
                    >
                      {enAlertaViva && (
                        <AlertTriangle className="h-3 w-3 shrink-0" />
                      )}
                      {enAlertaViva
                        ? (LABEL_ESTADO_SENSOR[estadoVivo] ?? `E${estadoVivo}`)
                        : lecturaViva
                          ? "EN LÍNEA"
                          : "SIN STREAM"}
                    </span>
                  </div>

                  {enAlertaViva ? (
                    <div className="flex items-start gap-1.5 rounded border border-alert-err-border/70 bg-background/85 p-2 text-[11px] leading-snug text-foreground">
                      <Info className="mt-0.5 h-3.5 w-3.5 shrink-0 text-alert-err-icon" />
                      <span>{describirEstadoSensor(estadoVivo)}</span>
                    </div>
                  ) : (
                    <div className="text-[11px] text-foreground-secondary">
                      {lecturaViva
                        ? `Última muestra: ${formatearEpochSegundos(lecturaViva.timestampMs)}`
                        : "Sin lecturas en vivo en este momento (verificá conexión en bus 1-Wire)."}
                    </div>
                  )}

                  {mostrarGraficoVivo && lecturaVivaValida && (
                    <div className="h-28 w-full pt-1">
                      {curvaViva.length >= 2 ? (
                        <ResponsiveContainer width="100%" height="100%">
                          <AreaChart
                            data={curvaViva}
                            margin={{ top: 4, right: 6, left: -18, bottom: 0 }}
                          >
                            <CartesianGrid
                              strokeDasharray="3 3"
                              stroke="currentColor"
                              className="opacity-15"
                            />
                            <XAxis
                              dataKey="hora"
                              tick={{ fontSize: 9 }}
                              minTickGap={22}
                            />
                            <YAxis
                              domain={["auto", "auto"]}
                              tick={{ fontSize: 9 }}
                              width={44}
                            />
                            <ReferenceLine
                              y={perfil.umbralSup}
                              stroke="#ef4444"
                              strokeDasharray="3 3"
                              ifOverflow="hidden"
                            />
                            <ReferenceLine
                              y={perfil.umbralInf}
                              stroke="#3b82f6"
                              strokeDasharray="3 3"
                              ifOverflow="hidden"
                            />
                            <Tooltip
                              formatter={(v: unknown) => [
                                `${Number(v ?? 0).toFixed(2)} °C`,
                                "En vivo",
                              ]}
                              contentStyle={{
                                fontSize: 11,
                                borderRadius: 8,
                              }}
                            />
                            <Area
                              type="monotone"
                              dataKey="temp"
                              stroke={enAlertaViva ? "#ef4444" : "#10b981"}
                              fill={enAlertaViva ? "#ef4444" : "#10b981"}
                              fillOpacity={0.18}
                              strokeWidth={2}
                              dot={false}
                              isAnimationActive={false}
                            />
                          </AreaChart>
                        </ResponsiveContainer>
                      ) : (
                        <div className="flex h-full items-center justify-center rounded border border-dashed border-border text-[11px] text-foreground-secondary">
                          Acumulando puntos en vivo ({curvaViva.length}/2)...
                        </div>
                      )}
                    </div>
                  )}
                </div>

                {statsCenso && (
                  <div className="space-y-2 border-t border-border pt-3 text-xs">
                    <div className="text-[10px] font-semibold tracking-wide text-foreground-secondary uppercase">
                      Histórico total acumulado en BD (N ={" "}
                      {statsCenso.totalLecturas})
                    </div>
                    <div className="grid grid-cols-2 gap-2">
                      <div className="rounded-lg border border-border bg-background p-2.5">
                        <div className="text-foreground-secondary">
                          Total lecturas
                        </div>
                        <div className="text-base font-bold">
                          {statsCenso.totalLecturas}
                        </div>
                      </div>
                      <div className="rounded-lg border border-border bg-background p-2.5">
                        <div className="text-foreground-secondary">
                          Media global
                        </div>
                        <div className="text-base font-bold text-primary">
                          {statsCenso.totalLecturas > 0
                            ? `${statsCenso.temperaturaPromedio.toFixed(2)} °C`
                            : "—"}
                        </div>
                      </div>
                      <div className="rounded-lg border border-border bg-background p-2.5">
                        <div className="text-foreground-secondary">
                          Mínima global
                        </div>
                        <div className="text-base font-bold text-info">
                          {statsCenso.totalLecturas > 0
                            ? `${statsCenso.temperaturaMinima.toFixed(2)} °C`
                            : "—"}
                        </div>
                      </div>
                      <div className="rounded-lg border border-border bg-background p-2.5">
                        <div className="text-foreground-secondary">
                          Máxima global
                        </div>
                        <div className="text-base font-bold text-error">
                          {statsCenso.totalLecturas > 0
                            ? `${statsCenso.temperaturaMaxima.toFixed(2)} °C`
                            : "—"}
                        </div>
                      </div>
                    </div>
                  </div>
                )}
              </>
            ) : (
              <div className="flex h-48 items-center justify-center gap-2 text-xs text-foreground-secondary">
                <Loader2 className="h-4 w-4 animate-spin text-primary" />
                Cargando perfil del sensor {macSeleccionada}...
              </div>
            )}
          </div>

          {/* Columna derecha (2 cols): Consulta Temporal sin límite + Gráfico Matemático */}
          <div
            data-tour="sens-curva"
            className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-2"
          >
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <div className="flex items-center gap-2">
                <History className="h-4 w-4 text-primary" />
                <h3 className="text-sm font-bold">
                  Análisis Temporal ({perfil?.nombre || macSeleccionada})
                </h3>
                <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px]">
                  {cargandoHistorial
                    ? `cargando ${progresoHistorial}${statsCenso?.totalLecturas ? ` / ${statsCenso.totalLecturas}` : ""}...`
                    : `n = ${serieHistorica.length.toLocaleString("es-AR")}`}
                </span>
              </div>

              <form
                onSubmit={consultarHistorial}
                className="flex flex-wrap items-center gap-1.5 text-xs"
              >
                <select
                  value={modoTemporal}
                  onChange={(e) =>
                    setModoTemporal(e.target.value as ModoConsultaTemporal)
                  }
                  className="rounded border border-border bg-background px-2 py-1"
                >
                  <option value="ULTIMOS_50">Últimos 50 censos</option>
                  <option value="TODO">Todo el historial</option>
                  <option value="ENTRE_FECHAS">Desde – Hasta</option>
                  <option value="DESDE_ADELANTE">Desde fecha en adelante</option>
                  <option value="HASTA_ATRAS">Hasta fecha hacia atrás</option>
                  <option value="EXPERIMENTO">
                    Experimento (cambio de nombre · beta)
                  </option>
                </select>

                {(modoTemporal === "ENTRE_FECHAS" ||
                  modoTemporal === "DESDE_ADELANTE") && (
                  <input
                    type="datetime-local"
                    value={fechaDesde}
                    onChange={(e) => setFechaDesde(e.target.value)}
                    title="Fecha/hora inicio"
                    className="rounded border border-border bg-background px-2 py-1"
                  />
                )}

                {(modoTemporal === "ENTRE_FECHAS" ||
                  modoTemporal === "HASTA_ATRAS") && (
                  <input
                    type="datetime-local"
                    value={fechaHasta}
                    onChange={(e) => setFechaHasta(e.target.value)}
                    title="Fecha/hora fin"
                    className="rounded border border-border bg-background px-2 py-1"
                  />
                )}

                {modoTemporal === "EXPERIMENTO" && (
                  <div className="inline-flex items-center gap-1">
                    <FlaskConical className="h-3.5 w-3.5 text-warning" />
                    <select
                      value={experimentoId}
                      onChange={(e) => setExperimentoId(e.target.value)}
                      disabled={cargandoExperimentos || experimentos.length === 0}
                      className="max-w-56 rounded border border-border bg-background px-2 py-1 text-xs"
                    >
                      {cargandoExperimentos && (
                        <option value="">Detectando experimentos...</option>
                      )}
                      {!cargandoExperimentos &&
                        experimentos.map((exp) => (
                          <option key={exp.id} value={exp.id}>
                            {exp.nombre} (
                            {formatearEpochSegundos(exp.inicioSeg).slice(0, 16)}
                            {exp.enCurso ? " · en curso" : ""})
                          </option>
                        ))}
                    </select>
                  </div>
                )}

                {cargandoHistorial ? (
                  <button
                    type="button"
                    onClick={detenerCargaHistorial}
                    className="btn btn-outline btn-error px-2.5 py-1 text-xs"
                    title="Detener lectura de bloques y conservar lo cargado"
                  >
                    <Square className="h-3 w-3" />
                    Detener
                  </button>
                ) : (
                  <button
                    type="submit"
                    disabled={!perfil}
                    className="btn btn-solid btn-primary px-3 py-1 text-xs"
                  >
                    <Search className="h-3.5 w-3.5" />
                    Consultar
                  </button>
                )}

                <button
                  type="button"
                  disabled={
                    serieHistorica.length === 0 ||
                    cargandoHistorial ||
                    exportandoExcel
                  }
                  onClick={() => void exportarCurvaExcel()}
                  className="btn btn-outline btn-success px-2.5 py-1 text-xs"
                  title="Exportar informe Excel (.xlsx) con fórmulas dinámicas y gráfico incrustado"
                >
                  {exportandoExcel ? (
                    <Loader2 className="h-3.5 w-3.5 animate-spin" />
                  ) : (
                    <FileSpreadsheet className="h-3.5 w-3.5" />
                  )}
                  Excel
                </button>

                <button
                  type="button"
                  disabled={serieHistorica.length === 0 || cargandoHistorial}
                  onClick={exportarCurvaCsv}
                  className="btn btn-outline px-2.5 py-1 text-xs"
                  title="Descargar todos los puntos cargados en la curva en formato CSV"
                >
                  <Download className="h-3.5 w-3.5" />
                  CSV
                </button>
              </form>
            </div>

            <div ref={chartContainerRef}>
              <SensorAnalyticsChart
                serie={serieHistorica}
                umbralInf={perfil?.umbralInf}
                umbralSup={perfil?.umbralSup}
                cargando={
                  cargandoDetalle ||
                  (cargandoHistorial && serieHistorica.length === 0)
                }
              />
            </div>
          </div>
        </div>
      )}

      {/* Formularios Exclusivos ADMIN: 3 tarjetas independientes (Metadatos | Umbrales | Calibración) */}
      {esAdmin && (
        <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
          {/* 1. Metadatos del sensor */}
          <form
            onSubmit={guardarMetadatos}
            className="flex flex-col justify-between space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="space-y-3">
              <div className="flex items-center gap-2 text-sm font-bold">
                <Edit3 className="h-4 w-4 text-primary" />
                Metadatos del Sensor {perfil ? `#${perfil.idSensor}` : ""}
              </div>
              <div>
                <label className="mb-1 block text-xs">Nombre</label>
                <input
                  type="text"
                  maxLength={31}
                  disabled={!perfil}
                  value={formMeta.nombre}
                  onChange={(e) =>
                    setFormMeta({ ...formMeta, nombre: e.target.value })
                  }
                  className="w-full rounded border border-border bg-background px-2.5 py-1.5 text-xs"
                />
              </div>
              <div>
                <label className="mb-1 block text-xs">Ubicación</label>
                <input
                  type="text"
                  maxLength={31}
                  disabled={!perfil}
                  value={formMeta.ubicacion}
                  onChange={(e) =>
                    setFormMeta({ ...formMeta, ubicacion: e.target.value })
                  }
                  className="w-full rounded border border-border bg-background px-2.5 py-1.5 text-xs"
                />
              </div>
              <div>
                <label className="mb-1 block text-xs">Descripción</label>
                <input
                  type="text"
                  maxLength={127}
                  disabled={!perfil}
                  value={formMeta.descripcion}
                  onChange={(e) =>
                    setFormMeta({ ...formMeta, descripcion: e.target.value })
                  }
                  className="w-full rounded border border-border bg-background px-2.5 py-1.5 text-xs"
                />
              </div>
              <label className="flex items-center gap-2 text-xs">
                <input
                  type="checkbox"
                  disabled={!perfil}
                  checked={formMeta.activa}
                  onChange={(e) =>
                    setFormMeta({ ...formMeta, activa: e.target.checked })
                  }
                />
                Sensor habilitado para muestreo y alarmas
              </label>
            </div>
            <button
              type="submit"
              disabled={!perfil || guardandoAccion === "META"}
              className="btn btn-solid btn-primary w-full text-xs font-semibold"
            >
              {guardandoAccion === "META" && (
                <Loader2 className="h-3.5 w-3.5 animate-spin" />
              )}
              Guardar Metadatos
            </button>
          </form>

          {/* 2. Umbrales de Alarma Térmica */}
          <form
            data-tour="sens-umbrales"
            onSubmit={guardarSoloUmbrales}
            className="flex flex-col justify-between space-y-3 rounded-xl border border-alert-err-border/70 bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="space-y-3">
              <div className="flex items-center gap-2 text-sm font-bold text-alert-err-icon">
                <BellRing className="h-4 w-4" />
                Umbrales de Alarma (TL / TH)
              </div>
              <p className="text-[11px] text-foreground-secondary">
                Define el rango térmico seguro del producto. Si la temperatura
                corregida sale de estos límites se activa la sirena y el LED. No
                modifica la calibración del sensor.
              </p>
              <div className="grid grid-cols-2 gap-2.5 text-xs">
                <div>
                  <label className="mb-1 block font-medium">
                    Mínimo TL (°C)
                  </label>
                  <NumericInput
                    value={formUmbrales.umbralInf}
                    onChange={(v) =>
                      setFormUmbrales((p) => ({ ...p, umbralInf: v }))
                    }
                    decimals={2}
                    min={-55}
                    max={126}
                    disabled={!perfil}
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block font-medium">
                    Máximo TH (°C)
                  </label>
                  <NumericInput
                    value={formUmbrales.umbralSup}
                    onChange={(v) =>
                      setFormUmbrales((p) => ({ ...p, umbralSup: v }))
                    }
                    decimals={2}
                    min={-55}
                    max={126}
                    disabled={!perfil}
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
              </div>
              {formUmbrales.umbralInf > formUmbrales.umbralSup && (
                <div className="rounded border border-alert-err-border bg-alert-err-bg px-2.5 py-1.5 text-[11px] text-alert-err-icon">
                  El umbral inferior (TL) no puede superar al superior (TH).
                </div>
              )}
            </div>
            <button
              type="submit"
              disabled={
                !perfil ||
                guardandoAccion === "UMBRALES" ||
                formUmbrales.umbralInf > formUmbrales.umbralSup
              }
              className="btn btn-solid btn-error w-full text-xs font-semibold"
            >
              {guardandoAccion === "UMBRALES" && (
                <Loader2 className="h-3.5 w-3.5 animate-spin" />
              )}
              Guardar Umbrales de Alarma
            </button>
          </form>

          {/* 3. Calibración de Precisión (Recta Lineal de 2 Puntos) */}
          <form
            data-tour="sens-calibracion"
            onSubmit={guardarSoloCalibracion}
            className="flex flex-col justify-between space-y-3 rounded-xl border border-alert-warn-border/70 bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="space-y-3">
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2 text-sm font-bold text-warning">
                  <Sliders className="h-4 w-4" />
                  <span>Calibración 2 Puntos (Manual)</span>
                </div>
                <FormulaTooltip
                  titulo="Modelo Recta Lineal de 2 Puntos"
                  archivoModelo="ModeloCalibracionDosPuntos.hpp"
                  ecuacion={
                    "m = (T_ref,sup − T_ref,inf) / (T_ebull − T_hielo)\nb = T_ref,inf − m · T_hielo\nT_cal = m · T_cruda + b"
                  }
                  sustitucion={
                    rectaPreview.usaDosPuntos
                      ? `m = (${refEbull.toFixed(2)} − ${refCong.toFixed(2)}) / (${formCalib.errorSup.toFixed(2)} − ${formCalib.errorInf.toFixed(2)}) = ${rectaPreview.pendienteM.toFixed(4)} | b = ${rectaPreview.ordenadaB.toFixed(2)} °C`
                      : "Sin 2 puntos cargados → T_cal = T_cruda"
                  }
                  descripcion="Corrige pendiente y corrimiento en todo el rango térmico usando los puntos medidos en hielo y agua hirviendo. No altera los umbrales TL/TH."
                />
              </div>

              <p className="text-[11px] text-foreground-secondary">
                Cargue las lecturas crudas obtenidas en hielo (
                {refCong.toFixed(2)} °C) y agua hirviendo ({refEbull.toFixed(2)}{" "}
                °C), o use el <strong>Asistente Wizard</strong> arriba.
              </p>

              <div className="grid grid-cols-2 gap-2.5 text-xs">
                <div>
                  <label className="mb-1 block">T_hielo medida (°C)</label>
                  <NumericInput
                    value={formCalib.errorInf}
                    onChange={(v) =>
                      setFormCalib((p) => ({ ...p, errorInf: v }))
                    }
                    decimals={2}
                    min={-55}
                    max={126}
                    disabled={!perfil}
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">T_ebull medida (°C)</label>
                  <NumericInput
                    value={formCalib.errorSup}
                    onChange={(v) =>
                      setFormCalib((p) => ({ ...p, errorSup: v }))
                    }
                    decimals={2}
                    min={-55}
                    max={126}
                    disabled={!perfil}
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
              </div>

              <div className="rounded-lg border border-border bg-background px-2.5 py-2 font-mono text-[11px]">
                <div className="text-[10px] text-foreground-secondary">
                  Ecuación resultante en vivo:
                </div>
                <div className="mt-0.5 font-bold text-primary">
                  {formatearEcuacionRecta(rectaPreview)}
                </div>
              </div>
            </div>

            <div className="flex items-center gap-2">
              <button
                type="submit"
                disabled={
                  !perfil ||
                  guardandoAccion === "CALIB" ||
                  !rectaPreview.usaDosPuntos
                }
                className="btn btn-solid btn-warning flex-1 text-xs font-semibold"
              >
                {guardandoAccion === "CALIB" && (
                  <Loader2 className="h-3.5 w-3.5 animate-spin" />
                )}
                Aplicar Recta 2P
              </button>
              <button
                type="button"
                disabled={!perfil || guardandoAccion === "CALIB"}
                onClick={() => void restablecerCalibracionFabrica()}
                className="btn btn-outline px-2.5 py-2 text-xs"
                title="Quitar calibración y volver a lectura cruda de fábrica"
              >
                Fábrica
              </button>
            </div>
          </form>
        </div>
      )}

      {/* Panel inferior del sensor seleccionado: Censos + Auditorías de Eventos integradas */}
      {perfil && (
        <div
          data-tour="sens-tabla"
          className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
        >
          <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
            <div className="flex flex-wrap items-center gap-3">
              <div>
                <div className="flex items-center gap-2">
                  <h2 className="text-base font-bold">
                    Registros del Sensor #{perfil.idSensor} (
                    {perfil.nombre || perfil.mac})
                  </h2>
                  {(cargandoTabla || cargandoAuditoria) && (
                    <Loader2 className="h-4 w-4 animate-spin text-primary" />
                  )}
                </div>
                <p className="text-xs text-foreground-secondary">
                  {tabRegistros === "CENSOS"
                    ? `Lecturas persistidas en MicroSD exclusivamente para ${perfil.mac} · Scroll infinito en bloques de ${LOTE_SCROLL_CENSO}`
                    : `Transiciones de estado, alarmas y envíos de correo autónomos filtrados para ${perfil.mac}`}
                </p>
              </div>
            </div>

            <div className="flex flex-wrap items-center gap-2 text-xs">
              {/* Switcher Censos vs Auditorías del Sensor */}
              <div className="flex rounded-lg bg-secondary p-1">
                <button
                  type="button"
                  onClick={() => setTabRegistros("CENSOS")}
                  className={`flex items-center gap-1.5 rounded-md px-3 py-1 font-semibold ${
                    tabRegistros === "CENSOS"
                      ? "bg-card text-foreground shadow-xs"
                      : "text-foreground-secondary hover:text-foreground"
                  }`}
                >
                  <Thermometer className="h-3.5 w-3.5" />
                  Censos ({filasCenso.length})
                </button>
                <button
                  type="button"
                  onClick={() => setTabRegistros("AUDITORIAS")}
                  className={`flex items-center gap-1.5 rounded-md px-3 py-1 font-semibold ${
                    tabRegistros === "AUDITORIAS"
                      ? "bg-card text-foreground shadow-xs"
                      : "text-foreground-secondary hover:text-foreground"
                  }`}
                >
                  <ShieldCheck className="h-3.5 w-3.5" />
                  Auditorías ({filasAuditoria.length})
                </button>
              </div>

              {tabRegistros === "CENSOS" ? (
                <>
                  <button
                    type="button"
                    className="btn btn-outline px-2.5 py-1"
                    disabled={cargandoTabla}
                    onClick={() => void cargarTablaCenso(true, true)}
                    title="Recargar censos sin caché"
                  >
                    <RefreshCw
                      className={`h-3.5 w-3.5 ${cargandoTabla ? "animate-spin" : ""}`}
                    />
                  </button>
                  <button
                    type="button"
                    className="btn btn-outline btn-success px-2.5 py-1"
                    disabled={exportandoExcel || cargandoTabla}
                    onClick={() => void exportarTablaCensoExcel()}
                    title="Exportar toda la historia de este sensor a Excel (.xlsx)"
                  >
                    {exportandoExcel ? (
                      <Loader2 className="h-3.5 w-3.5 animate-spin" />
                    ) : (
                      <FileSpreadsheet className="h-3.5 w-3.5" />
                    )}
                    Excel
                  </button>
                </>
              ) : (
                <>
                  <button
                    type="button"
                    className="btn btn-outline px-2.5 py-1"
                    disabled={cargandoAuditoria}
                    onClick={() => void cargarAuditoriasSensor()}
                    title="Actualizar auditorías del sensor"
                  >
                    <RefreshCw
                      className={`h-3.5 w-3.5 ${cargandoAuditoria ? "animate-spin" : ""}`}
                    />
                  </button>
                  <button
                    type="button"
                    className="btn btn-outline btn-success px-2.5 py-1"
                    disabled={filasAuditoria.length === 0}
                    onClick={() => void exportarAuditoriasSensorExcel()}
                    title="Exportar auditorías de este sensor a Excel (.xlsx)"
                  >
                    <FileSpreadsheet className="h-3.5 w-3.5" />
                    Excel
                  </button>
                </>
              )}
            </div>
          </div>

          {tabRegistros === "CENSOS" ? (
            <>
              {/* Herramientas ADMIN sobre el sensor seleccionado */}
              {esAdmin && (
                <div className="mb-4 grid grid-cols-1 gap-3 rounded-lg border border-border bg-background p-3 lg:grid-cols-2">
                  <form
                    onSubmit={registrarCensoManual}
                    className="flex flex-wrap items-end gap-2 text-xs"
                  >
                    <div className="pb-1">
                      <span className="rounded bg-secondary px-2 py-1 font-mono text-[11px] font-semibold">
                        Sensor #{perfil.idSensor}
                      </span>
                    </div>
                    <div>
                      <label className="mb-1 block">Cruda (°C)</label>
                      <NumericInput
                        value={nuevoCenso.temperaturaCruda}
                        onChange={(v) =>
                          setNuevoCenso((p) => ({ ...p, temperaturaCruda: v }))
                        }
                        decimals={2}
                        min={-55}
                        max={126}
                        className="w-24 rounded border border-border bg-card px-2 py-1 font-mono"
                      />
                    </div>
                    <div>
                      <label className="mb-1 block">Corregida (°C)</label>
                      <NumericInput
                        value={nuevoCenso.temperaturaCorregida}
                        onChange={(v) =>
                          setNuevoCenso((p) => ({
                            ...p,
                            temperaturaCorregida: v,
                          }))
                        }
                        decimals={2}
                        min={-55}
                        max={126}
                        className="w-24 rounded border border-border bg-card px-2 py-1 font-mono"
                      />
                    </div>
                    <div>
                      <label className="mb-1 block">Estado</label>
                      <select
                        value={nuevoCenso.estadoSensor}
                        onChange={(e) =>
                          setNuevoCenso({
                            ...nuevoCenso,
                            estadoSensor: Number(
                              e.target.value
                            ) as EstadoSensor,
                          })
                        }
                        className="rounded border border-border bg-card px-2 py-1"
                      >
                        {Object.entries(LABEL_ESTADO_SENSOR).map(([k, v]) => (
                          <option key={k} value={k}>
                            {v}
                          </option>
                        ))}
                      </select>
                    </div>
                    <button
                      type="submit"
                      disabled={guardandoAccion === "CENSO"}
                      className="btn btn-solid btn-success px-3 py-1 text-xs font-semibold"
                    >
                      <PlusCircle className="h-3.5 w-3.5" />
                      Insertar Censo
                    </button>
                  </form>

                  <div className="flex flex-wrap items-end justify-end gap-2 text-xs">
                    <div>
                      <label className="mb-1 block">Purgar anteriores a:</label>
                      <input
                        type="datetime-local"
                        value={fechaPurga}
                        onChange={(e) => setFechaPurga(e.target.value)}
                        className="rounded border border-border bg-card px-2 py-1"
                      />
                    </div>
                    <button
                      type="button"
                      onClick={() => void purgarCensos()}
                      className="btn btn-solid btn-error px-3 py-1 text-xs font-semibold"
                    >
                      <Trash2 className="h-3.5 w-3.5" />
                      Purgar Historial
                    </button>
                  </div>
                </div>
              )}

              <div
                className={`transition-opacity ${
                  cargandoTabla ? "opacity-55" : "opacity-100"
                }`}
              >
                <DataTable
                  data={filasCenso}
                  rowKey={(row) => row.idCenso}
                  infiniteScroll
                  maxBodyHeightClass="max-h-[420px]"
                  hasMore={hayMasCensos}
                  loadingMore={cargandoMasCensos}
                  onLoadMore={() => void cargarTablaCenso(false, false)}
                  dateFilter={{
                    label: "Fecha",
                    getEpochSeg: (r) => r.fechaCreacion,
                  }}
                  filters={[
                    {
                      key: "estado",
                      label: "Estado DS18B20",
                      getValue: (r) => r.estadoSensor,
                      options: Object.entries(LABEL_ESTADO_SENSOR).map(
                        ([value, label]) => ({ value, label })
                      ),
                    },
                  ]}
                  columns={[
                    {
                      key: "idCenso",
                      header: "ID Censo",
                      sortValue: (r) => r.idCenso,
                      cellClassName: "font-mono",
                      cell: (r) => `#${r.idCenso}`,
                    },
                    {
                      key: "cruda",
                      header: "Temp. Cruda",
                      sortValue: (r) => r.temperaturaCruda,
                      cellClassName: "font-mono text-foreground-secondary",
                      cell: (r) => `${r.temperaturaCruda.toFixed(2)} °C`,
                    },
                    {
                      key: "corregida",
                      header: "Temp. Corregida",
                      sortValue: (r) => r.temperaturaCorregida,
                      cellClassName: "font-mono font-semibold",
                      cell: (r) => `${r.temperaturaCorregida.toFixed(2)} °C`,
                    },
                    {
                      key: "estado",
                      header: "Estado DS18B20",
                      sortValue: (r) => r.estadoSensor,
                      cell: (r) => (
                        <span
                          title={describirEstadoSensor(r.estadoSensor)}
                          className={`rounded px-2 py-0.5 text-[10px] font-semibold ${
                            r.estadoSensor === EstadoSensor.OK
                              ? "bg-alert-success-bg text-alert-success-icon"
                              : "bg-alert-err-bg text-alert-err-icon"
                          }`}
                        >
                          {LABEL_ESTADO_SENSOR[r.estadoSensor] ??
                            `Estado ${r.estadoSensor}`}
                        </span>
                      ),
                    },
                    {
                      key: "fecha",
                      header: "Fecha / Hora",
                      sortValue: (r) => r.fechaCreacion,
                      cellClassName: "font-mono text-foreground-secondary",
                      cell: (r) => formatearEpochSegundos(r.fechaCreacion),
                    },
                    ...(esAdmin
                      ? ([
                          {
                            key: "eliminar",
                            header: "Eliminar",
                            headerClassName: "text-right",
                            cellClassName: "text-right",
                            cell: (r) => (
                              <button
                                className="btn btn-icon btn-error"
                                onClick={() => void eliminarCenso(r.idCenso)}
                                title="Eliminar registro"
                              >
                                <Trash2 className="h-3.5 w-3.5" />
                              </button>
                            ),
                          },
                        ] as DataTableColumn<CensoRowDTO>[])
                      : []),
                  ]}
                  emptyMessage={
                    cargandoTabla
                      ? "Consultando registros en MicroSD..."
                      : "Sin registros para este sensor."
                  }
                />
              </div>
            </>
          ) : (
            <div
              className={`transition-opacity ${
                cargandoAuditoria ? "opacity-55" : "opacity-100"
              }`}
            >
              <DataTable
                data={filasAuditoria}
                rowKey={(ev) => ev.idAuditoriaSensor}
                infiniteScroll
                maxBodyHeightClass="max-h-[420px]"
                dateFilter={{
                  label: "Fecha",
                  getEpochSeg: (ev) =>
                    parsearFechaTextoAEpochSeg(ev.fechaCreacion),
                }}
                filters={
                  [
                    {
                      key: "estadoNuevo",
                      label: "Estado Nuevo",
                      getValue: (ev) => ev.estadoNuevoSensor,
                      options: [
                        EstadoSensor.OK,
                        EstadoSensor.ALARMA_SUPERIOR,
                        EstadoSensor.ALARMA_INFERIOR,
                        EstadoSensor.DESCONECTADO,
                        EstadoSensor.REINICIO_ENERGIA_85C,
                        EstadoSensor.ERROR_CRC_SCRATCHPAD,
                      ].map((e) => ({
                        value: String(e),
                        label: LABEL_ESTADO_SENSOR[e] ?? String(e),
                      })),
                    },
                    {
                      key: "criticidad",
                      label: "Criticidad",
                      getValue: (ev) => ev.criticidadSensor,
                      options: Object.entries(LABEL_CRITICIDAD).map(
                        ([value, label]) => ({ value, label })
                      ),
                    },
                    {
                      key: "reporte",
                      label: "Correo",
                      getValue: (ev) => ev.estadoReporte,
                      options: Object.entries(LABEL_ESTADO_ENVIO).map(
                        ([value, label]) => ({ value, label })
                      ),
                    },
                  ] as DataTableFilter<AuditoriaSensorRowDTO>[]
                }
                columns={[
                  {
                    key: "id",
                    header: "Evento / Censo",
                    sortValue: (ev) => ev.idAuditoriaSensor,
                    cellClassName: "font-mono",
                    cell: (ev) => (
                      <div>
                        <div className="font-bold">#{ev.idAuditoriaSensor}</div>
                        <div className="text-[10px] text-foreground-secondary">
                          Censo #{ev.idCenso}
                        </div>
                      </div>
                    ),
                  },
                  {
                    key: "transicion",
                    header: "Transición de Estado",
                    sortValue: (ev) =>
                      LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ??
                      ev.estadoNuevoSensor,
                    cell: (ev) => (
                      <div title={describirEstadoSensor(ev.estadoNuevoSensor)}>
                        <div className="font-mono text-[11px]">
                          <span className="text-foreground-secondary">
                            {LABEL_ESTADO_SENSOR[ev.estadoPrevioSensor] ??
                              ev.estadoPrevioSensor}
                          </span>{" "}
                          →{" "}
                          <span className="font-bold text-foreground">
                            {LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ??
                              ev.estadoNuevoSensor}
                          </span>
                        </div>
                        {ev.estadoNuevoSensor !== EstadoSensor.OK && (
                          <div className="mt-0.5 max-w-md truncate text-[10px] text-foreground-secondary">
                            {describirEstadoSensor(ev.estadoNuevoSensor)}
                          </div>
                        )}
                      </div>
                    ),
                  },
                  {
                    key: "bus",
                    header: "Bus 1-Wire",
                    sortValue: (ev) =>
                      LABEL_ESTADO_BUS[ev.estadoBus] ?? ev.estadoBus,
                    cellClassName: "font-mono text-[11px]",
                    cell: (ev) => LABEL_ESTADO_BUS[ev.estadoBus] ?? ev.estadoBus,
                  },
                  {
                    key: "criticidad",
                    header: "Tipo / Criticidad",
                    sortValue: (ev) => ev.criticidadSensor,
                    cell: (ev) => (
                      <>
                        <span className="mr-1.5 rounded bg-secondary px-1.5 py-0.5 text-[10px]">
                          {LABEL_TIPO_EVENTO[ev.tipoEventoSensor] ??
                            ev.tipoEventoSensor}
                        </span>
                        <span
                          className={`rounded px-1.5 py-0.5 text-[10px] font-semibold ${badgeCriticidad(
                            ev.criticidadSensor
                          )}`}
                        >
                          {LABEL_CRITICIDAD[ev.criticidadSensor] ??
                            ev.criticidadSensor}
                        </span>
                      </>
                    ),
                  },
                  {
                    key: "reporte",
                    header: "Correo Autónomo",
                    sortValue: (ev) => ev.estadoReporte,
                    cell: (ev) => (
                      <span
                        className={`rounded px-2 py-0.5 text-[10px] font-semibold ${badgeEstadoCorreo(
                          ev.estadoReporte
                        )}`}
                      >
                        {LABEL_ESTADO_ENVIO[ev.estadoReporte] ??
                          ev.estadoReporte}
                      </span>
                    ),
                  },
                  {
                    key: "fecha",
                    header: "Fecha",
                    sortValue: (ev) => ev.fechaCreacion,
                    cellClassName: "font-mono text-foreground-secondary",
                    cell: (ev) => ev.fechaCreacion || "—",
                  },
                  ...(esAdmin
                    ? ([
                        {
                          key: "acciones",
                          header: "Eliminar",
                          headerClassName: "text-right",
                          cellClassName: "text-right",
                          stopRowClick: true,
                          cell: (ev) => (
                            <button
                              className="btn btn-icon btn-error"
                              onClick={() =>
                                void eliminarAuditoriaSensor(
                                  ev.idAuditoriaSensor
                                )
                              }
                              title="Eliminar evento"
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          ),
                        },
                      ] as DataTableColumn<AuditoriaSensorRowDTO>[])
                    : []),
                ]}
                emptyMessage="Sin eventos de auditoría registrados para este sensor."
              />
            </div>
          )}
        </div>
      )}

      {esAdmin && (
        <CalibrationWizardModal
          open={wizardAbierto}
          sensores={sensores}
          macInicial={macSeleccionada}
          onClose={() => setWizardAbierto(false)}
          onIrAConfigAmbiente={onIrAConfigAmbiente}
          onCompletado={() => {
            void cargarCatalogo(true);
            if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
            void cargarTablaCenso(true, true);
          }}
        />
      )}
    </div>
  );
}