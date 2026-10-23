// src/preload/index.ts
import { contextBridge, ipcRenderer } from 'electron';
import type { ServiceBridge } from '../shared/types/bridge.types';

function buildBridge(): ServiceBridge {
  const api: Record<string, Record<string, unknown>> = {};
  
  // Obtenemos dinámicamente las rutas registradas desde el proceso principal
  const layout = ipcRenderer.sendSync('get-api-layout') as Record<string, string[]>;

  for (const [namespace, methods] of Object.entries(layout)) {
    api[namespace] = {};
    for (const method of methods) {
      api[namespace][method] = (...args: unknown[]) =>
        ipcRenderer.invoke(`${namespace}:${method}`, ...args);
    }
  }

  return api as ServiceBridge;
}

contextBridge.exposeInMainWorld('service', buildBridge());