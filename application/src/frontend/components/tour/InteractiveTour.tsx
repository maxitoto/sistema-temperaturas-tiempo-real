import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  X,
  ChevronLeft,
  ChevronRight,
  CheckCircle2,
  Info,
  Target,
  AlertTriangle,
  MousePointerClick,
  RotateCcw,
  Sparkles,
} from "lucide-react";
import type { Vista } from "../layouts/SidelBar";
import { RolUsuario } from "#types/rpc.types";
import {
  NOMBRE_VISTA_TOUR,
  PasoTour,
  TOUR_POR_VISTA,
} from "./tourSteps";

const STORAGE_VISTOS = "untdf_tour_vistos_v1";
const STORAGE_AUTO = "untdf_tour_auto_v1";

function leerVistasVistas(): Set<string> {
  try {
    const raw = localStorage.getItem(STORAGE_VISTOS);
    if (!raw) return new Set();
    const arr = JSON.parse(raw);
    return Array.isArray(arr) ? new Set(arr.map(String)) : new Set();
  } catch {
    return new Set();
  }
}

function marcarVistaVista(vista: Vista): void {
  try {
    const set = leerVistasVistas();
    set.add(vista);
    localStorage.setItem(STORAGE_VISTOS, JSON.stringify(Array.from(set)));
  } catch {
    // Ignorar si storage no está disponible
  }
}

function reiniciarTodasLasGuias(): void {
  try {
    localStorage.removeItem(STORAGE_VISTOS);
    localStorage.setItem(STORAGE_AUTO, "true");
  } catch {
    // Ignorar
  }
}

function leerAutoActivo(): boolean {
  try {
    return localStorage.getItem(STORAGE_AUTO) !== "false";
  } catch {
    return true;
  }
}

function guardarAutoActivo(activo: boolean): void {
  try {
    localStorage.setItem(STORAGE_AUTO, activo ? "true" : "false");
  } catch {
    // Ignorar
  }
}

interface SpotlightRect {
  top: number;
  left: number;
  width: number;
  height: number;
}

