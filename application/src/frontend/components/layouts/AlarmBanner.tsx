import { useEffect, useState } from "react";
import { AlertTriangle, VolumeX, RefreshCw, BellRing, ArrowUpRight } from "lucide-react";
import type { DestinoNavegacion } from "../../pages/App";
import { toast } from "react-toastify";
import { client, safeRpc } from "../../lib/service";
import { useAuth } from "../../contexts/AuthContext";
import { EstadoAlarmasActivasDTO, RolUsuario } from "#types/rpc.types";
import { LABEL_ESTADO_BUS, LABEL_ESTADO_SENSOR } from "../../lib/labels";

export function AlarmBanner({
  onNavegar,
}: {
  onNavegar?: (destino: DestinoNavegacion) => void;
}) {
  const { rol, usuario } = useAuth();
  const [alarmas, setAlarmas] = useState<EstadoAlarmasActivasDTO | null>(null);
  const [cargando, setCargando] = useState(false);

  const cargar = async () => {
    const res = await safeRpc(
      () => client.obtenerEstadoAlarmasActivas(),
      "obtenerEstadoAlarmasActivas",
      { silent: true }
    );
    if (res) setAlarmas(res);
  };

  useEffect(() => {
    if (!usuario || rol === RolUsuario.NINGUNO) return;
    cargar();
    const id = window.setInterval(cargar, 3000);
    return () => window.clearInterval(id);
  }, [usuario, rol]);

  const silenciar = async () => {
    setCargando(true);
    const res = await safeRpc(
      () => client.silenciarAlarmaActiva(),
      "silenciarAlarmaActiva"
    );
    setCargando(false);
    if (res) {
      toast.success(
        "Bocina silenciada. El LED de alarma permanece encendido hasta normalizar."
      );
      cargar();
    }
  };

  if (!alarmas?.hayAlarmaActiva) return null;

  return (
    <div className="border-b border-alert-err-border bg-alert-err-bg px-4 py-2.5 text-sm text-foreground">
      <div className="flex flex-wrap items-center gap-3">
        <div className="flex items-center gap-2 font-bold text-alert-err-icon">
          <AlertTriangle className="h-5 w-5 animate-pulse" />
          <span>ALARMA TÉRMICA / BUS ACTIVA</span>
        </div>

        <div className="flex flex-wrap items-center gap-2 text-xs">
          <span className="rounded border border-alert-err-border bg-background px-2 py-0.5">
            Bus 1-Wire:{" "}
            <strong>
              {LABEL_ESTADO_BUS[alarmas.estadoBus] ?? `Cod ${alarmas.estadoBus}`}
            </strong>
          </span>
          <span className="rounded border border-alert-err-border bg-background px-2 py-0.5">
            Bocina:{" "}
            <strong
              className={
                alarmas.bocinaSonando ? "text-alert-err-icon" : "text-foreground-secondary"
              }
            >
              {alarmas.bocinaSonando ? "SONANDO" : "SILENCIADA"}
            </strong>
          </span>
          <span className="rounded border border-alert-err-border bg-background px-2 py-0.5">
            LED:{" "}
            <strong>
              {alarmas.ledAlarmaEncendido ? "ENCENDIDO" : "APAGADO"}
            </strong>
          </span>
        </div>

        <div className="flex flex-1 flex-wrap items-center gap-2">
          {alarmas.sensoresAfectados.map((s) => (
            <button
              key={`${s.slot}-${s.mac}`}
              type="button"
              onClick={() =>
                onNavegar?.({
                  vista: "sensores",
                  sensorMac: s.mac.toUpperCase(),
                })
              }
              title={`Abrir detalle y calibración del sensor ${s.mac}`}
              className="group inline-flex cursor-pointer items-center gap-1.5 rounded border border-alert-err-border bg-background px-2.5 py-1 font-mono text-xs transition-colors hover:border-error hover:bg-error-light/60"
            >
              <BellRing className="h-3.5 w-3.5 text-alert-err-icon" />
              <strong>{s.mac}</strong>
              <span>· {s.temperaturaCorregida.toFixed(2)} °C</span>
              <span className="text-foreground-secondary">
                (TH {s.umbralSuperior.toFixed(1)} / TL {s.umbralInferior.toFixed(1)})
              </span>
              <span className="rounded bg-error-light px-1.5 py-0.5 font-sans text-[10px] font-semibold text-error">
                {LABEL_ESTADO_SENSOR[s.estado] ?? `E${s.estado}`}
              </span>
              <ArrowUpRight className="h-3.5 w-3.5 text-alert-err-icon opacity-60 transition-transform group-hover:translate-x-0.5 group-hover:-translate-y-0.5 group-hover:opacity-100" />
            </button>
          ))}
        </div>

        {rol === RolUsuario.ADMIN && (
          <button
            className="btn btn-solid btn-error px-3 py-1.5 text-xs font-semibold"
            onClick={silenciar}
            disabled={cargando || !alarmas.bocinaSonando}
          >
            {cargando ? (
              <RefreshCw className="h-4 w-4 animate-spin" />
            ) : (
              <VolumeX className="h-4 w-4" />
            )}
            {alarmas.bocinaSonando
              ? "Silenciar Bocina (Mute)"
              : "Bocina Silenciada"}
          </button>
        )}
      </div>
    </div>
  );
}