import { useCallback, useEffect, useRef, useState } from 'react';
import './ConfiguracionRedPanel.css';

// =========================================================================
// TIPOS — 1:1 con el contrato RPC_DB_OBTENER_RED_Y_NTP
// =========================================================================
interface CredencialRpcDTO {
  emisorCertificado: string;
  firmaCertificado: string;
  username: string;
  pinHash: string;
}

// FilaServidorNTP
interface ServidorNtp {
  idServer: number;
  server: string;
  activa: number;
  fechaCreacion: number;
  fechaModificacion: number;
}

// FilaConfiguracionConectividad + array FilaServidorNTP
// (los passwords NO se exponen al frontend por seguridad)
interface EstadoRed {
  idConfiguracionConectividad: number;
  activa: number;
  ssidSta: string;
  ssidAp: string;
  puertoTcp: number;
  puertoUdp: number;
  baudiosUsb: number;
  fechaCreacion: number;
  fechaModificacion: number;
  servidoresNtp: ServidorNtp[];
}

// guardarConfigRed devuelve otra FilaConfiguracionConectividad
interface ResultadoGuardar {
  idConfiguracionConectividad: number;
  activa: number;
  ssidSta: string;
  ssidAp: string;
  puertoTcp: number;
  puertoUdp: number;
  baudiosUsb: number;
  fechaCreacion: number;
  fechaModificacion: number;
}

type Fase = 'idle' | 'cargando' | 'guardando' | 'guardado' | 'reiniciando' | 'reiniciado' | 'error';

const CREDENCIAL_DEFAULT: CredencialRpcDTO = {
  emisorCertificado: 'CA-UNTDF-LAB',
  firmaCertificado: 'CERT-TLS-UNTDF-2026-VALIDO',
  username: 'silvana_admin',
  pinHash: 'HASH_ADMIN_2026',
};

const PUERTO_TCP_DEFAULT = 80;
const PUERTO_UDP_DEFAULT = 81;
const BAUDIOS_DEFAULT    = 115200;

// =========================================================================
// NORMALIZADOR — tolera snake_case / camelCase / nombres explícitos del serializer
// =========================================================================
function normalizarServidorNtp(raw: any): ServidorNtp {
  return {
    idServer:          Number(raw?.idServer          ?? raw?.id_server          ?? 0),
    server:            String(raw?.server            ?? raw?.serverNtp          ?? raw?.host ?? ''),
    activa:            Number(raw?.activa            ?? 1),
    fechaCreacion:     Number(raw?.fechaCreacion     ?? raw?.fecha_creacion     ?? 0),
    fechaModificacion: Number(raw?.fechaModificacion ?? raw?.fecha_modificacion ?? 0),
  };
}

function normalizarEstadoRed(raw: any): EstadoRed {
  const cfg = raw?.config ?? raw?.outRed ?? raw?.red ?? raw ?? {};
  const ntpRaw = raw?.servidoresNtp ?? raw?.servidores_ntp ?? raw?.outNtp ?? raw?.ntp ?? [];
  return {
    idConfiguracionConectividad: Number(
      cfg?.idConfiguracionConectividad ?? cfg?.id_configuracion_conectividad ?? 1),
    activa:            Number(cfg?.activa ?? 1),
    ssidSta:           String(cfg?.ssidSta           ?? cfg?.SSID_STA ?? cfg?.ssid_sta ?? ''),
    ssidAp:            String(cfg?.ssidAp            ?? cfg?.SSID_AP  ?? cfg?.ssid_ap  ?? ''),
    puertoTcp:         Number(cfg?.puertoTcp         ?? cfg?.puerto_tcp ?? PUERTO_TCP_DEFAULT),
    puertoUdp:         Number(cfg?.puertoUdp         ?? cfg?.puerto_udp ?? PUERTO_UDP_DEFAULT),
    baudiosUsb:        Number(cfg?.baudiosUsb        ?? cfg?.baudios_usb ?? BAUDIOS_DEFAULT),
    fechaCreacion:     Number(cfg?.fechaCreacion     ?? cfg?.fecha_creacion     ?? 0),
    fechaModificacion: Number(cfg?.fechaModificacion ?? cfg?.fecha_modificacion ?? 0),
    servidoresNtp: Array.isArray(ntpRaw) ? ntpRaw.map(normalizarServidorNtp) : [],
  };
}

