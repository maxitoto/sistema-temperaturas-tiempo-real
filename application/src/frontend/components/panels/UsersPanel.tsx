import { useEffect, useState } from 'react';
import { Badge, Button, Card, CardBody, CardHeader, EmptyState, ErrorBanner, Input, Label, Spinner } from '../ui';

interface ResumenUsuario {
    idUsuario: number; usuario: string; rol: number;
    intentosFallidos: number; bloqueado: boolean; activa: boolean;
    actualizadoEpoch: number;
}

interface PerfilUsuario {
    idUsuario: number; usuario: string; rol: number;
    intentosFallidos: number; bloqueado: boolean; activa: boolean;
    creadoEl: string; actualizadoEl: string;
}

interface Estadisticas {
    totalUsuarios: number; usuariosActivos: number;
    usuariosBloqueados: number; maxIntentosFallidos: number;
}

const ROLES = ['ANONIMO', 'LECTOR', 'OPERADOR', 'ADMIN'];

export function UsersPanel() {
    const [usuarios, setUsuarios] = useState<ResumenUsuario[]>([]);
    const [stats, setStats] = useState<Estadisticas | null>(null);
    const [soloActivos, setSoloActivos] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [busy, setBusy] = useState<string | null>(null);

    const [nuevoUsuario, setNuevoUsuario] = useState('');
    const [nuevaClave, setNuevaClave] = useState('');
    const [nuevoRol, setNuevoRol] = useState(1);

    useEffect(() => { recargar(); }, [soloActivos]);

    async function recargar() {
        setBusy('list'); setError(null);
        try {
            const [lista, s] = await Promise.all([
                window.service.client.listarUsuarios(32, 0, soloActivos),
                window.service.client.obtenerEstadisticasUsuarios(),
            ]);
            setUsuarios(lista);
            setStats(s);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function crear() {
        if (!nuevoUsuario || !nuevaClave) return;
        setBusy('create'); setError(null);
        try {
            await window.service.client.crearUsuario(nuevoUsuario, nuevaClave, nuevoRol);
            setNuevoUsuario(''); setNuevaClave(''); setNuevoRol(1);
            await recargar();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function toggleEstado(u: ResumenUsuario) {
        setBusy(`toggle-${u.usuario}`); setError(null);
        try {
            await window.service.client.actualizarRolYEstadoUsuario(u.usuario, u.rol, !u.activa);
            await recargar();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function desbloquear(u: ResumenUsuario) {
        setBusy(`unlock-${u.usuario}`); setError(null);
        try {
            await window.service.client.desbloquearUsuario(u.usuario);
            await recargar();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function eliminar(u: ResumenUsuario) {
        if (!confirm(`¿Eliminar al usuario "${u.usuario}"?`)) return;
        setBusy(`del-${u.usuario}`); setError(null);
        try {
            await window.service.client.eliminarUsuario(u.usuario);
            await recargar();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    return (
        <div className="space-y-4">
            {error && <ErrorBanner message={error} onClose={() => setError(null)} />}

            {stats && (
                <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
                    <Stat label="Total" value={stats.totalUsuarios} />
                    <Stat label="Activos" value={stats.usuariosActivos} tone="success" />
                    <Stat label="Bloqueados" value={stats.usuariosBloqueados} tone="danger" />
                    <Stat label="Máx. intentos" value={stats.maxIntentosFallidos} tone="warning" />
                </div>
            )}

            <Card>
                <CardHeader
                    title="Usuarios"
                    description="Máx. 32 registros por página"
                    action={
                        <div className="flex items-center gap-2">
                            <label className="flex items-center gap-1.5 text-xs text-muted-foreground">
                                <input type="checkbox" checked={soloActivos} onChange={e => setSoloActivos(e.target.checked)} />
                                Solo activos
                            </label>
                            <Button size="sm" variant="secondary" onClick={recargar} disabled={busy === 'list'}>
                                {busy === 'list' ? <Spinner /> : null} Refrescar
                            </Button>
                        </div>
                    }
                />
                <CardBody>
                    {usuarios.length === 0 ? (
                        <EmptyState title="Sin usuarios" message="No hay registros o el filtro no coincide." />
                    ) : (
                        <div className="overflow-x-auto">
                            <table className="w-full text-left text-xs">
                                <thead className="text-muted-foreground">
                                    <tr className="border-b border-border">
                                        <th className="py-2 pr-3">ID</th>
                                        <th className="py-2 pr-3">Usuario</th>
                                        <th className="py-2 pr-3">Rol</th>
                                        <th className="py-2 pr-3">Estado</th>
                                        <th className="py-2 pr-3">Fallos</th>
                                        <th className="py-2 text-right">Acciones</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {usuarios.map(u => (
                                        <tr key={u.idUsuario} className="border-b border-border/50">
                                            <td className="py-2 pr-3 font-mono text-muted-foreground">{u.idUsuario}</td>
                                            <td className="py-2 pr-3 font-medium">{u.usuario}</td>
                                            <td className="py-2 pr-3"><Badge tone="info">{ROLES[u.rol] ?? u.rol}</Badge></td>
                                            <td className="py-2 pr-3 space-x-1">
                                                <Badge tone={u.activa ? 'success' : 'neutral'}>{u.activa ? 'Activo' : 'Inactivo'}</Badge>
                                                {u.bloqueado && <Badge tone="danger">Bloqueado</Badge>}
                                            </td>
                                            <td className="py-2 pr-3">{u.intentosFallidos}</td>
                                            <td className="py-2 text-right">
                                                <div className="flex flex-wrap justify-end gap-1">
                                                    <Button size="sm" variant="secondary" onClick={() => toggleEstado(u)} disabled={busy !== null}>Toggle</Button>
                                                    {u.bloqueado && <Button size="sm" variant="secondary" onClick={() => desbloquear(u)} disabled={busy !== null}>Desbloquear</Button>}
                                                    <Button size="sm" variant="danger" onClick={() => eliminar(u)} disabled={busy !== null}>Eliminar</Button>
                                                </div>
                                            </td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                    )}
                </CardBody>
            </Card>

            <Card>
                <CardHeader title="Crear usuario" />
                <CardBody className="space-y-3">
                    <div>
                        <Label htmlFor="nu">Usuario</Label>
                        <Input id="nu" value={nuevoUsuario} onChange={e => setNuevoUsuario(e.target.value)} />
                    </div>
                    <div>
                        <Label htmlFor="nc">Contraseña</Label>
                        <Input id="nc" type="password" value={nuevaClave} onChange={e => setNuevaClave(e.target.value)} />
                    </div>
                    <div>
                        <Label htmlFor="nr">Rol</Label>
                        <select
                            id="nr"
                            value={nuevoRol}
                            onChange={e => setNuevoRol(Number(e.target.value))}
                            className="w-full rounded-md border border-border bg-background px-3 py-2 text-sm"
                        >
                            {ROLES.map((r, i) => <option key={i} value={i}>{r}</option>)}
                        </select>
                    </div>
                    <Button onClick={crear} disabled={busy !== null || !nuevoUsuario || !nuevaClave}>
                        {busy === 'create' ? <Spinner /> : null} Crear
                    </Button>
                </CardBody>
            </Card>

            <PasswordChanger usuarios={usuarios} />
        </div>
    );
}

function PasswordChanger({ usuarios }: { usuarios: ResumenUsuario[] }) {
    const [sel, setSel] = useState<string>('');
    const [actual, setActual] = useState('');
    const [nueva, setNueva] = useState('');
    const [busy, setBusy] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [ok, setOk] = useState(false);

    async function aplicar() {
        if (!sel || !actual || !nueva) return;
        setBusy(true); setError(null); setOk(false);
        try {
            await window.service.client.cambiarContrasenaUsuario(sel, actual, nueva);
            setActual(''); setNueva(''); setOk(true);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(false); }
    }

    return (
        <Card>
            <CardHeader title="Cambiar contraseña" description="Requiere la clave actual del usuario" />
            <CardBody className="space-y-3">
                {error && <ErrorBanner message={error} onClose={() => setError(null)} />}
                {ok && <div className="rounded-md border border-success/30 bg-success-light px-3 py-2 text-xs text-success">Contraseña actualizada.</div>}
                <div>
                    <Label htmlFor="pw-user">Usuario</Label>
                    <select
                        id="pw-user"
                        value={sel}
                        onChange={e => setSel(e.target.value)}
                        className="w-full rounded-md border border-border bg-background px-3 py-2 text-sm"
                    >
                        <option value="">(seleccionar)</option>
                        {usuarios.map(u => <option key={u.idUsuario} value={u.usuario}>{u.usuario}</option>)}
                    </select>
                </div>
                <div>
                    <Label htmlFor="pw-cur">Contraseña actual</Label>
                    <Input id="pw-cur" type="password" value={actual} onChange={e => setActual(e.target.value)} />
                </div>
                <div>
                    <Label htmlFor="pw-new">Contraseña nueva</Label>
                    <Input id="pw-new" type="password" value={nueva} onChange={e => setNueva(e.target.value)} />
                </div>
                <Button onClick={aplicar} disabled={busy || !sel || !actual || !nueva}>
                    {busy ? <Spinner /> : null} Cambiar contraseña
                </Button>
            </CardBody>
        </Card>
    );
}

function Stat({ label, value, tone = 'neutral' }: { label: string; value: number; tone?: 'success' | 'danger' | 'warning' | 'neutral' }) {
    const tones = {
        success: 'border-success/30 text-success',
        danger:  'border-error/30 text-error',
        warning: 'border-warning/30 text-warning',
        neutral: 'border-border text-foreground',
    };
    return (
        <div className={`rounded-lg border bg-card px-3 py-2 ${tones[tone]}`}>
            <div className="text-[10px] uppercase tracking-wide opacity-70">{label}</div>
            <div className="text-lg font-semibold">{value}</div>
        </div>
    );
}