// src/backend/handles.ts
import { ipcMain } from 'electron';
import { serviceRegistry } from './services';

/**
 * `true` cuando Electron corre con Vite dev server (`npm run dev`).
 *
 * En producción (`electron-builder`), `VITE_DEV_SERVER_URL` no existe, por
 * lo que `isDev` es `false`. Se usa para **filtrar** namespaces sensibles
 * (ej. `developer`) del bridge público y de los handlers IPC.
 */
const isDev = !!process.env['VITE_DEV_SERVER_URL'];

/**
 * Registra todos los canales IPC que conectan el renderer con los servicios
 * del backend.
 *
 * **Debe llamarse ANTES de `createWindow()`** en `main.ts`. Razón: el
 * `preload.ts` hace `ipcRenderer.sendSync('get-api-layout')` en el momento
 * en que se carga, y `sendSync` **bloquea** hasta que el main responda. Si
 * los handlers no están registrados cuando se crea la ventana, el
 * `sendSync` se queda esperando (o peor, falla con "no handler registered").
 *
 * Registra **dos tipos** de canales:
 *
 * 1. `get-api-layout` (sync, `ipcMain.on`): el preload lo llama **una sola
 *    vez** para descubrir la estructura `{ [namespace]: string[] }`. Devuelve
 *    el layout sin ejecutar ningún método. Es sync porque el preload necesita
 *    el resultado antes de exponer `window.service` al renderer.
 *
 * 2. `<namespace>:<method>` (async, `ipcMain.handle`): un canal por cada
 *    método de cada servicio. El preload los invoca con `ipcRenderer.invoke`,
 *    que devuelve una `Promise`. Los argumentos se pasan como rest params
 *    (`...args`) para soportar firmas multi-argumento (`sumar(5, 2, 0)` sin
 *    envolver en un objeto).
 *
 * **Filtro `isDev`:** el namespace `developer` (logs de debug, ecos, etc.)
 * **no** se expone en producción. Esto se aplica en **ambos** canales:
 *   · En el layout, para que el preload no cree `window.service.developer`.
 *   · En los handlers, para que un `invoke('developer:logDebugInfo')` manual
 *     falle con "No handler registered" en vez de ejecutarse.
 *
 * ⚠️ **`ipcMain.handle` es único por canal:** registrar dos veces el mismo
 * `namespace:method` lanza `Error: Attempted to register a second handler`.
 * Por eso `startHandlers` debe llamarse exactamente una vez. Si en el futuro
 * se necesita re-registrar (hot reload), hay que llamar
 * `ipcMain.removeHandler(channel)` antes.
 *
 * @example
 * // En main.ts:
 * app.whenReady().then(() => {
 *   startHandlers();   // ← ANTES de createWindow()
 *   createWindow();
 * });
 */
export function startHandlers() {
  /**
   * Canal de descubrimiento del layout.
   *
   * **Por qué `ipcMain.on` y no `ipcMain.handle`:**
   *   · `ipcMain.on` responde con `event.returnValue` de forma **síncrona**,
   *     que es lo que `ipcRenderer.sendSync` espera.
   *   · `ipcMain.handle` responde con `Promise`, incompatible con `sendSync`.
   *
   * El layout es la **única fuente de verdad** para el preload: en lugar de
   * mantener una lista manual de métodos, el preload descubre la estructura
   * en runtime. Agregar un método al `serviceRegistry` lo expone
   * automáticamente; eliminarlo lo oculta.
   */
  ipcMain.on('get-api-layout', (event) => {
    const layout: Record<string, string[]> = {};
    for (const [namespace, service] of Object.entries(serviceRegistry)) {
      if (namespace === 'developer' && !isDev) continue;
      layout[namespace] = Object.keys(service);
    }
    // `returnValue` es el mecanismo de respuesta de `ipcMain.on` cuando el
    // emisor usó `sendSync`. Sin asignarlo, `sendSync` devolvería `undefined`.
    event.returnValue = layout;
  });

  /**
   * Registro de handlers reales, uno por método.
   *
   * El nombre del canal es `"<namespace>:<method>"`, mismo formato que el
   * preload usa en `ipcRenderer.invoke(\`${namespace}:${method}\`, ...)`. Si
   * cambia uno, cambia el otro — están acoplados por contrato, no por import.
   *
   * El cast `(fn as (...a: unknown[]) => unknown)` es necesario porque
   * `Object.entries` devuelve `[string, unknown]`; TypeScript no puede
   * inferir que `fn` es una función invocable. En runtime sabemos que lo es
   * porque `serviceRegistry` es un objeto de métodos (validado en tests).
   */
  for (const [namespace, service] of Object.entries(serviceRegistry)) {
    if (namespace === 'developer' && !isDev) continue;
    for (const [method, fn] of Object.entries(service)) {
      ipcMain.handle(`${namespace}:${method}`, (_e, ...args: unknown[]) =>
        (fn as (...a: unknown[]) => unknown)(...args)
      );
    }
  }
}