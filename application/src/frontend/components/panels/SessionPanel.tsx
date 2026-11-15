import { useState } from 'react';
import { Badge, Button, Card, CardBody, CardHeader, ErrorBanner, Input, Label, Spinner } from '../ui';

const ROLES = ['ANONIMO', 'LECTOR', 'OPERADOR', 'ADMIN'];

export function SessionPanel({ onSessionChange }: { onSessionChange?: (active: boolean) => void }) {
    const [usuario, setUsuario] = useState('');
    const [clave, setClave] = useState('');
    const [sesion, setSesion] = useState<{ rolAsignado: number; tiempoExpiracionSegundos: number } | null>(null);
    const [busy, setBusy] = useState(false);
    const [error, setError] = useState<string | null>(null);

    async function login() {
        setBusy(true); setError(null);
        try {
            const s = await window.service.client.iniciarSesion(usuario, clave);
            setSesion(s);
            setClave('');
            onSessionChange?.(true);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(false); }
    }

    async function logout() {
        setBusy(true); setError(null);
        try {
            await window.service.client.cerrarSesion();
            setSesion(null);
            onSessionChange?.(false);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(false); }
    }

    return (
        <div className="space-y-4">
            {error && <ErrorBanner message={error} onClose={() => setError(null)} />}

            <Card>
                <CardHeader
                    title="Sesión"
                    description="Autenticación (L2 · RPAuth)"
                    action={<Badge tone={sesion ? 'success' : 'neutral'}>{sesion ? 'Autenticado' : 'Sin sesión'}</Badge>}
                />
                <CardBody className="space-y-3">
                    {sesion ? (
                        <div className="space-y-3">
                            <div className="rounded-md border border-border bg-muted/50 px-3 py-2 text-xs">
                                <div><span className="text-muted-foreground">Rol asignado:</span> <span className="font-medium">{ROLES[sesion.rolAsignado] ?? sesion.rolAsignado}</span></div>
                                <div><span className="text-muted-foreground">Expira en:</span> <span className="font-medium">{sesion.tiempoExpiracionSegundos}s</span></div>
                            </div>
                            <Button variant="danger" onClick={logout} disabled={busy}>
                                {busy ? <Spinner /> : null} Cerrar sesión
                            </Button>
                        </div>
                    ) : (
                        <div className="space-y-3">
                            <div>
                                <Label htmlFor="login-usuario">Usuario</Label>
                                <Input id="login-usuario" value={usuario} onChange={e => setUsuario(e.target.value)} autoComplete="username" />
                            </div>
                            <div>
                                <Label htmlFor="login-clave">Contraseña</Label>
                                <Input id="login-clave" type="password" value={clave} onChange={e => setClave(e.target.value)} autoComplete="current-password" />
                            </div>
                            <Button onClick={login} disabled={busy || !usuario || !clave}>
                                {busy ? <Spinner /> : null} Iniciar sesión
                            </Button>
                        </div>
                    )}
                </CardBody>
            </Card>
        </div>
    );
}