export function InteractiveTour({
  vista,
  rol,
  triggerManual,
}: {
  vista: Vista;
  rol: RolUsuario;
  triggerManual: number;
}) {
  const [abierto, setAbierto] = useState(false);
  const [indice, setIndice] = useState(0);
  const [rect, setRect] = useState<SpotlightRect | null>(null);
  const [autoInicio, setAutoInicio] = useState<boolean>(() => leerAutoActivo());
  const ultimoTriggerRef = useRef<number>(triggerManual);

  const pasos: PasoTour[] = useMemo(() => {
    const todos = TOUR_POR_VISTA[vista] ?? [];
    return todos.filter((p) => !p.soloAdmin || rol === RolUsuario.ADMIN);
  }, [vista, rol]);

  const pasoActual = pasos[indice] ?? null;

  // 1. Lanzamiento automático la primera vez que se visita una ventana
  useEffect(() => {
    setAbierto(false);
    setIndice(0);
    setRect(null);

    if (pasos.length === 0 || !leerAutoActivo()) return;
    if (leerVistasVistas().has(vista)) return;

    const timer = window.setTimeout(() => {
      setIndice(0);
      setAbierto(true);
    }, 650);

    return () => window.clearTimeout(timer);
  }, [vista, pasos.length]);

  // 2. Lanzamiento manual desde el botón '?' en TopBar
  useEffect(() => {
    if (triggerManual === ultimoTriggerRef.current) return;
    ultimoTriggerRef.current = triggerManual;
    if (pasos.length === 0) return;
    setIndice(0);
    setAbierto(true);
  }, [triggerManual, pasos.length]);

  const cerrarTour = useCallback(() => {
    marcarVistaVista(vista);
    setAbierto(false);
  }, [vista]);

  const siguiente = useCallback(() => {
    if (indice + 1 >= pasos.length) {
      cerrarTour();
    } else {
      setIndice((i) => i + 1);
    }
  }, [indice, pasos.length, cerrarTour]);

  const anterior = useCallback(() => {
    setIndice((i) => Math.max(0, i - 1));
  }, []);

  // 3. Medición y seguimiento del elemento resaltado (spotlight)
  const actualizarRect = useCallback(() => {
    if (!abierto || !pasoActual?.selector) {
      setRect(null);
      return;
    }
    const el = document.querySelector(pasoActual.selector) as HTMLElement | null;
    if (!el) {
      setRect(null);
      return;
    }
    const r = el.getBoundingClientRect();
    if (r.width === 0 || r.height === 0) {
      setRect(null);
      return;
    }
    const pad = 6;
    setRect({
      top: Math.max(4, r.top - pad),
      left: Math.max(4, r.left - pad),
      width: Math.min(window.innerWidth - 8, r.width + pad * 2),
      height: Math.min(window.innerHeight - 8, r.height + pad * 2),
    });
  }, [abierto, pasoActual]);

  useEffect(() => {
    if (!abierto || !pasoActual) return;

    if (pasoActual.selector) {
      const el = document.querySelector(pasoActual.selector) as HTMLElement | null;
      if (el) {
        el.scrollIntoView({
          behavior: "smooth",
          block: "center",
          inline: "nearest",
        });
      }
    }

    actualizarRect();
    const t1 = window.setTimeout(actualizarRect, 220);
    const t2 = window.setTimeout(actualizarRect, 450);

    window.addEventListener("resize", actualizarRect);
    window.addEventListener("scroll", actualizarRect, true);

    return () => {
      window.clearTimeout(t1);
      window.clearTimeout(t2);
      window.removeEventListener("resize", actualizarRect);
      window.removeEventListener("scroll", actualizarRect, true);
    };
  }, [abierto, pasoActual, actualizarRect]);

  // 4. Navegación por teclado (Esc, flechas)
  useEffect(() => {
    if (!abierto) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        e.preventDefault();
        cerrarTour();
      } else if (e.key === "ArrowRight") {
        e.preventDefault();
        siguiente();
      } else if (e.key === "ArrowLeft") {
        e.preventDefault();
        anterior();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [abierto, cerrarTour, siguiente, anterior]);

  if (!abierto || !pasoActual) return null;

  // Cálculo de posición de la tarjeta para nunca quedar cortada fuera del viewport
  const cardW = Math.min(440, window.innerWidth - 24);
  const cardEstH = 385;
  let cardTop = Math.max(16, (window.innerHeight - cardEstH) / 2);
  let cardLeft = Math.max(12, (window.innerWidth - cardW) / 2);

  if (rect) {
    const espacioAbajo = window.innerHeight - (rect.top + rect.height);
    const espacioArriba = rect.top;
    const espacioDerecha = window.innerWidth - (rect.left + rect.width);
    const espacioIzquierda = rect.left;

    if (espacioAbajo >= cardEstH + 16) {
      cardTop = rect.top + rect.height + 12;
      cardLeft = rect.left + rect.width / 2 - cardW / 2;
    } else if (espacioArriba >= cardEstH + 16) {
      cardTop = rect.top - cardEstH - 12;
      cardLeft = rect.left + rect.width / 2 - cardW / 2;
    } else if (espacioDerecha >= cardW + 20) {
      cardLeft = rect.left + rect.width + 14;
      cardTop = rect.top + rect.height / 2 - cardEstH / 2;
    } else if (espacioIzquierda >= cardW + 20) {
      cardLeft = rect.left - cardW - 14;
      cardTop = rect.top + rect.height / 2 - cardEstH / 2;
    } else {
      // Si la sección ocupa casi toda la pantalla, fijamos en esquina inferior derecha
      cardTop = window.innerHeight - cardEstH - 16;
      cardLeft = window.innerWidth - cardW - 16;
    }

    cardLeft = Math.max(12, Math.min(window.innerWidth - cardW - 12, cardLeft));
    cardTop = Math.max(12, Math.min(window.innerHeight - cardEstH - 12, cardTop));
  }

  return (
    <div className="fixed inset-0 z-[70]">
      {/* Capa bloqueadora de clics + fondo oscurecido si no hay target en DOM */}
      <div
        className={`fixed inset-0 ${rect ? "bg-transparent" : "bg-black/60 backdrop-blur-[1px]"}`}
        onClick={cerrarTour}
      />

      {/* Marco Spotlight con sombra de recorte */}
      {rect && (
        <div
          className="pointer-events-none fixed rounded-xl ring-2 ring-primary transition-all duration-300"
          style={{
            top: `${rect.top}px`,
            left: `${rect.left}px`,
            width: `${rect.width}px`,
            height: `${rect.height}px`,
            boxShadow: "0 0 0 9999px rgba(0, 0, 0, 0.62)",
          }}
        />
      )}

      {/* Tarjeta explicativa estructurada */}
      <div
        role="dialog"
        aria-label={`Guía interactiva: ${pasoActual.titulo}`}
        style={{
          top: `${cardTop}px`,
          left: `${cardLeft}px`,
          width: `${cardW}px`,
        }}
        className="fixed z-[75] flex max-h-[calc(100vh-24px)] flex-col overflow-hidden rounded-xl border border-primary/50 bg-card text-card-foreground shadow-2xl transition-all duration-200"
      >
        {/* Cabecera */}
        <div className="flex items-start justify-between gap-2 border-b border-border bg-primary-light/60 px-4 py-3">
          <div className="min-w-0">
            <div className="flex items-center gap-1.5 text-[10px] font-bold tracking-wide text-primary uppercase">
              <Sparkles className="h-3 w-3" />
              <span>
                Guía · {NOMBRE_VISTA_TOUR[vista]} ({indice + 1}/{pasos.length})
              </span>
            </div>
            <h3 className="mt-0.5 text-sm font-bold text-foreground">
              {pasoActual.titulo}
            </h3>
            {pasoActual.subtitulo && (
              <div className="text-[11px] text-foreground-secondary">
                {pasoActual.subtitulo}
              </div>
            )}
          </div>
          <button
            type="button"
            onClick={cerrarTour}
            className="btn btn-icon btn-secondary shrink-0"
            title="Cerrar guía (Esc)"
          >
            <X className="h-4 w-4" />
          </button>
        </div>

        {/* 4 bloques de auto-ayuda */}
        <div className="flex-1 space-y-2.5 overflow-y-auto px-4 py-3 text-xs leading-relaxed">
          <div className="rounded-lg border border-border bg-background p-2.5">
            <div className="mb-0.5 flex items-center gap-1.5 font-bold text-foreground">
              <Info className="h-3.5 w-3.5 text-info shrink-0" />
              ¿Qué es?
            </div>
            <p className="text-xs text-foreground-secondary">
              {pasoActual.queEs}
            </p>
          </div>

          <div className="rounded-lg border border-border bg-background p-2.5">
            <div className="mb-0.5 flex items-center gap-1.5 font-bold text-foreground">
              <Target className="h-3.5 w-3.5 text-primary shrink-0" />
              ¿Para qué sirve?
            </div>
            <p className="text-xs text-foreground-secondary">
              {pasoActual.paraQueSirve}
            </p>
          </div>

          <div className="rounded-lg border border-border bg-background p-2.5">
            <div className="mb-0.5 flex items-center gap-1.5 font-bold text-foreground">
              <AlertTriangle className="h-3.5 w-3.5 text-warning shrink-0" />
              ¿En qué afecta?
            </div>
            <p className="text-xs text-foreground-secondary">
              {pasoActual.enQueAfecta}
            </p>
          </div>

          <div className="rounded-lg border border-border bg-background p-2.5">
            <div className="mb-0.5 flex items-center gap-1.5 font-bold text-foreground">
              <MousePointerClick className="h-3.5 w-3.5 text-success shrink-0" />
              ¿Cómo se usa?
            </div>
            <p className="text-xs text-foreground-secondary">
              {pasoActual.comoSeUsa}
            </p>
          </div>
        </div>

        {/* Pie con preferencia de auto-inicio y controles */}
        <div className="space-y-2 border-t border-border bg-secondary/40 px-4 py-2.5">
          <div className="flex items-center justify-between gap-2 text-[11px] text-foreground-secondary">
            <label className="flex cursor-pointer items-center gap-1.5">
              <input
                type="checkbox"
                checked={!autoInicio}
                onChange={(e) => {
                  const nuevoAuto = !e.target.checked;
                  setAutoInicio(nuevoAuto);
                  guardarAutoActivo(nuevoAuto);
                }}
              />
              No abrir automáticamente en ventanas nuevas
            </label>
            <button
              type="button"
              onClick={() => {
                reiniciarTodasLasGuias();
                setAutoInicio(true);
                setIndice(0);
              }}
              className="inline-flex items-center gap-1 hover:text-foreground"
              title="Volver a habilitar la guía automática en todas las ventanas"
            >
              <RotateCcw className="h-3 w-3" />
              Reset
            </button>
          </div>

          <div className="flex items-center justify-between gap-2">
            {/* Puntos de progreso */}
            <div className="flex items-center gap-1">
              {pasos.map((p, idx) => (
                <button
                  key={p.id}
                  type="button"
                  onClick={() => setIndice(idx)}
                  aria-label={`Ir al paso ${idx + 1}`}
                  className={`h-2 rounded-full transition-all ${
                    idx === indice
                      ? "w-5 bg-primary"
                      : "w-2 bg-border hover:bg-foreground-secondary"
                  }`}
                />
              ))}
            </div>

            <div className="flex items-center gap-1.5">
              <button
                type="button"
                disabled={indice === 0}
                onClick={anterior}
                className="btn btn-outline px-2.5 py-1 text-xs"
              >
                <ChevronLeft className="h-3.5 w-3.5" />
                Anterior
              </button>
              <button
                type="button"
                onClick={siguiente}
                className="btn btn-solid btn-primary px-3 py-1 text-xs font-bold"
              >
                {indice + 1 >= pasos.length ? (
                  <>
                    <CheckCircle2 className="h-3.5 w-3.5" />
                    Finalizar
                  </>
                ) : (
                  <>
                    Siguiente
                    <ChevronRight className="h-3.5 w-3.5" />
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}