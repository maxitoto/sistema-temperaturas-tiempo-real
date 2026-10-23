// src/backend/handles.ts
import { ipcMain } from 'electron';
import { serviceRegistry } from './services';

const isDev = !!process.env['VITE_DEV_SERVER_URL'];

export function startHandlers() {
  // 1. Exponer la estructura de los servicios de forma síncrona para el preload
  ipcMain.on('get-api-layout', (event) => {
    const layout: Record<string, string[]> = {};
    for (const [namespace, service] of Object.entries(serviceRegistry)) {
      if (namespace === 'developer' && !isDev) continue;
      layout[namespace] = Object.keys(service);
    }
    event.returnValue = layout;
  });

  // 2. Registrar los handlers reales
  for (const [namespace, service] of Object.entries(serviceRegistry)) {
    if (namespace === 'developer' && !isDev) continue;
    for (const [method, fn] of Object.entries(service)) {
      ipcMain.handle(`${namespace}:${method}`, (_e, ...args: unknown[]) =>
        (fn as (...a: unknown[]) => unknown)(...args)
      );
    }
  }
}