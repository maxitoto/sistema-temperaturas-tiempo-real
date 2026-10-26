// src/renderer/App.tsx
import { SumarYLeerEstadoPanel } from '#components/SumarYLeerEstadoPanel';
import { DatabaseExplorer } from '#components/DatabaseExplorer'
import { DatosEnVivoPanel } from '#components/DatosEnVivoPanel';

export default function App() {
  return (
    <main style={{ padding: '1rem', minHeight: '100vh', background: '#A5A5A5' }}>
      <SumarYLeerEstadoPanel />
      <DatabaseExplorer />
      <DatosEnVivoPanel />
    </main>
  );
}