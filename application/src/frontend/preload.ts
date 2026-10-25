// src/preload/index.ts
import { contextBridge, ipcRenderer } from 'electron';
import type { ServiceBridge } from '../shared/types/bridge.types';

/**
 * Construye el objeto `window.service` que el renderer usará para invocar
 * servicios del backend.
 *
 * **Modelo de seguridad:** el renderer corre con `contextIsolation: true` y
 * `nodeIntegration: false` (ver `main.ts`). No tiene acceso directo a Node,
 * a `ipcRenderer`, ni al sistema de archivos. La **única** superficie que se
 * le expone es el objeto que esta función retorna, publicado vía
 * `contextBridge.exposeInMainWorld`.
 *
 * **Descubrimiento dinámico:** en lugar de hardcodear los nombres de los
 * servicios y métodos, se le pide al proceso principal (main) el "layout"
 * del registry vía un mensaje IPC sincrónico. Esto garantiza que:
 *   · Agregar un método al backend lo expone automáticamente al renderer.
 *   · Eliminarlo del backend lo hace desaparecer del renderer.
 *   · No hay lista duplicada que pueda desincronizarse.
 *
 * **`sendSync` (bloqueante, de una sola vez):** la llamada se ejecuta **una
 * única vez** al cargar el preload, antes de que el renderer empiece a
 * ejecutar código. Bloquear el event loop por ~1 ms es aceptable; la
 * alternativa (`send` + `await` + race con la primera llamada del renderer)
 * introduce condiciones de carrera. El precio de `sendSync` es cero después
 * de esta inicialización.
 *
 * **`invoke` (asíncrono):** cada método expuesto usa `ipcRenderer.invoke`,
 * que:
 *   · Envía el mensaje al main y espera la respuesta.
 *   · Devuelve siempre `Promise<T>`.
 *   · Propaga errores del handler como rechazo de la Promise.
 *
 * ⚠️ **Contrato del tipo `ServiceBridge` vs. realidad de `invoke`:**
 * `BridgeMethod<F>` preserva la firma original del servicio. Si un servicio
 * declara `void`, el tipo dice `void`, pero `invoke` **siempre** devuelve
 * `Promise<void>`. El renderer debe tratarlos como Promise (usar `void` o
 * `await`) aunque el tipo diga `void`. Si se requiere fire-and-forget real
 * (sin Promise), habría que sustituir `invoke` por `send` y discriminar por
 * `fn.constructor.name === 'AsyncFunction'` en el main — no es el caso aquí.
 *
 * @returns Objeto con la estructura `{ [namespace]: { [method]: fn } }` que
 *          cumple el contrato `ServiceBridge`.
 */
function buildBridge(): ServiceBridge {
  const api: Record<string, Record<string, unknown>> = {};

  // Layout: { client: ['sumarYLeerEstado', ...], developer: ['logDebugInfo', ...] }
  const layout = ipcRenderer.sendSync('get-api-layout') as Record<string, string[]>;

  for (const [namespace, methods] of Object.entries(layout)) {
    api[namespace] = {};
    for (const method of methods) {
      api[namespace][method] = (...args: unknown[]) =>
        ipcRenderer.invoke(`${namespace}:${method}`, ...args);
    }
  }

  // Cast: el shape se cumple por construcción (el layout viene del mismo
  // registry que define ServiceBridge). TypeScript no puede verificarlo
  // estructuralmente porque `api` es `Record<string, unknown>`.
  return api as ServiceBridge;
}

/**
 * Publica el bridge en el objeto global `window.service`.
 *
 * **Por qué `contextBridge` y no `window.service = ...`:**
 * con `contextIsolation: true`, el preload y el renderer viven en contextos
 * V8 separados. Una asignación directa a `window` desde el preload **no es
 * visible** desde el renderer. `contextBridge.exposeInMainWorld` es el único
 * mecanismo oficial para exponer objetos entre ambos mundos, y además
 * **clona** los valores (no comparte referencias), lo que evita fugas de
 * objetos del preload al renderer.
 *
 * El renderer consume el bridge como `window.service.client.sumarYLeerEstado(...)`,
 * con tipado completo gracias al `declare global` de `vite-env.d.ts`.
 */
contextBridge.exposeInMainWorld('service', buildBridge());