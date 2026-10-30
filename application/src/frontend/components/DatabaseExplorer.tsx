import { useCallback, useEffect, useState } from 'react';
import './DatabaseExplorer.css';

// =========================================================================
// TIPOS
// =========================================================================
type TabId = 'sensores' | 'sensos' | 'eventos' | 'auditorias' | 'correos' | 'smtp' | 'red';

interface CredencialRpcDTO {
  emisorCertificado: string;
  firmaCertificado: string;
  username: string;
  pinHash: string;
}

const CREDENCIAL_DEFAULT: CredencialRpcDTO = {
  emisorCertificado: 'CA-UNTDF-LAB',
  firmaCertificado: 'CERT-TLS-UNTDF-2026-VALIDO',
  username: 'silvana_admin',
  pinHash: 'HASH_ADMIN_2026',
};

// --- HELPERS (sustituye los existentes) ---
const fmtTs = (unixSeg?: number | null): string => {
  if (!unixSeg) return '—';
  return new Date(unixSeg * 1000).toLocaleString();
};

const fmtTemp = (n?: number | null): string => {
  if (!Number.isFinite(n)) return '—';   // cubre undefined, null, NaN
  return `${(n as number).toFixed(2)} °C`;
};

// NEW: número genérico con decimales configurables
const fmtNum = (n?: number | null, decimals = 2): string => {
  if (!Number.isFinite(n)) return '—';
  return (n as number).toFixed(decimals);
};

// =========================================================================
// COMPONENTE PRINCIPAL
// =========================================================================
export function DatabaseExplorer() {
  const [tab, setTab] = useState<TabId>('sensores');
  const [cred] = useState<CredencialRpcDTO>(CREDENCIAL_DEFAULT);

  return (
    <section className="dbx">
      {/* ─── HEADER ─────────────────────────────────────────────────── */}
      <header className="dbx__header">
        <div className="dbx__header-title">
          <span className="dbx__header-icon" aria-hidden>🗄️</span>
          <div>
            <h2>Explorador del Firmware SGBD</h2>
            <p>
              Las consultas viajan <strong>Frontend → IPC → Backend → RPC (Capa 7) → MicroSD del ESP8266</strong>.
              Cada pestaña documenta la fuente, la query equivalente y el resultado real.
            </p>
          </div>
        </div>

        <div className="dbx__credentials">
          <div className="dbx__cred-item">
            <span className="dbx__cred-label">Usuario</span>
            <code>{cred.username}</code>
          </div>
          <div className="dbx__cred-item">
            <span className="dbx__cred-label">PIN Hash</span>
            <code>{cred.pinHash}</code>
          </div>
          <div className="dbx__cred-item">
            <span className="dbx__cred-label">Certificado</span>
            <code>{cred.firmaCertificado.slice(0, 24)}…</code>
          </div>
        </div>
      </header>

      {/* ─── FLOW DIAGRAM ───────────────────────────────────────────── */}
      <div className="dbx__flow" aria-label="Diagrama de flujo">
        <span className="dbx__flow-node dbx__flow-node--fe">Frontend</span>
        <span className="dbx__flow-arrow">→</span>
        <span className="dbx__flow-node dbx__flow-node--ipc">IPC</span>
        <span className="dbx__flow-arrow">→</span>
        <span className="dbx__flow-node dbx__flow-node--be">Backend</span>
        <span className="dbx__flow-arrow">→</span>
        <span className="dbx__flow-node dbx__flow-node--rpc">RPC (Capa 7)</span>
        <span className="dbx__flow-arrow">→</span>
        <span className="dbx__flow-node dbx__flow-node--fw">Firmware SGBD</span>
        <span className="dbx__flow-arrow">→</span>
        <span className="dbx__flow-node dbx__flow-node--sd">MicroSD</span>
      </div>

      {/* ─── TABS ───────────────────────────────────────────────────── */}
      <nav className="dbx__tabs" role="tablist">
        {(
          [
            { id: 'sensores',   label: 'Sensores',     icon: '🌡️' },
            { id: 'sensos',     label: 'Sensos',       icon: '📈' },
            { id: 'eventos',    label: 'Eventos',      icon: '⚡' },
            { id: 'auditorias', label: 'Auditorías',   icon: '📋' },
            { id: 'correos',    label: 'Correos',      icon: '✉️' },
            { id: 'smtp',       label: 'Config SMTP',  icon: '📨' },
            { id: 'red',        label: 'Red / NTP',    icon: '📶' },
          ] as const
        ).map((t) => (
          <button
            key={t.id}
            role="tab"
            aria-selected={tab === t.id}
            className="dbx__tab"
            data-active={tab === t.id}
            onClick={() => setTab(t.id)}
          >
            <span aria-hidden>{t.icon}</span>
            <span>{t.label}</span>
          </button>
        ))}
      </nav>

      {/* ─── CONTENIDO ──────────────────────────────────────────────── */}
      <div className="dbx__content">
        {tab === 'sensores'   && <SensoresTab   cred={cred} />}
        {tab === 'sensos'     && <SensosTab     cred={cred} />}
        {tab === 'eventos'    && <EventosTab    cred={cred} />}
        {tab === 'auditorias' && <AuditoriasTab cred={cred} />}
        {tab === 'correos'    && <CorreosTab    cred={cred} />}
        {tab === 'smtp'       && <SmtpTab       cred={cred} />}
        {tab === 'red'        && <RedNtpTab     cred={cred} />}
      </div>
    </section>
  );
}

