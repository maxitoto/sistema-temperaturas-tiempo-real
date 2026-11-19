import { useEffect, useState } from "react";
import {
  Usb,
  Wifi,
  Sun,
  Moon,
  LogOut,
  Terminal,
  Shield,
  User,
  Users,
  Radio,
} from "lucide-react";
import { useAuth } from "../../contexts/AuthContext";
import { useTheme } from "../../contexts/ThemeContext";
import { client, safeRpc } from "../../lib/service";
import { InfoRedPublicaDTO, RolUsuario } from "#types/rpc.types";
import { ConsoleDrawer } from "./ConsoleDrawer";

export function TopBar() {
  const {
    usuario,
    rol,
    medioActivo,
    perfilUarts,
    ipWifiConectada,
    segundosRestantes,
    logout,
  } = useAuth();
  const { theme, toggleTheme } = useTheme();
  const [consolaAbierta, setConsolaAbierta] = useState(false);
  const [redInfo, setRedInfo] = useState<InfoRedPublicaDTO | null>(null);

  useEffect(() => {
    if (!usuario || rol === RolUsuario.NINGUNO) return;
    const cargarRed = async () => {
      const r = await safeRpc(
        () => client.obtenerInfoRedPublica(),
        "obtenerInfoRedPublica",
        { silent: true }
      );
      if (r) setRedInfo(r);
    };
    cargarRed();
    const id = window.setInterval(cargarRed, 8000);
    return () => window.clearInterval(id);
  }, [usuario, rol]);

  const mm = Math.floor(segundosRestantes / 60);
  const ss = segundosRestantes % 60;
  const ipMostrar =
    ipWifiConectada ||
    (redInfo?.conectadoARouter ? redInfo.ipSta : redInfo?.ipAp) ||
    "—";

  return (
    <>
      <header className="flex flex-wrap items-center justify-between gap-3 border-b border-border bg-card px-4 py-2.5 text-card-foreground">
        <div className="flex flex-wrap items-center gap-2.5">
          <div className="flex items-center gap-1.5 rounded-md border border-border bg-secondary px-2.5 py-1 text-xs font-semibold">
            {medioActivo === "UART-S" ? (
              <Usb className="h-4 w-4 text-primary" />
            ) : (
              <Wifi className="h-4 w-4 text-info" />
            )}
            <span>{medioActivo}</span>
            {medioActivo === "UART-S" && perfilUarts && (
              <span className="font-mono text-[11px] text-foreground-secondary">
                · MTU {perfilUarts.mtuBytes}B
              </span>
            )}
          </div>

          {redInfo && (
            <div className="hidden items-center gap-2 rounded-md border border-border bg-background px-2.5 py-1 text-xs text-foreground-secondary md:flex">
              <span className="font-mono text-foreground">IP: {ipMostrar}</span>
              <span>·</span>
              <span className="inline-flex items-center gap-1">
                <Radio className="h-3.5 w-3.5 text-success" />
                {redInfo.rssiDbm} dBm
              </span>
              <span>·</span>
              <span className="inline-flex items-center gap-1">
                <Users className="h-3.5 w-3.5" />
                {redInfo.clientesAutenticados} cli
              </span>
            </div>
          )}

          <span className="hidden text-xs font-medium text-foreground-secondary xl:inline">
            UNTDF · Orquestador BIOS FreeRTOS & SGBD PostgreSQL Embebido
          </span>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          <div className="flex items-center gap-2 rounded-md border border-border bg-secondary px-2.5 py-1 text-xs">
            <User className="h-3.5 w-3.5 text-foreground-secondary" />
            <span className="font-semibold">{usuario}</span>
            <span
              className={`flex items-center gap-1 rounded px-1.5 py-0.5 text-[10px] font-bold ${
                rol === RolUsuario.ADMIN
                  ? "bg-primary text-primary-foreground"
                  : "bg-info text-info-foreground"
              }`}
            >
              <Shield className="h-3 w-3" />
              {rol === RolUsuario.ADMIN ? "ADMIN" : "OPERADOR"}
            </span>
          </div>

          <div
            className={`rounded-md border border-border px-2.5 py-1 font-mono text-xs ${
              segundosRestantes < 60
                ? "border-error bg-error-light font-bold text-error"
                : "bg-secondary"
            }`}
            title="Tiempo restante de sesión L2"
          >
            {String(mm).padStart(2, "0")}:{String(ss).padStart(2, "0")}
          </div>

          <button
            className="btn btn-icon btn-secondary"
            onClick={toggleTheme}
            title={
              theme === "dark"
                ? "Cambiar a Modo Claro"
                : "Cambiar a Modo Oscuro"
            }
          >
            {theme === "dark" ? (
              <Sun className="h-4 w-4 text-warning" />
            ) : (
              <Moon className="h-4 w-4" />
            )}
          </button>

          <button
            className="btn btn-icon btn-primary"
            onClick={() => setConsolaAbierta(true)}
            title="Abrir Consola de Tramas L1"
          >
            <Terminal className="h-4 w-4" />
          </button>

          <button
            className="btn btn-solid btn-error px-3 py-1.5 text-xs font-semibold"
            onClick={logout}
          >
            <LogOut className="h-3.5 w-3.5" />
            Cerrar Sesión
          </button>
        </div>
      </header>

      <ConsoleDrawer
        open={consolaAbierta}
        onClose={() => setConsolaAbierta(false)}
      />
    </>
  );
}