import React, { useEffect, useState } from "react";
import {
  Usb,
  Wifi,
  RefreshCw,
  Lock,
  User,
  LogOut,
  Terminal,
  Sun,
  Moon,
  ShieldAlert,
  Cpu,
  Trash2,
  ArrowLeftRight,
} from "lucide-react";
import { useAuth } from "../contexts/AuthContext";
import { useTheme } from "../contexts/ThemeContext";
import { client, safeRpc } from "../lib/service";
import { PasswordInput } from "../components/ui/PasswordInput";
import { RolUsuario } from "#types/rpc.types";

const STORAGE_KEY_REMEMBER = "untdf_auth_remember_v1";

interface DatosRecordadosLogin {
  recordar: boolean;
  usuario: string;
  clave: string;
  tab: "UARTS" | "WIFI";
  ipWifi: string;
}

function leerDatosRecordados(): DatosRecordadosLogin {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY_REMEMBER);
    if (!raw) {
      return {
        recordar: false,
        usuario: "admin",
        clave: "",
        tab: "UARTS",
        ipWifi: "192.168.4.1",
      };
    }
    const parsed = JSON.parse(raw) as Partial<DatosRecordadosLogin>;
    return {
      recordar: Boolean(parsed.recordar),
      usuario: typeof parsed.usuario === "string" && parsed.usuario ? parsed.usuario : "admin",
      clave: typeof parsed.clave === "string" ? parsed.clave : "",
      tab: parsed.tab === "WIFI" ? "WIFI" : "UARTS",
      ipWifi: typeof parsed.ipWifi === "string" && parsed.ipWifi ? parsed.ipWifi : "192.168.4.1",
    };
  } catch {
    return {
      recordar: false,
      usuario: "admin",
      clave: "",
      tab: "UARTS",
      ipWifi: "192.168.4.1",
    };
  }
}

function guardarDatosRecordados(datos: DatosRecordadosLogin | null) {
  try {
    if (!datos || !datos.recordar) {
      window.localStorage.removeItem(STORAGE_KEY_REMEMBER);
      return;
    }
    window.localStorage.setItem(STORAGE_KEY_REMEMBER, JSON.stringify(datos));
  } catch {
    // Ignorar si localStorage no está disponible
  }
}

