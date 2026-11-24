import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  ArrowDown,
  ArrowUp,
  ArrowUpDown,
  Calendar,
  ChevronLeft,
  ChevronRight,
  Filter,
  Loader2,
  Search,
  X,
} from "lucide-react";
import { inputLocalAEpochSegundos } from "../../lib/format";

export interface DataTableColumn<T> {
  key: string;
  header: React.ReactNode;
  cell: (row: T) => React.ReactNode;
  sortValue?: (row: T) => string | number | boolean | null | undefined;
  headerClassName?: string;
  cellClassName?: string;
  stopRowClick?: boolean;
}

export interface DataTableFilterOption {
  value: string;
  label: string;
}

export interface DataTableFilter<T> {
  key: string;
  label: string;
  getValue: (row: T) => string | number | boolean | null | undefined;
  options: DataTableFilterOption[];
}

export interface DataTableDateFilter<T> {
  label?: string;
  /** Devuelve epoch en segundos (o ms, se normaliza) de la fila */
  getEpochSeg: (row: T) => number;
}

export interface DataTableProps<T> {
  data: T[];
  columns: DataTableColumn<T>[];
  rowKey: (row: T, index: number) => string | number;
  filters?: DataTableFilter<T>[];
  dateFilter?: DataTableDateFilter<T>;
  searchPlaceholder?: string;
  searchValue?: (row: T) => string;
  defaultSort?: { key: string; dir: "asc" | "desc" };
  onRowClick?: (row: T) => void;
  rowClassName?: (row: T) => string;
  emptyMessage?: string;
  /** Paginación local opcional (si no se usa scroll infinito) */
  pageSize?: number;
  /** Scroll infinito / carga incremental opcional contra el ESP32 */
  infiniteScroll?: boolean;
  onLoadMore?: () => void;
  hasMore?: boolean;
  loadingMore?: boolean;
  maxBodyHeightClass?: string;
}

function normalizarSeg(v: number): number {
  if (!Number.isFinite(v) || v <= 0) return 0;
  return v >= 10_000_000_000 ? Math.floor(v / 1000) : v;
}

function compararValores(
  a: string | number | boolean | null | undefined,
  b: string | number | boolean | null | undefined
): number {
  const aVacio = a === null || a === undefined || a === "";
  const bVacio = b === null || b === undefined || b === "";
  if (aVacio && bVacio) return 0;
  if (aVacio) return 1;
  if (bVacio) return -1;

  if (typeof a === "number" && typeof b === "number") {
    return a - b;
  }
  if (typeof a === "boolean" && typeof b === "boolean") {
    return Number(a) - Number(b);
  }
  return String(a).localeCompare(String(b), "es", {
    numeric: true,
    sensitivity: "base",
  });
}

