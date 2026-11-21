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
  ChevronLeft,
  ChevronRight,
  Search,
  TrendingUp,
  TrendingDown,
  ListOrdered,
  Loader2,
} from "lucide-react";
import {
  ResponsiveContainer,
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
} from "recharts";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import {
  fechaAInputLocal,
  formatearEpochSegundos,
  inputLocalAEpochSegundos,
} from "../lib/format";
import { LABEL_ESTADO_SENSOR } from "../lib/labels";
import { NumericInput } from "../components/ui/NumericInput";
import { CalibrationWizardModal } from "../components/calibration/CalibrationWizardModal";
import {
  EstadoSensor,
  LecturaSensorCliente,
  PerfilSensorCliente,
  ResumenEstadisticoSensoCliente,
  ResumenSensorCliente,
  RolUsuario,
  SensoRowDTO,
} from "#types/rpc.types";

const PAGE_SIZE_SENSO = 15;
type ModoTablaSenso = "RECIENTES" | "PICOS_ALTOS" | "PICOS_BAJOS";

export function SensoresView({
  macInicial,
  onIrAConfigAmbiente,
}: {
  macInicial?: string;
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
  const [statsSenso, setStatsSenso] =
    useState<ResumenEstadisticoSensoCliente | null>(null);

  // Estados de carga + tokens anti-race-condition (evita sensores "colados")
  const [cargandoCatalogo, setCargandoCatalogo] = useState(false);
  const [cargandoDetalle, setCargandoDetalle] = useState(false);
  const [cargandoHistorial, setCargandoHistorial] = useState(false);
  const [cargandoTabla, setCargandoTabla] = useState(false);
  const [guardandoAccion, setGuardandoAccion] = useState<string | null>(null);

  const detalleReqRef = useRef(0);
  const historialReqRef = useRef(0);
  const tablaReqRef = useRef(0);

  // Formularios separados (ADMIN): Metadatos | Umbrales TL/TH | Calibración 2P/Offset
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

  // Historial gráfico por MAC / Rango de fechas (fragmentado)
  const [modoHistorial, setModoHistorial] = useState<"ULTIMOS" | "FECHAS">(
    "ULTIMOS"
  );
  const [limiteHistorial, setLimiteHistorial] = useState(32);
  const [fechaDesde, setFechaDesde] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 3600 * 1000))
  );
  const [fechaHasta, setFechaHasta] = useState(() =>
    fechaAInputLocal(new Date())
  );
  const [serieHistorica, setSerieHistorica] = useState<LecturaSensorCliente[]>(
    []
  );

  // Explorador tabla Senso (Paginado + Picos altos/bajos)
  const [filtroIdSensorPag, setFiltroIdSensorPag] = useState<number>(0);
  const [modoTablaSenso, setModoTablaSenso] =
    useState<ModoTablaSenso>("RECIENTES");
  const [offsetSenso, setOffsetSenso] = useState<number>(0);
  const [filasSenso, setFilasSenso] = useState<SensoRowDTO[]>([]);

  // Censo manual y purga (ADMIN)
  const [nuevoSenso, setNuevoSenso] = useState({
    idSensor: 1,
    temperaturaCruda: 22.5,
    temperaturaCorregida: 22.5,
    estadoSensor: EstadoSensor.OK,
  });
  const [fechaPurga, setFechaPurga] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 7 * 86400 * 1000))
  );

  useEffect(() => {
    if (macInicial) {
      setMacSeleccionada(macInicial.toUpperCase());
    }
  }, [macInicial]);

  const cargarCatalogo = useCallback(
    async (forzarSinCache = false) => {
      setCargandoCatalogo(true);
      if (forzarSinCache) {
        await safeRpc(() => client.invalidarCache(), "invalidarCache", {
          silent: true,
        });
      }
      const lista = await safeRpc(
        () => client.listarSensores(12, 0, soloActivos),
        "listarSensores"
      );
      setCargandoCatalogo(false);
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

  const cargarDetalleSensor = useCallback(async (mac: string) => {
    const reqId = ++detalleReqRef.current;
    if (!mac) {
      setPerfil(null);
      setStatsSenso(null);
      setSerieHistorica([]);
      setCargandoDetalle(false);
      return;
    }

    setCargandoDetalle(true);
    const [det, st, hist] = await Promise.all([
      safeRpc(() => client.obtenerSensorPorMac(mac), "obtenerSensorPorMac"),
      safeRpc(
        () => client.obtenerEstadisticasSensoPorMac(mac),
        "obtenerEstadisticasSensoPorMac",
        { silent: true }
      ),
      safeRpc(
        () => client.obtenerCensosPorMac(mac, 32),
        "obtenerCensosPorMac",
        { silent: true }
      ),
    ]);

    if (reqId !== detalleReqRef.current) return;
    setCargandoDetalle(false);

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
      setNuevoSenso((prev) => ({ ...prev, idSensor: det.idSensor }));
    }
    if (st) setStatsSenso(st);
    if (hist) setSerieHistorica([...hist].reverse());
  }, []);

  const cargarTablaSenso = useCallback(
    async (forzarSinCache = false) => {
      const reqId = ++tablaReqRef.current;
      setCargandoTabla(true);
      if (forzarSinCache) {
        await safeRpc(() => client.invalidarCache(), "invalidarCache", {
          silent: true,
        });
      }

      let rows: SensoRowDTO[] | null = null;
      if (modoTablaSenso === "RECIENTES") {
        rows = await safeRpc(
          () =>
            client.listarSensosPaginados({
              idSensor: filtroIdSensorPag,
              limiteRegistros: PAGE_SIZE_SENSO,
              offsetRegistros: offsetSenso,
            }),
          "listarSensosPaginados"
        );
      } else {
        rows = await safeRpc(
          () =>
            client.listarPicosSensos(
              filtroIdSensorPag,
              modoTablaSenso === "PICOS_ALTOS" ? "ALTOS" : "BAJOS",
              PAGE_SIZE_SENSO,
              320
            ),
          "listarPicosSensos"
        );
      }

      if (reqId !== tablaReqRef.current) return;
      setCargandoTabla(false);
      if (rows) setFilasSenso(rows);
    },
    [filtroIdSensorPag, offsetSenso, modoTablaSenso]
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
    void cargarTablaSenso(false);
  }, [cargarTablaSenso]);

  const aplicarAtajoRango = (horas: number) => {
    const ahora = new Date();
    setFechaHasta(fechaAInputLocal(ahora));
    setFechaDesde(fechaAInputLocal(new Date(ahora.getTime() - horas * 3600 * 1000)));
    setModoHistorial("FECHAS");
    if (limiteHistorial < 64) setLimiteHistorial(128);
  };

  const consultarHistorial = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!macSeleccionada) return;
    const reqId = ++historialReqRef.current;
    setCargandoHistorial(true);

    if (modoHistorial === "ULTIMOS") {
      const lim = Math.max(1, Math.min(32, Math.round(limiteHistorial)));
      const res = await safeRpc(
        () => client.obtenerCensosPorMac(macSeleccionada, lim),
        "obtenerCensosPorMac"
      );
      if (reqId !== historialReqRef.current) return;
      setCargandoHistorial(false);
      if (res) setSerieHistorica([...res].reverse());
    } else {
      const desde = inputLocalAEpochSegundos(fechaDesde, false);
      const hasta = inputLocalAEpochSegundos(fechaHasta, true);
      const maxReg = Math.max(1, Math.min(320, Math.round(limiteHistorial)));
      const res = await safeRpc(
        () =>
          client.obtenerCensosPorMacYFechaFragmentado(
            macSeleccionada,
            desde,
            hasta,
            maxReg
          ),
        "obtenerCensosPorMacYFechaFragmentado"
      );
      if (reqId !== historialReqRef.current) return;
      setCargandoHistorial(false);
      if (res) setSerieHistorica(res);
    }
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

  const guardarSoloCalibracion = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    setGuardandoAccion("CALIB");
    const actualizado = await safeRpc(
      () =>
        client.actualizarCalibracionSensor(
          perfil.mac,
          perfil.umbralSup,
          perfil.umbralInf,
          formCalib.errorSup,
          formCalib.errorInf,
          formCalib.offset
        ),
      "actualizarCalibracionSensor",
      {
        mensajeExito:
          "Calibración de 2 puntos y offset aplicada sin modificar los umbrales de alarma.",
      }
    );
    setGuardandoAccion(null);
    if (actualizado) {
      setPerfil(actualizado);
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

  const registrarSensoManual = async (e: React.FormEvent) => {
    e.preventDefault();
    setGuardandoAccion("SENSO");
    const creado = await safeRpc(
      () =>
        client.crearSensoManual({
          idSensor: Math.round(nuevoSenso.idSensor),
          temperaturaCruda: nuevoSenso.temperaturaCruda,
          temperaturaCorregida: nuevoSenso.temperaturaCorregida,
          estadoSensor: Number(nuevoSenso.estadoSensor) as EstadoSensor,
        }),
      "crearSensoManual",
      { mensajeExito: "Censo manual registrado en PostgreSQL embebido." }
    );
    setGuardandoAccion(null);
    if (creado) {
      void cargarTablaSenso(false);
      if (macSeleccionada) void cargarDetalleSensor(macSeleccionada);
    }
  };

  const eliminarSenso = async (idSenso: number) => {
    await safeRpc(
      () => client.eliminarSensoPorId(idSenso),
      "eliminarSensoPorId",
      { mensajeExito: `Registro Senso #${idSenso} eliminado.` }
    );
    void cargarTablaSenso(false);
  };

  const purgarSensos = async () => {
    const limiteSeg = inputLocalAEpochSegundos(fechaPurga, false);
    if (!limiteSeg) return;
    if (
      !window.confirm(
        `¿Purgar todos los registros de Senso anteriores a ${formatearEpochSegundos(limiteSeg)}?`
      )
    ) {
      return;
    }
    const eliminados = await safeRpc(
      () => client.purgarSensosAnterioresA(limiteSeg),
      "purgarSensosAnterioresA",
      { mensajeExito: "Purga histórica completada." }
    );
    if (eliminados !== null) {
      setOffsetSenso(0);
      void cargarTablaSenso(false);
    }
  };

  const chartHistorico = serieHistorica.map((item) => ({
    hora: formatearEpochSegundos(item.timestampMs),
    temp: Number(item.temperaturaCorregida.toFixed(2)),
  }));

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado y Catálogo de Sensores */}
      <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
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
                title="La Calibración Ambiental Global ahora vive en Configuración"
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

        <div className="overflow-x-auto">
          <table className="w-full text-left text-xs">
            <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
              <tr>
                <th className="px-3 py-2">ID</th>
                <th className="px-3 py-2">Dirección MAC</th>
                <th className="px-3 py-2">Nombre</th>
                <th className="px-3 py-2">Ubicación</th>
                <th className="px-3 py-2">TL / TH (°C)</th>
                <th className="px-3 py-2">Offset</th>
                <th className="px-3 py-2">Estado</th>
                {esAdmin && <th className="px-3 py-2 text-right">Acciones</th>}
              </tr>
            </thead>
            <tbody className="divide-y divide-border">
              {sensores.map((s) => {
                const seleccionado =
                  s.mac.toUpperCase() === macSeleccionada.toUpperCase();
                return (
                  <tr
                    key={s.mac}
                    onClick={() => setMacSeleccionada(s.mac.toUpperCase())}
                    className={`cursor-pointer transition-colors ${
                      seleccionado ? "bg-primary-light" : "hover:bg-secondary/40"
                    }`}
                  >
                    <td className="px-3 py-2 font-mono font-semibold">
                      #{s.idSensor}
                    </td>
                    <td className="px-3 py-2 font-mono">{s.mac}</td>
                    <td className="px-3 py-2 font-medium">
                      {s.nombre || "Sin nombre"}
                    </td>
                    <td className="px-3 py-2 text-foreground-secondary">
                      {s.ubicacion || "—"}
                    </td>
                    <td className="px-3 py-2 font-mono">
                      {s.umbralInf.toFixed(1)} / {s.umbralSup.toFixed(1)}
                    </td>
                    <td className="px-3 py-2 font-mono">
                      {s.offset >= 0
                        ? `+${s.offset.toFixed(2)}`
                        : s.offset.toFixed(2)}
                    </td>
                    <td className="px-3 py-2">
                      <span
                        className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                          s.activa
                            ? "bg-alert-success-bg text-alert-success-icon"
                            : "bg-alert-inactive-bg text-alert-inactive-icon"
                        }`}
                      >
                        {s.activa ? "ACTIVO" : "INACTIVO"}
                      </span>
                    </td>
                    {esAdmin && (
                      <td
                        className="px-3 py-2 text-right"
                        onClick={(e) => e.stopPropagation()}
                      >
                        <button
                          className="btn btn-icon btn-error"
                          onClick={() => void eliminarSensor(s.mac)}
                          title="Desactivar sensor"
                        >
                          <Trash2 className="h-3.5 w-3.5" />
                        </button>
                      </td>
                    )}
                  </tr>
                );
              })}
              {sensores.length === 0 && (
                <tr>
                  <td
                    colSpan={esAdmin ? 8 : 7}
                    className="px-3 py-6 text-center text-foreground-secondary"
                  >
                    {cargandoCatalogo
                      ? "Cargando catálogo de sensores..."
                      : "No hay sensores registrados en la base de datos."}
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Perfil, Estadísticas e Historial Gráfico del Sensor Seleccionado */}
      {macSeleccionada && (
        <div
          className={`grid grid-cols-1 gap-4 transition-opacity xl:grid-cols-3 ${
            cargandoDetalle ? "opacity-75" : "opacity-100"
          }`}
        >
          {/* Columna izquierda: Perfil y Estadísticas */}
          <div className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            {perfil ? (
              <>
                <div>
                  <div className="flex items-center justify-between">
                    <span className="inline-flex items-center gap-1.5 font-mono text-xs text-primary">
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

                {statsSenso && (
                  <div className="grid grid-cols-2 gap-2 border-t border-border pt-3 text-xs">
                    <div className="rounded-lg border border-border bg-background p-2.5">
                      <div className="text-foreground-secondary">
                        Total lecturas
                      </div>
                      <div className="text-base font-bold">
                        {statsSenso.totalLecturas}
                      </div>
                    </div>
                    <div className="rounded-lg border border-border bg-background p-2.5">
                      <div className="text-foreground-secondary">Promedio</div>
                      <div className="text-base font-bold text-primary">
                        {statsSenso.totalLecturas > 0
                          ? `${statsSenso.temperaturaPromedio.toFixed(2)} °C`
                          : "—"}
                      </div>
                    </div>
                    <div className="rounded-lg border border-border bg-background p-2.5">
                      <div className="text-foreground-secondary">Mínima</div>
                      <div className="text-base font-bold text-info">
                        {statsSenso.totalLecturas > 0
                          ? `${statsSenso.temperaturaMinima.toFixed(2)} °C`
                          : "—"}
                      </div>
                    </div>
                    <div className="rounded-lg border border-border bg-background p-2.5">
                      <div className="text-foreground-secondary">Máxima</div>
                      <div className="text-base font-bold text-error">
                        {statsSenso.totalLecturas > 0
                          ? `${statsSenso.temperaturaMaxima.toFixed(2)} °C`
                          : "—"}
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

          {/* Columna derecha (2 cols): Consulta y Gráfico Histórico */}
          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-2">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <div className="flex items-center gap-2">
                <History className="h-4 w-4 text-primary" />
                <h3 className="text-sm font-bold">
                  Curva Histórica de Censos ({perfil?.mac || macSeleccionada})
                </h3>
                <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px]">
                  {serieHistorica.length} pts
                </span>
              </div>

              <form
                onSubmit={consultarHistorial}
                className="flex flex-wrap items-center gap-1.5 text-xs"
              >
                <select
                  value={modoHistorial}
                  onChange={(e) => {
                    const modo = e.target.value as "ULTIMOS" | "FECHAS";
                    setModoHistorial(modo);
                    setLimiteHistorial(modo === "FECHAS" ? 128 : 32);
                  }}
                  className="rounded border border-border bg-background px-2 py-1"
                >
                  <option value="ULTIMOS">Últimos N (máx 32)</option>
                  <option value="FECHAS">Rango de fechas (fragmentado)</option>
                </select>

                {modoHistorial === "FECHAS" && (
                  <>
                    <input
                      type="datetime-local"
                      value={fechaDesde}
                      onChange={(e) => setFechaDesde(e.target.value)}
                      className="rounded border border-border bg-background px-2 py-1"
                    />
                    <input
                      type="datetime-local"
                      value={fechaHasta}
                      onChange={(e) => setFechaHasta(e.target.value)}
                      className="rounded border border-border bg-background px-2 py-1"
                    />
                  </>
                )}

                <NumericInput
                  value={limiteHistorial}
                  onChange={setLimiteHistorial}
                  decimals={0}
                  allowNegative={false}
                  min={1}
                  max={modoHistorial === "FECHAS" ? 320 : 32}
                  className="w-16 rounded border border-border bg-background px-2 py-1 font-mono"
                  title={
                    modoHistorial === "FECHAS"
                      ? "Máximo de puntos (se consulta en bloques de 32)"
                      : "Límite (máx 32)"
                  }
                />

                <button
                  type="submit"
                  disabled={cargandoHistorial}
                  className="btn btn-solid btn-primary px-3 py-1 text-xs"
                >
                  {cargandoHistorial ? (
                    <Loader2 className="h-3.5 w-3.5 animate-spin" />
                  ) : (
                    <Search className="h-3.5 w-3.5" />
                  )}
                  Consultar
                </button>
              </form>
            </div>

            {modoHistorial === "FECHAS" && (
              <div className="mb-2 flex flex-wrap items-center gap-1.5 text-[11px] text-foreground-secondary">
                <span>Atajos:</span>
                {[
                  { label: "Última 1h", h: 1 },
                  { label: "Últimas 6h", h: 6 },
                  { label: "Últimas 24h", h: 24 },
                  { label: "Últimos 7 días", h: 168 },
                ].map((it) => (
                  <button
                    key={it.label}
                    type="button"
                    onClick={() => aplicarAtajoRango(it.h)}
                    className="rounded border border-border bg-secondary px-2 py-0.5 hover:border-primary"
                  >
                    {it.label}
                  </button>
                ))}
              </div>
            )}

            <div className="relative h-60 w-full rounded-lg border border-border bg-background p-2">
              {(cargandoDetalle || cargandoHistorial) && (
                <div className="absolute inset-0 z-10 flex items-center justify-center gap-2 rounded-lg bg-background/70 text-xs backdrop-blur-[1px]">
                  <Loader2 className="h-4 w-4 animate-spin text-primary" />
                  Consultando historial en MicroSD...
                </div>
              )}
              {chartHistorico.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={chartHistorico}>
                    <CartesianGrid
                      strokeDasharray="3 3"
                      stroke="currentColor"
                      className="opacity-15"
                    />
                    <XAxis
                      dataKey="hora"
                      tick={{ fontSize: 10 }}
                      minTickGap={25}
                    />
                    <YAxis
                      domain={["auto", "auto"]}
                      tick={{ fontSize: 11 }}
                      unit=" °C"
                      width={55}
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: "var(--card)",
                        borderColor: "var(--border)",
                        color: "var(--card-foreground)",
                        fontSize: "12px",
                      }}
                    />
                    <Line
                      type="monotone"
                      dataKey="temp"
                      stroke="#10b981"
                      strokeWidth={2}
                      dot={chartHistorico.length <= 40 ? { r: 2 } : false}
                      isAnimationActive={false}
                      name="Temp. Corregida (°C)"
                    />
                  </LineChart>
                </ResponsiveContainer>
              ) : (
                <div className="flex h-full items-center justify-center text-xs text-foreground-secondary">
                  Sin lecturas históricas en el rango consultado.
                </div>
              )}
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

          {/* 2. Umbrales de Alarma Térmica (SEPARADOS de la calibración) */}
          <form
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
                corregida sale de estos límites se activa la sirena y el LED.
                No modifica la calibración del sensor.
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

          {/* 3. Calibración de Precisión (2 Puntos + Offset, SEPARADA de umbrales) */}
          <form
            onSubmit={guardarSoloCalibracion}
            className="flex flex-col justify-between space-y-3 rounded-xl border border-alert-warn-border/70 bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="space-y-3">
              <div className="flex items-center gap-2 text-sm font-bold text-warning">
                <Sliders className="h-4 w-4" />
                Calibración de Precisión (Manual)
              </div>
              <p className="text-[11px] text-foreground-secondary">
                Ajuste fino metrológico mediante puntos de hielo / ebullición u
                offset lineal. Conserva los umbrales de alarma actuales.
              </p>
              <div className="grid grid-cols-2 gap-2.5 text-xs">
                <div>
                  <label className="mb-1 block">Medición Hielo (°C)</label>
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
                  <label className="mb-1 block">Medición Ebullición (°C)</label>
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
              <div className="text-xs">
                <label className="mb-1 block">Offset lineal adicional (°C)</label>
                <NumericInput
                  value={formCalib.offset}
                  onChange={(v) => setFormCalib((p) => ({ ...p, offset: v }))}
                  decimals={2}
                  min={-20}
                  max={20}
                  disabled={!perfil}
                  className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                />
              </div>
            </div>
            <button
              type="submit"
              disabled={!perfil || guardandoAccion === "CALIB"}
              className="btn btn-solid btn-warning w-full text-xs font-semibold"
            >
              {guardandoAccion === "CALIB" && (
                <Loader2 className="h-3.5 w-3.5 animate-spin" />
              )}
              Aplicar Calibración al Sensor
            </button>
          </form>
        </div>
      )}

      {/* Explorador Paginado + Picos de la Tabla Senso */}
      <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-base font-bold">
                Explorador de Tabla <code>Senso</code> (Cruda vs. Corregida)
              </h2>
              {cargandoTabla && (
                <Loader2 className="h-4 w-4 animate-spin text-primary" />
              )}
            </div>
            <p className="text-xs text-foreground-secondary">
              {modoTablaSenso === "RECIENTES"
                ? "Registros almacenados en MicroSD con paginación binaria"
                : modoTablaSenso === "PICOS_ALTOS"
                  ? "Top picos más altos detectados (sobre últimas 320 lecturas válidas)"
                  : "Top picos más bajos detectados (sobre últimas 320 lecturas válidas)"}
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-2 text-xs">
            <div className="flex rounded-lg bg-secondary p-0.5">
              <button
                type="button"
                onClick={() => {
                  setModoTablaSenso("RECIENTES");
                  setOffsetSenso(0);
                }}
                className={`inline-flex items-center gap-1 rounded-md px-2.5 py-1 font-semibold ${
                  modoTablaSenso === "RECIENTES"
                    ? "bg-card text-foreground shadow-xs"
                    : "text-foreground-secondary hover:text-foreground"
                }`}
              >
                <ListOrdered className="h-3.5 w-3.5" />
                Recientes
              </button>
              <button
                type="button"
                onClick={() => setModoTablaSenso("PICOS_ALTOS")}
                className={`inline-flex items-center gap-1 rounded-md px-2.5 py-1 font-semibold ${
                  modoTablaSenso === "PICOS_ALTOS"
                    ? "bg-card text-error shadow-xs"
                    : "text-foreground-secondary hover:text-foreground"
                }`}
              >
                <TrendingUp className="h-3.5 w-3.5" />
                Picos Altos
              </button>
              <button
                type="button"
                onClick={() => setModoTablaSenso("PICOS_BAJOS")}
                className={`inline-flex items-center gap-1 rounded-md px-2.5 py-1 font-semibold ${
                  modoTablaSenso === "PICOS_BAJOS"
                    ? "bg-card text-info shadow-xs"
                    : "text-foreground-secondary hover:text-foreground"
                }`}
              >
                <TrendingDown className="h-3.5 w-3.5" />
                Picos Bajos
              </button>
            </div>

            <select
              value={filtroIdSensorPag}
              onChange={(e) => {
                setFiltroIdSensorPag(Number(e.target.value));
                setOffsetSenso(0);
              }}
              className="rounded border border-border bg-background px-2.5 py-1.5"
            >
              <option value={0}>Todos los sensores</option>
              {sensores.map((s) => (
                <option key={s.idSensor} value={s.idSensor}>
                  #{s.idSensor} · {s.nombre || s.mac}
                </option>
              ))}
            </select>

            <button
              type="button"
              className="btn btn-outline px-2.5 py-1"
              disabled={cargandoTabla}
              onClick={() => void cargarTablaSenso(true)}
              title="Recargar sin caché"
            >
              <RefreshCw
                className={`h-3.5 w-3.5 ${cargandoTabla ? "animate-spin" : ""}`}
              />
            </button>

            {modoTablaSenso === "RECIENTES" && (
              <>
                <button
                  type="button"
                  className="btn btn-outline px-2.5 py-1"
                  disabled={offsetSenso === 0 || cargandoTabla}
                  onClick={() =>
                    setOffsetSenso((o) => Math.max(0, o - PAGE_SIZE_SENSO))
                  }
                >
                  <ChevronLeft className="h-4 w-4" />
                </button>
                <span className="font-mono">
                  Offset {offsetSenso} – {offsetSenso + filasSenso.length}
                </span>
                <button
                  type="button"
                  className="btn btn-outline px-2.5 py-1"
                  disabled={filasSenso.length < PAGE_SIZE_SENSO || cargandoTabla}
                  onClick={() => setOffsetSenso((o) => o + PAGE_SIZE_SENSO)}
                >
                  <ChevronRight className="h-4 w-4" />
                </button>
              </>
            )}
          </div>
        </div>

        {/* Herramientas ADMIN sobre Senso: Inserción manual y Purga */}
        {esAdmin && (
          <div className="mb-4 grid grid-cols-1 gap-3 rounded-lg border border-border bg-background p-3 lg:grid-cols-2">
            <form
              onSubmit={registrarSensoManual}
              className="flex flex-wrap items-end gap-2 text-xs"
            >
              <div>
                <label className="mb-1 block">ID Sensor</label>
                <NumericInput
                  value={nuevoSenso.idSensor}
                  onChange={(v) =>
                    setNuevoSenso((p) => ({ ...p, idSensor: v }))
                  }
                  decimals={0}
                  allowNegative={false}
                  min={1}
                  className="w-20 rounded border border-border bg-card px-2 py-1 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Cruda (°C)</label>
                <NumericInput
                  value={nuevoSenso.temperaturaCruda}
                  onChange={(v) =>
                    setNuevoSenso((p) => ({ ...p, temperaturaCruda: v }))
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
                  value={nuevoSenso.temperaturaCorregida}
                  onChange={(v) =>
                    setNuevoSenso((p) => ({ ...p, temperaturaCorregida: v }))
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
                  value={nuevoSenso.estadoSensor}
                  onChange={(e) =>
                    setNuevoSenso({
                      ...nuevoSenso,
                      estadoSensor: Number(e.target.value) as EstadoSensor,
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
                disabled={guardandoAccion === "SENSO"}
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
                onClick={() => void purgarSensos()}
                className="btn btn-solid btn-error px-3 py-1 text-xs font-semibold"
              >
                <Trash2 className="h-3.5 w-3.5" />
                Purgar Historial
              </button>
            </div>
          </div>
        )}

        <div
          className={`overflow-x-auto transition-opacity ${
            cargandoTabla ? "opacity-55" : "opacity-100"
          }`}
        >
          <table className="w-full text-left text-xs">
            <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
              <tr>
                <th className="px-3 py-2">ID Senso</th>
                <th className="px-3 py-2">ID Sensor</th>
                <th className="px-3 py-2">Temp. Cruda</th>
                <th className="px-3 py-2">Temp. Corregida</th>
                <th className="px-3 py-2">Estado DS18B20</th>
                <th className="px-3 py-2">Fecha / Hora</th>
                {esAdmin && <th className="px-3 py-2 text-right">Eliminar</th>}
              </tr>
            </thead>
            <tbody className="divide-y divide-border">
              {filasSenso.map((row) => (
                <tr key={row.idSenso} className="hover:bg-secondary/40">
                  <td className="px-3 py-2 font-mono">{row.idSenso}</td>
                  <td className="px-3 py-2 font-mono">#{row.idSensor}</td>
                  <td className="px-3 py-2 font-mono text-foreground-secondary">
                    {row.temperaturaCruda.toFixed(2)} °C
                  </td>
                  <td className="px-3 py-2 font-mono font-semibold">
                    {row.temperaturaCorregida.toFixed(2)} °C
                  </td>
                  <td className="px-3 py-2">
                    <span
                      className={`rounded px-2 py-0.5 text-[10px] font-semibold ${
                        row.estadoSensor === EstadoSensor.OK
                          ? "bg-alert-success-bg text-alert-success-icon"
                          : "bg-alert-err-bg text-alert-err-icon"
                      }`}
                    >
                      {LABEL_ESTADO_SENSOR[row.estadoSensor] ??
                        `Estado ${row.estadoSensor}`}
                    </span>
                  </td>
                  <td className="px-3 py-2 font-mono text-foreground-secondary">
                    {formatearEpochSegundos(row.fechaCreacion)}
                  </td>
                  {esAdmin && (
                    <td className="px-3 py-2 text-right">
                      <button
                        className="btn btn-icon btn-error"
                        onClick={() => void eliminarSenso(row.idSenso)}
                        title="Eliminar registro"
                      >
                        <Trash2 className="h-3.5 w-3.5" />
                      </button>
                    </td>
                  )}
                </tr>
              ))}
              {filasSenso.length === 0 && (
                <tr>
                  <td
                    colSpan={esAdmin ? 7 : 6}
                    className="px-3 py-6 text-center text-foreground-secondary"
                  >
                    {cargandoTabla
                      ? "Consultando registros en MicroSD..."
                      : "Sin registros en esta vista."}
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>
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
            void cargarTablaSenso(true);
          }}
        />
      )}
    </div>
  );
}