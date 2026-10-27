// Archivo: ./src/backend/plugins/conectividad/servicosConectivos.ts
import { ID_NODO_USB, ID_NODO_WIFI, pila } from "../ContenedorComunicacion";
import { registrarStubsServidorBackend } from "../../stubs/serverExample";
import { ClienteWiFiNode } from "./modem/ClienteWiFiNode";
import { PuertoUSBNode, InfoPuertoSerial } from "./usb/PuertoUSBNode";
import { activarDebugPilaOSI } from "../../debug/DebugLogger";

const driverUSB = new PuertoUSBNode();
const driverWiFi = new ClienteWiFiNode("127.0.0.1", 9080);

// Vinculamos los dos descriptores físicos una sola vez en el arranque del módulo
pila.fisica.vincularMedio(ID_NODO_USB, driverUSB);
pila.fisica.vincularMedio(ID_NODO_WIFI, driverWiFi);

// Registramos los procedimientos RPC donde el Backend actúa como Servidor
registrarStubsServidorBackend();
activarDebugPilaOSI();

let timerLatido: ReturnType<typeof setInterval> | null = null;

const fallosHeartbeat: Record<number, number> = { [ID_NODO_USB]: 0, [ID_NODO_WIFI]: 0 };
const MAX_FALLOS_HEARTBEAT = 3;

function iniciarLoopHeartbeat(): void {
  if (timerLatido) return;
  timerLatido = setInterval(async () => {
    pila.sesion.auditarSesiones();
    for (const idNodo of [ID_NODO_USB, ID_NODO_WIFI]) {
      if (pila.fisica.estaActivo(idNodo) && pila.sesion.estaAutenticada(idNodo)) {
        try {
          await pila.sesion.enviarLatido(idNodo);
          fallosHeartbeat[idNodo] = 0;
        } catch {
          fallosHeartbeat[idNodo] = (fallosHeartbeat[idNodo] ?? 0) + 1;
          if (fallosHeartbeat[idNodo] >= MAX_FALLOS_HEARTBEAT) {
            pila.sesion.cerrarSesionLocal(idNodo);
            fallosHeartbeat[idNodo] = 0;
          }
        }
      }
    }
  }, 5000);
}


export const serviciosConectivos = {
  // CONEXIÓN Y DESCONEXIÓN POR USB (idNodo = 0)
  listarPuertosUSB : async (): Promise<InfoPuertoSerial[]> => {
    return PuertoUSBNode.listarPuertos();
  },
  conectarPorUSB: async (pathManual?: string, baudRate = 115200) => {
    const pathAbierto = await driverUSB.conectar(pathManual, baudRate);
    try {
      const sessionId = await pila.sesion.iniciarSesion(ID_NODO_USB);
      pila.appRpc.cambiarMedioActivo(ID_NODO_USB);
      iniciarLoopHeartbeat();
      return {
        conectado: true,
        medio: "USB" as const,
        idNodo: ID_NODO_USB,
        puerto: pathAbierto,
        sessionId,
      };
    } catch (err) {
      await driverUSB.desconectar();
      pila.sesion.cerrarSesionLocal(ID_NODO_USB);
      throw err;
    }
  },
  desconectarPorUSB: async (): Promise<void> => {
    pila.sesion.cerrarSesionLocal(ID_NODO_USB);
    await driverUSB.desconectar();
    if (pila.fisica.estaActivo(ID_NODO_WIFI) && pila.sesion.estaAutenticada(ID_NODO_WIFI)) {
      pila.appRpc.cambiarMedioActivo(ID_NODO_WIFI);
    }
  },
  // CONEXIÓN Y DESCONEXIÓN POR WI-FI TCP (idNodo = 1)
  conectarPorWiFi: async (host = "192.168.4.1", puerto = 80) => {
    driverWiFi.cambiarDestino(host, puerto);
    await driverWiFi.conectar(4000);
    try {
      const sessionId = await pila.sesion.iniciarSesion(ID_NODO_WIFI);
      if (!pila.sesion.estaAutenticada(ID_NODO_USB)) {
        pila.appRpc.cambiarMedioActivo(ID_NODO_WIFI);
      }
      iniciarLoopHeartbeat();
      return {
        conectado: true,
        medio: "WIFI" as const,
        idNodo: ID_NODO_WIFI,
        host,
        puerto,
        sessionId,
      };
    } catch (err) {
      driverWiFi.desconectar();
      pila.sesion.cerrarSesionLocal(ID_NODO_WIFI);
      throw err;
    }
  },
  desconectarPorWiFi: async (): Promise<void> => {
    pila.sesion.cerrarSesionLocal(ID_NODO_WIFI);
    driverWiFi.desconectar();
    if (pila.fisica.estaActivo(ID_NODO_USB) && pila.sesion.estaAutenticada(ID_NODO_USB)) {
      pila.appRpc.cambiarMedioActivo(ID_NODO_USB);
    }
  },
  // CONSULTA DE ESTADO REAL (Capa 1 Física + Capa 5 Sesión)
  estaConectado: (idNodo?: number): boolean => {
    const nodo = typeof idNodo === "number" ? idNodo : pila.appRpc.obtenerMedioActivo();
    return pila.fisica.estaActivo(nodo) && pila.sesion.estaAutenticada(nodo);
  },
  obtenerEstadoConexion: () => {
    const usbOk = pila.fisica.estaActivo(ID_NODO_USB) && pila.sesion.estaAutenticada(ID_NODO_USB);
    const wifiOk = pila.fisica.estaActivo(ID_NODO_WIFI) && pila.sesion.estaAutenticada(ID_NODO_WIFI);
    const idNodoActivo = pila.appRpc.obtenerMedioActivo();

    return {
      conectado: usbOk || wifiOk,
      usbConectado: usbOk,
      wifiConectado: wifiOk,
      idNodoActivo,
      medioActivo: idNodoActivo === ID_NODO_USB ? "USB" : "WIFI",
    };
  }
};