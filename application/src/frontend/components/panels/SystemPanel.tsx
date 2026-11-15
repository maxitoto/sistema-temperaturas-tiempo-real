import { useEffect, useState } from 'react';
import { Button, Card, CardBody, CardHeader, ErrorBanner, Input, Label, Spinner } from '../ui';

interface Diagnostico { heapLibreBytes: number; uptimeSegundos: number; }
interface ConfigWiFi { ipSta: string; ipAp: string; }

export function SystemPanel() {
    const [diag, setDiag] = useState<Diagnostico | null>(null);
    const [ssidSta, setSsidSta] = useState('');
    const [claveSta, setClaveSta] = useState('');
    const [ssidAp, setSsidAp] = useState('');
    const [claveAp, setClaveAp] = useState('');
    const [cfg, setCfg] = useState<ConfigWiFi | null>(null);
    const [busy, setBusy] = useState<string | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => { diagnosticar(); }, []);

    async function diagnosticar() {
        setBusy('diag'); setError(null);
        try { setDiag(await window.service.client.ejecutarDiagnosticoSistema()); }
        catch (e) { setError(e instanceof Error ? e.message : String(e)); }
        finally { setBusy(null); }
    }

    async function guardarWiFi() {
        setBusy('wifi'); setError(null);
        try {
            const r = await window.service.client.actualizarConfiguracionWiFi(ssidSta, claveSta, ssidAp, claveAp);
            setCfg(r);
            setClaveSta(''); setClaveAp('');
        } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
        finally { setBusy(null); }
    }

    async function reiniciar() {
        if (!confirm('¿Reiniciar el hardware ESP32?')) return;
        setBusy('reboot'); setError(null);
        try { await window.service.client.reiniciarHardware(); }
        catch (e) { setError(e instanceof Error ? e.message : String(e)); }
        finally { setBusy(null); }
    }

    function fmtUptime(s: number) {
        const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
        return `${d}d ${h}h ${m}m ${sec}s`;
    }

    return (
        <div className="space-y-4">
            {error && <ErrorBanner message={error} onClose={() => setError(null)} />}

            <Card>
                <CardHeader
                    title="Diagnóstico"
                    description="Heap libre y uptime del ESP32"
                    action={
                        <Button size="sm" variant="secondary" onClick={diagnosticar} disabled={busy !== null}>
                            {busy === 'diag' ? <Spinner /> : null} Refrescar
                        </Button>
                    }
                />
                <CardBody>
                    {diag ? (
                        <div className="grid grid-cols-2 gap-3">
                            <div className="rounded-lg border border-border bg-card px-3 py-2">
                                <div className="text-[10px] uppercase tracking-wide text-muted-foreground">Heap libre</div>
                                <div className="text-lg font-semibold">{(diag.heapLibreBytes / 1024).toFixed(1)} KB</div>
                            </div>
                            <div className="rounded-lg border border-border bg-card px-3 py-2">
                                <div className="text-[10px] uppercase tracking-wide text-muted-foreground">Uptime</div>
                                <div className="text-lg font-semibold">{fmtUptime(diag.uptimeSegundos)}</div>
                            </div>
                        </div>
                    ) : <p className="text-xs text-muted-foreground">Sin datos.</p>}
                </CardBody>
            </Card>

            <Card>
                <CardHeader title="WiFi" description="Actualizar configuración (STA + AP)" />
                <CardBody className="space-y-3">
                    <div className="grid gap-3 md:grid-cols-2">
                        <div>
                            <Label htmlFor="ssid-sta">SSID STA</Label>
                            <Input id="ssid-sta" value={ssidSta} onChange={e => setSsidSta(e.target.value)} />
                        </div>
                        <div>
                            <Label htmlFor="clave-sta">Clave STA</Label>
                            <Input id="clave-sta" type="password" value={claveSta} onChange={e => setClaveSta(e.target.value)} />
                        </div>
                        <div>
                            <Label htmlFor="ssid-ap">SSID AP</Label>
                            <Input id="ssid-ap" value={ssidAp} onChange={e => setSsidAp(e.target.value)} />
                        </div>
                        <div>
                            <Label htmlFor="clave-ap">Clave AP</Label>
                            <Input id="clave-ap" type="password" value={claveAp} onChange={e => setClaveAp(e.target.value)} />
                        </div>
                    </div>
                    <Button onClick={guardarWiFi} disabled={busy !== null}>
                        {busy === 'wifi' ? <Spinner /> : null} Guardar
                    </Button>
                    {cfg && (
                        <div className="rounded-md border border-success/30 bg-success-light px-3 py-2 text-xs text-success">
                            IP STA: <span className="font-mono">{cfg.ipSta}</span> · IP AP: <span className="font-mono">{cfg.ipAp}</span>
                        </div>
                    )}
                </CardBody>
            </Card>

            <Card>
                <CardHeader title="Zona de riesgo" description="Reinicio del hardware remoto" />
                <CardBody>
                    <Button variant="danger" onClick={reiniciar} disabled={busy !== null}>
                        {busy === 'reboot' ? <Spinner /> : null} Reiniciar hardware
                    </Button>
                </CardBody>
            </Card>
        </div>
    );
}