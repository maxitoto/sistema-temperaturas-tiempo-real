import { useCallback, useEffect, useRef, useState } from 'react';
import './ConfiguracionRedPanel.css';

// =========================================================================
// TIPOS LOCALES
// =========================================================================
interface CredencialRpcDTO {
  emisorCertificado: string;
  firmaCertificado: string;
  username: string;
  pinHash: string;
}

interface EstadoRed {
  ssidSta: string;
  ssidAp: string;
  puertoUdp: number;
  fechaModificacion: number;
  staConectado: boolean;
  ipSta: string;
  apActivo: boolean;
  ipAp: string;
  macWiFi: string;
  clientesUdpActivos: number;
  servidoresNtp: string[];
}

interface ResultadoGuardar {
  ssidSta: string;
  ssidAp: string;
  puertoUdp: number;
  fechaModificacion: number;
  requiereReinicio: boolean;
  mensaje: string;
}

type Fase = 'idle' | 'cargando' | 'guardando' | 'guardado' | 'reiniciando' | 'reiniciado' | 'error';

const CREDENCIAL_DEFAULT: CredencialRpcDTO = {
  emisorCertificado: 'CA-UNTDF-LAB',
  firmaCertificado: 'CERT-TLS-UNTDF-2026-VALIDO',
  username: 'silvana_admin',
  pinHash: 'HASH_ADMIN_2026',
};

const PUERTO_UDP_DEFAULT = 81;

