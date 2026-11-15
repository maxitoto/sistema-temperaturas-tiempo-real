import { useState } from 'react';
import { ConnectionPanel } from '../components/panels/ConnectionPanel';
import { SessionPanel }    from '../components/panels/SessionPanel';
import { UsersPanel }      from '../components/panels/UsersPanel';
import { SensorsPanel }    from '../components/panels/SensorsPanel';
import { SystemPanel }     from '../components/panels/SystemPanel';
import { LogsPanel }       from '../components/panels/LogsPanel';

type TabId = 'connection' | 'session' | 'users' | 'sensors' | 'system' | 'logs';

const TABS: { id: TabId; label: string; icon: string }[] = [
    { id: 'connection', label: 'Conexión', icon: '🔌' },
    { id: 'session',    label: 'Sesión',   icon: '🔐' },
    { id: 'users',      label: 'Usuarios', icon: '👤' },
    { id: 'sensors',    label: 'Sensores', icon: '🌡️' },
    { id: 'system',     label: 'Sistema',  icon: '⚙️' },
    { id: 'logs',       label: 'Logs',     icon: '📋' },
];

export default function App() {
    const [tab, setTab] = useState<TabId>('connection');
    const [sessionActive, setSessionActive] = useState(false);

    return (
        <div className="min-h-screen bg-background text-foreground">
            <header className="sticky top-0 z-10 border-b border-border bg-card/80 backdrop-blur">
                <div className="mx-auto flex max-w-6xl items-center justify-between gap-4 px-6 py-3">
                    <div>
                        <h1 className="text-lg font-bold tracking-tight">Panel de Control · ESP32</h1>
                        <p className="text-xs text-muted-foreground">Cliente RPC sobre UART-S / WiFi</p>
                    </div>
                    {sessionActive && (
                        <span className="rounded-full border border-success/30 bg-success-light px-2.5 py-1 text-xs text-success">
                            Sesión activa
                        </span>
                    )}
                </div>
                <nav className="mx-auto flex max-w-6xl gap-1 overflow-x-auto px-4 pb-2">
                    {TABS.map(t => (
                        <button
                            key={t.id}
                            onClick={() => setTab(t.id)}
                            className={`shrink-0 rounded-md px-3 py-1.5 text-xs font-medium transition-colors ${
                                tab === t.id
                                    ? 'bg-primary text-primary-foreground'
                                    : 'text-foreground-secondary hover:bg-muted'
                            }`}
                        >
                            <span className="mr-1">{t.icon}</span> {t.label}
                        </button>
                    ))}
                </nav>
            </header>

            <main className="mx-auto max-w-6xl px-6 py-6">
                {tab === 'connection' && <ConnectionPanel />}
                {tab === 'session'    && <SessionPanel onSessionChange={setSessionActive} />}
                {tab === 'users'      && <UsersPanel />}
                {tab === 'sensors'    && <SensorsPanel />}
                {tab === 'system'     && <SystemPanel />}
                {tab === 'logs'       && <LogsPanel />}
            </main>
        </div>
    );
}