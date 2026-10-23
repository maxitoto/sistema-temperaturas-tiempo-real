// Archivo: ./src/frontend/components/ConnectionBadge.tsx
import { useCallback, useEffect, useRef, useState } from 'react';
import './ConnectionBadge.css';

type Status = 'disconnected' | 'connected' | 'error' | 'connecting' | 'disconnecting';
type Medio = 'USB' | 'WIFI';

interface Props {
  pollIntervalMs?: number;
}

export function ConnectionBadge({ pollIntervalMs = 2000 }: Props) {
  const [medio, setMedio] = useState<Medio>('USB');
  const [serialPort, setSerialPort] = useState("/tmp/virtual_serial");
  const [bauidosSerialPort, setBauidosSerialPort] = useState('115200');
  const [hostWiFi, setHostWiFi] = useState('192.168.4.1');
  const [portWiFi, setPortWiFi] = useState('9080');
  const [status, setStatus] = useState<Status>('disconnected');
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  const inFlightRef = useRef(false);
  const mountedRef = useRef(true);
  const wasConnectedRef = useRef(false);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  useEffect(() => {
    let cancelled = false;
    const idNodo = medio === 'USB' ? 0 : 1;

    const tick = async () => {
      if (inFlightRef.current) return;
      try {
        const connected = await window.service.client.estaConectado(idNodo);
        if (cancelled || !mountedRef.current) return;

        if (connected) {
          setStatus('connected');
          setErrorMsg(null);
          wasConnectedRef.current = true;
        } else {
          if (wasConnectedRef.current) {
            setStatus('error');
            setErrorMsg(`Sesión perdida en ${medio}`);
          } else {
            setStatus('disconnected');
          }
          wasConnectedRef.current = false;
        }
      } catch (e) {
        if (cancelled || !mountedRef.current) return;
        setStatus('error');
        setErrorMsg(e instanceof Error ? e.message : String(e));
      }
    };

    void tick();
    const h = setInterval(tick, pollIntervalMs);
    return () => {
      cancelled = true;
      clearInterval(h);
    };
  }, [pollIntervalMs, medio]);

  const toggle = useCallback(async () => {
    if (inFlightRef.current) return;
    inFlightRef.current = true;
    setErrorMsg(null);

    try {
      if (status === 'connected') {
        setStatus('disconnecting');
        if (medio === 'USB') {
          await window.service.client.desconectarPorUSB();
        } else {
          await window.service.client.desconectarPorWiFi();
        }
        if (!mountedRef.current) return;
        wasConnectedRef.current = false;
        setStatus('disconnected');
      } else {
        setStatus('connecting');
        if (medio === 'USB') {
          await window.service.client.conectarPorUSB(serialPort, Number(bauidosSerialPort));
        } else {
          await window.service.client.conectarPorWiFi(hostWiFi, Number(portWiFi));
        }
        if (!mountedRef.current) return;
        wasConnectedRef.current = true;
        setStatus('connected');
      }
    } catch (e) {
      if (!mountedRef.current) return;
      wasConnectedRef.current = false;
      setStatus('error');
      setErrorMsg(e instanceof Error ? e.message : String(e));
    } finally {
      inFlightRef.current = false;
    }
  }, [status, medio, hostWiFi, portWiFi]);

  const labels: Record<Status, string> = {
    disconnected: `Conectar ${medio}`,
    connected: `${medio} Autenticado`,
    error: `Reintentar ${medio}`,
    connecting: 'Autenticando...',
    disconnecting: 'Cerrando...',
  };

  const busy = status === 'connecting' || status === 'disconnecting';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '0.35rem' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
        <select
          value={medio}
          disabled={busy || status === 'connected'}
          onChange={(e) => {
            setMedio(e.target.value as Medio);
            setStatus('disconnected');
            setErrorMsg(null);
          }}
          style={{
            padding: '0.35rem 0.6rem',
            borderRadius: '8px',
            border: '1px solid #d1d5db',
            fontSize: '0.8rem',
            fontWeight: 600,
          }}
        >
          <option value="USB">USB (Nodo 0)</option>
          <option value="WIFI">Wi-Fi (Nodo 1)</option>
        </select>

        {medio === 'USB' && status !== 'connected' && (
          <div >
            <input
              type="text"
              value={serialPort}
              onChange={(e) => setSerialPort(e.target.value)}
              disabled={busy}
              placeholder="Puerto Serial del ESP"
              style={{
                width: '115px',
                padding: '0.35rem 0.5rem',
                borderRadius: '8px',
                border: '1px solid #d1d5db',
                fontSize: '0.8rem',
              }}
            />
            <input
              type="text"
              value={bauidosSerialPort}
              onChange={(e) => setBauidosSerialPort(e.target.value)}
              disabled={busy}
              placeholder="Baudios Puerto Serial del ESP"
              style={{
                width: '115px',
                padding: '0.35rem 0.5rem',
                borderRadius: '8px',
                border: '1px solid #d1d5db',
                fontSize: '0.8rem',
              }}
            />
          </div>
        )}

        {medio === 'WIFI' && status !== 'connected' && (
          <div >
            <input
              type="text"
              value={hostWiFi}
              onChange={(e) => setHostWiFi(e.target.value)}
              disabled={busy}
              placeholder="IP del ESP"
              style={{
                width: '115px',
                padding: '0.35rem 0.5rem',
                borderRadius: '8px',
                border: '1px solid #d1d5db',
                fontSize: '0.8rem',
              }}
            />
            <input
              type="text"
              value={portWiFi}
              onChange={(e) => setPortWiFi(e.target.value)}
              disabled={busy}
              placeholder="Puerto del ESP"
              style={{
                width: '115px',
                padding: '0.35rem 0.5rem',
                borderRadius: '8px',
                border: '1px solid #d1d5db',
                fontSize: '0.8rem',
              }}
            />
          </div>
        )}

        <button
          type="button"
          className="conn-badge"
          data-status={status}
          onClick={toggle}
          disabled={busy}
          title={errorMsg ?? labels[status]}
        >
          <span className="conn-badge__dot" aria-hidden />
          <span className="conn-badge__label">{labels[status]}</span>
          {busy && <span className="conn-badge__spinner" aria-hidden />}
        </button>
      </div>

      {errorMsg && (
        <span style={{ fontSize: '0.75rem', color: '#dc2626', maxWidth: '280px', textAlign: 'right' }}>
          {errorMsg}
        </span>
      )}
    </div>
  );
}