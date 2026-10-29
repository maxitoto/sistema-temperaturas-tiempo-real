// src/renderer/App.tsx
import { SumarYLeerEstadoPanel } from '../components/SumarYLeerEstadoPanel';
import { DatabaseExplorer } from '../components/DatabaseExplorer';
import { DatosEnVivoPanel } from '#components/DatosEnVivoPanel';
import { ConfiguracionRedPanel } from '#components/ConfiguracionRedPanel';

export default function App() {
  return (
    <main style={{ padding: '1rem', minHeight: '100vh', background: '#f0f2f5' }}>
      <SumarYLeerEstadoPanel />
      <DatabaseExplorer />
      <DatosEnVivoPanel />
      <ConfiguracionRedPanel />
    </main>
  );
}