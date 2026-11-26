import { useCallback, useEffect, useRef, useState } from "react";
import {
  ShieldCheck,
  ShieldAlert,
  Lock,
  Thermometer,
  Server,
  RefreshCw,
  Trash2,
  Eye,
  X,
  CheckCircle2,
  XCircle,
  Hash,
  FileSpreadsheet,
} from "lucide-react";
import { toast } from "react-toastify";
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
import { sufijoFechaArchivo } from "../lib/exportCsv";
import { exportarTablaExcel } from "../lib/exportExcel";
import {
  DataTable,
  DataTableColumn,
  DataTableFilter,
} from "../components/ui/DataTable";
import {
  AuditoriaAuthDetalleDTO,
  AuditoriaSensorRowDTO,
  AuditoriaSistemaRowDTO,
  CriticidadEvento,
  EstadoEnvioCorreo,
  EstadoSensor,
  EstadisticasAuditoriaSensoresDTO,
  ResultadoAuditoria,
  ResultadoIntegridadAuthDTO,
  ResumenAuditoriaAuthDTO,
  ResumenSensorCliente,
  RolUsuario,
} from "#types/rpc.types";

const PAGE_AUTH = 16; // Máximo 16 por bloque según MTU RPC
const PAGE_SISTEMA = 6; // Máximo 6 por bloque según MTU RPC

function parsearFechaTextoAEpochSeg(texto: string): number {
  if (!texto) return 0;
  const iso = texto.includes("T") ? texto : texto.replace(" ", "T");
  const ms = Date.parse(iso);
  return Number.isFinite(ms) ? Math.floor(ms / 1000) : 0;
}

