import { useEffect, useRef, useState } from 'react';
import { Button, Card, CardBody, CardHeader } from '../ui';

export function LogsPanel() {
    const [logs, setLogs] = useState<string[]>([]);
    const [auto, setAuto] = useState(true);
    const ref = useRef<HTMLPreElement>(null);

    useEffect(() => {
        const t = setInterval(async () => {
            if (!auto) return;
            try { setLogs(await window.service.client.obtenerLogsVisor()); }
            catch { /* silencioso */ }
        }, 750);
        return () => clearInterval(t);
    }, [auto]);

    useEffect(() => {
        if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
    }, [logs]);

    async function limpiar() {
        try { await window.service.client.limpiarLogsVisor(); setLogs([]); }
        catch { /* silencioso */ }
    }

    return (
        <Card>
            <CardHeader
                title="Consola de diagnóstico"
                description="Últimos 200 eventos · FIFO"
                action={
                    <div className="flex items-center gap-2">
                        <label className="flex items-center gap-1.5 text-xs text-muted-foreground">
                            <input type="checkbox" checked={auto} onChange={e => setAuto(e.target.checked)} /> Auto
                        </label>
                        <Button size="sm" variant="secondary" onClick={limpiar}>Limpiar</Button>
                    </div>
                }
            />
            <CardBody className="p-0">
                <pre
                    ref={ref}
                    className="max-h-[560px] overflow-auto bg-black/90 px-4 py-3 font-mono text-xs leading-relaxed text-emerald-300"
                >
{logs.length === 0 ? '(sin registros)' : logs.join('\n')}
                </pre>
            </CardBody>
        </Card>
    );
}