function normalizarResultadoGuardar(raw: any, fallback: {
  ssidSta: string; ssidAp: string; puertoTcp: number; puertoUdp: number; baudiosUsb: number;
}): ResultadoGuardar {
  const cfg = raw?.outRed ?? raw?.config ?? raw ?? {};
  return {
    idConfiguracionConectividad: Number(
      cfg?.idConfiguracionConectividad ?? cfg?.id_configuracion_conectividad ?? 1),
    activa:            Number(cfg?.activa ?? 1),
    ssidSta:           String(cfg?.ssidSta    ?? cfg?.SSID_STA ?? fallback.ssidSta),
    ssidAp:            String(cfg?.ssidAp     ?? cfg?.SSID_AP  ?? fallback.ssidAp),
    puertoTcp:         Number(cfg?.puertoTcp  ?? cfg?.puerto_tcp  ?? fallback.puertoTcp),
    puertoUdp:         Number(cfg?.puertoUdp  ?? cfg?.puerto_udp  ?? fallback.puertoUdp),
    baudiosUsb:        Number(cfg?.baudiosUsb ?? cfg?.baudios_usb ?? fallback.baudiosUsb),
    fechaCreacion:     Number(cfg?.fechaCreacion     ?? cfg?.fecha_creacion     ?? 0),
    fechaModificacion: Number(cfg?.fechaModificacion ?? cfg?.fecha_modificacion
                              ?? Math.floor(Date.now() / 1000)),
  };
}

const fmtTs = (unixSeg?: number | null): string => {
  if (!unixSeg) return '—';
  return new Date(unixSeg * 1000).toLocaleString();
};

