// Archivo: ./src/frontend/components/Conectar.tsx
import { useState } from 'react';
import './Conectar.css';

export function Conectar() {
  const [estado, setEstado] = useState<'connecting' | 'connected' | 'disconnecting' | 'disconnected'>('disconnected');
  const [error, setError] = useState<string | null>(null);

  const handleConnect = async () => {
    try {
      if (estado === 'connecting' || estado === 'disconnecting') return;

      if (estado === 'connected') {
        setEstado('disconnecting');
        window.service.client.desconectarUarts();
        setEstado('disconnected');
        return;
      }
      if( estado === 'disconnected') {
        setEstado('connecting');
        await window.service.client.iniciarConexionAdaptativa('/dev/cu.usbserial-0001', 115200);
        setEstado('connected');
        return;
      }
    } catch (err) {
      console.error(err);
      setError(err instanceof Error ? err.message : String(err));
      setEstado('disconnected');
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '0.35rem' }}>
      <button className="conn-badge" data-status={estado} onClick={handleConnect}>
        <span className="conn-badge__dot" />
        <span className="conn-badge__label">
          {estado === 'connecting' && 'Conectando...'}
          {estado === 'connected' && 'Conectado'}
          {estado === 'disconnecting' && 'Desconectando...'}
          {estado === 'disconnected' && 'Desconectado'}
        </span>
      </button>
      {error && <div className="conn-badge__error">{error}</div>}
    </div>
  );
}