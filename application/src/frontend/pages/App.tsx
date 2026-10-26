import { SumarYLeerEstadoPanel } from '../components/SumarYLeerEstadoPanel';
import { DatosEnVivoPanel }      from '../components/DatosEnVivoPanel';
import { DatabaseExplorer }      from '../components/DatabaseExplorer';
import { ConfiguracionRedPanel } from '../components/ConfiguracionRedPanel';

export default function App() {
  return (
    <main style={{ padding: '1rem', minHeight: '100vh', background: '#f0f2f5' }}>
      <SumarYLeerEstadoPanel />
      <DatosEnVivoPanel />
      <DatabaseExplorer />
      <ConfiguracionRedPanel />
    </main>
  );
}