import { toast } from "react-toastify";
import type { ServiceClientAPI } from "#types/rpc.types";

export const client = new Proxy({} as ServiceClientAPI, {
  get(_target, prop: string) {
    const bridge = (
      window as unknown as { service?: { client?: Record<string, unknown> } }
    ).service?.client;
    if (!bridge || typeof bridge[prop] !== "function") {
      return () =>
        Promise.reject(
          new Error(
            `IPCBridgeError: el método client.${prop} no está disponible en preload`
          )
        );
    }
    return (...args: unknown[]) =>
      (bridge[prop] as (...a: unknown[]) => Promise<unknown>)(...args);
  },
});

export function extraerMensajeError(err: unknown): string {
  const raw =
    err instanceof Error ? err.message : String(err ?? "Error desconocido");
  const match = raw.match(
    /Error invoking remote method '[^']+':\s*(?:Error:\s*)?(.*)$/
  );
  return (match ? match[1] : raw).trim();
}

export const extraerMensajeErrorRpc = extraerMensajeError;

export interface SafeRpcOptions {
  silent?: boolean;
  silencioso?: boolean;
  mensajeExito?: string;
  onError?: (mensaje: string) => void;
}

export async function safeRpc<T>(
  fn: () => Promise<T>,
  contexto?: string,
  opciones: SafeRpcOptions = {}
): Promise<T | null> {
  try {
    const res = await fn();
    if (opciones.mensajeExito) {
      toast.success(opciones.mensajeExito);
    }
    return res;
  } catch (err) {
    const mensaje = extraerMensajeError(err);
    if (contexto) {
      console.error(`[RPC ${contexto}]`, mensaje);
    }
    if (!opciones.silent && !opciones.silencioso) {
      toast.error(mensaje);
    }
    opciones.onError?.(mensaje);
    return null;
  }
}