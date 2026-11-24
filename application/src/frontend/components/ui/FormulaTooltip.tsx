import React, { useCallback, useRef, useState } from "react";
import { Sigma } from "lucide-react";

export interface FormulaTooltipProps {
  titulo: string;
  archivoModelo?: string;
  ecuacion: string;
  sustitucion?: string;
  descripcion: string;
  children?: React.ReactNode;
}

export function FormulaTooltip({
  titulo,
  archivoModelo,
  ecuacion,
  sustitucion,
  descripcion,
  children,
}: FormulaTooltipProps) {
  const triggerRef = useRef<HTMLSpanElement | null>(null);
  const [visible, setVisible] = useState(false);
  const [pos, setPos] = useState<{ top: number; left: number }>({
    top: 0,
    left: 0,
  });

  const abrir = useCallback(() => {
    const el = triggerRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const popW = 320;
    const popH = 180;

    let left = r.left + r.width / 2 - popW / 2;
    left = Math.max(10, Math.min(window.innerWidth - popW - 10, left));

    const cabeArriba = r.top >= popH + 14;
    const top = cabeArriba ? r.top - popH - 8 : r.bottom + 8;

    setPos({
      top: Math.max(10, Math.min(window.innerHeight - popH - 10, top)),
      left,
    });
    setVisible(true);
  }, []);

  return (
    <span
      ref={triggerRef}
      onMouseEnter={abrir}
      onMouseLeave={() => setVisible(false)}
      onFocus={abrir}
      onBlur={() => setVisible(false)}
      tabIndex={0}
      className="inline-flex cursor-help items-center gap-1 align-middle"
    >
      {children ?? (
        <span className="inline-flex h-4 w-4 items-center justify-center rounded-full border border-info/50 bg-info-light text-info transition-colors hover:bg-info hover:text-info-foreground">
          <Sigma className="h-2.5 w-2.5" />
        </span>
      )}

      {visible && (
        <div
          role="tooltip"
          style={{ top: `${pos.top}px`, left: `${pos.left}px`, width: "320px" }}
          className="pointer-events-none fixed z-[90] rounded-xl border border-info/50 bg-card p-3 text-left text-xs text-card-foreground shadow-2xl"
        >
          <div className="flex items-center justify-between gap-2">
            <span className="font-bold text-foreground">{titulo}</span>
            {archivoModelo && (
              <span className="rounded bg-secondary px-1.5 py-0.5 font-mono text-[9px] text-foreground-secondary">
                {archivoModelo}
              </span>
            )}
          </div>

          <div className="mt-2 rounded-md border border-border bg-zinc-950 px-2.5 py-2 font-mono text-[11px] leading-relaxed text-emerald-300 whitespace-pre-wrap">
            {ecuacion}
          </div>

          {sustitucion && (
            <div className="mt-1.5 rounded bg-info-light/60 px-2 py-1 font-mono text-[10px] text-foreground">
              {sustitucion}
            </div>
          )}

          <p className="mt-1.5 text-[11px] leading-snug text-foreground-secondary">
            {descripcion}
          </p>
        </div>
      )}
    </span>
  );
}