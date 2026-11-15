import { useEffect, useState } from 'react';
import { Button, Card, CardBody, CardHeader, EmptyState, ErrorBanner, Input, Label, Spinner } from '../ui';

interface Lectura {
    timestampMs: number;
    idSensor: number;
    macHex: string;
    temperaturaCorregida: number;
}

export function SensorsPanel() {
    const [lecturas, setLecturas] = useState<Lectura[]>([]);
    const [mac, setMac] = useState('');
    const [desde, setDesde] = useState('');
    const [hasta, setHasta] = useState('');
    const [busy, setBusy] = useState<string | null>(null);
    const [error, setError] = useState<string | null>(null);
    const [streaming, setStreaming] = useState(false);

    useEffect(() => {
        let activo = true;
        const t = setInterval(async () => {
            if (!activo || !streaming) return;
            try {
                const u = await window.service.client.obtenerUltimoLoteTiempoReal();
                if (Array.isArray(u) && u.length > 0) setLecturas(u);
            } catch { /* silencioso */ }
        }, 1000);
        return () => { activo = false; clearInterval(t); };
    }, [streaming]);

    async function suscribir() {
        setBusy('sub'); setError(null);
        try {
            const lote = await window.service.client.suscribirSensoresTiempoReal();
            setLecturas(lote);
            setStreaming(true);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function consultarMac() {
        setBusy('mac'); setError(null); setStreaming(false);
        try {
            setLecturas(await window.service.client.obtenerCensosPorMac(mac, 32));
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function consultarRango() {
        setBusy('rango'); setError(null); setStreaming(false);
        try {
            const d = Math.floor(new Date(desde).getTime() / 1000);
            const h = Math.floor(new Date(hasta).getTime() / 1000);
            if (isNaN(d) || isNaN(h)) throw new Error('Fechas inválidas');
            setLecturas(await window.service.client.obtenerCensosPorMacYFecha(mac, d, h, 32));
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    return (
        <div className="space-y-4">
            {error && <ErrorBanner message={error} onClose={() => setError(null)} />}

            <Card>
                <CardHeader
                    title="Sensores"
                    description={streaming ? 'Streaming activo (poll 1 s)' : 'Sin stream activo'}
                    action={
                        <Button size="sm" onClick={suscribir} disabled={busy !== null}>
                            {busy === 'sub' ? <Spinner /> : null} {streaming ? 'Re-suscribir' : 'Suscribir'}
                        </Button>
                    }
                />
                <CardBody>
                    {lecturas.length === 0 ? (
                        <EmptyState title="Sin lecturas" message="Suscribite o consultá por MAC." />
                    ) : (
                        <div className="max-h-96 overflow-auto">
                            <table className="w-full text-left text-xs">
                                <thead className="sticky top-0 bg-card text-muted-foreground">
                                    <tr className="border-b border-border">
                                        <th className="py-2 pr-3">Timestamp</th>
                                        <th className="py-2 pr-3">ID</th>
                                        <th className="py-2 pr-3">MAC</th>
                                        <th className="py-2 pr-3">Temp (°C)</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {lecturas.map((l, i) => (
                                        <tr key={i} className="border-b border-border/50">
                                            <td className="py-1.5 pr-3 font-mono text-muted-foreground">{new Date(l.timestampMs).toLocaleString()}</td>
                                            <td className="py-1.5 pr-3">{l.idSensor}</td>
                                            <td className="py-1.5 pr-3 font-mono">{l.macHex}</td>
                                            <td className="py-1.5 pr-3 font-medium">{l.temperaturaCorregida.toFixed(2)}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                    )}
                </CardBody>
            </Card>

            <div className="grid gap-4 md:grid-cols-2">
                <Card>
                    <CardHeader title="Consulta por MAC" />
                    <CardBody className="space-y-3">
                        <div>
                            <Label htmlFor="mac">MAC (hex, 16 chars)</Label>
                            <Input id="mac" value={mac} onChange={e => setMac(e.target.value)} placeholder="0011223344556677" />
                        </div>
                        <Button onClick={consultarMac} disabled={busy !== null || !mac}>
                            {busy === 'mac' ? <Spinner /> : null} Consultar
                        </Button>
                    </CardBody>
                </Card>

                <Card>
                    <CardHeader title="Consulta por MAC y fecha" />
                    <CardBody className="space-y-3">
                        <div>
                            <Label htmlFor="desde">Desde</Label>
                            <Input id="desde" type="datetime-local" value={desde} onChange={e => setDesde(e.target.value)} />
                        </div>
                        <div>
                            <Label htmlFor="hasta">Hasta</Label>
                            <Input id="hasta" type="datetime-local" value={hasta} onChange={e => setHasta(e.target.value)} />
                        </div>
                        <Button onClick={consultarRango} disabled={busy !== null || !mac || !desde || !hasta}>
                            {busy === 'rango' ? <Spinner /> : null} Consultar rango
                        </Button>
                    </CardBody>
                </Card>
            </div>
        </div>
    );
}