// =========================================================================
// COMPONENTE
// =========================================================================
export function ConfiguracionRedPanel() {
  const [credencial] = useState<CredencialRpcDTO>(CREDENCIAL_DEFAULT);

  const [estadoRed, setEstadoRed]   = useState<EstadoRed | null>(null);
  const [errorCarga, setErrorCarga] = useState<string | null>(null);

  // Formulario — refleja TODOS los campos editables del GuardarRedInputDTO
  const [ssidSta,    setSsidSta]    = useState('');
  const [passSta,    setPassSta]    = useState('');
  const [ssidAp,     setSsidAp]     = useState('');
  const [passAp,     setPassAp]     = useState('');
  const [puertoTcp,  setPuertoTcp]  = useState<string>(String(PUERTO_TCP_DEFAULT));
  const [puertoUdp,  setPuertoUdp]  = useState<string>(String(PUERTO_UDP_DEFAULT));
  const [baudiosUsb, setBaudiosUsb] = useState<string>(String(BAUDIOS_DEFAULT));

  const [fase,             setFase]             = useState<Fase>('idle');
  const [mensajeError,     setMensajeError]     = useState<string | null>(null);
  const [resultadoGuardar, setResultadoGuardar] = useState<ResultadoGuardar | null>(null);
  const inFlightRef = useRef(false);

  // ─── Cargar ───────────────────────────────────────────────────────
  const cargar = useCallback(async () => {
    setFase('cargando');
    setErrorCarga(null);
    try {
      const raw = await window.service.client.obtenerConfigRedYNtpFirmware(credencial);
      const estado = normalizarEstadoRed(raw);
      setEstadoRed(estado);
      setSsidSta(estado.ssidSta);
      setSsidAp(estado.ssidAp);
      setPuertoTcp(String(estado.puertoTcp));
      setPuertoUdp(String(estado.puertoUdp));
      setBaudiosUsb(String(estado.baudiosUsb));
      setFase('idle');
    } catch (e) {
      setErrorCarga(e instanceof Error ? e.message : String(e));
      setFase('error');
    }
  }, [credencial]);

  useEffect(() => { void cargar(); }, [cargar]);

  // ─── Validación ───────────────────────────────────────────────────
  const passApInvalida   = passAp.length > 0 && passAp.length < 8;
  const puertoTcpInvalido = (() => { const n = Number(puertoTcp); return !Number.isInteger(n) || n <= 0 || n > 65535; })();
  const puertoUdpInvalido = (() => { const n = Number(puertoUdp); return !Number.isInteger(n) || n <= 0 || n > 65535; })();
  const baudiosInvalido   = (() => { const n = Number(baudiosUsb); return !Number.isInteger(n) || n < 300 || n > 3000000; })();
  const formValido =
    ssidSta.trim().length > 0 &&
    ssidAp.trim().length > 0 &&
    !passApInvalida &&
    !puertoTcpInvalido &&
    !puertoUdpInvalido &&
    !baudiosInvalido;

  // ─── Guardar ──────────────────────────────────────────────────────
  const guardar = useCallback(async () => {
    if (inFlightRef.current) return;
    if (!formValido) {
      setMensajeError('Revise los campos marcados en rojo antes de guardar.');
      return;
    }
    inFlightRef.current = true;
    setMensajeError(null);
    setFase('guardando');
    try {
      const fallback = {
        ssidSta: ssidSta.trim(),
        ssidAp:  ssidAp.trim(),
        puertoTcp:  Number(puertoTcp),
        puertoUdp:  Number(puertoUdp),
        baudiosUsb: Number(baudiosUsb),
      };
      const raw = await window.service.client.guardarConfigRedFirmware({
        credencial,
        ssidSta:           fallback.ssidSta,
        passwordStaPlana:  passSta,
        ssidAp:            fallback.ssidAp,
        passwordApPlana:   passAp,
        puertoTcp:         fallback.puertoTcp,
        puertoUdp:         fallback.puertoUdp,
        baudiosUsb:        fallback.baudiosUsb,
      });
      setResultadoGuardar(normalizarResultadoGuardar(raw, fallback));
      setFase('guardado');
      void cargar();
    } catch (e) {
      setMensajeError(e instanceof Error ? e.message : String(e));
      setFase('error');
    } finally {
      inFlightRef.current = false;
    }
  }, [credencial, ssidSta, passSta, ssidAp, passAp, puertoTcp, puertoUdp, baudiosUsb, formValido, cargar]);

  // ─── Reiniciar ────────────────────────────────────────────────────
  const reiniciar = useCallback(async () => {
    if (inFlightRef.current) return;
    inFlightRef.current = true;
    setMensajeError(null);
    setFase('reiniciando');
    try {
      const r = await window.service.client.reiniciarFirmware(credencial);
      if (!r.reiniciando) throw new Error('El firmware rechazó la solicitud de reinicio.');
      setFase('reiniciado');
      setTimeout(() => {
        void cargar();
        setFase('idle');
        setResultadoGuardar(null);
        setPassSta('');
        setPassAp('');
      }, 2500);
    } catch (e) {
      setMensajeError(e instanceof Error ? e.message : String(e));
      setFase('error');
    } finally {
      inFlightRef.current = false;
    }
  }, [credencial, cargar]);

  const limpiarFormulario = () => {
    if (!estadoRed) return;
    setSsidSta(estadoRed.ssidSta);
    setSsidAp(estadoRed.ssidAp);
    setPassSta('');
    setPassAp('');
    setPuertoTcp(String(estadoRed.puertoTcp));
    setPuertoUdp(String(estadoRed.puertoUdp));
    setBaudiosUsb(String(estadoRed.baudiosUsb));
    setMensajeError(null);
  };

  const busy = fase === 'guardando' || fase === 'reiniciando' || fase === 'cargando';

  // ─── Render ───────────────────────────────────────────────────────
  return (
    <section className="red-panel">
      <header className="red-panel__header">
        <div className="red-panel__brand">
          <span className="red-panel__icon">📶</span>
          <div>
            <h2>Configuración de Red del Firmware</h2>
            <p>
              Persiste en <code>wifi.bin</code> (FilaConfiguracionConectividad) y{' '}
              <code>ntp.bin</code> (FilaServidorNTP). Reinicia para aplicar.
            </p>
          </div>
        </div>
        <button
          type="button"
          className="red-panel__refresh"
          onClick={() => void cargar()}
          disabled={busy}
          title="Recargar estado desde el firmware"
        >
          {fase === 'cargando' ? '⏳' : '⟳'}
        </button>
      </header>

      {errorCarga && (
        <div className="red-panel__alert is-error">
          <strong>No se pudo leer el estado:</strong> {errorCarga}
        </div>
      )}

      {/* ── Estado actual: TODOS los campos de FilaConfiguracionConectividad ── */}
      {estadoRed && (
        <>
          <div className="red-panel__status">
            <EstadoBadge
              titulo="Config conectividad"
              activo={estadoRed.activa === 1}
              linea1={`ID ${estadoRed.idConfiguracionConectividad} · activa=${estadoRed.activa}`}
              linea2={`Modificado: ${fmtTs(estadoRed.fechaModificacion)}`}
              color="gray"
            />
            <EstadoBadge
              titulo="SSID STA"
              activo={!!estadoRed.ssidSta}
              linea1={estadoRed.ssidSta || '—'}
              linea2={`Creado: ${fmtTs(estadoRed.fechaCreacion)}`}
              color="blue"
            />
            <EstadoBadge
              titulo="SSID AP"
              activo={!!estadoRed.ssidAp}
              linea1={estadoRed.ssidAp || '—'}
              linea2={`Password: ${estadoRed.ssidAp ? 'configurada' : '—'}`}
              color="green"
            />
            <EstadoBadge
              titulo="Puertos TCP / UDP"
              activo
              linea1={`${estadoRed.puertoTcp} / ${estadoRed.puertoUdp}`}
              linea2={`Baudios USB: ${estadoRed.baudiosUsb}`}
              color="purple"
            />
          </div>

          {/* ── Tabla de servidores NTP con TODOS sus campos ── */}
          <div className="red-panel__ntp">
            <h3>Servidores NTP ({estadoRed.servidoresNtp.length})</h3>
            {estadoRed.servidoresNtp.length === 0 ? (
              <p className="red-panel__hint">Ninguno configurado.</p>
            ) : (
              <table className="red-panel__table">
                <thead>
                  <tr>
                    <th>ID</th>
                    <th>Servidor</th>
                    <th>Activa</th>
                    <th>Creado</th>
                    <th>Modificado</th>
                  </tr>
                </thead>
                <tbody>
                  {estadoRed.servidoresNtp.map((s) => (
                    <tr key={s.idServer}>
                      <td>{s.idServer}</td>
                      <td><code>{s.server || '—'}</code></td>
                      <td>{s.activa === 1 ? 'Sí' : 'No'}</td>
                      <td className="red-panel__muted">{fmtTs(s.fechaCreacion)}</td>
                      <td className="red-panel__muted">{fmtTs(s.fechaModificacion)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </>
      )}

      {/* ── Formulario: TODOS los campos editables de GuardarRedInputDTO ── */}
      <form className="red-panel__form" onSubmit={(e) => { e.preventDefault(); void guardar(); }}>
        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend><span className="red-panel__fieldset-icon">🌐</span>Red STA</legend>
          <label className="red-panel__field">
            <span>SSID</span>
            <input type="text" value={ssidSta} onChange={(e) => setSsidSta(e.target.value)}
                   placeholder="Wokwi-GUEST" maxLength={32} autoComplete="off" required />
          </label>
          <label className="red-panel__field">
            <span>Contraseña</span>
            <input type="password" value={passSta} onChange={(e) => setPassSta(e.target.value)}
                   placeholder="dejar vacío si es red abierta" maxLength={64} autoComplete="new-password" />
          </label>
        </fieldset>

        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend><span className="red-panel__fieldset-icon">📡</span>Red AP</legend>
          <label className="red-panel__field">
            <span>SSID del AP</span>
            <input type="text" value={ssidAp} onChange={(e) => setSsidAp(e.target.value)}
                   placeholder="Sensor-Refri-01" maxLength={32} autoComplete="off" required />
          </label>
          <label className="red-panel__field">
            <span>Contraseña del AP</span>
            <input type="password" value={passAp} onChange={(e) => setPassAp(e.target.value)}
                   placeholder="≥ 8 caracteres (o vacío para red abierta)" maxLength={64}
                   autoComplete="new-password" className={passApInvalida ? 'is-invalid' : ''} />
            {passApInvalida && (
              <small className="red-panel__hint is-error">
                WPA2 requiere al menos 8 caracteres, o dejalo vacío para red abierta.
              </small>
            )}
          </label>
        </fieldset>

        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend><span className="red-panel__fieldset-icon">🔌</span>Puertos y baudios</legend>
          <label className="red-panel__field">
            <span>Puerto TCP (1 – 65535)</span>
            <input type="number" value={puertoTcp} onChange={(e) => setPuertoTcp(e.target.value)}
                   min={1} max={65535} className={puertoTcpInvalido ? 'is-invalid' : ''} required />
            {puertoTcpInvalido && (
              <small className="red-panel__hint is-error">Debe ser entero entre 1 y 65535.</small>
            )}
          </label>
          <label className="red-panel__field">
            <span>Puerto UDP (1 – 65535)</span>
            <input type="number" value={puertoUdp} onChange={(e) => setPuertoUdp(e.target.value)}
                   min={1} max={65535} className={puertoUdpInvalido ? 'is-invalid' : ''} required />
            {puertoUdpInvalido && (
              <small className="red-panel__hint is-error">Debe ser entero entre 1 y 65535.</small>
            )}
          </label>
          <label className="red-panel__field">
            <span>Baudios USB</span>
            <input type="number" value={baudiosUsb} onChange={(e) => setBaudiosUsb(e.target.value)}
                   min={300} max={3000000} step={300}
                   className={baudiosInvalido ? 'is-invalid' : ''} required />
            {baudiosInvalido && (
              <small className="red-panel__hint is-error">Rango 300 – 3 000 000.</small>
            )}
          </label>
        </fieldset>

        {mensajeError && (
          <div className="red-panel__alert is-error">
            <strong>Error:</strong> {mensajeError}
          </div>
        )}

        <div className="red-panel__actions">
          <button type="submit" className="red-panel__btn red-panel__btn--primary"
                  disabled={busy || !formValido}>
            {fase === 'guardando'
              ? <><span className="red-panel__spinner" /> Guardando…</>
              : '💾 Guardar configuración'}
          </button>
          <button type="button" className="red-panel__btn"
                  onClick={limpiarFormulario} disabled={busy}>
            ↺ Descartar cambios
          </button>
        </div>
      </form>

      {/* ── Banner post-guardado: muestra lo que devolvió el RPC ── */}
      {resultadoGuardar && fase !== 'reiniciado' && (
        <div className="red-panel__restart">
          <div className="red-panel__restart-info">
            <strong>✅ Guardado en la MicroSD</strong>
            <p>
              Config actual: <code>STA={resultadoGuardar.ssidSta}</code> ·{' '}
              <code>AP={resultadoGuardar.ssidAp}</code> ·{' '}
              <code>TCP={resultadoGuardar.puertoTcp}</code> ·{' '}
              <code>UDP={resultadoGuardar.puertoUdp}</code> ·{' '}
              <code>Baud={resultadoGuardar.baudiosUsb}</code>
            </p>
            <small>
              ID {resultadoGuardar.idConfiguracionConectividad} · activa={resultadoGuardar.activa} ·{' '}
              modificado: {fmtTs(resultadoGuardar.fechaModificacion)}
            </small>
          </div>
          <button type="button" className="red-panel__btn red-panel__btn--danger"
                  onClick={() => void reiniciar()} disabled={busy}>
            {fase === 'reiniciando'
              ? <><span className="red-panel__spinner" /> Reiniciando…</>
              : '🔄 Reiniciar firmware ahora'}
          </button>
        </div>
      )}

      {fase === 'reiniciado' && (
        <div className="red-panel__alert is-success">
          <strong>🔄 Reinicio enviado.</strong> El ESP rearranca en 2–3 s.
        </div>
      )}
    </section>
  );
}

// =========================================================================
// SUBCOMPONENTE
// =========================================================================
function EstadoBadge({
  titulo, activo, linea1, linea2, color,
}: {
  titulo: string; activo: boolean; linea1: string; linea2: string;
  color: 'blue' | 'green' | 'purple' | 'gray';
}) {
  return (
    <div className="red-panel__badge" data-color={color} data-activo={activo}>
      <div className="red-panel__badge-titulo">{titulo}</div>
      <div className="red-panel__badge-linea1">{linea1}</div>
      <div className="red-panel__badge-linea2">{linea2}</div>
      <div className="red-panel__badge-dot" />
    </div>
  );
}