export function DataTable<T>({
  data,
  columns,
  rowKey,
  filters = [],
  dateFilter,
  searchPlaceholder,
  searchValue,
  defaultSort,
  onRowClick,
  rowClassName,
  emptyMessage = "Sin registros para mostrar.",
  pageSize,
  infiniteScroll = false,
  onLoadMore,
  hasMore = false,
  loadingMore = false,
  maxBodyHeightClass,
}: DataTableProps<T>) {
  const [sortKey, setSortKey] = useState<string>(defaultSort?.key ?? "");
  const [sortDir, setSortDir] = useState<"asc" | "desc" | null>(
    defaultSort?.dir ?? null
  );
  const [busqueda, setBusqueda] = useState<string>("");
  const [filtrosActivos, setFiltrosActivos] = useState<Record<string, string>>(
    {}
  );
  const [fechaDesde, setFechaDesde] = useState<string>("");
  const [fechaHasta, setFechaHasta] = useState<string>("");
  const [paginaLocal, setPaginaLocal] = useState<number>(1);
  const [visiblesScroll, setVisiblesScroll] = useState<number>(pageSize ?? 30);

  const sentinelaRef = useRef<HTMLDivElement | null>(null);
  const scrollContainerRef = useRef<HTMLDivElement | null>(null);

  const alternarOrden = (key: string) => {
    if (sortKey !== key) {
      setSortKey(key);
      setSortDir("asc");
    } else if (sortDir === "asc") {
      setSortDir("desc");
    } else {
      setSortKey("");
      setSortDir(null);
    }
  };

  const hayFiltrosBarra =
    Boolean(searchValue) || filters.length > 0 || Boolean(dateFilter);
  const hayFiltroAplicado =
    busqueda.trim().length > 0 ||
    fechaDesde !== "" ||
    fechaHasta !== "" ||
    Object.values(filtrosActivos).some((v) => v !== "");

  const limpiarFiltros = () => {
    setBusqueda("");
    setFiltrosActivos({});
    setFechaDesde("");
    setFechaHasta("");
    setPaginaLocal(1);
  };

  const filasProcesadas = useMemo(() => {
    const q = busqueda.trim().toLowerCase();
    const desdeSeg = fechaDesde ? inputLocalAEpochSegundos(fechaDesde, false) : 0;
    const hastaSeg = fechaHasta ? inputLocalAEpochSegundos(fechaHasta, true) : 0;

    const filtradas = data.filter((row) => {
      if (q && searchValue) {
        if (!searchValue(row).toLowerCase().includes(q)) return false;
      }
      for (const f of filters) {
        const seleccionado = filtrosActivos[f.key];
        if (!seleccionado) continue;
        const val = f.getValue(row);
        if (String(val ?? "") !== seleccionado) return false;
      }
      if (dateFilter && (desdeSeg > 0 || hastaSeg > 0)) {
        const ts = normalizarSeg(dateFilter.getEpochSeg(row));
        if (desdeSeg > 0 && ts < desdeSeg) return false;
        if (hastaSeg > 0 && ts > hastaSeg) return false;
      }
      return true;
    });

    if (!sortKey || !sortDir) return filtradas;
    const col = columns.find((c) => c.key === sortKey);
    if (!col?.sortValue) return filtradas;

    const getVal = col.sortValue;
    return [...filtradas].sort((a, b) => {
      const cmp = compararValores(getVal(a), getVal(b));
      return sortDir === "asc" ? cmp : -cmp;
    });
  }, [
    data,
    busqueda,
    searchValue,
    filters,
    filtrosActivos,
    dateFilter,
    fechaDesde,
    fechaHasta,
    sortKey,
    sortDir,
    columns,
  ]);

  useEffect(() => {
    setPaginaLocal(1);
    setVisiblesScroll(pageSize ?? 30);
  }, [busqueda, filtrosActivos, fechaDesde, fechaHasta, pageSize]);

  // Scroll infinito: expande filas locales o pide el siguiente bloque remoto
  useEffect(() => {
    if (!infiniteScroll && !onLoadMore) return;
    const el = sentinelaRef.current;
    if (!el) return;

    const obs = new IntersectionObserver(
      (entries) => {
        if (!entries[0]?.isIntersecting) return;
        if (visiblesScroll < filasProcesadas.length) {
          setVisiblesScroll((v) => v + (pageSize ?? 25));
        } else if (onLoadMore && hasMore && !loadingMore) {
          onLoadMore();
        }
      },
      {
        root: maxBodyHeightClass ? scrollContainerRef.current : null,
        rootMargin: "120px",
      }
    );

    obs.observe(el);
    return () => obs.disconnect();
  }, [
    infiniteScroll,
    onLoadMore,
    hasMore,
    loadingMore,
    visiblesScroll,
    filasProcesadas.length,
    pageSize,
    maxBodyHeightClass,
  ]);

  const usaPaginacionLocal = Boolean(pageSize) && !infiniteScroll && !onLoadMore;
  const totalPaginas = usaPaginacionLocal
    ? Math.max(1, Math.ceil(filasProcesadas.length / (pageSize ?? 20)))
    : 1;
  const paginaSegura = Math.min(paginaLocal, totalPaginas);

  const filasVisibles = useMemo(() => {
    if (usaPaginacionLocal && pageSize) {
      const ini = (paginaSegura - 1) * pageSize;
      return filasProcesadas.slice(ini, ini + pageSize);
    }
    if (infiniteScroll) {
      return filasProcesadas.slice(0, visiblesScroll);
    }
    return filasProcesadas;
  }, [
    filasProcesadas,
    usaPaginacionLocal,
    pageSize,
    paginaSegura,
    infiniteScroll,
    visiblesScroll,
  ]);

  return (
    <div className="space-y-2.5">
      {hayFiltrosBarra && (
        <div className="flex flex-wrap items-center gap-2 rounded-lg border border-border bg-background/70 px-3 py-2 text-xs">
          <span className="inline-flex items-center gap-1 font-semibold text-foreground-secondary">
            <Filter className="h-3.5 w-3.5" />
            Filtros:
          </span>

          {searchValue && (
            <div className="relative">
              <Search className="pointer-events-none absolute top-1/2 left-2 h-3.5 w-3.5 -translate-y-1/2 text-foreground-secondary" />
              <input
                type="text"
                value={busqueda}
                onChange={(e) => setBusqueda(e.target.value)}
                placeholder={searchPlaceholder || "Buscar..."}
                className="w-48 rounded border border-border bg-card py-1 pr-2 pl-7 text-xs"
              />
            </div>
          )}

          {filters.map((f) => (
            <select
              key={f.key}
              value={filtrosActivos[f.key] ?? ""}
              onChange={(e) =>
                setFiltrosActivos((prev) => ({
                  ...prev,
                  [f.key]: e.target.value,
                }))
              }
              aria-label={f.label}
              className="rounded border border-border bg-card px-2 py-1 text-xs"
            >
              <option value="">{f.label}: Todos</option>
              {f.options.map((opt) => (
                <option key={opt.value} value={opt.value}>
                  {opt.label}
                </option>
              ))}
            </select>
          ))}

          {dateFilter && (
            <div className="inline-flex flex-wrap items-center gap-1.5 rounded border border-border bg-card px-2 py-0.5 text-[11px]">
              <Calendar className="h-3 w-3 text-foreground-secondary" />
              <span className="text-foreground-secondary">
                {dateFilter.label || "Fecha"}:
              </span>
              <input
                type="datetime-local"
                value={fechaDesde}
                onChange={(e) => setFechaDesde(e.target.value)}
                title="Desde esta fecha en adelante (dejar Hasta vacío si es abierto)"
                className="bg-transparent font-mono text-[11px] focus:outline-none"
              />
              <span className="text-foreground-secondary">→</span>
              <input
                type="datetime-local"
                value={fechaHasta}
                onChange={(e) => setFechaHasta(e.target.value)}
                title="Hasta esta fecha hacia atrás (dejar Desde vacío si es abierto)"
                className="bg-transparent font-mono text-[11px] focus:outline-none"
              />
            </div>
          )}

          {hayFiltroAplicado && (
            <button
              type="button"
              onClick={limpiarFiltros}
              className="inline-flex items-center gap-1 rounded border border-border bg-secondary px-2 py-1 text-[11px] font-semibold hover:bg-hover"
            >
              <X className="h-3 w-3" />
              Limpiar
            </button>
          )}

          <span className="ml-auto font-mono text-[11px] text-foreground-secondary">
            {filasProcesadas.length} / {data.length} filas
          </span>
        </div>
      )}

      <div
        ref={scrollContainerRef}
        className={`overflow-x-auto ${maxBodyHeightClass ? `overflow-y-auto ${maxBodyHeightClass}` : ""}`}
      >
        <table className="w-full text-left text-xs">
          <thead className="sticky top-0 z-10 border-b border-border bg-secondary/90 text-foreground-secondary uppercase backdrop-blur-xs">
            <tr>
              {columns.map((col) => {
                const ordenable = Boolean(col.sortValue);
                const activo = sortKey === col.key && sortDir !== null;
                return (
                  <th
                    key={col.key}
                    className={`px-2.5 py-2 select-none ${col.headerClassName ?? ""}`}
                  >
                    {ordenable ? (
                      <button
                        type="button"
                        onClick={() => alternarOrden(col.key)}
                        className={`inline-flex cursor-pointer items-center gap-1 uppercase transition-colors hover:text-foreground ${
                          activo ? "font-bold text-primary" : ""
                        }`}
                      >
                        <span>{col.header}</span>
                        {activo ? (
                          sortDir === "asc" ? (
                            <ArrowUp className="h-3 w-3 shrink-0" />
                          ) : (
                            <ArrowDown className="h-3 w-3 shrink-0" />
                          )
                        ) : (
                          <ArrowUpDown className="h-3 w-3 shrink-0 opacity-40" />
                        )}
                      </button>
                    ) : (
                      col.header
                    )}
                  </th>
                );
              })}
            </tr>
          </thead>
          <tbody className="divide-y divide-border">
            {filasVisibles.map((row, idx) => {
              const customRowClass = rowClassName ? rowClassName(row) : "";
              return (
                <tr
                  key={rowKey(row, idx)}
                  onClick={onRowClick ? () => onRowClick(row) : undefined}
                  className={`${
                    onRowClick ? "cursor-pointer" : ""
                  } transition-colors ${customRowClass || "hover:bg-secondary/40"}`}
                >
                  {columns.map((col) => (
                    <td
                      key={col.key}
                      onClick={
                        col.stopRowClick ? (e) => e.stopPropagation() : undefined
                      }
                      className={`px-2.5 py-2 ${col.cellClassName ?? ""}`}
                    >
                      {col.cell(row)}
                    </td>
                  ))}
                </tr>
              );
            })}

            {filasVisibles.length === 0 && (
              <tr>
                <td
                  colSpan={columns.length}
                  className="px-3 py-6 text-center text-foreground-secondary"
                >
                  {hayFiltroAplicado
                    ? "Ningún registro coincide con los filtros seleccionados."
                    : emptyMessage}
                </td>
              </tr>
            )}
          </tbody>
        </table>

        {(infiniteScroll || onLoadMore) && (
          <div ref={sentinelaRef} className="flex items-center justify-center py-2.5">
            {loadingMore ? (
              <span className="inline-flex items-center gap-1.5 text-xs text-foreground-secondary">
                <Loader2 className="h-3.5 w-3.5 animate-spin text-primary" />
                Cargando más registros desde la MicroSD...
              </span>
            ) : visiblesScroll < filasProcesadas.length ? (
              <button
                type="button"
                onClick={() => setVisiblesScroll((v) => v + (pageSize ?? 25))}
                className="btn btn-outline px-3 py-1 text-xs"
              >
                Mostrar más ({filasProcesadas.length - visiblesScroll} pendientes)
              </button>
            ) : onLoadMore && hasMore ? (
              <button
                type="button"
                onClick={onLoadMore}
                className="btn btn-outline px-3 py-1 text-xs"
              >
                Cargar más registros antiguos
              </button>
            ) : filasProcesadas.length > 0 ? (
              <span className="text-[11px] text-foreground-secondary">
                Fin del listado ({filasProcesadas.length} registros cargados)
              </span>
            ) : null}
          </div>
        )}
      </div>

      {usaPaginacionLocal && totalPaginas > 1 && (
        <div className="flex items-center justify-end gap-2 text-xs">
          <button
            type="button"
            className="btn btn-outline px-2 py-1"
            disabled={paginaSegura <= 1}
            onClick={() => setPaginaLocal((p) => Math.max(1, p - 1))}
          >
            <ChevronLeft className="h-3.5 w-3.5" />
          </button>
          <span className="font-mono text-[11px] text-foreground-secondary">
            Página {paginaSegura} / {totalPaginas}
          </span>
          <button
            type="button"
            className="btn btn-outline px-2 py-1"
            disabled={paginaSegura >= totalPaginas}
            onClick={() => setPaginaLocal((p) => Math.min(totalPaginas, p + 1))}
          >
            <ChevronRight className="h-3.5 w-3.5" />
          </button>
        </div>
      )}
    </div>
  );
}