import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Database,
  Play,
  RefreshCw,
  Table2,
  KeyRound,
  Link2,
  Terminal,
  CheckCircle2,
  XCircle,
  Clock,
  Sparkles,
  Layers,
  Loader2,
} from "lucide-react";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import {
  ColumnaEstructuraDTO,
  EstructuraCompletaDbDTO,
  NodoTablaEstructuraDTO,
  RelacionFkEstructuraDTO,
  ResultadoSqlTerminalDTO,
  RolUsuario,
} from "#types/rpc.types";

const SQL_PRESETS: Array<{ label: string; sql: string }> = [
  { label: "SHOW TABLES", sql: "SHOW TABLES;" },
  { label: "\\d Censo", sql: "\\d Censo;" },
  { label: "COUNT(*) Censo", sql: "SELECT COUNT(*) FROM Censo;" },
  { label: "SELECT Sensor", sql: "SELECT * FROM Sensor LIMIT 10;" },
  {
    label: "SELECT Censo (últimos 12)",
    sql: "SELECT id_censo, id_sensor, temperatura_cruda, temperatura_corregida, estado_sensor FROM Censo ORDER BY id_censo DESC LIMIT 12;",
  },
  {
    label: "SELECT Usuario",
    sql: "SELECT id_usuario, usuario, rol, intentos_fallidos, bloqueado FROM Usuario;",
  },
  {
    label: "CREATE TABLE Prueba",
    sql: "CREATE TABLE LaboratorioPrueba (id_prueba SERIAL PRIMARY KEY, nombre VARCHAR(32) NOT NULL, temp REAL);",
  },
  {
    label: "INSERT Prueba",
    sql: "INSERT INTO LaboratorioPrueba (nombre, temp) VALUES ('Heladera Vacunas', 4.25);",
  },
  {
    label: "DROP TABLE Prueba",
    sql: "DROP TABLE IF EXISTS LaboratorioPrueba;",
  },
];

// Geometría compacta: 4 columnas x 3 filas (~756x268 px) visible completa sin scroll
const ER_COLS = 4;
const CARD_W = 168;
const CARD_H = 68;
const GAP_X = 20;
const GAP_Y = 20;
const PAD = 12;

interface PosicionNodo {
  x: number;
  y: number;
  cx: number;
  cy: number;
}

