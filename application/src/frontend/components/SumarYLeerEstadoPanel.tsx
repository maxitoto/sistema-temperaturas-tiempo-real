import { useCallback, useRef, useState } from 'react';
import '/src/frontend/components/SumarYLeerEstadoPanel.css';
import { ConnectionBadge } from './ConnectionBadge';

type Resultado = Awaited<ReturnType<typeof window.service.client.sumarYLeerEstado>>;
type ResultadoSeguridad = Awaited<ReturnType<typeof window.service.client.guardarConfiguracionSegura>>;

interface HistorialItem {
  id: number;
  timestamp: number;
  base: number;
  factor: number;
  resultado: Resultado | null;
  error: string | null;
  duracionMs: number;
}

const PERFILES_PRUEBA = {
  ADMIN_VALIDO: {
    label: '👩‍💻 silvana_admin (Rol: ADMIN)',
    username: 'silvana_admin',
    pinHash: 'HASH_ADMIN_2026',
  },
  OPERADOR_LECTURA: {
    label: '👷 juan_operador (Rol: OPERADOR - Sin permiso)',
    username: 'juan_operador',
    pinHash: 'HASH_OPER_2026',
  },
  INTRUSO_PIN_FALSO: {
    label: '🕵️ silvana_admin (PIN/Hash Falso)',
    username: 'silvana_admin',
    pinHash: 'HASH_PIRATA_0000',
  },
} as const;

type ClavePerfil = keyof typeof PERFILES_PRUEBA;

let nextId = 1;
const MAX_HISTORIAL = 10;

const parseNumero = (s: string): number | null => {
  const n = Number(s);
  return Number.isFinite(n) ? n : null;
};

const formatearBytes = (bytes: number): string => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
};

