import { useState } from "react";
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
import { ConfiguracionView } from "../views/ConfiguracionView";
import { BiosHardwareView } from "../views/BiosHardwareView";
import { RolUsuario } from "#types/rpc.types";

function Shell() {
  const { usuario, rol, conectado } = useAuth();
  const { theme } = useTheme();
  const [vista, setVista] = useState<Vista>("dashboard");

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
      <TopBar />
      <AlarmBanner />
      <div className="flex flex-1 overflow-hidden">
        <Sidebar vista={vista} onVista={setVista} />
        <main className="flex-1 overflow-auto">
          {vista === "dashboard" && <DashboardView />}
          {vista === "sensores" && <SensoresView />}
          {vista === "database" && <DatabaseSqlView />}
          {vista === "auditorias" && <AuditoriasView />}
          {vista === "usuarios" && <UsuariosView />}
          {vista === "configuracion" && <ConfiguracionView />}
          {vista === "bios" && rol === RolUsuario.ADMIN && <BiosHardwareView />}
        </main>
      </div>
      <ToastContainer
        position="bottom-right"
        theme={theme === "dark" ? "dark" : "light"}
      />
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