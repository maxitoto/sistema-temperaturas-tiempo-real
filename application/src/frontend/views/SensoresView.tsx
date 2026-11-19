import React, { useCallback, useEffect, useState } from "react";
import {
  Thermometer,
  RefreshCw,
  Sliders,
  Edit3,
  Trash2,
  PlusCircle,
  Globe,
  History,
  ChevronLeft,
  ChevronRight,
  Search,
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
import {
  CalibracionAmbienteDTO,
  EstadoSensor,
  LecturaSensorCliente,
  PerfilSensorCliente,
  ResumenEstadisticoSensoCliente,
  ResumenSensorCliente,
  RolUsuario,
  SensoRowDTO,
} from "#types/rpc.types";

const PAGE_SIZE_SENSO = 15;

export function SensoresView() {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  // Catálogo y selección
  const [sensores, setSensores] = useState<ResumenSensorCliente[]>([]);
  const [soloActivos, setSoloActivos] = useState(false);
  const [macSeleccionada, setMacSeleccionada] = useState<string>("");
  const [perfil, setPerfil] = useState<PerfilSensorCliente | null>(null);
  const [statsSenso, setStatsSenso] =
    useState<ResumenEstadisticoSensoCliente | null>(null);

  // Calibración ambiental global
  const [ambiente, setAmbiente] = useState<CalibracionAmbienteDTO | null>(null);
  const [formAmbiente, setFormAmbiente] = useState({
    altura: 0,
    presionAtmosferica: 1013.25,
    tempCongelacionRef: 0,
    tempEbullicionRef: 100,
    activa: true,
  });

  // Edición de metadatos y calibración del sensor (ADMIN)
  const [formMeta, setFormMeta] = useState({
    nombre: "",
    ubicacion: "",
    descripcion: "",
    activa: true,
  });
  const [formCalib, setFormCalib] = useState({
    umbralSup: 40,
    umbralInf: 0,
    errorSup: 0,
    errorInf: 0,
    offset: 0,
  });

  // Historial gráfico por MAC / Rango de fechas
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

  // Explorador paginado tabla Senso
  const [filtroIdSensorPag, setFiltroIdSensorPag] = useState<number>(0);
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

  const cargarCatalogo = useCallback(async () => {
    const [lista, amb] = await Promise.all([
      safeRpc(
        () => client.listarSensores(32, 0, soloActivos),
        "listarSensores"
      ),
      safeRpc(
        () => client.obtenerCalibracionAmbiente(),
        "obtenerCalibracionAmbiente",
        { silent: true }
      ),
    ]);
    if (lista) {
      setSensores(lista);
      if (lista.length > 0 && !lista.some((s) => s.mac === macSeleccionada)) {
        setMacSeleccionada(lista[0].mac);
      }
    }
    if (amb) {
      setAmbiente(amb);
      setFormAmbiente({
        altura: amb.altura,
        presionAtmosferica: amb.presionAtmosferica,
        tempCongelacionRef: amb.tempCongelacionRef,
        tempEbullicionRef: amb.tempEbullicionRef,
        activa: amb.activa,
      });
    }
  }, [soloActivos, macSeleccionada]);

  const cargarDetalleSensor = useCallback(async (mac: string) => {
    if (!mac) {
      setPerfil(null);
      setStatsSenso(null);
      setSerieHistorica([]);
      return;
    }
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
    if (det) {
      setPerfil(det);
      setFormMeta({
        nombre: det.nombre,
        ubicacion: det.ubicacion,
        descripcion: det.descripcion,
        activa: det.activa,
      });
      setFormCalib({
        umbralSup: det.umbralSup,
        umbralInf: det.umbralInf,
        errorSup: det.errorSup,
        errorInf: det.errorInf,
        offset: det.offset,
      });
      setNuevoSenso((prev) => ({ ...prev, idSensor: det.idSensor }));
    }
    if (st) setStatsSenso(st);
    if (hist) setSerieHistorica([...hist].reverse());
  }, []);

  const cargarTablaSenso = useCallback(async () => {
    const rows = await safeRpc(
      () =>
        client.listarSensosPaginados({
          idSensor: filtroIdSensorPag,
          limiteRegistros: PAGE_SIZE_SENSO,
          offsetRegistros: offsetSenso,
        }),
      "listarSensosPaginados"
    );
    if (rows) setFilasSenso(rows);
  }, [filtroIdSensorPag, offsetSenso]);

  useEffect(() => {
    cargarCatalogo();
  }, [cargarCatalogo]);

  useEffect(() => {
    if (macSeleccionada) {
      cargarDetalleSensor(macSeleccionada);
    }
  }, [macSeleccionada, cargarDetalleSensor]);

  useEffect(() => {
    cargarTablaSenso();
  }, [cargarTablaSenso]);

  const consultarHistorial = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!macSeleccionada) return;
    if (modoHistorial === "ULTIMOS") {
      const res = await safeRpc(
        () => client.obtenerCensosPorMac(macSeleccionada, limiteHistorial),
        "obtenerCensosPorMac"
      );
      if (res) setSerieHistorica([...res].reverse());
    } else {
      const desde = inputLocalAEpochSegundos(fechaDesde);
      const hasta = inputLocalAEpochSegundos(fechaHasta);
      const res = await safeRpc(
        () =>
          client.obtenerCensosPorMacYFecha(
            macSeleccionada,
            desde,
            hasta,
            limiteHistorial
          ),
        "obtenerCensosPorMacYFecha"
      );
      if (res) setSerieHistorica([...res].reverse());
    }
  };

  const guardarMetadatos = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
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
    if (actualizado) {
      setPerfil(actualizado);
      cargarCatalogo();
    }
  };

  const guardarCalibracion = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!perfil) return;
    const actualizado = await safeRpc(
      () =>
        client.actualizarCalibracionSensor(
          perfil.mac,
          Number(formCalib.umbralSup),
          Number(formCalib.umbralInf),
          Number(formCalib.errorSup),
          Number(formCalib.errorInf),
          Number(formCalib.offset)
        ),
      "actualizarCalibracionSensor",
      { mensajeExito: "Calibración y umbrales aplicados en el ESP32." }
    );
    if (actualizado) {
      setPerfil(actualizado);
      cargarCatalogo();
    }
  };

  const eliminarSensor = async (mac: string) => {
    if (
      !window.confirm(
        `¿Eliminar el sensor ${mac} y sus referencias del catálogo?`
      )
    ) {
      return;
    }
    await safeRpc(
      () => client.eliminarSensorPorMac(mac),
      "eliminarSensorPorMac",
      { mensajeExito: `Sensor ${mac} eliminado.` }
    );
    if (macSeleccionada === mac) setMacSeleccionada("");
    cargarCatalogo();
  };

  const guardarAmbiente = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarCalibracionAmbiente({
          altura: Number(formAmbiente.altura),
          presionAtmosferica: Number(formAmbiente.presionAtmosferica),
          tempCongelacionRef: Number(formAmbiente.tempCongelacionRef),
          tempEbullicionRef: Number(formAmbiente.tempEbullicionRef),
          activa: formAmbiente.activa,
        }),
      "actualizarCalibracionAmbiente",
      { mensajeExito: "Calibración ambiental global actualizada." }
    );
    if (res) setAmbiente(res);
  };

  const registrarSensoManual = async (e: React.FormEvent) => {
    e.preventDefault();
    const creado = await safeRpc(
      () =>
        client.crearSensoManual({
          idSensor: Number(nuevoSenso.idSensor),
          temperaturaCruda: Number(nuevoSenso.temperaturaCruda),
          temperaturaCorregida: Number(nuevoSenso.temperaturaCorregida),
          estadoSensor: Number(nuevoSenso.estadoSensor) as EstadoSensor,
        }),
      "crearSensoManual",
      { mensajeExito: "Censo manual registrado en PostgreSQL embebido." }
    );
    if (creado) {
      cargarTablaSenso();
      if (macSeleccionada) cargarDetalleSensor(macSeleccionada);
    }
  };

  const eliminarSenso = async (idSenso: number) => {
    await safeRpc(
      () => client.eliminarSensoPorId(idSenso),
      "eliminarSensoPorId",
      { mensajeExito: `Registro Senso #${idSenso} eliminado.` }
    );
    cargarTablaSenso();
  };

  const purgarSensos = async () => {
    const limiteSeg = inputLocalAEpochSegundos(fechaPurga);
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
      "purgarSensosAnterioresA"
    );
    if (eliminados !== null) {
      cargarTablaSenso();
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
          </div>
          <div className="flex items-center gap-3">
            <label className="flex cursor-pointer items-center gap-1.5 text-xs">
              <input
                type="checkbox"
                checked={soloActivos}
                onChange={(e) => setSoloActivos(e.target.checked)}
              />
              Solo activos
            </label>
            <button
              className="btn btn-outline px-3 py-1.5 text-xs"
              onClick={cargarCatalogo}
            >
              <RefreshCw className="h-3.5 w-3.5" />
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
                const seleccionado = s.mac === macSeleccionada;
                return (
                  <tr
                    key={s.mac}
                    onClick={() => setMacSeleccionada(s.mac)}
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
                      {s.offset >= 0 ? `+${s.offset.toFixed(2)}` : s.offset.toFixed(2)}
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
                          onClick={() => eliminarSensor(s.mac)}
                          title="Eliminar sensor"
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
                    No hay sensores registrados en la base de datos.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Perfil, Estadísticas e Historial Gráfico del Sensor Seleccionado */}
      {perfil && (
        <div className="grid grid-cols-1 gap-4 xl:grid-cols-3">
          {/* Columna izquierda: Perfil y Estadísticas */}
          <div className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div>
              <div className="flex items-center justify-between">
                <span className="font-mono text-xs text-primary">
                  {perfil.mac}
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
                  Puntos calib.: E_inf={perfil.errorInf.toFixed(2)}°C · E_sup=
                  {perfil.errorSup.toFixed(2)}°C · Offset=
                  {perfil.offset.toFixed(2)}°C
                </div>
              </div>
            </div>

            {statsSenso && (
              <div className="grid grid-cols-2 gap-2 border-t border-border pt-3 text-xs">
                <div className="rounded-lg border border-border bg-background p-2.5">
                  <div className="text-foreground-secondary">Total lecturas</div>
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
          </div>

          {/* Columna derecha (2 cols): Consulta y Gráfico Histórico */}
          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-2">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <div className="flex items-center gap-2">
                <History className="h-4 w-4 text-primary" />
                <h3 className="text-sm font-bold">
                  Curva Histórica de Censos ({perfil.mac})
                </h3>
              </div>

              <form
                onSubmit={consultarHistorial}
                className="flex flex-wrap items-center gap-2 text-xs"
              >
                <select
                  value={modoHistorial}
                  onChange={(e) =>
                    setModoHistorial(e.target.value as "ULTIMOS" | "FECHAS")
                  }
                  className="rounded border border-border bg-background px-2 py-1"
                >
                  <option value="ULTIMOS">Últimos N</option>
                  <option value="FECHAS">Rango de fechas</option>
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

                <input
                  type="number"
                  min={1}
                  max={32}
                  value={limiteHistorial}
                  onChange={(e) => setLimiteHistorial(Number(e.target.value))}
                  className="w-16 rounded border border-border bg-background px-2 py-1"
                  title="Límite (máx 32 por paquete)"
                />

                <button
                  type="submit"
                  className="btn btn-solid btn-primary px-3 py-1 text-xs"
                >
                  <Search className="h-3.5 w-3.5" />
                  Consultar
                </button>
              </form>
            </div>

            <div className="h-60 w-full rounded-lg border border-border bg-background p-2">
              {chartHistorico.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={chartHistorico}>
                    <CartesianGrid
                      strokeDasharray="3 3"
                      stroke="currentColor"
                      className="opacity-15"
                    />
                    <XAxis dataKey="hora" tick={{ fontSize: 10 }} minTickGap={25} />
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
                      dot={{ r: 2 }}
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

      {/* Formularios Exclusivos ADMIN: Metadatos, Calibración 2 Puntos y Ambiente */}
      {esAdmin && (
        <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
          {/* Metadatos del sensor */}
          <form
            onSubmit={guardarMetadatos}
            className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
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
            <button
              type="submit"
              disabled={!perfil}
              className="btn btn-solid btn-primary w-full text-xs font-semibold"
            >
              Guardar Metadatos
            </button>
          </form>

          {/* Calibración de 2 puntos + Umbrales */}
          <form
            onSubmit={guardarCalibracion}
            className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="flex items-center gap-2 text-sm font-bold">
              <Sliders className="h-4 w-4 text-warning" />
              Calibración 2 Puntos y Umbrales
            </div>
            <div className="grid grid-cols-2 gap-2 text-xs">
              <div>
                <label className="mb-1 block">Umbral Inf. TL (°C)</label>
                <input
                  type="number"
                  step="0.1"
                  disabled={!perfil}
                  value={formCalib.umbralInf}
                  onChange={(e) =>
                    setFormCalib({
                      ...formCalib,
                      umbralInf: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Umbral Sup. TH (°C)</label>
                <input
                  type="number"
                  step="0.1"
                  disabled={!perfil}
                  value={formCalib.umbralSup}
                  onChange={(e) =>
                    setFormCalib({
                      ...formCalib,
                      umbralSup: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Medición Hielo (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  disabled={!perfil}
                  value={formCalib.errorInf}
                  onChange={(e) =>
                    setFormCalib({
                      ...formCalib,
                      errorInf: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Medición Ebullición (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  disabled={!perfil}
                  value={formCalib.errorSup}
                  onChange={(e) =>
                    setFormCalib({
                      ...formCalib,
                      errorSup: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
            </div>
            <div className="text-xs">
              <label className="mb-1 block">Offset lineal adicional (°C)</label>
              <input
                type="number"
                step="0.01"
                disabled={!perfil}
                value={formCalib.offset}
                onChange={(e) =>
                  setFormCalib({
                    ...formCalib,
                    offset: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
              />
            </div>
            <button
              type="submit"
              disabled={!perfil}
              className="btn btn-solid btn-warning w-full text-xs font-semibold"
            >
              Aplicar Calibración al Sensor
            </button>
          </form>

          {/* Calibración Ambiental Global */}
          <form
            onSubmit={guardarAmbiente}
            className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2 text-sm font-bold">
                <Globe className="h-4 w-4 text-info" />
                Calibración Ambiental Global
              </div>
              {ambiente && (
                <span className="font-mono text-[10px] text-foreground-secondary">
                  Mod: {formatearEpochSegundos(ambiente.fechaModificacion)}
                </span>
              )}
            </div>
            <div className="grid grid-cols-2 gap-2 text-xs">
              <div>
                <label className="mb-1 block">Altura s.n.m. (m)</label>
                <input
                  type="number"
                  step="1"
                  value={formAmbiente.altura}
                  onChange={(e) =>
                    setFormAmbiente({
                      ...formAmbiente,
                      altura: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Presión Atm. (hPa)</label>
                <input
                  type="number"
                  step="0.1"
                  value={formAmbiente.presionAtmosferica}
                  onChange={(e) =>
                    setFormAmbiente({
                      ...formAmbiente,
                      presionAtmosferica: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Ref. Congelación (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  value={formAmbiente.tempCongelacionRef}
                  onChange={(e) =>
                    setFormAmbiente({
                      ...formAmbiente,
                      tempCongelacionRef: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Ref. Ebullición (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  value={formAmbiente.tempEbullicionRef}
                  onChange={(e) =>
                    setFormAmbiente({
                      ...formAmbiente,
                      tempEbullicionRef: Number(e.target.value),
                    })
                  }
                  className="w-full rounded border border-border bg-background px-2 py-1.5 font-mono"
                />
              </div>
            </div>
            <label className="flex items-center gap-2 text-xs">
              <input
                type="checkbox"
                checked={formAmbiente.activa}
                onChange={(e) =>
                  setFormAmbiente({ ...formAmbiente, activa: e.target.checked })
                }
              />
              Compensación barométrica activa
            </label>
            <button
              type="submit"
              className="btn btn-solid btn-info w-full text-xs font-semibold"
            >
              Guardar Parámetros Ambientales
            </button>
          </form>
        </div>
      )}

      {/* Explorador Paginado de la Tabla Senso (Cruda vs Corregida + Estado) */}
      <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
          <div>
            <h2 className="text-base font-bold">
              Explorador de Tabla <code>Senso</code> (Temperatura Cruda vs.
              Corregida)
            </h2>
            <p className="text-xs text-foreground-secondary">
              Registros almacenados en MicroSD con estado de diagnóstico DS18B20
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-2 text-xs">
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
              className="btn btn-outline px-2.5 py-1"
              disabled={offsetSenso === 0}
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
              className="btn btn-outline px-2.5 py-1"
              disabled={filasSenso.length < PAGE_SIZE_SENSO}
              onClick={() => setOffsetSenso((o) => o + PAGE_SIZE_SENSO)}
            >
              <ChevronRight className="h-4 w-4" />
            </button>
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
                <input
                  type="number"
                  min={1}
                  value={nuevoSenso.idSensor}
                  onChange={(e) =>
                    setNuevoSenso({
                      ...nuevoSenso,
                      idSensor: Number(e.target.value),
                    })
                  }
                  className="w-20 rounded border border-border bg-card px-2 py-1 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Cruda (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  value={nuevoSenso.temperaturaCruda}
                  onChange={(e) =>
                    setNuevoSenso({
                      ...nuevoSenso,
                      temperaturaCruda: Number(e.target.value),
                    })
                  }
                  className="w-24 rounded border border-border bg-card px-2 py-1 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Corregida (°C)</label>
                <input
                  type="number"
                  step="0.01"
                  value={nuevoSenso.temperaturaCorregida}
                  onChange={(e) =>
                    setNuevoSenso({
                      ...nuevoSenso,
                      temperaturaCorregida: Number(e.target.value),
                    })
                  }
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
                onClick={purgarSensos}
                className="btn btn-solid btn-error px-3 py-1 text-xs font-semibold"
              >
                <Trash2 className="h-3.5 w-3.5" />
                Purgar Historial
              </button>
            </div>
          </div>
        )}

        <div className="overflow-x-auto">
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
                        onClick={() => eliminarSenso(row.idSenso)}
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
                    Sin registros en esta página.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}