// =========================================================================
// COMPONENTE
// =========================================================================
export function ConfiguracionRedPanel() {
  const [credencial] = useState<CredencialRpcDTO>(CREDENCIAL_DEFAULT);

  // Estado actual del firmware (lo que está aplicado en este momento)
  const [estadoRed, setEstadoRed] = useState<EstadoRed | null>(null);
  const [errorCarga, setErrorCarga] = useState<string | null>(null);

  // Formulario
  const [ssidSta, setSsidSta] = useState('');
  const [passSta, setPassSta] = useState('');
  const [ssidAp, setSsidAp] = useState('');
  const [passAp, setPassAp] = useState('');
  const [puertoUdp, setPuertoUdp] = useState<string>(String(PUERTO_UDP_DEFAULT));

  // Control de fase
  const [fase, setFase] = useState<Fase>('idle');
  const [mensajeError, setMensajeError] = useState<string | null>(null);
  const [resultadoGuardar, setResultadoGuardar] = useState<ResultadoGuardar | null>(null);
  const inFlightRef = useRef(false);

  // ─── Cargar estado actual ────────────────────────────────────────
  const cargar = useCallback(async () => {
    setFase('cargando');
    setErrorCarga(null);
    try {
      const r = await window.service.client.obtenerConfigRedYNtpFirmware(credencial);
      setEstadoRed(r as EstadoRed);
      // Pre-rellenar los SSID (no las contraseñas: no viajan en claro por seguridad)
      setSsidSta(r.ssidSta ?? '');
      setSsidAp(r.ssidAp ?? '');
      setPuertoUdp(String(r.puertoUdp ?? PUERTO_UDP_DEFAULT));
      setFase('idle');
    } catch (e) {
      setErrorCarga(e instanceof Error ? e.message : String(e));
      setFase('error');
    }
  }, [credencial]);

  useEffect(() => { void cargar(); }, [cargar]);

  // ─── Validación local ────────────────────────────────────────────
  const passApInvalida = passAp.length > 0 && passAp.length < 8;
  const puertoInvalido = (() => {
    const n = Number(puertoUdp);
    return !Number.isInteger(n) || n <= 0 || n > 65535;
  })();
  const formValido =
    ssidSta.trim().length > 0 &&
    ssidAp.trim().length > 0 &&
    !passApInvalida &&
    !puertoInvalido;

  // ─── Guardar ─────────────────────────────────────────────────────
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
      const r = await window.service.client.guardarConfigRedFirmware({
        credencial,
        ssidSta: ssidSta.trim(),
        passwordStaPlana: passSta,
        ssidAp: ssidAp.trim(),
        passwordApPlana: passAp,
        puertoUdp: Number(puertoUdp),
      });
      setResultadoGuardar(r as ResultadoGuardar);
      setFase('guardado');
      // Refrescar el estado a la espera del reinicio
      void cargar();
    } catch (e) {
      setMensajeError(e instanceof Error ? e.message : String(e));
      setFase('error');
    } finally {
      inFlightRef.current = false;
    }
  }, [credencial, ssidSta, passSta, ssidAp, passAp, puertoUdp, formValido, cargar]);

  // ─── Reiniciar ───────────────────────────────────────────────────
  const reiniciar = useCallback(async () => {
    if (inFlightRef.current) return;
    inFlightRef.current = true;
    setMensajeError(null);
    setFase('reiniciando');
    try {
      const r = await window.service.client.reiniciarFirmware(credencial);
      if (!r.reiniciando) {
        throw new Error('El firmware rechazó la solicitud de reinicio.');
      }
      setFase('reiniciado');
      // Esperar ~2.5s para que el ESP complete el reboot y volver a cargar estado
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
    setSsidSta(estadoRed.ssidSta ?? '');
    setSsidAp(estadoRed.ssidAp ?? '');
    setPassSta('');
    setPassAp('');
    setPuertoUdp(String(estadoRed.puertoUdp ?? PUERTO_UDP_DEFAULT));
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
              Escribe la configuración en <code>/db/wifi.bin</code> del ESP y reinicia para aplicarla.
            </p>
          </div>
        </div>
        <button
          type="button"
          className="red-panel__refresh"
          onClick={() => void cargar()}
          disabled={busy}
          title="Recargar estado actual desde el firmware"
        >
          {fase === 'cargando' ? '⏳' : '⟳'}
        </button>
      </header>

      {/* ── Estado actual del firmware ─────────────────────────── */}
      {errorCarga && (
        <div className="red-panel__alert is-error">
          <strong>No se pudo leer el estado:</strong> {errorCarga}
        </div>
      )}

      {estadoRed && (
        <div className="red-panel__status">
          <EstadoBadge
            titulo="STA (Cliente)"
            activo={estadoRed.staConectado}
            linea1={estadoRed.staConectado ? estadoRed.ipSta : 'No conectado'}
            linea2={estadoRed.ssidSta || '—'}
            color="blue"
          />
          <EstadoBadge
            titulo="AP (Punto de acceso)"
            activo={estadoRed.apActivo}
            linea1={estadoRed.apActivo ? estadoRed.ipAp : 'Inactivo'}
            linea2={estadoRed.ssidAp || '—'}
            color="green"
          />
          <EstadoBadge
            titulo="Puerto UDP"
            activo
            linea1={String(estadoRed.puertoUdp)}
            linea2={`${estadoRed.clientesUdpActivos} cliente(s)`}
            color="purple"
          />
          <EstadoBadge
            titulo="MAC WiFi"
            activo
            linea1={estadoRed.macWiFi || '—'}
            linea2={`Modificado: ${new Date(estadoRed.fechaModificacion * 1000).toLocaleString()}`}
            color="gray"
          />
        </div>
      )}

      {/* ── Formulario ──────────────────────────────────────────── */}
      <form
        className="red-panel__form"
        onSubmit={(e) => { e.preventDefault(); void guardar(); }}
      >
        {/* Bloque STA */}
        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend>
            <span className="red-panel__fieldset-icon">🌐</span>
            Red STA — se conecta a la red del edificio
          </legend>

          <label className="red-panel__field">
            <span>SSID</span>
            <input
              type="text"
              value={ssidSta}
              onChange={(e) => setSsidSta(e.target.value)}
              placeholder="Wokwi-GUEST"
              maxLength={32}
              autoComplete="off"
              required
            />
          </label>

          <label className="red-panel__field">
            <span>Contraseña</span>
            <input
              type="password"
              value={passSta}
              onChange={(e) => setPassSta(e.target.value)}
              placeholder="dejar vacío si la red es abierta"
              maxLength={64}
              autoComplete="new-password"
            />
          </label>
        </fieldset>

        {/* Bloque AP */}
        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend>
            <span className="red-panel__fieldset-icon">📡</span>
            Red AP — el ESP crea su propia red
          </legend>

          <label className="red-panel__field">
            <span>SSID del AP</span>
            <input
              type="text"
              value={ssidAp}
              onChange={(e) => setSsidAp(e.target.value)}
              placeholder="Sensor-Refri-01"
              maxLength={32}
              autoComplete="off"
              required
            />
          </label>

          <label className="red-panel__field">
            <span>Contraseña del AP</span>
            <input
              type="password"
              value={passAp}
              onChange={(e) => setPassAp(e.target.value)}
              placeholder="≥ 8 caracteres (o vacío para red abierta)"
              maxLength={64}
              autoComplete="new-password"
              className={passApInvalida ? 'is-invalid' : ''}
            />
            {passApInvalida && (
              <small className="red-panel__hint is-error">
                WPA2 requiere al menos 8 caracteres, o dejalo vacío para red abierta.
              </small>
            )}
          </label>
        </fieldset>

        {/* Bloque Puerto UDP */}
        <fieldset className="red-panel__fieldset" disabled={busy}>
          <legend>
            <span className="red-panel__fieldset-icon">🔌</span>
            Puerto UDP del servidor
          </legend>

          <label className="red-panel__field">
            <span>Puerto (1 – 65535)</span>
            <input
              type="number"
              value={puertoUdp}
              onChange={(e) => setPuertoUdp(e.target.value)}
              min={1}
              max={65535}
              className={puertoInvalido ? 'is-invalid' : ''}
              required
            />
            {puertoInvalido && (
              <small className="red-panel__hint is-error">
                Debe ser un entero entre 1 y 65535.
              </small>
            )}
            <small className="red-panel__hint">
              El gateway de Wokwi debe apuntar a este puerto con <code>--forward udp:9081:10.13.37.X:{puertoUdp || '81'}</code>
            </small>
          </label>
        </fieldset>

        {/* Errores + acciones */}
        {mensajeError && (
          <div className="red-panel__alert is-error">
            <strong>Error:</strong> {mensajeError}
          </div>
        )}

        <div className="red-panel__actions">
          <button
            type="submit"
            className="red-panel__btn red-panel__btn--primary"
            disabled={busy || !formValido}
          >
            {fase === 'guardando'
              ? <><span className="red-panel__spinner" /> Guardando…</>
              : '💾 Guardar en /db/wifi.bin'}
          </button>

          <button
            type="button"
            className="red-panel__btn"
            onClick={limpiarFormulario}
            disabled={busy}
          >
            ↺ Descartar cambios
          </button>
        </div>
      </form>

      {/* ── Banner de reinicio (aparece después de guardar) ─────── */}
      {resultadoGuardar && fase !== 'reiniciado' && (
        <div className="red-panel__restart">
          <div className="red-panel__restart-info">
            <strong>✅ Guardado en la MicroSD</strong>
            <p>{resultadoGuardar.mensaje}</p>
            <small>
              Config actual: <code>STA={resultadoGuardar.ssidSta}</code> ·{' '}
              <code>AP={resultadoGuardar.ssidAp}</code> ·{' '}
              <code>UDP={resultadoGuardar.puertoUdp}</code>
            </small>
          </div>
          <button
            type="button"
            className="red-panel__btn red-panel__btn--danger"
            onClick={() => void reiniciar()}
            disabled={busy}
          >
            {fase === 'reiniciando'
              ? <><span className="red-panel__spinner" /> Reiniciando…</>
              : '🔄 Reiniciar firmware ahora'}
          </button>
        </div>
      )}

      {fase === 'reiniciado' && (
        <div className="red-panel__alert is-success">
          <strong>🔄 Reinicio enviado.</strong> El ESP está rearrancando. La nueva
          configuración de red se aplicará en 2–3 segundos. Se recargará el estado
          automáticamente.
        </div>
      )}
    </section>
  );
}

// =========================================================================
// SUBCOMPONENTE: badge de estado
// =========================================================================
function EstadoBadge({
  titulo, activo, linea1, linea2, color,
}: {
  titulo: string;
  activo: boolean;
  linea1: string;
  linea2: string;
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