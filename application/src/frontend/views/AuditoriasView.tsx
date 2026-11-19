import { useCallback, useEffect, useState } from "react";
import {
  ShieldCheck,
  ShieldAlert,
  Lock,
  Thermometer,
  Server,
  RefreshCw,
  Trash2,
  ChevronLeft,
  ChevronRight,
  Eye,
  X,
  CheckCircle2,
  XCircle,
  Hash,
} from "lucide-react";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import {
  fechaAInputLocal,
  formatearBytes,
  formatearDuracionSegundos,
  formatearEpochSegundos,
  inputLocalAEpochSegundos,
  inputLocalAFechaIsoSql,
} from "../lib/format";
import {
  LABEL_CRITICIDAD,
  LABEL_ESTADO_BUS,
  LABEL_ESTADO_ENVIO,
  LABEL_ESTADO_SENSOR,
  LABEL_FUENTE_TIEMPO,
  LABEL_MEDIO_CONEXION,
  LABEL_RESULTADO_AUDITORIA,
  LABEL_TIPO_EVENTO,
} from "../lib/labels";
import {
  AuditoriaAuthDetalleDTO,
  AuditoriaSensorRowDTO,
  AuditoriaSistemaRowDTO,
  CriticidadEvento,
  EstadoEnvioCorreo,
  EstadisticasAuditoriaSensoresDTO,
  ResultadoAuditoria,
  ResultadoIntegridadAuthDTO,
  ResumenAuditoriaAuthDTO,
  ResumenSensorCliente,
  RolUsuario,
} from "#types/rpc.types";

const PAGE_AUTH = 16; // Máximo 16 por página según protocolo
const PAGE_SISTEMA = 6; // Máximo 6 por página según protocolo

