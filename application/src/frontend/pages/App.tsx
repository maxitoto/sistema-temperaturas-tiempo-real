import { useCallback, useState, useRef } from "react";
import { ToastContainer } from "react-toastify";
import "react-toastify/dist/ReactToastify.css";
import "../styles/index.css";
import { ThemeProvider, useTheme } from "../contexts/ThemeContext";
import { AuthProvider, useAuth } from "../contexts/AuthContext";
import { AuthGate } from "../views/AuthGateView";
import { TopBar } from "../components/layouts/TopBar";
import { AlarmBanner } from "../components/layouts/AlarmBanner";
import { Sidebar, Vista } from "../components/layouts/SidelBar";
import { DashboardView } from "../views/DashboardView";
import { SensoresView } from "../views/SensoresView";
import { DatabaseSqlView } from "../views/DatabaseSqlView";
import { AuditoriasView } from "../views/AuditoriasView";
import { UsuariosView } from "../views/UsuariosView";
import { InteractiveTour } from "../components/tour/InteractiveTour";
import {
  ConfiguracionView,
  TabConfiguracion,
} from "../views/ConfiguracionView";
import { BiosHardwareView } from "../views/BiosHardwareView";
import { RolUsuario } from "#types/rpc.types";

export interface DestinoNavegacion {
  vista: Vista;
  sensorMac?: string;
  configTab?: TabConfiguracion;
}

function Shell() {
  const { usuario, rol, conectado } = useAuth();
  const { theme } = useTheme();
  const [vista, setVista] = useState<Vista>("dashboard");
  const [sensorMacInicial, setSensorMacInicial] = useState<string>("");
  const [sensorNavToken, setSensorNavToken] = useState<number>(0);
  const navSeqRef = useRef<number>(0);
  const [configTabInicial, setConfigTabInicial] =
    useState<TabConfiguracion>("SISTEMA");
  const [tourTrigger, setTourTrigger] = useState(0);

  const navegarA = useCallback(
    (destino: DestinoNavegacion) => {
      if (destino.vista === "bios" && rol !== RolUsuario.ADMIN) return;
      if (destino.sensorMac !== undefined) {
        setSensorMacInicial(destino.sensorMac);
        navSeqRef.current += 1;
        setSensorNavToken(navSeqRef.current);
      } else if (destino.vista === "sensores") {
        setSensorNavToken(0);
      }
      if (destino.configTab) {
        setConfigTabInicial(
          destino.configTab === "WIFI" && rol !== RolUsuario.ADMIN
            ? "SISTEMA"
            : destino.configTab
        );
      }
      setVista(destino.vista);
    },
    [rol]
  );

  if (!conectado || !usuario || rol === RolUsuario.NINGUNO) {
    return (
      <>
        <AuthGate />
        <ToastContainer
          position="bottom-right"
          theme={theme === "dark" ? "dark" : "light"}
        />
      </>
    );
  }

  return (
    <div className="flex h-screen flex-col bg-background text-foreground">
      <TopBar onAbrirAyuda={() => setTourTrigger((n) => n + 1)} />
      <AlarmBanner onNavegar={navegarA} />
      <div className="flex flex-1 overflow-hidden">
        <Sidebar
          vista={vista}
          onVista={(v) => {
            if (v === "sensores") setSensorNavToken(0);
            setVista(v);
          }}
        />
        <main className="flex-1 overflow-auto">
          {vista === "dashboard" && <DashboardView onNavegar={navegarA} />}
          {vista === "sensores" && (
            <SensoresView
              macInicial={sensorMacInicial}
              navToken={sensorNavToken}
              onIrAConfigAmbiente={() =>
                navegarA({ vista: "configuracion", configTab: "AMBIENTE" })
              }
            />
          )}
          {vista === "database" && <DatabaseSqlView />}
          {vista === "auditorias" && <AuditoriasView />}
          {vista === "usuarios" && <UsuariosView />}
          {vista === "configuracion" && (
            <ConfiguracionView tabInicial={configTabInicial} />
          )}
          {vista === "bios" && rol === RolUsuario.ADMIN && <BiosHardwareView />}
        </main>
      </div>
      <ToastContainer
        position="bottom-right"
        theme={theme === "dark" ? "dark" : "light"}
      />
      <InteractiveTour vista={vista} rol={rol} triggerManual={tourTrigger} />
    </div>
  );
}

export default function App() {
  return (
    <ThemeProvider>
      <AuthProvider>
        <Shell />
      </AuthProvider>
    </ThemeProvider>
  );
}