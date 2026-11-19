import React, { useCallback, useEffect, useMemo, useState } from "react";
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
  { label: "SELECT Sensor", sql: "SELECT * FROM Sensor LIMIT 10;" },
  {
    label: "SELECT Senso (últimos 12)",
    sql: "SELECT id_senso, id_sensor, temperatura_cruda, temperatura_corregida, estado_sensor FROM Senso ORDER BY id_senso DESC LIMIT 12;",
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
    label: "SELECT Prueba",
    sql: "SELECT * FROM LaboratorioPrueba;",
  },
  {
    label: "ALTER TABLE Prueba",
    sql: "ALTER TABLE LaboratorioPrueba ADD COLUMN observacion VARCHAR(48);",
  },
  {
    label: "DROP TABLE Prueba",
    sql: "DROP TABLE IF EXISTS LaboratorioPrueba;",
  },
];

// Geometría determinística para el grafo visual ER sin dependencias externas
const ER_COLS = 3;
const CARD_W = 236;
const CARD_H = 104;
const GAP_X = 64;
const GAP_Y = 54;
const PAD = 24;

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
  const [cargandoColumnas, setCargandoColumnas] = useState(false);

  // Estado Terminal SQL (Solo ADMIN)
  const [sqlInput, setSqlInput] = useState<string>("SHOW TABLES;");
  const [ejecutandoSql, setEjecutandoSql] = useState(false);
  const [resultadoSql, setResultadoSql] =
    useState<ResultadoSqlTerminalDTO | null>(null);
  const [historialComandos, setHistorialComandos] = useState<string[]>([]);

  const cargarEstructura = useCallback(async () => {
    const est = await safeRpc(
      () => client.obtenerEstructuraBaseDatos(),
      "obtenerEstructuraBaseDatos"
    );
    if (est) {
      setEstructura(est);
      if (
        est.tablas.length > 0 &&
        (!tablaSeleccionada ||
          !est.tablas.some((t) => t.nombreTabla === tablaSeleccionada))
      ) {
        setTablaSeleccionada(est.tablas[0].nombreTabla);
      }
    }
  }, [tablaSeleccionada]);

  const cargarColumnas = useCallback(async (nombreTabla: string) => {
    if (!nombreTabla) {
      setColumnas([]);
      return;
    }
    setCargandoColumnas(true);
    const cols = await safeRpc(
      () => client.obtenerColumnasTablaEstructura(nombreTabla),
      "obtenerColumnasTablaEstructura"
    );
    setCargandoColumnas(false);
    if (cols) setColumnas(cols);
  }, []);

  useEffect(() => {
    cargarEstructura();
  }, [cargarEstructura]);

  useEffect(() => {
    if (tablaSeleccionada) {
      cargarColumnas(tablaSeleccionada);
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
          await cargarEstructura();
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

  // Cálculo de coordenadas del Grafo ER
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
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-light text-primary">
            <Database className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-lg font-bold">
              SGBD PostgreSQL Embebido · Esquema Relacional y Terminal SQL
            </h1>
            <p className="text-xs text-foreground-secondary">
              {estructura
                ? `${estructura.totalTablas} tablas · ${estructura.totalRelacionesFk} claves foráneas · ${estructura.totalEnums} tipos ENUM en MicroSD`
                : "Cargando catálogo relacional desde el ESP32..."}
            </p>
          </div>
        </div>

        <button
          type="button"
          className="btn btn-outline px-3 py-1.5 text-xs"
          onClick={() => {
            void cargarEstructura();
            if (tablaSeleccionada) void cargarColumnas(tablaSeleccionada);
          }}
        >
          <RefreshCw className="h-3.5 w-3.5" />
          Sincronizar Esquema
        </button>
      </div>

      {/* Sub-vista 1: Grafo ER Interactivo + Inspector de Columnas */}
      <div className="grid grid-cols-1 gap-4 xl:grid-cols-12">
        {/* Lienzo Visual ER (8 cols) */}
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-8">
          <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
            <div className="flex items-center gap-2">
              <Layers className="h-4 w-4 text-primary" />
              <h2 className="text-sm font-bold">
                Diagrama Entidad-Relación (Haga clic en una tabla para
                inspeccionar)
              </h2>
            </div>
            <div className="flex items-center gap-3 text-[11px] text-foreground-secondary">
              <span className="inline-flex items-center gap-1">
                <span className="h-2.5 w-2.5 rounded-xs bg-primary" /> Tabla
                seleccionada
              </span>
              <span className="inline-flex items-center gap-1">
                <span className="h-2.5 w-2.5 rounded-xs bg-info" /> Relación FK
                vinculada
              </span>
            </div>
          </div>

          <div className="overflow-auto rounded-lg border border-border bg-background">
            {estructura && estructura.tablas.length > 0 ? (
              <div
                className="relative"
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
                      markerWidth="8"
                      markerHeight="8"
                      refX="7"
                      refY="4"
                      orient="auto"
                    >
                      <path
                        d="M0,1 L7,4 L0,7 Z"
                        fill="currentColor"
                        className="text-foreground-secondary/60"
                      />
                    </marker>
                    <marker
                      id="fk-arrow-active"
                      markerWidth="9"
                      markerHeight="9"
                      refX="8"
                      refY="4.5"
                      orient="auto"
                    >
                      <path d="M0,1 L8,4.5 L0,8 Z" fill="var(--info)" />
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

                      // Auto-referencia (FK a la misma tabla)
                      if (fk.tablaOrigen === fk.tablaDestino) {
                        const sx = src.x + CARD_W;
                        const sy = src.y + 26;
                        const ex = src.x + CARD_W - 24;
                        const ey = src.y;
                        const d = `M ${sx} ${sy} C ${sx + 38} ${sy}, ${ex} ${ey - 38}, ${ex} ${ey}`;
                        return (
                          <path
                            key={`${fk.tablaOrigen}-${fk.columnaOrigen}-${idx}`}
                            d={d}
                            fill="none"
                            stroke={
                              vinculada ? "var(--info)" : "var(--border)"
                            }
                            strokeWidth={vinculada ? 2.2 : 1.4}
                            markerEnd={
                              vinculada
                                ? "url(#fk-arrow-active)"
                                : "url(#fk-arrow)"
                            }
                          />
                        );
                      }

                      // Puntos de anclaje en los bordes más cercanos
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
                        <g key={`${fk.tablaOrigen}-${fk.columnaOrigen}-${idx}`}>
                          <path
                            d={d}
                            fill="none"
                            stroke={
                              vinculada ? "var(--info)" : "var(--border)"
                            }
                            strokeWidth={vinculada ? 2.4 : 1.4}
                            strokeDasharray={vinculada ? undefined : "4 3"}
                            markerEnd={
                              vinculada
                                ? "url(#fk-arrow-active)"
                                : "url(#fk-arrow)"
                            }
                          />
                          {vinculada && (
                            <g transform={`translate(${mx}, ${my})`}>
                              <rect
                                x={-58}
                                y={-10}
                                width={116}
                                height={20}
                                rx={4}
                                fill="var(--card)"
                                stroke="var(--info)"
                                strokeWidth={1}
                              />
                              <text
                                x={0}
                                y={3}
                                textAnchor="middle"
                                fontSize={9}
                                fontFamily="monospace"
                                fill="var(--foreground)"
                              >
                                {fk.columnaOrigen} → {fk.columnaDestino}
                              </text>
                            </g>
                          )}
                        </g>
                      );
                    }
                  )}
                </svg>

                {/* Tarjetas de Tablas */}
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
                      className={`absolute z-10 flex flex-col justify-between rounded-lg border p-3 text-left transition-all ${
                        seleccionada
                          ? "border-primary bg-primary-light shadow-md ring-2 ring-primary/40"
                          : relacionada
                            ? "border-info bg-info-light/60 shadow-xs"
                            : "border-border bg-card hover:border-primary/50"
                      }`}
                    >
                      <div className="flex items-center justify-between gap-1.5">
                        <span className="flex items-center gap-1.5 truncate font-mono text-xs font-bold text-foreground">
                          <Table2
                            className={`h-3.5 w-3.5 shrink-0 ${
                              seleccionada ? "text-primary" : "text-info"
                            }`}
                          />
                          {t.nombreTabla}
                        </span>
                        <span className="shrink-0 rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px]">
                          {t.totalColumnas} cols
                        </span>
                      </div>

                      <div className="flex items-center gap-1.5 truncate rounded bg-background/80 px-2 py-1 font-mono text-[11px]">
                        <KeyRound className="h-3 w-3 shrink-0 text-warning" />
                        <span className="truncate">
                          {t.pkColumna || "sin PK"}
                        </span>
                      </div>

                      <div className="flex items-center justify-between text-[10px] text-foreground-secondary">
                        <span>Índices: {t.totalIndices}</span>
                        <span>
                          {seleccionada
                            ? "Inspeccionando"
                            : relacionada
                              ? "Vinculada por FK"
                              : "Ver columnas"}
                        </span>
                      </div>
                    </button>
                  );
                })}
              </div>
            ) : (
              <div className="p-8 text-center text-xs text-foreground-secondary">
                No se pudo cargar la estructura relacional del SGBD.
              </div>
            )}
          </div>

          {/* Lista compacta de Claves Foráneas */}
          {estructura && estructura.relacionesFk.length > 0 && (
            <div className="mt-3 flex flex-wrap items-center gap-2 text-[11px]">
              <span className="inline-flex items-center gap-1 font-semibold text-foreground-secondary">
                <Link2 className="h-3.5 w-3.5" />
                Claves Foráneas:
              </span>
              {estructura.relacionesFk.map((fk, idx) => (
                <button
                  key={idx}
                  type="button"
                  onClick={() => setTablaSeleccionada(fk.tablaOrigen)}
                  className={`rounded border px-2 py-0.5 font-mono transition-colors ${
                    fk.tablaOrigen === tablaSeleccionada ||
                    fk.tablaDestino === tablaSeleccionada
                      ? "border-info bg-info-light text-foreground font-semibold"
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

        {/* Inspector de Columnas de la Tabla Seleccionada (4 cols) */}
        <div className="flex flex-col rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs xl:col-span-4">
          <div className="mb-3 flex items-center justify-between border-b border-border pb-2.5">
            <div>
              <div className="text-[11px] font-semibold text-foreground-secondary uppercase">
                Inspector de Estructura
              </div>
              <h3 className="font-mono text-base font-bold text-primary">
                {tablaSeleccionada || "Seleccione una tabla"}
              </h3>
            </div>
            <span className="rounded bg-secondary px-2 py-0.5 font-mono text-xs">
              {columnas.length} columnas
            </span>
          </div>

          <div className="flex-1 space-y-2 overflow-auto">
            {cargandoColumnas ? (
              <div className="py-8 text-center text-xs text-foreground-secondary">
                Leyendo catálogo de columnas...
              </div>
            ) : columnas.length === 0 ? (
              <div className="py-8 text-center text-xs text-foreground-secondary">
                Sin columnas para mostrar.
              </div>
            ) : (
              columnas.map((col) => (
                <div
                  key={col.nombreColumna}
                  className="rounded-lg border border-border bg-background p-2.5 text-xs"
                >
                  <div className="flex items-center justify-between gap-2">
                    <span className="font-mono font-bold text-foreground">
                      {col.nombreColumna}
                    </span>
                    <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[11px] text-foreground-secondary">
                      {col.tipoSql}
                      {col.longitudMaxima > 0 ? `(${col.longitudMaxima})` : ""}
                    </span>
                  </div>

                  <div className="mt-1.5 flex flex-wrap gap-1">
                    {col.esPrimaryKey && (
                      <span className="rounded bg-warning-light px-1.5 py-0.5 font-mono text-[10px] font-bold text-warning">
                        PK
                      </span>
                    )}
                    {col.esForeignKey && (
                      <span className="rounded bg-info-light px-1.5 py-0.5 font-mono text-[10px] font-bold text-info">
                        FK → {col.fkTabla}.{col.fkColumna}
                      </span>
                    )}
                    {col.esIdentity && (
                      <span className="rounded bg-primary-light px-1.5 py-0.5 font-mono text-[10px] font-bold text-primary">
                        IDENTITY (AUTO)
                      </span>
                    )}
                    {col.esNotNull && (
                      <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px] text-foreground-secondary">
                        NOT NULL
                      </span>
                    )}
                    {col.esUnique && (
                      <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[10px] text-foreground-secondary">
                        UNIQUE
                      </span>
                    )}
                  </div>
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
                  Terminal SQL Interactiva PostgreSQL (Motor Embebido ESP32)
                </h2>
                <p className="text-xs text-foreground-secondary">
                  Ejecute sentencias DDL/DML/DQL directamente sobre la base de
                  datos en MicroSD · Atajo: <code>Ctrl + Enter</code>
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

          {/* Plantillas SQL Rápidas */}
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

          {/* Editor SQL */}
          <textarea
            rows={4}
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

          {/* Consola de Salida estilo psql */}
          {resultadoSql && (
            <div className="mt-4 overflow-hidden rounded-lg border border-border">
              <div
                className={`flex flex-wrap items-center justify-between gap-2 px-3 py-2 text-xs ${
                  resultadoSql.exito
                    ? "bg-alert-success-bg text-alert-success-icon border-b border-alert-success-border"
                    : "bg-alert-err-bg text-alert-err-icon border-b border-alert-err-border"
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

              <pre className="max-h-80 overflow-auto bg-zinc-950 p-3.5 font-mono text-xs leading-relaxed text-zinc-100 whitespace-pre">
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