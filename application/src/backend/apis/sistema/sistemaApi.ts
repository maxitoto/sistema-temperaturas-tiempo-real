import { comm } from "../../connections/ContenedorComunicacion";
import { MediumType } from "../../connections/proxy/ITransport";
import { WiFiTransport } from "../../connections/interfaces/WiFi/WiFiTransport";
import { conexionApi, mapearEstadoRed } from "../conexion/conexionApi";
import { TuplaEstadoRedDTO } from "../conexion/conexionDTO";
import { ejecutarStreamTemperaturas } from "../sensores/sensoresApi";
import { ResultadoTestMedioDTO } from "./sistemaDTO";

let sistemaEndpointsRegistrados = false;

export function registrarSistemaEndpointsLocales(): void {
  if (sistemaEndpointsRegistrados) return;
  sistemaEndpointsRegistrados = true;

  comm.srv.bind("system.echo", (mensaje: string) => mensaje);
  comm.srv.bind("add", (a: number, b: number) => a + b);
}

export const sistemaApi = {
  decirHolaAlFirmware: async (texto = "hola", clientId?: number): Promise<string> => {
    const target = comm.resolverClienteExplicitado(clientId);
    return (await comm.c.callTo(target, "system.echo", texto)).as<string>();
  },

  sumarEnFirmware: async (a: number, b: number, clientId?: number): Promise<number> => {
    const target = comm.resolverClienteExplicitado(clientId);
    return (await comm.c.callTo(target, "add", a, b)).as<number>();
  },

  testUartS: async (pathManual?: string): Promise<ResultadoTestMedioDTO> => {
    const t0 = performance.now();
    if (!comm.proxy.isMediumConnected(MediumType.UART_S)) {
      const ok = await conexionApi.conectarSerial(pathManual);
      if (!ok) {
        throw new Error(
          `No se pudo autenticar UART-S (motivo=${comm.uartS.getLastDisconnectReason()})`
        );
      }
    }

    const [resSuma, resEcho] = await Promise.all([
      comm.c.callTo(comm.CLIENT_ID_UART, "add", 2, 3),
      comm.c.callTo(comm.CLIENT_ID_UART, "system.echo", "hola desde UART-S"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await comm.c.callTo(comm.CLIENT_ID_UART, "network.get_status")).as<TuplaEstadoRedDTO>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(comm.CLIENT_ID_UART, 5);

    return {
      medio: "UART-S",
      autenticado: true,
      sessionId: comm.uartS.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },

  testWiFiSta: async (ipDhcpManual?: string): Promise<ResultadoTestMedioDTO> => {
    const t0 = performance.now();
    let targetIp = ipDhcpManual?.trim() || "";

    if (!targetIp && comm.proxy.isMediumConnected(MediumType.UART_S)) {
      const red = mapearEstadoRed(
        (await comm.c.callTo(comm.CLIENT_ID_UART, "network.get_status")).as<TuplaEstadoRedDTO>()
      );
      if (red.staConectado && red.ipDhcpAsignada !== "0.0.0.0") {
        targetIp = red.ipDhcpAsignada;
      }
    }

    if (!targetIp && comm.wifi.getTargetIp() !== WiFiTransport.DEFAULT_AP_IP) {
      targetIp = comm.wifi.getTargetIp();
    }

    if (!targetIp) {
      throw new Error(
        "El ESP32 aún no tiene IP DHCP conocida. Pasá la IP a testWiFiSta('192.168.1.X') o consultá verEstadoRed() tras configurarWiFiCasa()."
      );
    }

    const ok = await conexionApi.conectarWiFi(targetIp);
    if (!ok) {
      throw new Error(
        `Fallo autenticando WiFi STA contra ${targetIp} (motivo=${comm.wifi.getLastDisconnectReason()})`
      );
    }

    const [resSuma, resEcho] = await Promise.all([
      comm.c.callTo(comm.CLIENT_ID_WIFI, "add", 20, 22),
      comm.c.callTo(comm.CLIENT_ID_WIFI, "system.echo", "hola desde WiFi-STA"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await comm.c.callTo(comm.CLIENT_ID_WIFI, "network.get_status")).as<TuplaEstadoRedDTO>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(comm.CLIENT_ID_WIFI, 5);

    return {
      medio: "WIFI-STA",
      autenticado: true,
      sessionId: comm.wifi.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },

  testWiFiAp: async (): Promise<ResultadoTestMedioDTO> => {
    const t0 = performance.now();
    const ipPrevia = comm.wifi.getTargetIp();
    const ok = await conexionApi.conectarWiFi(WiFiTransport.DEFAULT_AP_IP);
    if (!ok) {
      if (ipPrevia && ipPrevia !== WiFiTransport.DEFAULT_AP_IP) {
        comm.wifi.configureTarget(ipPrevia);
      }
      throw new Error(
        `No se pudo conectar al AP ${WiFiTransport.DEFAULT_AP_IP} (motivo=${comm.wifi.getLastDisconnectReason()}). Conectá el WiFi de tu Mac al AP del ESP32.`
      );
    }

    const [resSuma, resEcho] = await Promise.all([
      comm.c.callTo(comm.CLIENT_ID_WIFI, "add", 10, 5),
      comm.c.callTo(comm.CLIENT_ID_WIFI, "system.echo", "hola desde WiFi-AP"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await comm.c.callTo(comm.CLIENT_ID_WIFI, "network.get_status")).as<TuplaEstadoRedDTO>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(comm.CLIENT_ID_WIFI, 5);

    return {
      medio: "WIFI-AP",
      autenticado: true,
      sessionId: comm.wifi.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },
};