export function AuditoriasView() {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [tab, setTab] = useState<"AUTH" | "SENSORES" | "SISTEMA">("AUTH");

  // --- Estado Pestaña 1: Auditoría Auth + HMAC-64 ---
  const [filasAuth, setFilasAuth] = useState<ResumenAuditoriaAuthDTO[]>([]);
  const [hayMasAuth, setHayMasAuth] = useState(false);
  const [cargandoMasAuth, setCargandoMasAuth] = useState(false);
  const [integridadHmac, setIntegridadHmac] =
    useState<ResultadoIntegridadAuthDTO | null>(null);
  const [verificandoHmac, setVerificandoHmac] = useState(false);
  const [detalleAuth, setDetalleAuth] =
    useState<AuditoriaAuthDetalleDTO | null>(null);
  const [fechaPurgaAuth, setFechaPurgaAuth] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 30 * 86400 * 1000))
  );
  const filasAuthLenRef = useRef(0);
  filasAuthLenRef.current = filasAuth.length;

  // --- Estado Pestaña 2: Auditoría Eventos Sensores ---
  const [statsSensores, setStatsSensores] =
    useState<EstadisticasAuditoriaSensoresDTO | null>(null);
  const [sensoresCatalogo, setSensoresCatalogo] = useState<
    ResumenSensorCliente[]
  >([]);
  const [macAuditoria, setMacAuditoria] = useState<string>("");
  const [filasAudSensor, setFilasAudSensor] = useState<AuditoriaSensorRowDTO[]>(
    []
  );

  // --- Estado Pestaña 3: Auditoría Sistema ---
  const [filasSistema, setFilasSistema] = useState<AuditoriaSistemaRowDTO[]>(
    []
  );
  const [hayMasSistema, setHayMasSistema] = useState(false);
  const [cargandoMasSistema, setCargandoMasSistema] = useState(false);
  const [fechaPurgaSistema, setFechaPurgaSistema] = useState(() =>
    fechaAInputLocal(new Date(Date.now() - 30 * 86400 * 1000))
  );
  const filasSistemaLenRef = useRef(0);
  filasSistemaLenRef.current = filasSistema.length;

  // Cargas por pestaña
  const cargarAuth = useCallback(async (reiniciar = true) => {
    const offset = reiniciar ? 0 : filasAuthLenRef.current;
    if (!reiniciar) setCargandoMasAuth(true);
    const rows = await safeRpc(
      () => client.listarAuditoriaAuth(PAGE_AUTH, offset),
      "listarAuditoriaAuth"
    );
    setCargandoMasAuth(false);
    if (rows) {
      setHayMasAuth(rows.length === PAGE_AUTH);
      if (reiniciar) {
        setFilasAuth(rows);
      } else {
        setFilasAuth((prev) => {
          const ids = new Set(prev.map((r) => r.idAuditoriaAuth));
          return [...prev, ...rows.filter((r) => !ids.has(r.idAuditoriaAuth))];
        });
      }
    }
  }, []);

  const cargarSensoresAud = useCallback(async () => {
    const [st, lista] = await Promise.all([
      safeRpc(
        () => client.obtenerEstadisticasAuditoriaSensores(),
        "obtenerEstadisticasAuditoriaSensores",
        { silent: true }
      ),
      safeRpc(() => client.listarSensores(12, 0, false), "listarSensores", {
        silent: true,
      }),
    ]);
    if (st) setStatsSensores(st);
    if (lista) {
      setSensoresCatalogo(lista);
      setMacAuditoria((prev) =>
        prev || (lista.length > 0 ? lista[0].mac : "")
      );
    }
  }, []);

  const cargarEventosDeMac = useCallback(async () => {
    if (!macAuditoria) {
      setFilasAudSensor([]);
      return;
    }
    const rows = await safeRpc(
      () => client.listarAuditoriaSensorPorMac(macAuditoria, 32),
      "listarAuditoriaSensorPorMac",
      { silent: true }
    );
    if (rows) setFilasAudSensor(rows);
  }, [macAuditoria]);

  const cargarSistema = useCallback(async (reiniciar = true) => {
    const offset = reiniciar ? 0 : filasSistemaLenRef.current;
    if (!reiniciar) setCargandoMasSistema(true);
    const rows = await safeRpc(
      () => client.listarAuditoriaSistema(PAGE_SISTEMA, offset),
      "listarAuditoriaSistema"
    );
    setCargandoMasSistema(false);
    if (rows) {
      setHayMasSistema(rows.length === PAGE_SISTEMA);
      if (reiniciar) {
        setFilasSistema(rows);
      } else {
        setFilasSistema((prev) => {
          const ids = new Set(prev.map((r) => r.idAuditoriaSistema));
          return [
            ...prev,
            ...rows.filter((r) => !ids.has(r.idAuditoriaSistema)),
          ];
        });
      }
    }
  }, []);

  useEffect(() => {
    if (tab === "AUTH") void cargarAuth(true);
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
    if (tab === "SISTEMA") void cargarSistema(true);
  }, [tab, cargarSistema]);

  // Refresco silencioso cada 15s para ver el cambio autónomo PENDIENTE -> ENVIADO / FALLIDO -> ENVIADO
  useEffect(() => {
    if (tab !== "SENSORES" && tab !== "SISTEMA") return;
    const id = window.setInterval(() => {
      if (tab === "SENSORES" && macAuditoria) {
        void cargarEventosDeMac();
        void cargarSensoresAud();
      } else if (tab === "SISTEMA") {
        void cargarSistema(true);
      }
    }, 15000);
    return () => window.clearInterval(id);
  }, [tab, macAuditoria, cargarEventosDeMac, cargarSensoresAud, cargarSistema]);

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
    void cargarAuth(true);
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
      void cargarAuth(true);
    }
  };

  // Acciones Auditoría Sensores (ADMIN)
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
    void cargarSistema(true);
  };

  const purgarSistema = async () => {
    const iso = inputLocalAFechaIsoSql(fechaPurgaSistema);
    if (!iso) return;
    if (!window.confirm(`¿Purgar auditorías del sistema anteriores a ${iso}?`)) {
      return;
    }
    const n = await safeRpc(
      () => client.purgarAuditoriaSistema(iso),
      "purgarAuditoriaSistema"
    );
    if (n !== null) {
      void cargarSistema(true);
    }
  };

  // Exportadores Excel (.xlsx)
  const exportarAuthExcel = async () => {
    if (filasAuth.length === 0) {
      toast.info("No hay registros de Auditoría Auth cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `auditoria_auth_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Auditoria Auth",
      titulo: "Auditoría de Autenticación y Transacciones (HMAC-64)",
      subtitulo: `${filasAuth.length} registros exportados`,
      columnas: [
        { header: "Seq", width: 10, numFmt: "0" },
        { header: "ID Usuario", width: 12, numFmt: "0" },
        { header: "Usuario", width: 18 },
        { header: "Acción", width: 28 },
        { header: "Medio", width: 14 },
        { header: "Origen Remoto", width: 18 },
        { header: "Resultado", width: 22 },
        { header: "Criticidad", width: 14 },
        { header: "Fecha / Hora", width: 22 },
      ],
      filas: filasAuth.map((r) => [
        r.seq,
        r.idUsuario,
        r.usuarioIntentado || "—",
        r.accion,
        LABEL_MEDIO_CONEXION[r.medioConexion] ?? String(r.medioConexion),
        r.origenRemoto,
        LABEL_RESULTADO_AUDITORIA[r.resultado] ?? String(r.resultado),
        LABEL_CRITICIDAD[r.criticidad] ?? String(r.criticidad),
        formatearEpochSegundos(r.fechaCreacion),
      ]),
    });
    toast.success(`Excel Auth exportado (${filasAuth.length} filas).`);
  };

  const exportarSensoresAudExcel = async () => {
    if (filasAudSensor.length === 0) {
      toast.info("No hay eventos de sensor cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `auditoria_sensor_${macAuditoria || "general"}_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Eventos Sensores",
      titulo: `Auditoría de Transiciones DS18B20 (${macAuditoria})`,
      subtitulo: `${filasAudSensor.length} eventos exportados`,
      resumen: statsSensores
        ? [
            { etiqueta: "Total Eventos Globales", valor: statsSensores.totalEventos, numFmt: "0" },
            { etiqueta: "Eventos Críticos", valor: statsSensores.eventosCriticos, numFmt: "0" },
            { etiqueta: "Reportes Pendientes", valor: statsSensores.reportesPendientes, numFmt: "0" },
          ]
        : undefined,
      columnas: [
        { header: "ID Evento", width: 12, numFmt: "0" },
        { header: "ID Censo", width: 12, numFmt: "0" },
        { header: "Estado Previo", width: 22 },
        { header: "Estado Nuevo", width: 22 },
        { header: "Bus 1-Wire", width: 22 },
        { header: "Tipo", width: 16 },
        { header: "Criticidad", width: 14 },
        { header: "Estado Reporte Correo", width: 20 },
        { header: "Fecha / Hora", width: 22 },
      ],
      filas: filasAudSensor.map((ev) => [
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
    toast.success(`Excel de eventos exportado (${filasAudSensor.length} filas).`);
  };

  const exportarSistemaExcel = async () => {
    if (filasSistema.length === 0) {
      toast.info("No hay registros de sistema cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `auditoria_sistema_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Auditoria Sistema",
      titulo: "Auditoría de Sistema, Arranques BIOS y Bus 1-Wire",
      subtitulo: `${filasSistema.length} registros exportados`,
      columnas: [
        { header: "ID", width: 10, numFmt: "0" },
        { header: "Boot ID", width: 10, numFmt: "0" },
        { header: "Motivo Reset", width: 20 },
        { header: "Firmware", width: 12 },
        { header: "Uptime (s)", width: 12, numFmt: "0" },
        { header: "Heap Libre (B)", width: 16, numFmt: "#,##0" },
        { header: "SD Libre (KB)", width: 16, numFmt: "#,##0" },
        { header: "Bus 1-Wire", width: 22 },
        { header: "Internet", width: 12 },
        { header: "Criticidad", width: 14 },
        { header: "Detalle", width: 32 },
        { header: "Estado Reporte", width: 16 },
        { header: "Fecha / Hora", width: 22 },
      ],
      filas: filasSistema.map((r) => [
        r.idAuditoriaSistema,
        r.bootId,
        r.resetReason || "—",
        r.versionFirmware,
        r.uptimeS,
        r.heapLibreBytes,
        r.espacioSdLibreKb,
        LABEL_ESTADO_BUS[r.estadoBus] ?? String(r.estadoBus),
        r.internet ? "ONLINE" : "OFFLINE",
        LABEL_CRITICIDAD[r.criticidad] ?? String(r.criticidad),
        r.detalle,
        LABEL_ESTADO_ENVIO[r.estadoReporte] ?? String(r.estadoReporte),
        r.fechaCreacion || "—",
      ]),
    });
    toast.success(`Excel de sistema exportado (${filasSistema.length} filas).`);
  };

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
              reportes SMTP autónomos del ESP32
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
                Registro de Autenticación y Transacciones ({filasAuth.length}{" "}
                cargados)
              </h2>
              <div className="flex items-center gap-2 text-xs">
                <button
                  type="button"
                  className="btn btn-outline px-2.5 py-1"
                  onClick={() => void cargarAuth(true)}
                  title="Recargar desde el más reciente"
                >
                  <RefreshCw className="h-3.5 w-3.5" />
                </button>
                <button
                  type="button"
                  className="btn btn-outline btn-success px-2.5 py-1"
                  disabled={filasAuth.length === 0}
                  onClick={() => void exportarAuthExcel()}
                  title="Exportar registros cargados a Excel (.xlsx)"
                >
                  <FileSpreadsheet className="h-3.5 w-3.5" />
                  Excel
                </button>
              </div>
            </div>

            <DataTable
              data={filasAuth}
              rowKey={(item) => item.idAuditoriaAuth}
              infiniteScroll
              maxBodyHeightClass="max-h-[460px]"
              hasMore={hayMasAuth}
              loadingMore={cargandoMasAuth}
              onLoadMore={() => void cargarAuth(false)}
              dateFilter={{
                label: "Fecha",
                getEpochSeg: (r) => r.fechaCreacion,
              }}
              onRowClick={
                esAdmin
                  ? (item) => void abrirDetalleAuth(item.idAuditoriaAuth)
                  : undefined
              }
              searchPlaceholder="Buscar usuario, acción, IP..."
              searchValue={(item) =>
                `${item.usuarioIntentado} ${item.accion} ${item.origenRemoto}`
              }
              filters={
                [
                  {
                    key: "resultado",
                    label: "Resultado",
                    getValue: (r) => r.resultado,
                    options: Object.entries(LABEL_RESULTADO_AUDITORIA).map(
                      ([value, label]) => ({ value, label })
                    ),
                  },
                  {
                    key: "criticidad",
                    label: "Criticidad",
                    getValue: (r) => r.criticidad,
                    options: Object.entries(LABEL_CRITICIDAD).map(
                      ([value, label]) => ({ value, label })
                    ),
                  },
                  {
                    key: "medio",
                    label: "Medio",
                    getValue: (r) => r.medioConexion,
                    options: Object.entries(LABEL_MEDIO_CONEXION).map(
                      ([value, label]) => ({ value, label })
                    ),
                  },
                ] as DataTableFilter<ResumenAuditoriaAuthDTO>[]
              }
              columns={[
                {
                  key: "seq",
                  header: "Seq",
                  sortValue: (r) => r.seq,
                  cellClassName: "font-mono font-bold",
                  cell: (r) => `#${r.seq}`,
                },
                {
                  key: "usuario",
                  header: "Usuario",
                  sortValue: (r) => r.usuarioIntentado,
                  cellClassName: "font-mono",
                  cell: (r) => r.usuarioIntentado || "—",
                },
                {
                  key: "accion",
                  header: "Acción",
                  sortValue: (r) => r.accion,
                  cellClassName: "font-mono text-[11px]",
                  cell: (r) => r.accion,
                },
                {
                  key: "medio",
                  header: "Medio / Origen",
                  sortValue: (r) =>
                    `${LABEL_MEDIO_CONEXION[r.medioConexion] ?? r.medioConexion} ${r.origenRemoto}`,
                  cellClassName: "text-foreground-secondary",
                  cell: (r) => (
                    <>
                      <span className="font-semibold text-foreground">
                        {LABEL_MEDIO_CONEXION[r.medioConexion] ??
                          r.medioConexion}
                      </span>{" "}
                      · <span className="font-mono">{r.origenRemoto}</span>
                    </>
                  ),
                },
                {
                  key: "resultado",
                  header: "Resultado",
                  sortValue: (r) =>
                    LABEL_RESULTADO_AUDITORIA[r.resultado] ?? r.resultado,
                  cell: (r) => (
                    <span
                      className={`rounded px-2 py-0.5 text-[10px] font-semibold ${
                        r.resultado === ResultadoAuditoria.EXITO
                          ? "bg-alert-success-bg text-alert-success-icon"
                          : "bg-alert-err-bg text-alert-err-icon"
                      }`}
                    >
                      {LABEL_RESULTADO_AUDITORIA[r.resultado] ?? r.resultado}
                    </span>
                  ),
                },
                {
                  key: "criticidad",
                  header: "Criticidad",
                  sortValue: (r) => r.criticidad,
                  cell: (r) => (
                    <span
                      className={`rounded px-2 py-0.5 text-[10px] font-semibold ${badgeCriticidad(
                        r.criticidad
                      )}`}
                    >
                      {LABEL_CRITICIDAD[r.criticidad] ?? r.criticidad}
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
                        key: "acciones",
                        header: "HMAC / Acciones",
                        headerClassName: "text-right",
                        cellClassName: "text-right",
                        stopRowClick: true,
                        cell: (r) => (
                          <div className="inline-flex items-center gap-1">
                            <button
                              className="btn btn-icon btn-info"
                              onClick={() =>
                                void abrirDetalleAuth(r.idAuditoriaAuth)
                              }
                              title="Inspeccionar hashes HMAC-64"
                            >
                              <Eye className="h-3.5 w-3.5" />
                            </button>
                            <button
                              className="btn btn-icon btn-error"
                              onClick={() =>
                                void eliminarAuth(r.idAuditoriaAuth)
                              }
                              title="Eliminar registro"
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          </div>
                        ),
                      },
                    ] as DataTableColumn<ResumenAuditoriaAuthDTO>[])
                  : []),
              ]}
              emptyMessage="Sin registros de autenticación."
            />
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
              <div>
                <h2 className="text-sm font-bold">
                  Transiciones de Estado por Sensor DS18B20
                </h2>
                <p className="text-[11px] text-foreground-secondary">
                  El estado de correo se actualiza automáticamente por el ESP32
                  (refresco cada 15 s)
                </p>
              </div>
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
                <button
                  type="button"
                  className="btn btn-outline px-2.5 py-1.5"
                  onClick={() => void cargarEventosDeMac()}
                  title="Actualizar eventos"
                >
                  <RefreshCw className="h-3.5 w-3.5" />
                </button>
                <button
                  type="button"
                  className="btn btn-outline btn-success px-2.5 py-1.5"
                  disabled={filasAudSensor.length === 0}
                  onClick={() => void exportarSensoresAudExcel()}
                  title="Exportar eventos a Excel (.xlsx)"
                >
                  <FileSpreadsheet className="h-3.5 w-3.5" />
                  Excel
                </button>
              </div>
            </div>

            <DataTable
              data={filasAudSensor}
              rowKey={(ev) => ev.idAuditoriaSensor}
              infiniteScroll
              maxBodyHeightClass="max-h-[460px]"
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
                  header: "ID",
                  sortValue: (ev) => ev.idAuditoriaSensor,
                  cellClassName: "font-mono",
                  cell: (ev) => `#${ev.idAuditoriaSensor}`,
                },
                {
                  key: "transicion",
                  header: "Transición de Estado",
                  sortValue: (ev) =>
                    LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ??
                    ev.estadoNuevoSensor,
                  cellClassName: "font-mono text-[11px]",
                  cell: (ev) => (
                    <>
                      <span className="text-foreground-secondary">
                        {LABEL_ESTADO_SENSOR[ev.estadoPrevioSensor] ??
                          ev.estadoPrevioSensor}
                      </span>{" "}
                      →{" "}
                      <span className="font-bold text-foreground">
                        {LABEL_ESTADO_SENSOR[ev.estadoNuevoSensor] ??
                          ev.estadoNuevoSensor}
                      </span>
                    </>
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
                      {LABEL_ESTADO_ENVIO[ev.estadoReporte] ?? ev.estadoReporte}
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
                        header: "Acciones",
                        headerClassName: "text-right",
                        cellClassName: "text-right",
                        stopRowClick: true,
                        cell: (ev) => (
                          <button
                            className="btn btn-icon btn-error"
                            onClick={() =>
                              void eliminarAudSensor(ev.idAuditoriaSensor)
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
              emptyMessage="Sin eventos registrados para el sensor seleccionado."
            />
          </div>
        </div>
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 3: AUDITORÍA DEL SISTEMA Y ARRANQUES                      */}
      {/* ================================================================= */}
      {tab === "SISTEMA" && (
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
            <div>
              <h2 className="text-sm font-bold">
                Historial de Arranques BIOS, Heap, MicroSD y Bus 1-Wire (
                {filasSistema.length} cargados)
              </h2>
              <p className="text-[11px] text-foreground-secondary">
                Scroll infinito · Los avisos SMTP pasan solos de PENDIENTE /
                FALLIDO a ENVIADO
              </p>
            </div>

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
                type="button"
                className="btn btn-outline px-2.5 py-1"
                onClick={() => void cargarSistema(true)}
                title="Recargar"
              >
                <RefreshCw className="h-3.5 w-3.5" />
              </button>
              <button
                type="button"
                className="btn btn-outline btn-success px-2.5 py-1"
                disabled={filasSistema.length === 0}
                onClick={() => void exportarSistemaExcel()}
                title="Exportar auditoría de sistema a Excel (.xlsx)"
              >
                <FileSpreadsheet className="h-3.5 w-3.5" />
                Excel
              </button>
            </div>
          </div>

          <DataTable
            data={filasSistema}
            rowKey={(row) => row.idAuditoriaSistema}
            infiniteScroll
            maxBodyHeightClass="max-h-[460px]"
            hasMore={hayMasSistema}
            loadingMore={cargandoMasSistema}
            onLoadMore={() => void cargarSistema(false)}
            dateFilter={{
              label: "Fecha",
              getEpochSeg: (r) => parsearFechaTextoAEpochSeg(r.fechaCreacion),
            }}
            searchPlaceholder="Buscar reset, detalle, FW..."
            searchValue={(row) =>
              `${row.resetReason} ${row.detalle} ${row.versionFirmware}`
            }
            filters={
              [
                {
                  key: "criticidad",
                  label: "Criticidad",
                  getValue: (r) => r.criticidad,
                  options: Object.entries(LABEL_CRITICIDAD).map(
                    ([value, label]) => ({ value, label })
                  ),
                },
                {
                  key: "reporte",
                  label: "Correo",
                  getValue: (r) => r.estadoReporte,
                  options: Object.entries(LABEL_ESTADO_ENVIO).map(
                    ([value, label]) => ({ value, label })
                  ),
                },
                {
                  key: "internet",
                  label: "Red",
                  getValue: (r) => (r.internet ? "ONLINE" : "OFFLINE"),
                  options: [
                    { value: "ONLINE", label: "Online" },
                    { value: "OFFLINE", label: "Offline" },
                  ],
                },
              ] as DataTableFilter<AuditoriaSistemaRowDTO>[]
            }
            columns={[
              {
                key: "id",
                header: "Boot / ID",
                sortValue: (r) => r.idAuditoriaSistema,
                cellClassName: "font-mono",
                cell: (r) => (
                  <>
                    <div className="font-bold">Boot #{r.bootId}</div>
                    <div className="text-[10px] text-foreground-secondary">
                      ID #{r.idAuditoriaSistema}
                    </div>
                  </>
                ),
              },
              {
                key: "reset",
                header: "Motivo Reset / FW",
                sortValue: (r) => r.resetReason,
                cell: (r) => (
                  <>
                    <div className="font-mono font-semibold">
                      {r.resetReason || "—"}
                    </div>
                    <div className="text-[10px] text-foreground-secondary">
                      FW {r.versionFirmware} · Uptime{" "}
                      {formatearDuracionSegundos(r.uptimeS)}
                    </div>
                  </>
                ),
              },
              {
                key: "recursos",
                header: "Heap / SD",
                sortValue: (r) => r.heapLibreBytes,
                cellClassName: "font-mono text-[11px]",
                cell: (r) => (
                  <>
                    <div>Heap: {formatearBytes(r.heapLibreBytes)}</div>
                    <div className="text-foreground-secondary">
                      SD libre: {r.espacioSdLibreKb} KB
                    </div>
                  </>
                ),
              },
              {
                key: "bus",
                header: "Bus / Tiempo / Red",
                sortValue: (r) => LABEL_ESTADO_BUS[r.estadoBus] ?? r.estadoBus,
                cellClassName: "text-[11px]",
                cell: (r) => (
                  <>
                    <div>
                      Bus:{" "}
                      <strong>
                        {LABEL_ESTADO_BUS[r.estadoBus] ?? r.estadoBus}
                      </strong>
                    </div>
                    <div className="text-foreground-secondary">
                      Reloj:{" "}
                      {LABEL_FUENTE_TIEMPO[r.fuenteTiempo] ?? r.fuenteTiempo} ·{" "}
                      {r.internet ? "Online" : "Offline"}
                    </div>
                  </>
                ),
              },
              {
                key: "detalle",
                header: "Detalle",
                sortValue: (r) => r.criticidad,
                cellClassName: "max-w-xs",
                cell: (r) => (
                  <>
                    <span
                      className={`mr-1.5 rounded px-1.5 py-0.5 text-[10px] font-semibold ${badgeCriticidad(
                        r.criticidad
                      )}`}
                    >
                      {LABEL_CRITICIDAD[r.criticidad] ?? r.criticidad}
                    </span>
                    <span className="text-foreground">{r.detalle}</span>
                  </>
                ),
              },
              {
                key: "reporte",
                header: "Correo Autónomo",
                sortValue: (r) => r.estadoReporte,
                cell: (r) => (
                  <span
                    className={`rounded px-2 py-0.5 text-[10px] font-semibold ${badgeEstadoCorreo(
                      r.estadoReporte
                    )}`}
                  >
                    {LABEL_ESTADO_ENVIO[r.estadoReporte] ?? r.estadoReporte}
                  </span>
                ),
              },
              {
                key: "fecha",
                header: "Fecha",
                sortValue: (r) => r.fechaCreacion,
                cellClassName: "font-mono text-foreground-secondary",
                cell: (r) => r.fechaCreacion || "—",
              },
              ...(esAdmin
                ? ([
                    {
                      key: "eliminar",
                      header: "Eliminar",
                      headerClassName: "text-right",
                      cellClassName: "text-right",
                      stopRowClick: true,
                      cell: (r) => (
                        <button
                          className="btn btn-icon btn-error"
                          onClick={() =>
                            void eliminarAudSistema(r.idAuditoriaSistema)
                          }
                          title="Eliminar registro"
                        >
                          <Trash2 className="h-3.5 w-3.5" />
                        </button>
                      ),
                    },
                  ] as DataTableColumn<AuditoriaSistemaRowDTO>[])
                : []),
            ]}
            emptyMessage="Sin registros de auditoría del sistema."
          />
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
                <span className="text-foreground-secondary">
                  Boot / Uptime:
                </span>
                <div className="font-mono">
                  Boot #{detalleAuth.bootId} ·{" "}
                  {formatearDuracionSegundos(detalleAuth.uptimeS)}
                </div>
              </div>
              <div>
                <span className="text-foreground-secondary">
                  Fuente Tiempo:
                </span>
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