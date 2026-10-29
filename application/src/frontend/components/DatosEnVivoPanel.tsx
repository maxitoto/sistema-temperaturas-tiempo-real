import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import './DatosEnVivoPanel.css';

// =========================================================================
// TIPOS
// =========================================================================
type Estado = 'idle' | 'conectando' | 'live' | 'degradado' | 'caido';

interface Muestra {
  n: number;           // número de llamada (1, 2, 3...)
  ts: number;          // performance.now() al recibir
  valor: number | null;
  latenciaMs: number;
  ok: boolean;
  errorMsg?: string;
}

const MAX_MUESTRAS = 100;
const INTERVALOS = [
  { label: '100 ms', valor: 100 },
  { label: '250 ms', valor: 250 },
  { label: '500 ms', valor: 500 },
  { label: '1 s',    valor: 1000 },
];

const TIMEOUT_MS = 800; // agresivo a propósito: si UDP pierde, lo vemos rápido

// =========================================================================
// COMPONENTE PRINCIPAL
// =========================================================================
export function DatosEnVivoPanel() {
  const [running, setRunning]       = useState(false);
  const [intervaloMs, setIntervalo] = useState(250);
  const [muestras, setMuestras]     = useState<Muestra[]>([]);
  const [estado, setEstado]         = useState<Estado>('idle');
  const [flash, setFlash]           = useState(0); // dispara animación del número

  const runningRef   = useRef(false);
  const timerRef     = useRef<ReturnType<typeof setTimeout> | null>(null);
  const contadorRef  = useRef(0);
  const prevValorRef = useRef<number | null>(null);
  const erroresConsRef = useRef(0);

  runningRef.current = running;

  // ─── Loop de polling ─────────────────────────────────────────────────
  useEffect(() => {
    if (!running) return;

    let cancelado = false;

    const tick = async () => {
      if (cancelado || !runningRef.current) return;

      contadorRef.current += 1;
      const n = contadorRef.current;
      const t0 = performance.now();

      let valor: number | null = null;
      let ok = false;
      let errorMsg: string | undefined;

      try {
        const r = await window.service.client.leerDatosEnVivo({ timeoutMs: TIMEOUT_MS });
        valor = Number(r.numeroRandom);
        ok = Number.isFinite(valor);
        if (!ok) errorMsg = 'Respuesta no numérica';
      } catch (e) {
        errorMsg = e instanceof Error ? e.message : String(e);
      }

      const latenciaMs = performance.now() - t0;

      if (!cancelado) {
        setMuestras(prev => {
          const nueva: Muestra = { n, ts: Date.now(), valor, latenciaMs, ok, errorMsg };
          const out = [...prev, nueva];
          return out.length > MAX_MUESTRAS ? out.slice(out.length - MAX_MUESTRAS) : out;
        });

        if (ok) {
          erroresConsRef.current = 0;
          prevValorRef.current = valor;
          setFlash(f => f + 1);
        } else {
          erroresConsRef.current += 1;
        }

        // Calcular estado global
        if (erroresConsRef.current === 0) setEstado('live');
        else if (erroresConsRef.current < 3) setEstado('degradado');
        else setEstado('caido');
      }

      if (!cancelado && runningRef.current) {
        timerRef.current = setTimeout(tick, intervaloMs);
      }
    };

    setEstado('conectando');
    void tick();

    return () => {
      cancelado = true;
      if (timerRef.current) {
        clearTimeout(timerRef.current);
        timerRef.current = null;
      }
    };
  }, [running, intervaloMs]);

  // ─── Controles ───────────────────────────────────────────────────────
  const toggle = useCallback(() => {
    if (running) {
      setRunning(false);
      setEstado('idle');
    } else {
      contadorRef.current = 0;
      prevValorRef.current = null;
      erroresConsRef.current = 0;
      setMuestras([]);
      setRunning(true);
    }
  }, [running]);

  const reset = useCallback(() => {
    setMuestras([]);
    contadorRef.current = 0;
    prevValorRef.current = null;
    erroresConsRef.current = 0;
    setEstado(running ? 'conectando' : 'idle');
  }, [running]);

  // ─── Derivados ───────────────────────────────────────────────────────
  const stats = useMemo(() => {
    const ok = muestras.filter(m => m.ok);
    const err = muestras.filter(m => !m.ok);
    const latencias = ok.map(m => m.latenciaMs);
    const valores = ok.map(m => m.valor as number);

    if (latencias.length === 0) {
      return {
        enviados: muestras.length, recibidos: 0, perdidos: err.length,
        minLat: 0, maxLat: 0, avgLat: 0, jitter: 0,
        min: 0, max: 0, avg: 0,
      };
    }

    const avg = latencias.reduce((a, b) => a + b, 0) / latencias.length;
    const variance = latencias.reduce((a, b) => a + (b - avg) ** 2, 0) / latencias.length;
    const avgV = valores.reduce((a, b) => a + b, 0) / valores.length;

    return {
      enviados: muestras.length,
      recibidos: ok.length,
      perdidos: err.length,
      minLat: Math.min(...latencias),
      maxLat: Math.max(...latencias),
      avgLat: avg,
      jitter: Math.sqrt(variance),
      min: Math.min(...valores),
      max: Math.max(...valores),
      avg: avgV,
    };
  }, [muestras]);

  const ultimo = muestras[muestras.length - 1] ?? null;
  const delta = ultimo?.ok && prevValorRef.current !== null
    ? (ultimo.valor as number) - prevValorRef.current
    : null;

  const pctOk = stats.enviados > 0 ? (stats.recibidos / stats.enviados) * 100 : 0;

  // ─── Render ──────────────────────────────────────────────────────────
  return (
    <section className="live-panel">
      {/* Header */}
      <header className="live-panel__header">
        <div className="live-panel__brand">
          <span className="live-panel__udp-badge">UDP</span>
          <div>
            <h2>Datos en Vivo</h2>
            <p>
              RPC por <code>PROTO_UDP</code> · sin ARQ, sin ACK, sin retransmisión
            </p>
          </div>
        </div>

        <div className="live-panel__status" data-estado={estado}>
          <span className="live-panel__dot" />
          <span>{estadoLabel[estado]}</span>
        </div>

        <div className="live-panel__controls">
          <select
            value={intervaloMs}
            disabled={running}
            onChange={(e) => setIntervalo(Number(e.target.value))}
          >
            {INTERVALOS.map(i => (
              <option key={i.valor} value={i.valor}>{i.label}</option>
            ))}
          </select>
          <button
            type="button"
            className="live-panel__toggle"
            data-running={running}
            onClick={toggle}
          >
            {running ? '⏸ Detener' : '▶ Iniciar'}
          </button>
          <button
            type="button"
            className="live-panel__reset"
            onClick={reset}
            disabled={muestras.length === 0}
          >
            ⟲ Limpiar
          </button>
        </div>
      </header>

      {/* Número grande + delta */}
      <div className="live-panel__hero" key={flash}>
        <div className="live-panel__hero-label">Último valor recibido</div>
        <div className="live-panel__hero-value">
          {ultimo?.ok
            ? (ultimo.valor as number).toLocaleString()
            : <span className="live-panel__hero-empty">—</span>}
        </div>
        {delta !== null && (
          <div className={`live-panel__delta ${delta >= 0 ? 'is-up' : 'is-down'}`}>
            {delta >= 0 ? '▲' : '▼'} {Math.abs(delta).toLocaleString()}
            <span className="live-panel__delta-label">vs anterior</span>
          </div>
        )}
        {ultimo && !ultimo.ok && (
          <div className="live-panel__hero-error">
            ⚠ {ultimo.errorMsg ?? 'Sin respuesta'}
          </div>
        )}
      </div>

      {/* Sparkline + latencia */}
      <div className="live-panel__charts">
        <Chart titulo="Valor" muestras={muestras} campo="valor" color="#6366f1" />
        <Chart titulo="Latencia (ms)" muestras={muestras} campo="latenciaMs" color="#10b981" />
      </div>

      {/* Stats */}
      <div className="live-panel__stats">
        <Stat label="Enviados"  valor={stats.enviados} />
        <Stat label="Recibidos" valor={stats.recibidos} tono="ok" />
        <Stat label="Perdidos"  valor={stats.perdidos}  tono={stats.perdidos > 0 ? 'err' : 'neutral'} />
        <Stat label="% Éxito"   valor={`${pctOk.toFixed(1)} %`} tono={pctOk > 95 ? 'ok' : pctOk > 80 ? 'warn' : 'err'} />
        <Stat label="Lat. media" valor={`${stats.avgLat.toFixed(1)} ms`} />
        <Stat label="Lat. mín"   valor={`${stats.minLat.toFixed(1)} ms`} />
        <Stat label="Lat. máx"   valor={`${stats.maxLat.toFixed(1)} ms`} />
        <Stat label="Jitter"     valor={`${stats.jitter.toFixed(1)} ms`} />
      </div>

      {/* Últimas llamadas */}
      <div className="live-panel__recent">
        <h3>Últimas llamadas</h3>
        {muestras.length === 0 ? (
          <p className="live-panel__empty">
            {running ? 'Esperando primera respuesta…' : 'Pulsa "Iniciar" para empezar.'}
          </p>
        ) : (
          <ul>
            {muestras.slice(-6).reverse().map((m) => (
              <li key={m.n} className={m.ok ? 'is-ok' : 'is-err'}>
                <span className="live-panel__recent-n">#{m.n}</span>
                <span className="live-panel__recent-v">
                  {m.ok ? (m.valor as number).toLocaleString() : '—'}
                </span>
                <span className="live-panel__recent-l">{m.latenciaMs.toFixed(1)} ms</span>
                <span className="live-panel__recent-s">{m.ok ? '✅' : '❌'}</span>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}

// =========================================================================
// SUBCOMPONENTES
// =========================================================================
const estadoLabel: Record<Estado, string> = {
  idle: 'Detenido',
  conectando: 'Conectando…',
  live: 'En vivo',
  degradado: 'Degradado',
  caido: 'Sin respuesta',
};

function Stat({ label, valor, tono = 'neutral' }: {
  label: string;
  valor: number | string;
  tono?: 'neutral' | 'ok' | 'warn' | 'err';
}) {
  return (
    <div className="live-panel__stat" data-tono={tono}>
      <div className="live-panel__stat-label">{label}</div>
      <div className="live-panel__stat-value">{valor}</div>
    </div>
  );
}

function Chart({ titulo, muestras, campo, color }: {
  titulo: string;
  muestras: Muestra[];
  campo: 'valor' | 'latenciaMs';
  color: string;
}) {
  const W = 400;
  const H = 60;
  const PAD = 4;

  const puntos = muestras
    .filter(m => m.ok)
    .map(m => campo === 'valor' ? (m.valor as number) : m.latenciaMs);

  if (puntos.length < 2) {
    return (
      <div className="live-panel__chart">
        <div className="live-panel__chart-title">{titulo}</div>
        <div className="live-panel__chart-empty">Sin datos suficientes</div>
      </div>
    );
  }

  const min = Math.min(...puntos);
  const max = Math.max(...puntos);
  const rango = max - min || 1;

  const path = puntos.map((v, i) => {
    const x = PAD + (i / (puntos.length - 1)) * (W - PAD * 2);
    const y = H - PAD - ((v - min) / rango) * (H - PAD * 2);
    return `${i === 0 ? 'M' : 'L'} ${x.toFixed(1)} ${y.toFixed(1)}`;
  }).join(' ');

  const areaPath = `${path} L ${W - PAD} ${H - PAD} L ${PAD} ${H - PAD} Z`;

  return (
    <div className="live-panel__chart">
      <div className="live-panel__chart-header">
        <span className="live-panel__chart-title">{titulo}</span>
        <span className="live-panel__chart-range">
          {min.toFixed(campo === 'valor' ? 0 : 1)} – {max.toFixed(campo === 'valor' ? 0 : 1)}
        </span>
      </div>
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" className="live-panel__chart-svg">
        <defs>
          <linearGradient id={`grad-${campo}`} x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%"   stopColor={color} stopOpacity="0.35" />
            <stop offset="100%" stopColor={color} stopOpacity="0" />
          </linearGradient>
        </defs>
        <path d={areaPath} fill={`url(#grad-${campo})`} />
        <path d={path} fill="none" stroke={color} strokeWidth="1.5" />
      </svg>
    </div>
  );
}