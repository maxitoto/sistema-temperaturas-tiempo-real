import React from "react";
import {
  LayoutDashboard,
  Thermometer,
  Database,
  ShieldCheck,
  Users,
  Settings,
  Cpu,
} from "lucide-react";
import { useAuth } from "../../contexts/AuthContext";
import { RolUsuario } from "#types/rpc.types";

export type Vista =
  | "dashboard"
  | "sensores"
  | "database"
  | "auditorias"
  | "usuarios"
  | "configuracion"
  | "bios";

const items: Array<{
  id: Vista;
  label: string;
  sublabel: string;
  icon: React.ReactNode;
  soloAdmin?: boolean;
}> = [
  {
    id: "dashboard",
    label: "Dashboard",
    sublabel: "Telemetría en vivo",
    icon: <LayoutDashboard className="h-4 w-4" />,
  },
  {
    id: "sensores",
    label: "Sensores y Censos",
    sublabel: "DS18B20 & Calibración",
    icon: <Thermometer className="h-4 w-4" />,
  },
  {
    id: "database",
    label: "Base de Datos / SQL",
    sublabel: "Esquema ER & psql",
    icon: <Database className="h-4 w-4" />,
  },
  {
    id: "auditorias",
    label: "Auditorías",
    sublabel: "HMAC-64, Sensores & Boot",
    icon: <ShieldCheck className="h-4 w-4" />,
  },
  {
    id: "usuarios",
    label: "Usuarios y Perfil",
    sublabel: "RBAC & Credenciales",
    icon: <Users className="h-4 w-4" />,
  },
  {
    id: "configuracion",
    label: "Configuración",
    sublabel: "Sistema, WiFi, SMTP, NTP",
    icon: <Settings className="h-4 w-4" />,
  },
  {
    id: "bios",
    label: "BIOS / FreeRTOS",
    sublabel: "Supervisor & Hardware",
    icon: <Cpu className="h-4 w-4" />,
    soloAdmin: true,
  },
];

export function Sidebar({
  vista,
  onVista,
}: {
  vista: Vista;
  onVista: (v: Vista) => void;
}) {
  const { rol } = useAuth();

  return (
    <aside className="flex w-60 shrink-0 flex-col gap-1 border-r border-sidebar-border bg-sidebar p-2.5 text-sidebar-foreground">
      <div className="px-2.5 py-1.5 text-[11px] font-semibold tracking-wider text-foreground-secondary uppercase">
        Módulos del Sistema
      </div>
      {items
        .filter((it) => !it.soloAdmin || rol === RolUsuario.ADMIN)
        .map((it) => {
          const activo = vista === it.id;
          return (
            <button
              key={it.id}
              onClick={() => onVista(it.id)}
              className={`flex items-center gap-3 rounded-lg px-3 py-2.5 text-left transition-colors ${
                activo
                  ? "bg-primary text-primary-foreground shadow-xs"
                  : "text-sidebar-foreground hover:bg-sidebar-accent"
              }`}
            >
              <span className="shrink-0">{it.icon}</span>
              <div className="min-w-0 flex-1">
                <div className="truncate text-xs font-semibold">{it.label}</div>
                <div
                  className={`truncate text-[10px] ${
                    activo
                      ? "text-primary-foreground/80"
                      : "text-foreground-secondary"
                  }`}
                >
                  {it.sublabel}
                </div>
              </div>
            </button>
          );
        })}
    </aside>
  );
}