export function SumarYLeerEstadoPanel() {
  const [base, setBase]       = useState('5');
  const [factor, setFactor]   = useState('2');
  const [loading, setLoading] = useState(false);
  const [historial, setHistorial] = useState<HistorialItem[]>([]);
  const [verHistorial, setVerHistorial] = useState(true);
  const inFlightRef = useRef(false);

  // Estados para el Laboratorio de Proceso de Usuario (DB + DTO + TLS + Cifrado)
  const [perfilKey, setPerfilKey] = useState<ClavePerfil>('ADMIN_VALIDO');
  const [certFirma, setCertFirma] = useState('CERT-TLS-UNTDF-2026-VALIDO');
  const [tempMin, setTempMin]     = useState('-18.5');
  const [tempMax, setTempMax]     = useState('-12.0');
  const [smtpPass, setSmtpPass]   = useState('SmtpHeladera#2026');
  const [loadingSec, setLoadingSec] = useState(false);
  const [resSeguridad, setResSeguridad] = useState<ResultadoSeguridad | null>(null);
  const [errSeguridad, setErrSeguridad] = useState<string | null>(null);

  const ultimo = historial[0] ?? null;
  const previewValido = parseNumero(base) !== null && parseNumero(factor) !== null;

  const ejecutar = useCallback(async () => {
    if (inFlightRef.current) return;

    const b = parseNumero(base);
    const f = parseNumero(factor);

    if (b === null || f === null) {
      setHistorial(h => [{
        id: nextId++, timestamp: Date.now(),
        base: 0, factor: 0,
        resultado: null,
        error: 'Base y factor deben ser números válidos.',
        duracionMs: 0,
      }, ...h].slice(0, MAX_HISTORIAL));
      return;
    }

    inFlightRef.current = true;
    setLoading(true);
    const t0 = performance.now();

    try {
      const r = await window.service.client.sumarYLeerEstado(b, f);
      const dur = performance.now() - t0;
      setHistorial(h => [{
        id: nextId++, timestamp: Date.now(),
        base: b, factor: f,
        resultado: r, error: null, duracionMs: dur,
      }, ...h].slice(0, MAX_HISTORIAL));
    } catch (e) {
      const dur = performance.now() - t0;
      setHistorial(h => [{
        id: nextId++, timestamp: Date.now(),
        base: b, factor: f,
        resultado: null,
        error: e instanceof Error ? e.message : String(e),
        duracionMs: dur,
      }, ...h].slice(0, MAX_HISTORIAL));
    } finally {
      setLoading(false);
      inFlightRef.current = false;
    }
  }, [base, factor]);

  const ejecutarGuardadoSeguro = useCallback(async () => {
    const tMin = parseNumero(tempMin);
    const tMax = parseNumero(tempMax);
    if (tMin === null || tMax === null) {
      setErrSeguridad('Las temperaturas mínima y máxima deben ser numéricas.');
      return;
    }

    setLoadingSec(true);
    setErrSeguridad(null);
    setResSeguridad(null);

    const perfil = PERFILES_PRUEBA[perfilKey];
    try {
      const out = await window.service.client.guardarConfiguracionSegura({
        emisorCertificado: 'CA-UNTDF-LAB',
        firmaCertificado: certFirma,
        username: perfil.username,
        pinHash: perfil.pinHash,
        tempMin: tMin,
        tempMax: tMax,
        smtpPasswordPlana: smtpPass,
      });
      setResSeguridad(out);
    } catch (e) {
      setErrSeguridad(e instanceof Error ? e.message : String(e));
    } finally {
      setLoadingSec(false);
    }
  }, [perfilKey, certFirma, tempMin, tempMax, smtpPass]);

  const limpiar = () => setHistorial([]);

  return (
    <section className="rpc-panel">
      <header className="rpc-panel__header">
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: '1rem' }}>
          <div>
            <h2>📡 RPC · Sumar y leer estado</h2>
            <p>Invoca <code>RPC_SUMAR_Y_ESTADO</code> con cifrado de canal en Capa 6.</p>
          </div>
          <ConnectionBadge pollIntervalMs={2000} />
        </div>
      </header>

      <form
        className="rpc-panel__form"
        onSubmit={(e) => { e.preventDefault(); void ejecutar(); }}
      >
        <label className="rpc-panel__field">
          <span>Base</span>
          <input
            type="number" step="any" inputMode="decimal"
            value={base} onChange={(e) => setBase(e.target.value)}
            disabled={loading} required
          />
        </label>
        <label className="rpc-panel__field">
          <span>Factor</span>
          <input
            type="number" step="any" inputMode="decimal"
            value={factor} onChange={(e) => setFactor(e.target.value)}
            disabled={loading} required
          />
        </label>
        <button type="submit" className="rpc-panel__button" disabled={loading || !previewValido}>
          {loading
            ? <><span className="rpc-panel__spinner" aria-hidden /> Ejecutando…</>
            : <>⚡ Ejecutar RPC</>}
        </button>

        {previewValido && (
          <p className="rpc-panel__preview" aria-live="polite">
            Preview: <strong>{parseNumero(base)}</strong> + <strong>{parseNumero(factor)} × 1.5</strong> ={' '}
            <strong>{(parseNumero(base)! + parseNumero(factor)! * 1.5).toLocaleString()}</strong>
          </p>
        )}
      </form>

      {ultimo && (
        <div className={`rpc-panel__result ${ultimo.error ? 'is-error' : 'is-success'}`}>
          {ultimo.error ? (
            <>
              <div className="rpc-panel__result-header">❌ Error</div>
              <pre className="rpc-panel__result-error">{ultimo.error}</pre>
            </>
          ) : (
            <div className="rpc-panel__stats">
              <div className="rpc-panel__stat">
                <div className="rpc-panel__stat-label">Resultado</div>
                <div className="rpc-panel__stat-value">{ultimo.resultado!.resultado.toLocaleString()}</div>
              </div>
              <div className="rpc-panel__stat">
                <div className="rpc-panel__stat-label">Estado</div>
                <div className="rpc-panel__stat-value">{ultimo.resultado!.estado}</div>
              </div>
              <div className="rpc-panel__stat">
                <div className="rpc-panel__stat-label">Heap libre</div>
                <div className="rpc-panel__stat-value">{formatearBytes(ultimo.resultado!.heapLibreBytes)}</div>
              </div>
              <div className="rpc-panel__stat">
                <div className="rpc-panel__stat-label">Latencia</div>
                <div className="rpc-panel__stat-value">{ultimo.duracionMs.toFixed(1)} ms</div>
              </div>
            </div>
          )}
        </div>
      )}

      {/* LABORATORIO DE PROCESO DE USUARIO: DB + DTO + CERTIFICADO TLS + CIFRADO */}
      <div style={{ marginTop: '1.5rem', padding: '1rem', borderRadius: '10px', border: '1px solid var(--border)', background: 'var(--bg-subtle)' }}>
        <h3 style={{ margin: '0 0 0.35rem', fontSize: '1rem' }}>
          🔐 Proceso de Usuario · DB + DTO + Certificado TLS + Cifrado de Campo
        </h3>
        <p style={{ margin: '0 0 0.85rem', fontSize: '0.82rem', color: 'var(--text-muted)' }}>
          Prueba <code>RPC_GUARDAR_CONFIG_SEGURA</code> validando certificado, rol en base de datos, rango de umbrales y desencriptación de clave SMTP en el ESP.
        </p>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '0.65rem', marginBottom: '0.75rem' }}>
          <label className="rpc-panel__field">
            <span>Usuario y Rol (DB Firmware)</span>
            <select
              value={perfilKey}
              onChange={(e) => setPerfilKey(e.target.value as ClavePerfil)}
              disabled={loadingSec}
              style={{ padding: '0.5rem', borderRadius: '8px', border: '1px solid var(--border)', background: 'var(--bg)', color: 'var(--text)' }}
            >
              {Object.entries(PERFILES_PRUEBA).map(([k, v]) => (
                <option key={k} value={k}>{v.label}</option>
              ))}
            </select>
          </label>

          <label className="rpc-panel__field">
            <span>Certificado TLS de Aplicación</span>
            <select
              value={certFirma}
              onChange={(e) => setCertFirma(e.target.value)}
              disabled={loadingSec}
              style={{ padding: '0.5rem', borderRadius: '8px', border: '1px solid var(--border)', background: 'var(--bg)', color: 'var(--text)' }}
            >
              <option value="CERT-TLS-UNTDF-2026-VALIDO">✅ CERT-TLS-UNTDF-2026-VALIDO</option>
              <option value="CERT-FALSO-ATACANTE-999">❌ CERT-FALSO-ATACANTE-999</option>
            </select>
          </label>

          <label className="rpc-panel__field">
            <span>Temp. Mín (°C) [Límite: -40]</span>
            <input
              type="number" step="0.5"
              value={tempMin} onChange={(e) => setTempMin(e.target.value)}
              disabled={loadingSec}
            />
          </label>

          <label className="rpc-panel__field">
            <span>Temp. Máx (°C) [Límite: +20]</span>
            <input
              type="number" step="0.5"
              value={tempMax} onChange={(e) => setTempMax(e.target.value)}
              disabled={loadingSec}
            />
          </label>

          <label className="rpc-panel__field" style={{ gridColumn: '1 / -1' }}>
            <span>Clave SMTP (Viaja cifrada como binario 0xC4 en el DTO)</span>
            <input
              type="text"
              value={smtpPass} onChange={(e) => setSmtpPass(e.target.value)}
              disabled={loadingSec}
            />
          </label>
        </div>

        <div style={{ display: 'flex', gap: '0.5rem', marginBottom: '0.75rem', flexWrap: 'wrap' }}>
          <button
            type="button"
            className="rpc-panel__button"
            style={{ flex: 1 }}
            disabled={loadingSec}
            onClick={() => void ejecutarGuardadoSeguro()}
          >
            {loadingSec ? 'Validando y guardando en DB…' : '🛡️ Enviar DTO Seguro y Guardar en DB'}
          </button>

          <button
            type="button"
            disabled={loadingSec}
            onClick={() => { setTempMin('-126'); setTempMax('126'); }}
            style={{ padding: '0.5rem 0.75rem', borderRadius: '8px', border: '1px solid var(--error)', color: 'var(--error)', background: 'transparent', fontSize: '0.8rem', fontWeight: 600, cursor: 'pointer' }}
          >
            Simular Ataque (-126°C / 126°C)
          </button>
        </div>

        {errSeguridad && (
          <div className="rpc-panel__result is-error" style={{ marginBottom: 0 }}>
            <div className="rpc-panel__result-header">🛡️ Rechazado por Política de Usuario en Firmware</div>
            <pre className="rpc-panel__result-error">{errSeguridad}</pre>
          </div>
        )}

        {resSeguridad && (
          <div className="rpc-panel__result is-success" style={{ marginBottom: 0 }}>
            <div className="rpc-panel__result-header">
              ✅ Registro #{resSeguridad.idRegistro} insertado en BaseDeDatosFirmware ({resSeguridad.totalFilasEnDB} filas)
            </div>
            <div style={{ fontSize: '0.82rem', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '0.4rem' }}>
              <div><strong>Autorizado a:</strong> {resSeguridad.modificadoPor} ({resSeguridad.rolUsuario})</div>
              <div><strong>Canal usado:</strong> {resSeguridad.medioUsado}</div>
              <div><strong>Umbrales DB:</strong> {resSeguridad.tempMinGuardada}°C a {resSeguridad.tempMaxGuardada}°C</div>
              <div><strong>SMTP desencriptada en ESP:</strong> <code>{resSeguridad.smtpDesencriptadaEnFirmware}</code></div>
              <div style={{ gridColumn: '1 / -1', color: 'var(--text-muted)' }}>
                <strong>Bytes cifrados enviados en el campo DTO:</strong> <code>0x{resSeguridad.hexCampoCifradoEnviado}</code>
              </div>
            </div>
          </div>
        )}
      </div>

      <div className="rpc-panel__history" style={{ marginTop: '1.25rem' }}>
        <div className="rpc-panel__history-header">
          <h3>Historial ({historial.length})</h3>
          <div className="rpc-panel__history-actions">
            <button type="button" onClick={() => setVerHistorial(v => !v)}>
              {verHistorial ? 'Ocultar' : 'Mostrar'}
            </button>
            {historial.length > 0 && (
              <button type="button" onClick={limpiar}>Limpiar</button>
            )}
          </div>
        </div>

        {verHistorial && historial.length === 0 && (
          <p className="rpc-panel__empty">Sin llamadas aún. Pulsa “Ejecutar RPC”.</p>
        )}

        {verHistorial && historial.length > 0 && (
          <ul className="rpc-panel__history-list">
            {historial.map((item) => (
              <li key={item.id} className={item.error ? 'is-error' : 'is-success'}>
                <span className="rpc-panel__history-time">
                  {new Date(item.timestamp).toLocaleTimeString()}
                </span>
                <span className="rpc-panel__history-args">
                  {item.base} × {item.factor}
                </span>
                <span className="rpc-panel__history-out">
                  {item.error
                    ? item.error
                    : `→ ${item.resultado!.resultado.toLocaleString()} · ${item.resultado!.estado} · ${formatearBytes(item.resultado!.heapLibreBytes)}`}
                </span>
                <span className="rpc-panel__history-dur">{item.duracionMs.toFixed(1)} ms</span>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}