export function DatabaseSqlView() {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [estructura, setEstructura] = useState<EstructuraCompletaDbDTO | null>(
    null
  );
  const [tablaSeleccionada, setTablaSeleccionada] = useState<string>("");
  const [columnas, setColumnas] = useState<ColumnaEstructuraDTO[]>([]);
  const [cargandoEstructura, setCargandoEstructura] = useState(false);
  const [cargandoColumnas, setCargandoColumnas] = useState(false);
  const colsReqRef = useRef(0);

  // Estado Terminal SQL (Solo ADMIN)
  const [sqlInput, setSqlInput] = useState<string>("SHOW TABLES;");
  const [ejecutandoSql, setEjecutandoSql] = useState(false);
  const [resultadoSql, setResultadoSql] =
    useState<ResultadoSqlTerminalDTO | null>(null);
  const [historialComandos, setHistorialComandos] = useState<string[]>([]);

  const cargarEstructura = useCallback(async (forzarSinCache = false) => {
    setCargandoEstructura(true);
    if (forzarSinCache) {
      await safeRpc(() => client.invalidarCache(), "invalidarCache", {
        silent: true,
      });
    }
    const est = await safeRpc(
      () => client.obtenerEstructuraBaseDatos(),
      "obtenerEstructuraBaseDatos"
    );
    setCargandoEstructura(false);
    if (est) {
      setEstructura(est);
      setTablaSeleccionada((prev) => {
        if (prev && est.tablas.some((t) => t.nombreTabla === prev)) return prev;
        return est.tablas.length > 0 ? est.tablas[0].nombreTabla : "";
      });
    }
  }, []);

  const cargarColumnas = useCallback(async (nombreTabla: string) => {
    const reqId = ++colsReqRef.current;
    if (!nombreTabla) {
      setColumnas([]);
      return;
    }
    setCargandoColumnas(true);
    const cols = await safeRpc(
      () => client.obtenerColumnasTablaEstructura(nombreTabla),
      "obtenerColumnasTablaEstructura"
    );
    if (reqId !== colsReqRef.current) return;
    setCargandoColumnas(false);
    if (cols) setColumnas(cols);
  }, []);

  useEffect(() => {
    void cargarEstructura(false);
  }, [cargarEstructura]);

  useEffect(() => {
    if (tablaSeleccionada) {
      void cargarColumnas(tablaSeleccionada);
    }
  }, [tablaSeleccionada, cargarColumnas]);

  const ejecutarConsulta = async () => {
    const query = sqlInput.trim();
    if (!query || !esAdmin) return;
    setEjecutandoSql(true);
    const res = await safeRpc(
      () => client.ejecutarSqlTerminal(query),
      "ejecutarSqlTerminal"
    );
    setEjecutandoSql(false);
    if (res) {
      setResultadoSql(res);
      setHistorialComandos((prev) =>
        [query, ...prev.filter((q) => q !== query)].slice(0, 10)
      );
      if (res.exito) {
        const cmdUpper = `${res.comandoEjecutado} ${query}`.toUpperCase();
        if (
          cmdUpper.includes("CREATE") ||
          cmdUpper.includes("ALTER") ||
          cmdUpper.includes("DROP")
        ) {
          await cargarEstructura(true);
          if (tablaSeleccionada) {
            await cargarColumnas(tablaSeleccionada);
          }
        }
      }
    }
  };

  const handleKeyDownSql = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
      e.preventDefault();
      void ejecutarConsulta();
    }
  };

  const layoutEr = useMemo(() => {
    const tablas = estructura?.tablas ?? [];
    const posiciones: Record<string, PosicionNodo> = {};
    tablas.forEach((t, idx) => {
      const col = idx % ER_COLS;
      const row = Math.floor(idx / ER_COLS);
      const x = PAD + col * (CARD_W + GAP_X);
      const y = PAD + row * (CARD_H + GAP_Y);
      posiciones[t.nombreTabla] = {
        x,
        y,
        cx: x + CARD_W / 2,
        cy: y + CARD_H / 2,
      };
    });
    const totalRows = Math.max(1, Math.ceil(tablas.length / ER_COLS));
    const width = PAD * 2 + ER_COLS * CARD_W + (ER_COLS - 1) * GAP_X;
    const height = PAD * 2 + totalRows * CARD_H + (totalRows - 1) * GAP_Y;
    return { posiciones, width, height };
  }, [estructura]);

  const relacionesActivas = useMemo(() => {
    if (!estructura || !tablaSeleccionada) return new Set<string>();
    const rel = new Set<string>();
    estructura.relacionesFk.forEach((fk) => {
      if (
        fk.tablaOrigen === tablaSeleccionada ||
        fk.tablaDestino === tablaSeleccionada
      ) {
        rel.add(fk.tablaOrigen);
        rel.add(fk.tablaDestino);
      }
    });
    return rel;
  }, [estructura, tablaSeleccionada]);

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado del Módulo */}
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-3.5 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-primary-light text-primary">
            <Database className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-base font-bold">
              SGBD PostgreSQL Embebido · Esquema Relacional y Terminal SQL
            </h1>
            <p className="text-xs text-foreground-secondary">
              {estructura
                ? `${estructura.totalTablas} tablas binarias (.pgb) · ${estructura.totalRelacionesFk} claves foráneas · ${estructura.totalEnums} tipos ENUM`
                : "Cargando catálogo relacional desde el ESP32..."}
            </p>
          </div>
        </div>

        <button
          type="button"
          className="btn btn-outline px-3 py-1.5 text-xs"
          disabled={cargandoEstructura}
          onClick={() => {
            void cargarEstructura(true);
            if (tablaSeleccionada) void cargarColumnas(tablaSeleccionada);
          }}
        >
          <RefreshCw
            className={`h-3.5 w-3.5 ${cargandoEstructura ? "animate-spin" : ""}`}
          />
          Sincronizar Esquema
        </button>
      </div>

      {/* Sub-vista 1: Grafo ER Compacto + Inspector Delgado */}
      <div className="grid grid-cols-1 gap-4 xl:grid-cols-12">
        {/* Lienzo Visual ER (8 cols) */}
        <div className="rounded-xl border border-border bg-card p-3.5 text-card-foreground shadow-xs xl:col-span-8">
          <div className="mb-2.5 flex flex-wrap items-center justify-between gap-2">
            <div className="flex items-center gap-2">
              <Layers className="h-4 w-4 text-primary" />
              <h2 className="text-sm font-bold">
                Diagrama Entidad-Relación (clic en una tabla)
              </h2>
            </div>
            <div className="flex items-center gap-3 text-[11px] text-foreground-secondary">
              <span className="inline-flex items-center gap-1">
                <span className="h-2 w-2 rounded-xs bg-primary" /> Seleccionada
              </span>
              <span className="inline-flex items-center gap-1">
                <span className="h-2 w-2 rounded-xs bg-info" /> Relación FK
              </span>
            </div>
          </div>

          <div className="overflow-x-auto rounded-lg border border-border bg-background">
            {estructura && estructura.tablas.length > 0 ? (
              <div
                className="relative mx-auto"
                style={{
                  width: `${layoutEr.width}px`,
                  height: `${layoutEr.height}px`,
                }}
              >
                {/* Capa SVG de Conectores FK */}
                <svg
                  width={layoutEr.width}
                  height={layoutEr.height}
                  className="pointer-events-none absolute inset-0"
                >
                  <defs>
                    <marker
                      id="fk-arrow"
                      markerWidth="7"
                      markerHeight="7"
                      refX="6"
                      refY="3.5"
                      orient="auto"
                    >
                      <path
                        d="M0,1 L6,3.5 L0,6 Z"
                        fill="currentColor"
                        className="text-foreground-secondary/60"
                      />
                    </marker>
                    <marker
                      id="fk-arrow-active"
                      markerWidth="8"
                      markerHeight="8"
                      refX="7"
                      refY="4"
                      orient="auto"
                    >
                      <path d="M0,1 L7,4 L0,7 Z" fill="var(--info)" />
                    </marker>
                  </defs>

                  {estructura.relacionesFk.map(
                    (fk: RelacionFkEstructuraDTO, idx: number) => {
                      const src = layoutEr.posiciones[fk.tablaOrigen];
                      const dst = layoutEr.posiciones[fk.tablaDestino];
                      if (!src || !dst) return null;

                      const vinculada =
                        fk.tablaOrigen === tablaSeleccionada ||
                        fk.tablaDestino === tablaSeleccionada;

                      const dx = dst.cx - src.cx;
                      const dy = dst.cy - src.cy;
                      let x1 = src.cx;
                      let y1 = src.cy;
                      let x2 = dst.cx;
                      let y2 = dst.cy;

                      if (Math.abs(dx) >= Math.abs(dy)) {
                        x1 = dx >= 0 ? src.x + CARD_W : src.x;
                        x2 = dx >= 0 ? dst.x : dst.x + CARD_W;
                      } else {
                        y1 = dy >= 0 ? src.y + CARD_H : src.y;
                        y2 = dy >= 0 ? dst.y : dst.y + CARD_H;
                      }

                      const mx = (x1 + x2) / 2;
                      const my = (y1 + y2) / 2;
                      const d =
                        Math.abs(dx) >= Math.abs(dy)
                          ? `M ${x1} ${y1} C ${mx} ${y1}, ${mx} ${y2}, ${x2} ${y2}`
                          : `M ${x1} ${y1} C ${x1} ${my}, ${x2} ${my}, ${x2} ${y2}`;

                      return (
                        <path
                          key={`${fk.tablaOrigen}-${fk.columnaOrigen}-${idx}`}
                          d={d}
                          fill="none"
                          stroke={vinculada ? "var(--info)" : "var(--border)"}
                          strokeWidth={vinculada ? 2.2 : 1.2}
                          strokeDasharray={vinculada ? undefined : "3 3"}
                          markerEnd={
                            vinculada
                              ? "url(#fk-arrow-active)"
                              : "url(#fk-arrow)"
                          }
                        />
                      );
                    }
                  )}
                </svg>

                {/* Tarjetas Compactas de Tablas */}
                {estructura.tablas.map((t: NodoTablaEstructuraDTO) => {
                  const pos = layoutEr.posiciones[t.nombreTabla];
                  if (!pos) return null;
                  const seleccionada = t.nombreTabla === tablaSeleccionada;
                  const relacionada =
                    !seleccionada && relacionesActivas.has(t.nombreTabla);

                  return (
                    <button
                      key={t.nombreTabla}
                      type="button"
                      onClick={() => setTablaSeleccionada(t.nombreTabla)}
                      style={{
                        left: `${pos.x}px`,
                        top: `${pos.y}px`,
                        width: `${CARD_W}px`,
                        height: `${CARD_H}px`,
                      }}
                      className={`absolute z-10 flex flex-col justify-between rounded-md border px-2 py-1.5 text-left transition-all ${
                        seleccionada
                          ? "border-primary bg-primary-light shadow-sm ring-1 ring-primary/50"
                          : relacionada
                            ? "border-info bg-info-light/60"
                            : "border-border bg-card hover:border-primary/50"
                      }`}
                    >
                      <div className="flex items-center justify-between gap-1">
                        <span className="flex items-center gap-1 truncate font-mono text-[11px] font-bold text-foreground">
                          <Table2
                            className={`h-3 w-3 shrink-0 ${
                              seleccionada ? "text-primary" : "text-info"
                            }`}
                          />
                          <span className="truncate">{t.nombreTabla}</span>
                        </span>
                        <span className="shrink-0 rounded bg-secondary px-1 py-0.2 font-mono text-[9px]">
                          {t.totalColumnas}c
                        </span>
                      </div>

                      <div className="flex items-center gap-1 truncate rounded bg-background/80 px-1.5 py-0.5 font-mono text-[10px]">
                        <KeyRound className="h-2.5 w-2.5 shrink-0 text-warning" />
                        <span className="truncate">
                          {t.pkColumna || "sin PK"}
                        </span>
                      </div>

                      <div className="flex items-center justify-between text-[9px] text-foreground-secondary">
                        <span>Idx: {t.totalIndices}</span>
                        <span>
                          {seleccionada
                            ? "Activa"
                            : relacionada
                              ? "FK"
                              : "Ver"}
                        </span>
                      </div>
                    </button>
                  );
                })}
              </div>
            ) : (
              <div className="flex items-center justify-center gap-2 p-8 text-xs text-foreground-secondary">
                {cargandoEstructura && (
                  <Loader2 className="h-4 w-4 animate-spin text-primary" />
                )}
                {cargandoEstructura
                  ? "Leyendo esquema pg_catalog..."
                  : "No se pudo cargar la estructura relacional del SGBD."}
              </div>
            )}
          </div>

          {/* Lista compacta de Claves Foráneas */}
          {estructura && estructura.relacionesFk.length > 0 && (
            <div className="mt-2.5 flex flex-wrap items-center gap-1.5 text-[10px]">
              <span className="inline-flex items-center gap-1 font-semibold text-foreground-secondary">
                <Link2 className="h-3 w-3" />
                FKs:
              </span>
              {estructura.relacionesFk.map((fk, idx) => (
                <button
                  key={idx}
                  type="button"
                  onClick={() => setTablaSeleccionada(fk.tablaOrigen)}
                  className={`rounded border px-1.5 py-0.5 font-mono transition-colors ${
                    fk.tablaOrigen === tablaSeleccionada ||
                    fk.tablaDestino === tablaSeleccionada
                      ? "border-info bg-info-light font-semibold text-foreground"
                      : "border-border bg-background text-foreground-secondary hover:text-foreground"
                  }`}
                >
                  {fk.tablaOrigen}.{fk.columnaOrigen} → {fk.tablaDestino}.
                  {fk.columnaDestino}
                </button>
              ))}
            </div>
          )}
        </div>

        {/* Inspector Delgado de Columnas con scroll temático (4 cols) */}
        <div className="flex flex-col rounded-xl border border-border bg-card p-3.5 text-card-foreground shadow-xs xl:col-span-4">
          <div className="mb-2 flex items-center justify-between border-b border-border pb-2">
            <div className="min-w-0">
              <div className="text-[10px] font-semibold text-foreground-secondary uppercase">
                Atributos de Tabla
              </div>
              <h3 className="truncate font-mono text-sm font-bold text-primary">
                {tablaSeleccionada || "Seleccione una tabla"}
              </h3>
            </div>
            <span className="inline-flex items-center gap-1 rounded bg-secondary px-2 py-0.5 font-mono text-[11px]">
              {cargandoColumnas && (
                <Loader2 className="h-3 w-3 animate-spin text-primary" />
              )}
              {columnas.length} cols
            </span>
          </div>

          <div className="max-h-[305px] flex-1 divide-y divide-border overflow-y-auto pr-1">
            {cargandoColumnas && columnas.length === 0 ? (
              <div className="flex items-center justify-center gap-2 py-8 text-xs text-foreground-secondary">
                <Loader2 className="h-4 w-4 animate-spin text-primary" />
                Leyendo columnas...
              </div>
            ) : columnas.length === 0 ? (
              <div className="py-8 text-center text-xs text-foreground-secondary">
                Sin columnas para mostrar.
              </div>
            ) : (
              columnas.map((col) => (
                <div
                  key={col.nombreColumna}
                  className="flex items-center justify-between gap-2 py-1.5 text-[11px] hover:bg-secondary/30"
                >
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-1.5">
                      <span className="truncate font-mono font-semibold text-foreground">
                        {col.nombreColumna}
                      </span>
                      {col.esPrimaryKey && (
                        <span className="rounded bg-warning-light px-1 font-mono text-[9px] font-bold text-warning">
                          PK
                        </span>
                      )}
                      {col.esForeignKey && (
                        <span
                          className="rounded bg-info-light px-1 font-mono text-[9px] font-bold text-info"
                          title={`FK → ${col.fkTabla}.${col.fkColumna}`}
                        >
                          FK→{col.fkTabla}
                        </span>
                      )}
                      {col.esIdentity && (
                        <span className="rounded bg-primary-light px-1 font-mono text-[9px] font-bold text-primary">
                          AUTO
                        </span>
                      )}
                      {col.esNotNull && !col.esPrimaryKey && (
                        <span className="font-mono text-[9px] text-foreground-secondary">
                          NN
                        </span>
                      )}
                      {col.esUnique && !col.esPrimaryKey && (
                        <span className="font-mono text-[9px] text-foreground-secondary">
                          UQ
                        </span>
                      )}
                    </div>
                  </div>
                  <span className="shrink-0 rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px] text-foreground-secondary">
                    {col.tipoSql}
                    {col.longitudMaxima > 0 ? `(${col.longitudMaxima})` : ""}
                  </span>
                </div>
              ))
            )}
          </div>
        </div>
      </div>

      {/* Sub-vista 2: Terminal SQL Interactiva PostgreSQL (Solo ADMIN) */}
      {esAdmin ? (
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
            <div className="flex items-center gap-2">
              <Terminal className="h-5 w-5 text-primary" />
              <div>
                <h2 className="text-base font-bold">
                  Terminal SQL Interactiva PostgreSQL (Motor Binario ESP32)
                </h2>
                <p className="text-xs text-foreground-secondary">
                  Ejecute sentencias DDL/DML/DQL sobre la MicroSD · Atajo:{" "}
                  <code>Ctrl + Enter</code>
                </p>
              </div>
            </div>

            <button
              type="button"
              onClick={() => void ejecutarConsulta()}
              disabled={ejecutandoSql || !sqlInput.trim()}
              className="btn btn-solid btn-primary px-4 py-2 text-xs font-bold"
            >
              {ejecutandoSql ? (
                <RefreshCw className="h-4 w-4 animate-spin" />
              ) : (
                <Play className="h-4 w-4" />
              )}
              Execute
            </button>
          </div>

          <div className="mb-3 flex flex-wrap items-center gap-1.5">
            <span className="inline-flex items-center gap-1 text-[11px] font-semibold text-foreground-secondary">
              <Sparkles className="h-3.5 w-3.5 text-warning" />
              Presets:
            </span>
            {SQL_PRESETS.map((preset) => (
              <button
                key={preset.label}
                type="button"
                onClick={() => setSqlInput(preset.sql)}
                className="rounded border border-border bg-secondary px-2 py-1 font-mono text-[11px] transition-colors hover:border-primary hover:bg-primary-light"
              >
                {preset.label}
              </button>
            ))}
          </div>

          <textarea
            rows={3}
            maxLength={383}
            value={sqlInput}
            onChange={(e) => setSqlInput(e.target.value)}
            onKeyDown={handleKeyDownSql}
            placeholder="Escriba una sentencia SQL (ej. SELECT * FROM Sensor;)"
            className="w-full rounded-lg border border-border bg-zinc-950 p-3 font-mono text-xs text-emerald-300 focus:border-primary focus:outline-none"
          />
          <div className="mt-1 flex items-center justify-between text-[11px] text-foreground-secondary">
            <span>
              {historialComandos.length > 0 && (
                <>
                  Último:{" "}
                  <button
                    type="button"
                    onClick={() => setSqlInput(historialComandos[0])}
                    className="font-mono underline hover:text-foreground"
                  >
                    {historialComandos[0].slice(0, 65)}
                    {historialComandos[0].length > 65 ? "..." : ""}
                  </button>
                </>
              )}
            </span>
            <span className="font-mono">{sqlInput.length} / 383 chars</span>
          </div>

          {resultadoSql && (
            <div className="mt-4 overflow-hidden rounded-lg border border-border">
              <div
                className={`flex flex-wrap items-center justify-between gap-2 px-3 py-2 text-xs ${
                  resultadoSql.exito
                    ? "border-b border-alert-success-border bg-alert-success-bg text-alert-success-icon"
                    : "border-b border-alert-err-border bg-alert-err-bg text-alert-err-icon"
                }`}
              >
                <div className="flex flex-wrap items-center gap-2 font-semibold">
                  {resultadoSql.exito ? (
                    <CheckCircle2 className="h-4 w-4" />
                  ) : (
                    <XCircle className="h-4 w-4" />
                  )}
                  <span>
                    {resultadoSql.exito ? "EJECUCIÓN EXITOSA" : "ERROR SQL"}
                  </span>
                  <span className="rounded bg-background/80 px-2 py-0.5 font-mono text-[11px] text-foreground">
                    CMD: {resultadoSql.comandoEjecutado || "SQL"}
                  </span>
                  <span className="rounded bg-background/80 px-2 py-0.5 font-mono text-[11px] text-foreground">
                    SQLSTATE: {resultadoSql.sqlState || "00000"}
                  </span>
                </div>

                <div className="flex items-center gap-3 font-mono text-[11px] text-foreground">
                  <span>Filas: {resultadoSql.filasAfectadas}</span>
                  <span className="inline-flex items-center gap-1">
                    <Clock className="h-3.5 w-3.5 text-primary" />
                    {resultadoSql.tiempoEjecucionUs} µs
                  </span>
                </div>
              </div>

              <pre className="max-h-72 overflow-auto bg-zinc-950 p-3.5 font-mono text-xs leading-relaxed text-zinc-100 whitespace-pre">
                {resultadoSql.salidaPsql || "(Sin salida textual)"}
              </pre>
            </div>
          )}
        </div>
      ) : (
        <div className="rounded-xl border border-border bg-card p-4 text-xs text-foreground-secondary">
          La Terminal SQL interactiva (<code>ejecutarSqlTerminal</code>) está
          restringida exclusivamente a usuarios con rol <strong>ADMIN</strong>.
        </div>
      )}
    </div>
  );
}