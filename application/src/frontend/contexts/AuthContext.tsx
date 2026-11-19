import React, {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
} from "react";
import { toast } from "react-toastify";
import { client, extraerMensajeErrorRpc } from "../lib/service";
import {
  MedioActivoCliente,
  PerfilEnlaceUartsCliente,
  RolUsuario,
} from "#types/rpc.types";

interface AuthContextValue {
  medioActivo: MedioActivoCliente;
  conectado: boolean;
  perfilUarts: PerfilEnlaceUartsCliente | null;
  ipWifiConectada: string;
  usuario: string;
  rol: RolUsuario;
  segundosRestantes: number;
  conectarSerie: (rutaPuerto: string) => Promise<void>;
  conectarWifi: (ipServidor: string) => Promise<void>;
  desconectarTransporte: () => Promise<void>;
  login: (usuarioInput: string, clavePlana: string) => Promise<void>;
  logout: () => Promise<void>;
}

const AuthContext = createContext<AuthContextValue>({
  medioActivo: "NINGUNO",
  conectado: false,
  perfilUarts: null,
  ipWifiConectada: "",
  usuario: "",
  rol: RolUsuario.NINGUNO,
  segundosRestantes: 0,
  conectarSerie: async () => {},
  conectarWifi: async () => {},
  desconectarTransporte: async () => {},
  login: async () => {},
  logout: async () => {},
});

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [medioActivo, setMedioActivo] = useState<MedioActivoCliente>("NINGUNO");
  const [conectado, setConectado] = useState(false);
  const [perfilUarts, setPerfilUarts] =
    useState<PerfilEnlaceUartsCliente | null>(null);
  const [ipWifiConectada, setIpWifiConectada] = useState("");
  const [usuario, setUsuario] = useState("");
  const [rol, setRol] = useState<RolUsuario>(RolUsuario.NINGUNO);
  const [segundosRestantes, setSegundosRestantes] = useState(0);

  const limpiarEstadoLocal = useCallback(() => {
    setMedioActivo("NINGUNO");
    setConectado(false);
    setPerfilUarts(null);
    setIpWifiConectada("");
    setUsuario("");
    setRol(RolUsuario.NINGUNO);
    setSegundosRestantes(0);
  }, []);

  useEffect(() => {
    (async () => {
      try {
        const activo = await client.estaConectado();
        const medio = await client.obtenerMedioActivo();
        setConectado(activo);
        setMedioActivo(medio);
      } catch {
        limpiarEstadoLocal();
      }
    })();
  }, [limpiarEstadoLocal]);

  useEffect(() => {
    if (!usuario || rol === RolUsuario.NINGUNO || segundosRestantes <= 0) {
      return;
    }
    const id = window.setInterval(() => {
      setSegundosRestantes((prev) => {
        if (prev <= 1) {
          window.clearInterval(id);
          toast.warning("La sesión L2 ha expirado. Inicie sesión nuevamente.");
          void client.cerrarSesion().catch(() => {});
          limpiarEstadoLocal();
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => window.clearInterval(id);
  }, [usuario, rol, segundosRestantes, limpiarEstadoLocal]);

  const conectarSerie = async (rutaPuerto: string) => {
    try {
      const perfil = await client.conectarPorSerie(rutaPuerto);
      setPerfilUarts(perfil);
      setIpWifiConectada("");
      setMedioActivo("UART-S");
      setConectado(true);
      toast.success(
        `Enlace UART-S establecido (${rutaPuerto}) · Proto v${perfil.versionServidor} · MTU ${perfil.mtuBytes} B`
      );
    } catch (err) {
      const msg = extraerMensajeErrorRpc(err);
      toast.error(msg);
      throw new Error(msg);
    }
  };

  const conectarWifi = async (ipServidor: string) => {
    try {
      await client.conectarPorWiFi(ipServidor);
      setPerfilUarts(null);
      setIpWifiConectada(ipServidor);
      setMedioActivo("WIFI");
      setConectado(true);
      toast.success(`Canal WiFi TLS/UDP conectado a ${ipServidor}`);
    } catch (err) {
      const msg = extraerMensajeErrorRpc(err);
      toast.error(msg);
      throw new Error(msg);
    }
  };

  const desconectarTransporte = async () => {
    try {
      await client.desconectar();
    } catch {
      // Ignora error si el canal ya estaba cerrado
    } finally {
      limpiarEstadoLocal();
      toast.info("Transporte desconectado.");
    }
  };

  const login = async (usuarioInput: string, clavePlana: string) => {
    try {
      const sesion = await client.iniciarSesion(usuarioInput, clavePlana);
      const rolNum = sesion.rolAsignado as RolUsuario;
      setUsuario(usuarioInput);
      setRol(rolNum);
      setSegundosRestantes(sesion.tiempoExpiracionSegundos);
      if (rolNum === RolUsuario.NINGUNO) {
        toast.warning(
          "Usuario autenticado sin rol asignado (NINGUNO). Solicite habilitación a un Administrador."
        );
      } else {
        toast.success(
          `Sesión iniciada como ${usuarioInput} (${
            rolNum === RolUsuario.ADMIN ? "ADMIN" : "OPERADOR"
          })`
        );
      }
    } catch (err) {
      const msg = extraerMensajeErrorRpc(err);
      toast.error(msg);
      throw new Error(msg);
    }
  };

  const logout = async () => {
    try {
      await client.cerrarSesion();
    } catch {
      // Ignora error de red al cerrar
    } finally {
      limpiarEstadoLocal();
      toast.info("Sesión cerrada.");
    }
  };

  return (
    <AuthContext.Provider
      value={{
        medioActivo,
        conectado,
        perfilUarts,
        ipWifiConectada,
        usuario,
        rol,
        segundosRestantes,
        conectarSerie,
        conectarWifi,
        desconectarTransporte,
        login,
        logout,
      }}
    >
      {children}
    </AuthContext.Provider>
  );
}

export const useAuth = () => useContext(AuthContext);