// =========================================================================
// COMPONENTES AUXILIARES COMUNES
// =========================================================================
interface TabShellProps {
  icon: string;
  title: string;
  description: string;
  sources: Array<{ name: string; code: string; where: 'firmware' | 'backend' | 'mixed' }>;
  children: React.ReactNode;
  onRefresh: () => void;
  loading: boolean;
  error: string | null;
}

function TabShell({ icon, title, description, sources, children, onRefresh, loading, error }: TabShellProps) {
  return (
    <div className="dbx-tab">
      <header className="dbx-tab__header">
        <div className="dbx-tab__title">
          <span aria-hidden className="dbx-tab__icon">{icon}</span>
          <div>
            <h3>{title}</h3>
            <p>{description}</p>
          </div>
        </div>
        <button
          type="button"
          className="dbx-tab__refresh"
          onClick={onRefresh}
          disabled={loading}
        >
          {loading ? '⏳ Consultando…' : '⟳ Refrescar'}
        </button>
      </header>

      <div className="dbx-tab__sources">
        {sources.map((s) => (
          <div key={s.name} className={`dbx-source dbx-source--${s.where}`}>
            <div className="dbx-source__name">{s.name}</div>
            <code className="dbx-source__code">{s.code}</code>
          </div>
        ))}
      </div>

      {error && (
        <div className="dbx-tab__error">
          <strong>❌ Error:</strong> {error}
        </div>
      )}

      <div className="dbx-tab__body">{children}</div>
    </div>
  );
}

function EmptyState({ message }: { message: string }) {
  return <div className="dbx-empty">{message}</div>;
}

// =========================================================================
// TAB 1 — SENSORES (fusión firmware + backend)
// =========================================================================
interface SensorEnriquecido {
  idSensor: number;
  hexadecimal: string;
  errorSuperior: number;
  errorInferior: number;
  umbralSuperior: number;
  umbralInferior: number;
  activa: number;
  fechaCreacion: number;
  fechaModificacion: number;
  nombre: string;
  descripcion: string | null;
  ubicacion: string;
  ubicacionDescripcion: string | null;
}

function SensoresTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<SensorEnriquecido[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const filas = await window.service.client.obtenerSensoresEnriquecidosConFirmware(cred);
      setData(filas as SensorEnriquecido[]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData([]);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="🌡️"
      title="Sensores (fusión Firmware + Backend)"
      description="El firmware lista sus sensores físicos (MicroSD). El backend enriquece cada fila con su nombre y ubicación (SQLite local). La fusión se hace por la dirección hexadecimal única."
      sources={[
        { name: 'Firmware (MicroSD)', code: 'SELECT * FROM sensor WHERE activa = 1', where: 'firmware' },
        { name: 'Backend (Prisma)',   code: 'prisma.sensor.findMany()',                where: 'backend' },
        { name: 'Fusión por hexadecimal', code: 'Map<hex, meta> + spread',             where: 'mixed' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       data.length === 0 ? <EmptyState message="No hay sensores registrados." /> : (
        <table className="dbx-table">
          <thead>
            <tr>
              <th>#</th>
              <th>Hexadecimal</th>
              <th>Nombre</th>
              <th>Ubicación</th>
              <th>Umbral (°C)</th>
              <th>Error (°C)</th>
              <th>Activa</th>
              <th>Creado</th>
            </tr>
          </thead>
          <tbody>
            {data.map((s) => (
              <tr key={s.hexadecimal}>
                <td className="dbx-table__num">{s.idSensor}</td>
                <td><code className="dbx-hex">{s.hexadecimal}</code></td>
                <td>
                  {s.nombre}
                  {s.descripcion && <div className="dbx-muted">{s.descripcion}</div>}
                </td>
                <td>
                  {s.ubicacion}
                  {s.ubicacionDescripcion && <div className="dbx-muted">{s.ubicacionDescripcion}</div>}
                </td>
                <td className="dbx-table__num">
                  {fmtNum(s.umbralInferior, 1)} / {fmtNum(s.umbralSuperior, 1)}
                </td>
                <td className="dbx-table__num">
                  {fmtNum(s.errorInferior, 2)} / {fmtNum(s.errorSuperior, 2)}
                </td>
                <td>
                  <span className={`dbx-pill ${s.activa ? 'dbx-pill--ok' : 'dbx-pill--off'}`}>
                    {s.activa ? 'Sí' : 'No'}
                  </span>
                </td>
                <td className="dbx-muted">{fmtTs(s.fechaCreacion)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 2 — SENSOS
// =========================================================================
interface Senso {
  idSenso: number;
  idSensor: number;
  temperaturaCruda: number;
  temperaturaCorregida: number;
  estadoSensor: number;
  fechaCreacion: number;
}

const ESTADO_SENSOR_LABEL: Record<number, string> = {
  0: 'OK',
  1: 'Desconexión 1-Wire',
  2: 'Ruido',
  3: 'Umbral inferior',
  4: 'Umbral superior',
};

function SensosTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<Senso[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const filas = await window.service.client.listarUltimosSensosFirmware(cred, 0, 15);
      setData(filas as Senso[]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData([]);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="📈"
      title="Últimos Sensos (telemetría)"
      description="El firmware registra una medición cada N segundos en la tabla senso (anillo circular de 1024 filas). Esta vista pide las últimas 15 al firmware."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_ULTIMOS_SENSOS(cred, 0, 15)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'senso.bin → ModeloCircular.recorrerActivos()', where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       data.length === 0 ? <EmptyState message="Sin sensos registrados aún." /> : (
        <table className="dbx-table">
          <thead>
            <tr>
              <th>#</th>
              <th>Sensor</th>
              <th>Cruda</th>
              <th>Corregida</th>
              <th>Estado</th>
              <th>Timestamp</th>
            </tr>
          </thead>
          <tbody>
            {data.map((s) => (
              <tr key={s.idSenso}>
                <td className="dbx-table__num">{s.idSenso}</td>
                <td className="dbx-table__num">{s.idSensor}</td>
                <td className="dbx-table__num">{fmtTemp(s.temperaturaCruda)}</td>
                <td className="dbx-table__num"><strong>{fmtTemp(s.temperaturaCorregida)}</strong></td>
                <td>
                  <span className={`dbx-pill ${s.estadoSensor === 0 ? 'dbx-pill--ok' : 'dbx-pill--warn'}`}>
                    {ESTADO_SENSOR_LABEL[s.estadoSensor] ?? `Estado ${s.estadoSensor}`}
                  </span>
                </td>
                <td className="dbx-muted">{fmtTs(s.fechaCreacion)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 3 — EVENTOS
// =========================================================================
interface Evento {
  idEventoSensor: number;
  idSensor: number;
  idSenso: number;
  estadoPrevioSensor: number;
  evento: number;
  criticidad: number;
  fechaCreacion: number;
}

const CRITICIDAD_LABEL: Record<number, string> = { 0: 'Baja', 1: 'Media', 2: 'Crítica' };
const CRITICIDAD_CLASS: Record<number, string> = { 0: 'dbx-pill--ok', 1: 'dbx-pill--warn', 2: 'dbx-pill--err' };

function EventosTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<Evento[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const filas = await window.service.client.listarUltimosEventosFirmware(cred, 0, 15);
      setData(filas as Evento[]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData([]);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="⚡"
      title="Últimos Eventos (transiciones confirmadas)"
      description="Cuando el estado de un sensor cambia y se sostiene tras un período anti-falsos-positivos, el firmware lo registra como evento. Cada fila referencia el senso que lo disparó."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_ULTIMOS_EVENTOS(cred, 0, 15)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'evento.bin → ModeloCircular',         where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       data.length === 0 ? <EmptyState message="Sin eventos registrados aún." /> : (
        <table className="dbx-table">
          <thead>
            <tr>
              <th>#</th>
              <th>Sensor</th>
              <th>Senso #</th>
              <th>Transición</th>
              <th>Criticidad</th>
              <th>Timestamp</th>
            </tr>
          </thead>
          <tbody>
            {data.map((e) => (
              <tr key={e.idEventoSensor}>
                <td className="dbx-table__num">{e.idEventoSensor}</td>
                <td className="dbx-table__num">{e.idSensor}</td>
                <td className="dbx-table__num">{e.idSenso}</td>
                <td>
                  <code>{ESTADO_SENSOR_LABEL[e.estadoPrevioSensor] ?? e.estadoPrevioSensor}</code>
                  {' → '}
                  <code>{ESTADO_SENSOR_LABEL[e.evento] ?? e.evento}</code>
                </td>
                <td>
                  <span className={`dbx-pill ${CRITICIDAD_CLASS[e.criticidad] ?? ''}`}>
                    {CRITICIDAD_LABEL[e.criticidad] ?? e.criticidad}
                  </span>
                </td>
                <td className="dbx-muted">{fmtTs(e.fechaCreacion)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 4 — AUDITORÍAS
// =========================================================================
interface Auditoria {
  idAuditoria: number;
  idUsuario: number;
  accion: string;
  detalle: string;
  fechaCreacion: number;
}

function AuditoriasTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<Auditoria[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const filas = await window.service.client.listarUltimasAuditoriasFirmware(cred, 20);
      setData(filas as Auditoria[]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData([]);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="📋"
      title="Auditoría del Firmware (append-only)"
      description="Cada acción sensible (login, cambio de umbral, reset, sync) queda registrada con el usuario y detalle. La tabla se escribe en la MicroSD y es independiente de la auditoría local del backend."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_ULTIMAS_AUDITORIAS(cred, 20)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'audit.bin → ModeloCircular',          where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       data.length === 0 ? <EmptyState message="Sin registros de auditoría aún." /> : (
        <table className="dbx-table">
          <thead>
            <tr>
              <th>#</th>
              <th>Usuario</th>
              <th>Acción</th>
              <th>Detalle</th>
              <th>Timestamp</th>
            </tr>
          </thead>
          <tbody>
            {data.map((a) => (
              <tr key={a.idAuditoria}>
                <td className="dbx-table__num">{a.idAuditoria}</td>
                <td className="dbx-table__num">{a.idUsuario}</td>
                <td><code>{a.accion}</code></td>
                <td>{a.detalle}</td>
                <td className="dbx-muted">{fmtTs(a.fechaCreacion)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 5 — CORREOS
// =========================================================================
interface Correo {
  idCorreo: number;
  nombre: string;
  apellido: string;
  correoElectronico: string;
  tipoRecepcionMensajes: number;
  estadoEnvio: number;
  fechaModificacion: number;
}

const RECEPCION_LABEL: Record<number, string> = {
  0: 'Todos', 1: 'Solo eventos', 2: 'Solo eventos críticos',
};
const ENVIO_LABEL: Record<number, string> = {
  0: 'Pendiente', 1: 'Enviado', 2: 'Fallido',
};
const ENVIO_CLASS: Record<number, string> = {
  0: 'dbx-pill--warn', 1: 'dbx-pill--ok', 2: 'dbx-pill--err',
};

function CorreosTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<Correo[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const filas = await window.service.client.listarCorreosFirmware(cred);
      setData(filas as Correo[]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData([]);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="✉️"
      title="Destinatarios de Correo"
      description="Lista de personas que reciben notificaciones del firmware. Cada una tiene un tipo de recepción (todos / eventos / solo críticos) y un estado de envío."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_LISTAR_CORREOS(cred)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'correos.bin → ModeloCatalogo', where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       data.length === 0 ? <EmptyState message="Sin destinatarios registrados." /> : (
        <table className="dbx-table">
          <thead>
            <tr>
              <th>#</th>
              <th>Nombre</th>
              <th>Email</th>
              <th>Recibe</th>
              <th>Estado envío</th>
              <th>Modificado</th>
            </tr>
          </thead>
          <tbody>
            {data.map((c) => (
              <tr key={c.idCorreo}>
                <td className="dbx-table__num">{c.idCorreo}</td>
                <td>{[c.nombre, c.apellido].filter(Boolean).join(' ')}</td>
                <td><code>{c.correoElectronico}</code></td>
                <td>{RECEPCION_LABEL[c.tipoRecepcionMensajes] ?? c.tipoRecepcionMensajes}</td>
                <td>
                  <span className={`dbx-pill ${ENVIO_CLASS[c.estadoEnvio] ?? ''}`}>
                    {ENVIO_LABEL[c.estadoEnvio] ?? c.estadoEnvio}
                  </span>
                </td>
                <td className="dbx-muted">{fmtTs(c.fechaModificacion)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 6 — CONFIG SMTP
// =========================================================================
interface ConfigSmtp {
  configurado: boolean;
  hostSmtp: string;
  puerto: number;
  usuario: string;
  emailRemitente: string;
  fechaModificacion: number;
}

function SmtpTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<ConfigSmtp | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const cfg = await window.service.client.obtenerConfigSmtpFirmware(cred);
      setData(cfg as ConfigSmtp);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData(null);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="📨"
      title="Configuración SMTP"
      description="Singleton PK=1 en la MicroSD del firmware. Almacena las credenciales para que el ESP envíe correos de alerta. La contraseña viaja cifrada por campo (Capa 7) y desencriptada en el firmware."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_OBTENER_SMTP(cred)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'smtp.bin → ModeloSingleton.obtener()', where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> :
       !data.configurado ? <EmptyState message="SMTP no configurado en el firmware." /> : (
        <div className="dbx-detail">
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Host</span>
            <code className="dbx-detail__value">{data.hostSmtp}</code>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Puerto</span>
            <span className="dbx-detail__value">{data.puerto}</span>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Usuario</span>
            <code className="dbx-detail__value">{data.usuario}</code>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Email remitente</span>
            <code className="dbx-detail__value">{data.emailRemitente}</code>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Última modificación</span>
            <span className="dbx-detail__value">{fmtTs(data.fechaModificacion)}</span>
          </div>
        </div>
      )}
    </TabShell>
  );
}

// =========================================================================
// TAB 7 — RED / NTP
// =========================================================================
interface ConfigRed {
  ssidSta: string;
  ssidAp: string;
  fechaModificacion: number;
  servidoresNtp: string[];
}

function RedNtpTab({ cred }: { cred: CredencialRpcDTO }) {
  const [data, setData] = useState<ConfigRed | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cargar = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const cfg = await window.service.client.obtenerConfigRedYNtpFirmware(cred);
      setData(cfg as ConfigRed);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setData(null);
    } finally {
      setLoading(false);
    }
  }, [cred]);

  useEffect(() => { void cargar(); }, [cargar]);

  return (
    <TabShell
      icon="📶"
      title="Red Wi-Fi y Servidores NTP"
      description="Credenciales Wi-Fi (STA + AP) y lista de servidores NTP persistidos en la MicroSD. Los passwords viajan cifrados por campo y no se muestran en la respuesta."
      sources={[
        { name: 'Firmware (RPC)', code: 'RPC_DB_OBTENER_RED_Y_NTP(cred)', where: 'firmware' },
        { name: 'MicroSD (SGBD)', code: 'wifi.bin + ntp.bin', where: 'firmware' },
      ]}
      onRefresh={cargar}
      loading={loading}
      error={error}
    >
      {!data ? <EmptyState message="Cargando…" /> : (
        <div className="dbx-detail">
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">SSID STA (cliente)</span>
            <code className="dbx-detail__value">{data.ssidSta || '—'}</code>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">SSID AP (access point)</span>
            <code className="dbx-detail__value">{data.ssidAp || '—'}</code>
          </div>
          <div className="dbx-detail__row">
            <span className="dbx-detail__label">Última modificación</span>
            <span className="dbx-detail__value">{fmtTs(data.fechaModificacion)}</span>
          </div>
          <div className="dbx-detail__row dbx-detail__row--column">
            <span className="dbx-detail__label">Servidores NTP</span>
            {data.servidoresNtp.length === 0 ? (
              <span className="dbx-muted">Ninguno configurado.</span>
            ) : (
              <ul className="dbx-ntp-list">
                {data.servidoresNtp.map((s, i) => (
                  <li key={`${s}-${i}`}>
                    <code>{s}</code>
                  </li>
                ))}
              </ul>
            )}
          </div>
        </div>
      )}
    </TabShell>
  );
}