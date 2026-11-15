import { useEffect, useState } from 'react';
import { Badge, Button, Card, CardBody, CardHeader, ErrorBanner, Input, Label, Spinner } from '../ui';

export function ConnectionPanel() {
    const [puertos, setPuertos] = useState<string[]>([]);
    const [puertoSel, setPuertoSel] = useState('');
    const [ipWifi, setIpWifi] = useState('192.168.4.1');
    const [conectado, setConectado] = useState(false);
    const [medio, setMedio] = useState('NINGUNO');
    const [busy, setBusy] = useState<string | null>(null);
    const [error, setError] = useState<string | null>(null);
    const [perfil, setPerfil] = useState<{ versionServidor: number; mtuBytes: number } | null>(null);

    useEffect(() => {
        refrescarPuertos();
        const t = setInterval(refrescarEstado, 1500);
        return () => clearInterval(t);
    }, []);

    async function refrescarPuertos() {
        try {
            const lista = await window.service.client.listarPuertosSerie();
            setPuertos(lista);
            if (!puertoSel && lista.length > 0) setPuertoSel(lista[0]);
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        }
    }

    async function refrescarEstado() {
        try {
            setConectado(await window.service.client.estaConectado());
            setMedio(await window.service.client.obtenerMedioActivo());
        } catch { /* silencioso */ }
    }

    async function conectarUart() {
        if (!puertoSel) return;
        setBusy('uart'); setError(null);
        try {
            const p = await window.service.client.conectarPorSerie(puertoSel);
            setPerfil(p);
            await refrescarEstado();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function conectarWifi() {
        setBusy('wifi'); setError(null);
        try {
            await window.service.client.conectarPorWiFi(ipWifi);
            await refrescarEstado();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    async function desconectar() {
        setBusy('off'); setError(null);
        try {
            await window.service.client.desconectar();
            setPerfil(null);
            await refrescarEstado();
        } catch (e) {
            setError(e instanceof Error ? e.message : String(e));
        } finally { setBusy(null); }
    }

    return (
        <div className="space-y-4">
            {error && <ErrorBanner message={error} onClose={() => setError(null)} />}

            <Card>
                <CardHeader
                    title="Estado del enlace"
                    description="Conexión física con el servidor ESP32"
                    action={
                        <Badge tone={conectado ? 'success' : 'neutral'}>
                            {conectado ? `Conectado (${medio})` : 'Desconectado'}
                        </Badge>
                    }
                />
                <CardBody>
                    {conectado ? (
                        <div className="flex flex-wrap items-center justify-between gap-3">
                            <div className="text-xs text-muted-foreground">
                                {perfil
                                    ? `Protocolo v${perfil.versionServidor} · MTU ${perfil.mtuBytes} bytes`
                                    : `Medio activo: ${medio}`}
                            </div>
                            <Button variant="danger" size="sm" onClick={desconectar} disabled={busy !== null}>
                                {busy === 'off' ? <Spinner /> : null} Desconectar
                            </Button>
                        </div>
                    ) : (
                        <p className="text-xs text-muted-foreground">Sin conexión activa.</p>
                    )}
                </CardBody>
            </Card>

            <Card>
                <CardHeader title="UART-S" description="Enlace serie (115200 8E1)" />
                <CardBody className="space-y-3">
                    <div>
                        <Label htmlFor="puerto">Puerto serie</Label>
                        <div className="flex gap-2">
                            <select
                                id="puerto"
                                value={puertoSel}
                                onChange={e => setPuertoSel(e.target.value)}
                                className="w-full rounded-md border border-border bg-background px-3 py-2 text-sm text-foreground"
                            >
                                {puertos.length === 0 && <option value="">(sin puertos detectados)</option>}
                                {puertos.map(p => <option key={p} value={p}>{p}</option>)}
                            </select>
                            <Button variant="secondary" size="sm" onClick={refrescarPuertos}>↻</Button>
                        </div>
                    </div>
                    <Button onClick={conectarUart} disabled={busy !== null || !puertoSel || conectado}>
                        {busy === 'uart' ? <Spinner /> : null} Conectar por UART-S
                    </Button>
                </CardBody>
            </Card>

            <Card>
                <CardHeader title="WiFi" description="Conexión TCP al servidor ESP32" />
                <CardBody className="space-y-3">
                    <div>
                        <Label htmlFor="ip">IP del servidor</Label>
                        <Input id="ip" value={ipWifi} onChange={e => setIpWifi(e.target.value)} placeholder="192.168.4.1" />
                    </div>
                    <Button onClick={conectarWifi} disabled={busy !== null || !ipWifi || conectado}>
                        {busy === 'wifi' ? <Spinner /> : null} Conectar por WiFi
                    </Button>
                </CardBody>
            </Card>
        </div>
    );
}