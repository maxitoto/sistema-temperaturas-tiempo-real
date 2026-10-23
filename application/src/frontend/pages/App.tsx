// src/renderer/App.tsx
import { SumarYLeerEstadoPanel } from '../components/SumarYLeerEstadoPanel';

export default function App() {
  return (
    <main style={{ padding: '1rem', minHeight: '100vh', background: '#f0f2f5' }}>
      <SumarYLeerEstadoPanel />
    </main>
  );
}