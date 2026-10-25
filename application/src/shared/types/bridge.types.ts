// Archivo: ./src/shared/types/bridge.types.ts
import type { ServiceRegistry } from '../../backend/services';

/**
 * Extrae la firma exacta de un método de servicio (argumentos + retorno)
 * y la preserva sin transformación.
 *
 * Propósito: el bridge IPC (`window.service`) debe exponer **la misma firma**
 * que el servicio del backend, no una versión "aplanada". Este tipo conserva:
 *   · Los **tipos de los argumentos** (tupla `A`).
 *   · El **tipo de retorno** (`R`), sea `void`, `Promise<T>`, o cualquier otro.
 *
 * Casos:
 *   · `(texto: string) => void`          → `(texto: string) => void`
 *   · `() => Promise<string>`            → `() => Promise<string>`
 *   · `(a: number, b: number) => number` → `(a: number, b: number) => number`
 *
 * El caso `never` es una **red de seguridad en compilación**: si alguien
 * agrega al registry un miembro que no es una función (ej. una constante),
 * su tipo en el bridge colapsa a `never`, haciendo que `window.service.foo.bar()`
 * sea un error de TypeScript inmediato en lugar de un `undefined` silencioso
 * en runtime.
 *
 * @remarks
 * El retorno `R` se preserva **tal cual**. Esto implica un contrato:
 *   - Si declarás `void` en el servicio, el renderer recibe `void` (y debe
 *     usar el runtime de `send`, no `invoke`).
 *   - Si declarás `Promise<T>`, el renderer recibe `Promise<T>` (y debe
 *     usar `invoke`).
 * El runtime de `preload.ts` debe verificar `fn.constructor.name === 'AsyncFunction'`
 * para elegir `send` vs `invoke` y mantener la coherencia con este tipo.
 */
type BridgeMethod<F> =
  F extends (...args: infer A) => infer R
      ? (...args: A) => R          // preserva firma exacta: args + retorno
    : never;                       // fallback: fuerza error si no es función

/**
 * Contrato tipado del bridge IPC expuesto en `window.service`.
 *
 * Es un **mapped type** que recorre recursivamente `ServiceRegistry`
 * (el objeto de servicios del backend) y reescribe cada método con
 * `BridgeMethod` para preservar su firma.
 *
 * Estructura del mapeo:
 *
 *   ServiceRegistry              ServiceBridge
 *   ──────────────              ─────────────
 *   client: {                    client: {
 *     sumarYLeerEstado: F1         sumarYLeerEstado: BridgeMethod<F1>
 *   }                            }
 *   developer: {                 developer: {
 *     logDebugInfo: F2             logDebugInfo: BridgeMethod<F2>
 *     echo: F3                     echo: BridgeMethod<F3>
 *   }                            }
 *
 * Beneficio: **cero mantenimiento**. Al agregar un método al registry del
 * backend, `ServiceBridge` se actualiza automáticamente. Al eliminar uno,
 * el bridge deja de exponerlo. No hay lista manual que sincronizar.
 *
 * @example
 * // En el renderer, con tipado completo y autocompletado:
 * const r = await window.service.client.sumarYLeerEstado(5, 2);
 * //    ^? Promise<{ resultado: number; estado: string; heapLibreBytes: number }>
 *
 * window.service.developer.logDebugInfo('hola');  // → void (send)
 * await window.service.developer.echo('hola');     // → Promise<string> (invoke)
 */
export type ServiceBridge = {
  [N in keyof ServiceRegistry]: {
    [M in keyof ServiceRegistry[N]]: BridgeMethod<ServiceRegistry[N][M]>;
  };
};