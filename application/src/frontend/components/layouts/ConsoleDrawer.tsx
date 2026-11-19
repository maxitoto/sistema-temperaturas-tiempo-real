import { useEffect, useRef, useState } from "react";
import { X, RefreshCw, Trash2, Terminal } from "lucide-react";
import { client, safeRpc } from "../../lib/service";

export function ConsoleDrawer({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  const [logs, setLogs] = useState<string[]>([]);
  const [auto, setAuto] = useState(true);
  const bottomRef = useRef<HTMLDivElement | null>(null);

  const cargar = async () => {
    const res = await safeRpc(() => client.obtenerLogsVisor(), "obtenerLogsVisor", {
      silent: true,
    });
    if (res) setLogs(res);
  };

  useEffect(() => {
    if (!open) return;
    cargar();
    if (!auto) return;
    const id = window.setInterval(cargar, 1000);
    return () => window.clearInterval(id);
  }, [open, auto]);

  useEffect(() => {
    if (open && auto) {
      bottomRef.current?.scrollIntoView({ behavior: "smooth" });
    }
  }, [logs, open, auto]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex justify-end bg-black/40 backdrop-blur-[1px]">
      <div className="flex h-full w-full max-w-2xl flex-col border-l border-border bg-card text-card-foreground shadow-2xl">
        <div className="flex items-center justify-between border-b border-border px-4 py-3">
          <div className="flex items-center gap-2">
            <Terminal className="h-4 w-4 text-primary" />
            <h2 className="text-sm font-semibold">
              Consola de Tramas L1 en Vivo ({logs.length} líneas)
            </h2>
          </div>
          <div className="flex items-center gap-2">
            <button
              className={`btn btn-icon ${auto ? "text-primary" : ""}`}
              onClick={() => setAuto((a) => !a)}
              title={auto ? "Pausar auto-refresco (1s)" : "Reanudar auto-refresco"}
            >
              <RefreshCw className={`h-4 w-4 ${auto ? "animate-spin" : ""}`} />
            </button>
            <button
              className="btn btn-icon btn-error"
              onClick={async () => {
                await safeRpc(() => client.limpiarLogsVisor(), "limpiarLogsVisor");
                setLogs([]);
              }}
              title="Limpiar tramas"
            >
              <Trash2 className="h-4 w-4" />
            </button>
            <button
              className="btn btn-icon btn-secondary"
              onClick={onClose}
              title="Cerrar consola"
            >
              <X className="h-4 w-4" />
            </button>
          </div>
        </div>
        <div className="flex-1 overflow-auto bg-zinc-950 p-3 font-mono text-xs leading-relaxed text-emerald-400">
          {logs.length === 0 ? (
            <p className="text-zinc-500">
              Sin tramas binarias registradas en el buffer L1.
            </p>
          ) : (
            logs.map((line, i) => (
              <div
                key={i}
                className="border-b border-zinc-900/80 py-0.5 whitespace-pre-wrap break-all"
              >
                {line}
              </div>
            ))
          )}
          <div ref={bottomRef} />
        </div>
      </div>
    </div>
  );
}