export function AuditoriasView() {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [tab, setTab] = useState<"AUTH" | "SENSORES" | "SISTEMA">("AUTH");

  // --- Estado Pestaña 1: Auditoría Auth + HMAC-64 ---
  const [offsetAuth, setOffsetAuth] = useState(0);
  const [filasAuth, setFilasAuth] = useState<ResumenAuditoriaAuthDTO[]>([]);
  const [integridadHmac, setIntegridadHmac] =
    useState<ResultadoIntegridadAuthDTO | null>(null);
  const [verificandoHmac, setVerificandoHmac] = useState(false);
  const [detalleAuth, setDetalleAuth] =
    useState<AuditoriaAuthDetalleDTO | null>(null);
  const [fechaPurgaAuth, setFechaPurgaAuth] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 30 * 86400 * 1000))
  );

  // --- Estado Pestaña 2: Auditoría Eventos Sensores ---
  const [statsSensores, setStatsSensores] =
    useState<EstadisticasAuditoriaSensoresDTO | null>(null);
  const [sensoresCatalogo, setSensoresCatalogo] = useState<
    ResumenSensorCliente[]
  >([]);
  const [macAuditoria, setMacAuditoria] = useState<string>("");
  const [limiteAudSensor, setLimiteAudSensor] = useState<number>(32);
  const [filasAudSensor, setFilasAudSensor] = useState<AuditoriaSensorRowDTO[]>(
    []
  );

  // --- Estado Pestaña 3: Auditoría Sistema ---
  const [offsetSistema, setOffsetSistema] = useState(0);
  const [filasSistema, setFilasSistema] = useState<AuditoriaSistemaRowDTO[]>(
    []
  );
  const [fechaPurgaSistema, setFechaPurgaSistema] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 30 * 86400 * 1000))
  );

  // Cargas por pestaña
  const cargarAuth = useCallback(async () => {
    const rows = await safeRpc(
      () => client.listarAuditoriaAuth(PAGE_AUTH, offsetAuth),
      "listarAuditoriaAuth"
    );
    if (rows) setFilasAuth(rows);
  }, [offsetAuth]);

  const cargarSensoresAud = useCallback(async () => {
    const [st, lista] = await Promise.all([
      safeRpc(
        () => client.obtenerEstadisticasAuditoriaSensores(),
        "obtenerEstadisticasAuditoriaSensores",
        { silent: true }
      ),
      safeRpc(() => client.listarSensores(32, 0, false), "listarSensores", {
        silent: true,
      }),
    ]);
    if (st) setStatsSensores(st);
    if (lista) {
      setSensoresCatalogo(lista);
      if (lista.length > 0 && !macAuditoria) {
        setMacAuditoria(lista[0].mac);
      }
    }
  }, [macAuditoria]);

  const cargarEventosDeMac = useCallback(async () => {
    if (!macAuditoria) {
      setFilasAudSensor([]);
      return;
    }
    const rows = await safeRpc(
      () => client.listarAuditoriaSensorPorMac(macAuditoria, limiteAudSensor),
      "listarAuditoriaSensorPorMac"
    );
    if (rows) setFilasAudSensor(rows);
  }, [macAuditoria, limiteAudSensor]);

  const cargarSistema = useCallback(async () => {
    const rows = await safeRpc(
      () => client.listarAuditoriaSistema(PAGE_SISTEMA, offsetSistema),
      "listarAuditoriaSistema"
    );
    if (rows) setFilasSistema(rows);
  }, [offsetSistema]);

  useEffect(() => {
    if (tab === "AUTH") void cargarAuth();
  }, [tab, cargarAuth]);

  useEffect(() => {
    if (tab === "SENSORES") void cargarSensoresAud();
  }, [tab, cargarSensoresAud]);

  useEffect(() => {
    if (tab === "SENSORES" && macAuditoria) {
      void cargarEventosDeMac();
    }
  }, [tab, macAuditoria, cargarEventosDeMac]);

  useEffect(() => {
    if (tab === "SISTEMA") void cargarSistema();
  }, [tab, cargarSistema]);

  // Acciones Auth (ADMIN)
  const verificarCadenaHmac = async () => {
    setVerificandoHmac(true);
    const res = await safeRpc(
      () => client.verificarIntegridadAuditoriaAuth(),
      "verificarIntegridadAuditoriaAuth"
    );
    setVerificandoHmac(false);
    if (res) setIntegridadHmac(res);
  };

  const abrirDetalleAuth = async (idAuditoriaAuth: number) => {
    const det = await safeRpc(
      () => client.obtenerAuditoriaAuthPorId(idAuditoriaAuth),
      "obtenerAuditoriaAuthPorId"
    );
    if (det) setDetalleAuth(det);
  };

  const eliminarAuth = async (idAuditoriaAuth: number) => {
    await safeRpc(
      () => client.eliminarAuditoriaAuthPorId(idAuditoriaAuth),
      "eliminarAuditoriaAuthPorId",
      { mensajeExito: `Registro Auth #${idAuditoriaAuth} eliminado.` }
    );
    void cargarAuth();
  };

  const purgarAuth = async () => {
    const seg = inputLocalAEpochSegundos(fechaPurgaAuth);
    if (!seg) return;
    if (
      !window.confirm(
        `¿Purgar auditorías de autenticación anteriores a ${formatearEpochSegundos(seg)}?`
      )
    ) {
      return;
    }
    const n = await safeRpc(
      () => client.purgarAuditoriaAuth(seg),
      "purgarAuditoriaAuth"
    );
    if (n !== null) {
      setOffsetAuth(0);
      void cargarAuth();
    }
  };

  // Acciones Auditoría Sensores (ADMIN)
  const cambiarEstadoReporteSensor = async (
    idAuditoriaSensor: number,
    nuevoEstado: EstadoEnvioCorreo
  ) => {
    const act = await safeRpc(
      () =>
        client.actualizarEstadoReporteAuditoriaSensor(
          idAuditoriaSensor,
          nuevoEstado
        ),
      "actualizarEstadoReporteAuditoriaSensor",
      { mensajeExito: "Estado de reporte actualizado." }
    );
    if (act) {
      void cargarEventosDeMac();
      void cargarSensoresAud();
    }
  };

  const eliminarAudSensor = async (idAuditoriaSensor: number) => {
    await safeRpc(
      () => client.eliminarAuditoriaSensorPorId(idAuditoriaSensor),
      "eliminarAuditoriaSensorPorId",
      { mensajeExito: `Evento de sensor #${idAuditoriaSensor} eliminado.` }
    );
    void cargarEventosDeMac();
    void cargarSensoresAud();
  };

  // Acciones Auditoría Sistema (ADMIN)
  const eliminarAudSistema = async (idAuditoriaSistema: number) => {
    await safeRpc(
      () => client.eliminarAuditoriaSistemaPorId(idAuditoriaSistema),
      "eliminarAuditoriaSistemaPorId",
      { mensajeExito: `Registro de sistema #${idAuditoriaSistema} eliminado.` }
    );
    void cargarSistema();
  };

  const purgarSistema = async () => {
    const iso = inputLocalAFechaIsoSql(fechaPurgaSistema);
    if (!iso) return;
    if (
      !window.confirm(
        `¿Purgar auditorías del sistema anteriores a ${iso}?`
      )
    ) {
      return;
    }
    const n = await safeRpc(
      () => client.purgarAuditoriaSistema(iso),
      "purgarAuditoriaSistema"
    );
    if (n !== null) {
      setOffsetSistema(0);
      void cargarSistema();
    }
  };

  const badgeCriticidad = (c: CriticidadEvento) => {
    if (c === CriticidadEvento.ALTA)
      return "bg-alert-err-bg text-alert-err-icon";
    if (c === CriticidadEvento.MEDIA)
      return "bg-alert-warn-bg text-alert-warn-icon";
    if (c === CriticidadEvento.BAJA) return "bg-info-light text-info";
    return "bg-secondary text-foreground-secondary";
  };

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado y Selector de Pestañas */}
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-light text-primary">
            <ShieldCheck className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-lg font-bold">
              Auditorías del Sistema y Cadena Criptográfica HMAC-64
            </h1>
            <p className="text-xs text-foreground-secondary">
              Trazabilidad inmutable de sesiones L2, transiciones DS18B20 y
              eventos de arranque RTOS
            </p>
          </div>
        </div>

        <div className="flex flex-wrap gap-1.5 rounded-lg bg-secondary p-1 text-xs">
          <button
            type="button"
            onClick={() => setTab("AUTH")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold transition-colors ${
              tab === "AUTH"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Lock className="h-3.5 w-3.5" />
            Auth & HMAC-64
          </button>
          <button
            type="button"
            onClick={() => setTab("SENSORES")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold transition-colors ${
              tab === "SENSORES"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Thermometer className="h-3.5 w-3.5" />
            Eventos de Sensores
          </button>
          <button
            type="button"
            onClick={() => setTab("SISTEMA")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold transition-colors ${
              tab === "SISTEMA"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Server className="h-3.5 w-3.5" />
            Sistema & Arranques
          </button>
        </div>
      </div>

      {/* ================================================================= */}
      {/* PESTAÑA 1: AUDITORÍA AUTH & VERIFICACIÓN HMAC-64                  */}
      {/* ================================================================= */}
      {tab === "AUTH" && (
        <div className="space-y-4">
          {esAdmin && (
            <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
              <div className="flex flex-wrap items-center gap-3">
                <button
                  type="button"
                  onClick={() => void verificarCadenaHmac()}
                  disabled={verificandoHmac}
                  className="btn btn-solid btn-primary px-3.5 py-2 text-xs font-bold"
                >
                  {verificandoHmac ? (
                    <RefreshCw className="h-4 w-4 animate-spin" />
                  ) : (
                    <Hash className="h-4 w-4" />
                  )}
                  Verificar Integridad Criptográfica HMAC-64
                </button>

                {integridadHmac && (
                  <div
                    className={`flex items-center gap-2 rounded-lg border px-3 py-1.5 text-xs font-semibold ${
                      integridadHmac.cadenaIntegra
                        ? "border-alert-success-border bg-alert-success-bg text-alert-success-icon"
                        : "border-alert-err-border bg-alert-err-bg text-alert-err-icon"
                    }`}
                  >
                    {integridadHmac.cadenaIntegra ? (
                      <CheckCircle2 className="h-4 w-4" />
                    ) : (
                      <XCircle className="h-4 w-4" />
                    )}
                    <span>
                      {integridadHmac.cadenaIntegra
                        ? `CADENA ÍNTEGRA (${integridadHmac.registrosVerificados} bloques verificados)`
                        : `ALERTA DE ALTERACIÓN (${integridadHmac.registrosVerificados} verificados)`}
                    </span>
                  </div>
                )}
              </div>

              <div className="flex flex-wrap items-center gap-2 text-xs">
                <input
                  type="datetime-local"
                  value={fechaPurgaAuth}
                  onChange={(e) => setFechaPurgaAuth(e.target.value)}
                  className="rounded border border-border bg-background px-2 py-1"
                />
                <button
                  type="button"
                  onClick={() => void purgarAuth()}
                  className="btn btn-outline btn-error px-3 py-1 text-xs"
                >
                  <Trash2 className="h-3.5 w-3.5" />
                  Purgar Auth
                </button>
              </div>
            </div>
          )}

          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <h2 className="text-sm font-bold">
                Registro de Autenticación y Transacciones (Máx. {PAGE_AUTH}{" "}
                por página)
              </h2>
              <div className="flex items-center gap-2 text-xs">
                <button
                  className="btn btn-outline px-2.5 py-1"
                  onClick={() => void cargarAuth()}
                >
                  <RefreshCw className="h-3.5 w-3.5" />
                </button>
                <button
                  className="btn btn-outline px-2.5 py-1"
                  disabled={offsetAuth === 0}
                  onClick={() =>
                    setOffsetAuth((o) => Math.max(0, o - PAGE_AUTH))
                  }
                >
                  <ChevronLeft className="h-4 w-4" />
                </button>
                <span className="font-mono">
                  Offset {offsetAuth} – {offsetAuth + filasAuth.length}
                </span>
                <button
                  className="btn btn-outline px-2.5 py-1"
                  disabled={filasAuth.length < PAGE_AUTH}
                  onClick={() => setOffsetAuth((o) => o + PAGE_AUTH)}
                >
                  <ChevronRight className="h-4 w-4" />
                </button>
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left text-xs">
                <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                  <tr>
                    <th className="px-2.5 py-2">Seq</th>
                    <th className="px-2.5 py-2">Usuario</th>
                    <th className="px-2.5 py-2">Acción</th>
                    <th className="px-2.5 py-2">Medio / Origen</th>
                    <th className="px-2.5 py-2">Resultado</th>
                    <th className="px-2.5 py-2">Criticidad</th>
                    <th className="px-2.5 py-2">Fecha / Hora</th>
                    {esAdmin && (
                      <th className="px-2.5 py-2 text-right">HMAC / Acciones</th>
                    )}
                  </tr>
                </thead>
                <tbody className="divide-y divide-border">
                  {filasAuth.map((item) => (
                    <tr
                      key={item.idAuditoriaAuth}
                      onClick={() =>
                        esAdmin && void abrirDetalleAuth(item.idAuditoriaAuth)
                      }
                      className={
                        esAdmin
                          ? "cursor-pointer hover:bg-secondary/40"
                          : "hover:bg-secondary/30"
                      }
                    >
                      <td className="px-2.5 py-2 font-mono font-bold">
                        #{item.seq}
                      </td>
                      <td className="px-2.5 py-2 font-mono">
                        {item.usuarioIntentado || "—"}
                      </td>
                      <td className="px-2.5 py-2 font-mono text-[11px]">
                        {item.accion}
                      </td>
                      <td className="px-2.5 py-2 text-foreground-secondary">
                        <span className="font-semibold text-foreground">
                          {LABEL_MEDIO_CONEXION[item.medioConexion] ??
                            item.medioConexion}
                        </span>{" "}
                        · <span className="font-mono">{item.origenRemoto}</span>
                      </td>
                      <td className="px-2.5 py-2">
                        <span
                          className={`rounded px-2 py-0.5 text-[10px] font-semibold ${
                            item.resultado === ResultadoAuditoria.EXITO
                              ? "bg-alert-success-bg text-alert-success-icon"
                              : "bg-alert-err-bg text-alert-err-icon"
                          }`}
                        >
                          {LABEL_RESULTADO_AUDITORIA[item.resultado] ??
                            item.resultado}
                        </span>
                      </td>
                      <td className="px-2.5 py-2">
                        <span
                          className={`rounded px-2 py-0.5 text-[10px] font-semibold ${badgeCriticidad(
                            item.criticidad
                          )}`}
                        >
                          {LABEL_CRITICIDAD[item.criticidad] ?? item.criticidad}
                        </span>
                      </td>
                      <td className="px-2.5 py-2 font-mono text-foreground-secondary">
                        {formatearEpochSegundos(item.fechaCreacion)}
                      </td>
                      {esAdmin && (
                        <td
                          className="px-2.5 py-2 text-right"
                          onClick={(e) => e.stopPropagation()}
                        >
                          <div className="inline-flex items-center gap-1">
                            <button
                              className="btn btn-icon btn-info"
                              onClick={() =>
                                void abrirDetalleAuth(item.idAuditoriaAuth)
                              }
                              title="Inspeccionar hashes HMAC-64"
                            >
                              <Eye className="h-3.5 w-3.5" />
                            </button>
                            <button
                              className="btn btn-icon btn-error"
                              onClick={() =>
                                void eliminarAuth(item.idAuditoriaAuth)
                              }
                              title="Eliminar registro"
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          </div>
                        </td>
                      )}
                    </tr>
                  ))}
                  {filasAuth.length === 0 && (
                    <tr>
                      <td
                        colSpan={esAdmin ? 8 : 7}
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
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 2: AUDITORÍA DE EVENTOS DE SENSORES                       */}
      {/* ================================================================= */}
      {tab === "SENSORES" && (
        <div className="space-y-4">
          {statsSensores && (
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-3">
              <div className="rounded-xl border border-border bg-card p-4 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Total Eventos Registrados
                </div>
                <div className="mt-1 text-2xl font-bold">
                  {statsSensores.totalEventos}
                </div>
              </div>
              <div className="rounded-xl border border-border bg-card p-4 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Eventos Críticos
                </div>
                <div className="mt-1 text-2xl font-bold text-error">
                  {statsSensores.eventosCriticos}
                </div>
              </div>
              <div className="rounded-xl border border-border bg-card p-4 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Reportes de Correo Pendientes
                </div>
                <div className="mt-1 text-2xl font-bold text-warning">
                  {statsSensores.reportesPendientes}
                </div>
              </div>
            </div>
          )}

          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <h2 className="text-sm font-bold">
                Transiciones de Estado por Sensor DS18B20
              </h2>
              <div className="flex flex-wrap items-center gap-2 text-xs">
                <select
                  value={macAuditoria}
                  onChange={(e) => setMacAuditoria(e.target.value)}
                  className="rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                >
                  {sensoresCatalogo.length === 0 && (
                    <option value="">Sin sensores</option>
                  )}
                  {sensoresCatalogo.map((s) => (
                    <option key={s.mac} value={s.mac}>
                      {s.mac} ({s.nombre || `ID #${s.idSensor}`})
                    </option>
                  ))}
                </select>
                <input
                  type="number"
                  min={1}
                  max={32}
                  value={limiteAudSensor}
                  onChange={(e) => setLimiteAudSensor(Number(e.target.value))}
                  className="w-16 rounded border border-border bg-background px-2 py-1.5"
                  title="Límite registros"
                />
                <button
                  className="btn btn-outline px-2.5 py-1.5"
                  onClick={() => void cargarEventosDeMac()}
                >
                  <RefreshCw className="h-3.5 w-3.5" />
                </button>
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left text-xs">
                <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                  <tr>
                    <th className="px-2.5 py-2">ID</th>
                    <th className="px-2.5 py-2">Transición de Estado</th>
                    <th className="px-2.5 py-2">Bus 1-Wire</th>
                    <th className="px-2.5 py-2">Tipo / Criticidad</th>
                    <th className="px-2.5 py-2">Estado Reporte</th>
                    <th className="px-2.5 py-2">Fecha</th>
                    {esAdmin && <th className="px-2.5 py-2 text-right">Acciones</th>}
                  </tr>
                </thead>
                <tbody className="divide-y divide-border">
                  {filasAudSensor.map((ev) => (
                    <tr
                      key={ev.idAuditoriaSensor}
                      className="hover:bg-secondary/40"
                    >
                      <td className="px-2.5 py-2 font-mono">
                        #{ev.idAuditoriaSensor}
                      </td>
                      <td className="px-2.5 py-2 font-mono text-[11px]">
                        <span className="text-foreground-secondary">
                          {LABEL_ESTADO_SENSOR[ev.estadoPrevioSensor] ??
                            ev.estadoPrevioSensor}
                        </span>{" "}
                        →{" "}
                        <span className="font-bold text-foreground">
                          {LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ??
                            ev.estadoNuevoSensor}
                        </span>
                      </td>
                      <td className="px-2.5 py-2 font-mono text-[11px]">
                        {LABEL_ESTADO_BUS[ev.estadoBus] ?? ev.estadoBus}
                      </td>
                      <td className="px-2.5 py-2">
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
                      </td>
                      <td className="px-2.5 py-2">
                        {esAdmin ? (
                          <select
                            value={ev.estadoReporte}
                            onChange={(e) =>
                              void cambiarEstadoReporteSensor(
                                ev.idAuditoriaSensor,
                                Number(e.target.value) as EstadoEnvioCorreo
                              )
                            }
                            className="rounded border border-border bg-background px-2 py-1 text-[11px]"
                          >
                            <option value={EstadoEnvioCorreo.PENDIENTE}>
                              PENDIENTE
                            </option>
                            <option value={EstadoEnvioCorreo.ENVIADO}>
                              ENVIADO
                            </option>
                            <option value={EstadoEnvioCorreo.FALLIDO}>
                              FALLIDO
                            </option>
                          </select>
                        ) : (
                          <span className="rounded bg-secondary px-2 py-0.5 text-[10px] font-semibold">
                            {LABEL_ESTADO_ENVIO[ev.estadoReporte] ??
                              ev.estadoReporte}
                          </span>
                        )}
                      </td>
                      <td className="px-2.5 py-2 font-mono text-foreground-secondary">
                        {ev.fechaCreacion || "—"}
                      </td>
                      {esAdmin && (
                        <td className="px-2.5 py-2 text-right">
                          <button
                            className="btn btn-icon btn-error"
                            onClick={() =>
                              void eliminarAudSensor(ev.idAuditoriaSensor)
                            }
                            title="Eliminar evento"
                          >
                            <Trash2 className="h-3.5 w-3.5" />
                          </button>
                        </td>
                      )}
                    </tr>
                  ))}
                  {filasAudSensor.length === 0 && (
                    <tr>
                      <td
                        colSpan={esAdmin ? 7 : 6}
                        className="px-3 py-6 text-center text-foreground-secondary"
                      >
                        Sin eventos registrados para el sensor seleccionado.
                      </td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 3: AUDITORÍA DEL SISTEMA Y ARRANQUES                      */}
      {/* ================================================================= */}
      {tab === "SISTEMA" && (
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
            <h2 className="text-sm font-bold">
              Historial de Arranques BIOS, Heap, MicroSD y Bus 1-Wire (Máx.{" "}
              {PAGE_SISTEMA} por página)
            </h2>

            <div className="flex flex-wrap items-center gap-2 text-xs">
              {esAdmin && (
                <>
                  <input
                    type="datetime-local"
                    value={fechaPurgaSistema}
                    onChange={(e) => setFechaPurgaSistema(e.target.value)}
                    className="rounded border border-border bg-background px-2 py-1"
                  />
                  <button
                    type="button"
                    onClick={() => void purgarSistema()}
                    className="btn btn-outline btn-error px-2.5 py-1 text-xs"
                  >
                    <Trash2 className="h-3.5 w-3.5" />
                    Purgar Sistema
                  </button>
                </>
              )}
              <button
                className="btn btn-outline px-2.5 py-1"
                onClick={() => void cargarSistema()}
              >
                <RefreshCw className="h-3.5 w-3.5" />
              </button>
              <button
                className="btn btn-outline px-2.5 py-1"
                disabled={offsetSistema === 0}
                onClick={() =>
                  setOffsetSistema((o) => Math.max(0, o - PAGE_SISTEMA))
                }
              >
                <ChevronLeft className="h-4 w-4" />
              </button>
              <span className="font-mono">
                Offset {offsetSistema} – {offsetSistema + filasSistema.length}
              </span>
              <button
                className="btn btn-outline px-2.5 py-1"
                disabled={filasSistema.length < PAGE_SISTEMA}
                onClick={() => setOffsetSistema((o) => o + PAGE_SISTEMA)}
              >
                <ChevronRight className="h-4 w-4" />
              </button>
            </div>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left text-xs">
              <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                <tr>
                  <th className="px-2.5 py-2">Boot / ID</th>
                  <th className="px-2.5 py-2">Motivo Reset / FW</th>
                  <th className="px-2.5 py-2">Recursos (Heap / SD)</th>
                  <th className="px-2.5 py-2">Bus / Tiempo / Red</th>
                  <th className="px-2.5 py-2">Detalle</th>
                  <th className="px-2.5 py-2">Fecha</th>
                  {esAdmin && <th className="px-2.5 py-2 text-right">Eliminar</th>}
                </tr>
              </thead>
              <tbody className="divide-y divide-border">
                {filasSistema.map((row) => (
                  <tr
                    key={row.idAuditoriaSistema}
                    className="hover:bg-secondary/40"
                  >
                    <td className="px-2.5 py-2 font-mono">
                      <div className="font-bold">Boot #{row.bootId}</div>
                      <div className="text-[10px] text-foreground-secondary">
                        ID #{row.idAuditoriaSistema}
                      </div>
                    </td>
                    <td className="px-2.5 py-2">
                      <div className="font-mono font-semibold">
                        {row.resetReason || "—"}
                      </div>
                      <div className="text-[10px] text-foreground-secondary">
                        FW {row.versionFirmware} · Uptime{" "}
                        {formatearDuracionSegundos(row.uptimeS)}
                      </div>
                    </td>
                    <td className="px-2.5 py-2 font-mono text-[11px]">
                      <div>Heap: {formatearBytes(row.heapLibreBytes)}</div>
                      <div className="text-foreground-secondary">
                        SD libre: {row.espacioSdLibreKb} KB
                      </div>
                    </td>
                    <td className="px-2.5 py-2 text-[11px]">
                      <div>
                        Bus:{" "}
                        <strong>
                          {LABEL_ESTADO_BUS[row.estadoBus] ?? row.estadoBus}
                        </strong>
                      </div>
                      <div className="text-foreground-secondary">
                        Reloj:{" "}
                        {LABEL_FUENTE_TIEMPO[row.fuenteTiempo] ??
                          row.fuenteTiempo}{" "}
                        · {row.internet ? "Online" : "Offline"}
                      </div>
                    </td>
                    <td className="max-w-xs px-2.5 py-2">
                      <span
                        className={`mr-1.5 rounded px-1.5 py-0.5 text-[10px] font-semibold ${badgeCriticidad(
                          row.criticidad
                        )}`}
                      >
                        {LABEL_CRITICIDAD[row.criticidad] ?? row.criticidad}
                      </span>
                      <span className="text-foreground">{row.detalle}</span>
                    </td>
                    <td className="px-2.5 py-2 font-mono text-foreground-secondary">
                      {row.fechaCreacion || "—"}
                    </td>
                    {esAdmin && (
                      <td className="px-2.5 py-2 text-right">
                        <button
                          className="btn btn-icon btn-error"
                          onClick={() =>
                            void eliminarAudSistema(row.idAuditoriaSistema)
                          }
                          title="Eliminar registro"
                        >
                          <Trash2 className="h-3.5 w-3.5" />
                        </button>
                      </td>
                    )}
                  </tr>
                ))}
                {filasSistema.length === 0 && (
                  <tr>
                    <td
                      colSpan={esAdmin ? 7 : 6}
                      className="px-3 py-6 text-center text-foreground-secondary"
                    >
                      Sin registros de auditoría del sistema en esta página.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {/* Modal de Inspección Criptográfica HMAC-64 (Solo ADMIN) */}
      {detalleAuth && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4 backdrop-blur-[1px]">
          <div className="w-full max-w-2xl rounded-xl border border-border bg-card p-5 text-card-foreground shadow-2xl">
            <div className="mb-3 flex items-center justify-between border-b border-border pb-2.5">
              <div className="flex items-center gap-2">
                <ShieldAlert className="h-5 w-5 text-primary" />
                <h3 className="text-base font-bold">
                  Bloque de Auditoría HMAC-64 · Seq #{detalleAuth.seq}
                </h3>
              </div>
              <button
                className="btn btn-icon btn-secondary"
                onClick={() => setDetalleAuth(null)}
              >
                <X className="h-4 w-4" />
              </button>
            </div>

            <div className="grid grid-cols-2 gap-3 text-xs sm:grid-cols-3">
              <div>
                <span className="text-foreground-secondary">Usuario:</span>
                <div className="font-mono font-bold">
                  {detalleAuth.usuarioIntentado} (ID {detalleAuth.idUsuario})
                </div>
              </div>
              <div>
                <span className="text-foreground-secondary">Acción:</span>
                <div className="font-mono font-bold">{detalleAuth.accion}</div>
              </div>
              <div>
                <span className="text-foreground-secondary">Tabla:</span>
                <div className="font-mono">
                  {detalleAuth.tablaAfectada || "—"}
                </div>
              </div>
              <div>
                <span className="text-foreground-secondary">Boot / Uptime:</span>
                <div className="font-mono">
                  Boot #{detalleAuth.bootId} ·{" "}
                  {formatearDuracionSegundos(detalleAuth.uptimeS)}
                </div>
              </div>
              <div>
                <span className="text-foreground-secondary">Fuente Tiempo:</span>
                <div>
                  {LABEL_FUENTE_TIEMPO[detalleAuth.fuenteTiempo] ??
                    detalleAuth.fuenteTiempo}
                </div>
              </div>
              <div>
                <span className="text-foreground-secondary">Timestamp:</span>
                <div className="font-mono">
                  {formatearEpochSegundos(detalleAuth.fechaCreacion)}
                </div>
              </div>
            </div>

            <div className="mt-3 space-y-2 text-xs">
              <div className="rounded border border-border bg-background p-2">
                <span className="text-foreground-secondary">Detalle: </span>
                <span>{detalleAuth.detalle || "—"}</span>
              </div>
              <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                <div className="rounded border border-border bg-background p-2 font-mono text-[11px]">
                  <div className="text-foreground-secondary">Valor Previo:</div>
                  <div className="break-all">
                    {detalleAuth.valorPrevio || "—"}
                  </div>
                </div>
                <div className="rounded border border-border bg-background p-2 font-mono text-[11px]">
                  <div className="text-foreground-secondary">Valor Nuevo:</div>
                  <div className="break-all">
                    {detalleAuth.valorNuevo || "—"}
                  </div>
                </div>
              </div>

              <div className="rounded-lg border border-border bg-zinc-950 p-3 font-mono text-[11px] text-emerald-400">
                <div className="mb-1 text-zinc-400">
                  hash_previo (SHA-256 / HMAC-64):
                </div>
                <div className="mb-2 break-all text-amber-300">
                  {detalleAuth.hashPrevio || "0".repeat(64)}
                </div>
                <div className="mb-1 text-zinc-400">
                  hash_registro (Encadenado):
                </div>
                <div className="break-all">{detalleAuth.hashRegistro}</div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}