export function AuthGate() {
  const {
    medioActivo,
    conectado,
    perfilUarts,
    ipWifiConectada,
    usuario,
    rol,
    conectarSerie,
    conectarWifi,
    desconectarTransporte,
    login,
    logout,
  } = useAuth();
  const { theme, toggleTheme } = useTheme();

  const inicial = leerDatosRecordados();

  const [tab, setTab] = useState<"UARTS" | "WIFI">(inicial.tab);
  const [puertos, setPuertos] = useState<string[]>([]);
  const [puertoSeleccionado, setPuertoSeleccionado] = useState("");
  const [ipInput, setIpInput] = useState(inicial.ipWifi);
  const [cargandoConexion, setCargandoConexion] = useState(false);

  const [userInput, setUserInput] = useState(inicial.usuario);
  const [passInput, setPassInput] = useState(inicial.clave);
  const [recordarLogin, setRecordarLogin] = useState(inicial.recordar);
  const [cargandoLogin, setCargandoLogin] = useState(false);

  const [mostrarLogs, setMostrarLogs] = useState(false);
  const [logs, setLogs] = useState<string[]>([]);

  const cargarPuertos = async () => {
    const lista = await safeRpc(
      () => client.listarPuertosSerie(),
      "listarPuertosSerie",
      { silent: true }
    );
    if (lista) {
      const normalizados = lista.map((p: unknown) =>
        typeof p === "string"
          ? p
          : ((p as { path?: string })?.path ?? String(p))
      );
      setPuertos(normalizados);
      if (normalizados.length > 0 && !normalizados.includes(puertoSeleccionado)) {
        setPuertoSeleccionado(normalizados[0]);
      }
    }
  };

  useEffect(() => {
    if (!conectado) {
      void cargarPuertos();
    }
  }, [conectado]);

  useEffect(() => {
    if (!mostrarLogs) return;
    const cargar = async () => {
      const res = await safeRpc(
        () => client.obtenerLogsVisor(),
        "obtenerLogsVisor",
        { silent: true }
      );
      if (res) setLogs(res);
    };
    void cargar();
    const id = window.setInterval(() => void cargar(), 1000);
    return () => window.clearInterval(id);
  }, [mostrarLogs]);

  const handleToggleRecordar = (checked: boolean) => {
    setRecordarLogin(checked);
    if (!checked) {
      guardarDatosRecordados(null);
    }
  };

  const handleConectarSerie = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!puertoSeleccionado) return;
    setCargandoConexion(true);
    try {
      await conectarSerie(puertoSeleccionado);
    } catch {
      // Ya notificado por AuthContext
    } finally {
      setCargandoConexion(false);
    }
  };

  const handleConectarWifi = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!ipInput.trim()) return;
    setCargandoConexion(true);
    try {
      await conectarWifi(ipInput.trim());
    } catch {
      // Ya notificado por AuthContext
    } finally {
      setCargandoConexion(false);
    }
  };

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!userInput.trim()) return;
    setCargandoLogin(true);
    try {
      await login(userInput.trim(), passInput);
      if (recordarLogin) {
        guardarDatosRecordados({
          recordar: true,
          usuario: userInput.trim(),
          clave: passInput,
          tab,
          ipWifi: ipInput.trim() || "192.168.4.1",
        });
      } else {
        guardarDatosRecordados(null);
      }
    } catch {
      // Ya notificado por AuthContext
    } finally {
      setCargandoLogin(false);
    }
  };

  return (
    <div className="flex min-h-screen flex-col justify-between bg-background p-4 text-foreground">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2.5">
          <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-primary text-primary-foreground">
            <Cpu className="h-5 w-5" />
          </div>
          <div>
            <div className="text-sm font-bold">
              UNTDF · Control Térmico Industrial ESP32
            </div>
            <div className="text-xs text-foreground-secondary">
              BIOS FreeRTOS · SGBD PostgreSQL Embebido · RPC Binario L1/L2
            </div>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <button
            className="btn btn-outline btn-secondary px-3 py-1.5 text-xs"
            onClick={() => setMostrarLogs((v) => !v)}
          >
            <Terminal className="h-3.5 w-3.5" />
            {mostrarLogs ? "Ocultar Tramas L1" : "Monitor Tramas L1"}
          </button>
          <button
            className="btn btn-icon btn-secondary"
            onClick={toggleTheme}
            title="Alternar tema"
          >
            {theme === "dark" ? (
              <Sun className="h-4 w-4 text-warning" />
            ) : (
              <Moon className="h-4 w-4" />
            )}
          </button>
        </div>
      </div>

      <div className="mx-auto my-6 w-full max-w-md">
        {/* Caso 3: Autenticado pero con rol NINGUNO (0) */}
        {conectado && usuario && rol === RolUsuario.NINGUNO ? (
          <div className="rounded-xl border border-alert-warn-border bg-card p-6 text-card-foreground shadow-lg">
            <div className="mb-4 flex items-center gap-3 text-alert-warn-icon">
              <ShieldAlert className="h-8 w-8 shrink-0" />
              <div>
                <h2 className="text-base font-bold text-foreground">
                  Acceso Pendiente de Habilitación (Rol NINGUNO)
                </h2>
                <p className="text-xs text-foreground-secondary">
                  Cuenta: <strong>{usuario}</strong>
                </p>
              </div>
            </div>
            <p className="mb-5 text-sm text-foreground-secondary">
              Su cuenta fue autenticada correctamente en el microcontrolador,
              pero actualmente posee el rol <strong>NINGUNO (0)</strong>. Un
              Administrador debe asignarle el rol <strong>NO_ADMIN</strong> o{" "}
              <strong>ADMIN</strong> para habilitar el acceso al sistema.
            </p>
            <button
              className="btn btn-solid btn-error w-full text-sm font-semibold"
              onClick={logout}
            >
              <LogOut className="h-4 w-4" />
              Cerrar Sesión
            </button>
          </div>
        ) : !conectado || medioActivo === "NINGUNO" ? (
          /* Caso 1: Selección de Transporte Físico L1 */
          <div className="rounded-xl border border-border bg-card p-6 text-card-foreground shadow-lg">
            <h1 className="mb-1 text-xl font-bold">Conexión Física L1</h1>
            <p className="mb-4 text-xs text-foreground-secondary">
              Seleccione el enlace físico hacia el nodo ESP32 antes de
              autenticarse.
            </p>

            <div className="mb-5 grid grid-cols-2 gap-2 rounded-lg bg-secondary p-1">
              <button
                type="button"
                onClick={() => setTab("UARTS")}
                className={`flex items-center justify-center gap-2 rounded-md py-2 text-xs font-semibold transition-colors ${
                  tab === "UARTS"
                    ? "bg-card text-foreground shadow-xs"
                    : "text-foreground-secondary hover:text-foreground"
                }`}
              >
                <Usb className="h-4 w-4" />
                UART-S (USB Serie)
              </button>
              <button
                type="button"
                onClick={() => setTab("WIFI")}
                className={`flex items-center justify-center gap-2 rounded-md py-2 text-xs font-semibold transition-colors ${
                  tab === "WIFI"
                    ? "bg-card text-foreground shadow-xs"
                    : "text-foreground-secondary hover:text-foreground"
                }`}
              >
                <Wifi className="h-4 w-4" />
                WiFi (AEAD + UDP)
              </button>
            </div>

            {tab === "UARTS" ? (
              <form onSubmit={handleConectarSerie} className="space-y-4">
                <div>
                  <div className="mb-1.5 flex items-center justify-between">
                    <label className="text-xs font-semibold">
                      Puerto Serie (115200 bps · 8E1)
                    </label>
                    <button
                      type="button"
                      onClick={() => void cargarPuertos()}
                      className="inline-flex items-center gap-1 text-xs font-medium text-primary hover:underline"
                    >
                      <RefreshCw className="h-3 w-3" />
                      Refrescar puertos
                    </button>
                  </div>
                  <select
                    value={puertoSeleccionado}
                    onChange={(e) => setPuertoSeleccionado(e.target.value)}
                    className="w-full rounded-md border border-border bg-background px-3 py-2 font-mono text-xs"
                  >
                    {puertos.length === 0 && (
                      <option value="">No se detectaron puertos serie</option>
                    )}
                    {puertos.map((p) => (
                      <option key={p} value={p}>
                        {p}
                      </option>
                    ))}
                  </select>
                </div>

                <div className="rounded-md border border-border bg-background p-2.5 text-xs text-foreground-secondary">
                  Negociación automática L1 Handshake (Proto v1 · MTU 1536 B).
                </div>

                <button
                  type="submit"
                  disabled={cargandoConexion || !puertoSeleccionado}
                  className="btn btn-solid btn-primary w-full text-sm font-semibold"
                >
                  {cargandoConexion ? (
                    <RefreshCw className="h-4 w-4 animate-spin" />
                  ) : (
                    <Usb className="h-4 w-4" />
                  )}
                  Conectar por Serie
                </button>
              </form>
            ) : (
              <form onSubmit={handleConectarWifi} className="space-y-4">
                <div>
                  <label className="mb-1.5 block text-xs font-semibold">
                    Dirección IPv4 del Servidor ESP32 (TCP 8443 + UDP 8266)
                  </label>
                  <input
                    type="text"
                    value={ipInput}
                    onChange={(e) => setIpInput(e.target.value)}
                    placeholder="192.168.4.1"
                    className="w-full rounded-md border border-border bg-background px-3 py-2 font-mono text-sm"
                  />
                  <div className="mt-2 flex flex-wrap gap-2">
                    <button
                      type="button"
                      onClick={() => setIpInput("192.168.4.1")}
                      className="rounded border border-border bg-secondary px-2.5 py-1 font-mono text-xs hover:bg-hover"
                    >
                      192.168.4.1 (SoftAP ESP32)
                    </button>
                    <button
                      type="button"
                      onClick={() => setIpInput("192.168.1.15")}
                      className="rounded border border-border bg-secondary px-2.5 py-1 font-mono text-xs hover:bg-hover"
                    >
                      192.168.1.15 (Red Local STA)
                    </button>
                  </div>
                </div>

                <button
                  type="submit"
                  disabled={cargandoConexion || !ipInput.trim()}
                  className="btn btn-solid btn-info w-full text-sm font-semibold"
                >
                  {cargandoConexion ? (
                    <RefreshCw className="h-4 w-4 animate-spin" />
                  ) : (
                    <Wifi className="h-4 w-4" />
                  )}
                  Conectar por WiFi
                </button>
              </form>
            )}
          </div>
        ) : (
          /* Caso 2: Transporte Conectado -> Inicio de Sesión L2 */
          <div className="rounded-xl border border-border bg-card p-6 text-card-foreground shadow-lg">
            <div className="mb-4 flex items-center justify-between rounded-lg border border-alert-success-border bg-alert-success-bg px-3 py-2 text-xs">
              <div className="flex items-center gap-2 font-semibold text-alert-success-icon">
                {medioActivo === "UART-S" ? (
                  <Usb className="h-4 w-4" />
                ) : (
                  <Wifi className="h-4 w-4" />
                )}
                <span>
                  Enlace {medioActivo} activo
                  {perfilUarts
                    ? ` (v${perfilUarts.versionServidor} · MTU ${perfilUarts.mtuBytes}B)`
                    : ipWifiConectada
                      ? ` (${ipWifiConectada})`
                      : ""}
                </span>
              </div>
              <button
                type="button"
                onClick={desconectarTransporte}
                className="inline-flex items-center gap-1 font-semibold text-foreground underline"
              >
                <ArrowLeftRight className="h-3 w-3" />
                Cambiar medio
              </button>
            </div>

            <h1 className="mb-1 text-xl font-bold">Inicio de Sesión L2</h1>
            <p className="mb-4 text-xs text-foreground-secondary">
              Autenticación contra tabla <code>Usuario</code> del SGBD
              embebido.
            </p>

            <form onSubmit={handleLogin} className="space-y-4">
              <div>
                <label className="mb-1 block text-xs font-semibold">
                  Usuario
                </label>
                <div className="relative">
                  <User className="pointer-events-none absolute top-2.5 left-3 h-4 w-4 text-foreground-secondary" />
                  <input
                    type="text"
                    maxLength={31}
                    value={userInput}
                    onChange={(e) => setUserInput(e.target.value)}
                    className="w-full rounded-md border border-border bg-background py-2 pr-3 pl-9 text-sm"
                    placeholder="admin"
                    required
                  />
                </div>
              </div>

              <div>
                <label className="mb-1 block text-xs font-semibold">
                  Contraseña
                </label>
                <PasswordInput
                  leftIcon={<Lock className="h-4 w-4" />}
                  maxLength={31}
                  value={passInput}
                  onChange={(e) => setPassInput(e.target.value)}
                  className="w-full rounded-md border border-border bg-background py-2 pl-9 text-sm"
                  placeholder="••••••••"
                  required
                />
              </div>

              <label className="flex cursor-pointer items-center justify-between gap-2 text-xs text-foreground-secondary">
                <span className="inline-flex items-center gap-2">
                  <input
                    type="checkbox"
                    checked={recordarLogin}
                    onChange={(e) => handleToggleRecordar(e.target.checked)}
                    className="rounded border-border"
                  />
                  Recordar usuario y contraseña en este equipo
                </span>
              </label>

              <button
                type="submit"
                disabled={cargandoLogin}
                className="btn btn-solid btn-primary w-full text-sm font-semibold"
              >
                {cargandoLogin ? (
                  <RefreshCw className="h-4 w-4 animate-spin" />
                ) : (
                  <Lock className="h-4 w-4" />
                )}
                Iniciar Sesión
              </button>
            </form>
          </div>
        )}
      </div>

      {mostrarLogs && (
        <div className="mx-auto w-full max-w-4xl rounded-lg border border-border bg-zinc-950 text-emerald-400 shadow-lg">
          <div className="flex items-center justify-between border-b border-zinc-800 px-3 py-2 text-xs">
            <span className="font-mono font-semibold">
              Monitor de Tramas Binarias L1 ({logs.length})
            </span>
            <button
              type="button"
              onClick={async () => {
                await safeRpc(() => client.limpiarLogsVisor(), "limpiarLogsVisor");
                setLogs([]);
              }}
              className="inline-flex items-center gap-1 text-zinc-400 hover:text-white"
            >
              <Trash2 className="h-3.5 w-3.5" />
              Limpiar
            </button>
          </div>
          <div className="max-h-44 overflow-auto p-3 font-mono text-[11px]">
            {logs.length === 0 ? (
              <div className="text-zinc-500">Sin tramas registradas.</div>
            ) : (
              logs.map((l, idx) => (
                <div key={idx} className="whitespace-pre-wrap break-all">
                  {l}
                </div>
              ))
            )}
          </div>
        </div>
      )}
    </div>
  );
}