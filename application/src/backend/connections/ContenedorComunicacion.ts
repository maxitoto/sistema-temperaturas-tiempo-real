import { app } from "electron";
import {
  DisconnectReason,
  MediumType,
  SessionHandle,
  TransportState,
} from "./proxy/ITransport";
import { LinkConfig, TransportProxy } from "./proxy/TransportProxy";
import { UartSTransport } from "./interfaces/UART-S/UartSTransport";
import { UartPhy, SerialPortInfo } from "./interfaces/UART-S/UART/UartPhy";
import { ReliabilityConfig } from "./interfaces/UART-S/Reliability/UartReliability";
import { WiFiTransport } from "./interfaces/WiFi/WiFiTransport";
import { rpc, RpcConfig } from "./messages/RPC/RpcApp";

export interface EstadoRedFirmware {
  staConectado: boolean;
  staSsid: string;
  ipDhcpAsignada: string;
  gatewayDhcp: string;
  mascaraSubred: string;
  rssiDbm: number;
  apActivo: boolean;
  apSsid: string;
  ipApPropia: string;
  macAddress: string;
  bootId: number;
  uptimeMs: number;
  resetReason: number;
  channel: number;
}

export interface LecturaTemperaturaStream {
  sensorId: number;
  celsius: number;
  timestampMs: number;
}

export interface ResultadoConexion {
  ok: boolean;
  medio: "UART-S" | "WIFI";
  clientId: number;
  sessionId: number;
  bootId: number;
  resetReason: number;
  rebootDetected: boolean;
  motivo: DisconnectReason;
}

export interface ResultadoTestMedio {
  medio: "UART-S" | "WIFI-STA" | "WIFI-AP";
  autenticado: boolean;
  sessionId: number;
  sumaRpc: number;
  echoRecibido: string;
  muestrasStreamRecibidas: LecturaTemperaturaStream[];
  estadoRed: EstadoRedFirmware;
  latenciaMs: number;
}

export const proxy = new TransportProxy();
export const uartS = new UartSTransport();
export const wifi = new WiFiTransport();

export const srv = new rpc.server(proxy);
export const c = new rpc.client(proxy, srv);

proxy.loadConfig();
uartS.loadConfig();
wifi.loadConfig();

const initLinkCfg = proxy.getConfig();
uartS.setLinkParameters(initLinkCfg.idleTimeoutMs, initLinkCfg.linkTimeoutMs);
wifi.setLinkParameters(initLinkCfg.idleTimeoutMs, initLinkCfg.linkTimeoutMs);

proxy.registerMedium(uartS);
proxy.registerMedium(wifi);

export const CLIENT_ID_UART = proxy.bindDefaultEndpoint(MediumType.UART_S, 1);
export const CLIENT_ID_WIFI = proxy.bindDefaultEndpoint(MediumType.WIFI, 1);

// T08: Reconexión perezosa bajo demanda cuando el medio entró en reposo (CLOSED_IDLE)
c.setEnsureConnectedHook(async (clientId: number): Promise<boolean> => {
  if (clientId === CLIENT_ID_UART && uartS.getState() === TransportState.CLOSED_IDLE) {
    return uartS.connect(3500);
  }
  if (clientId === CLIENT_ID_WIFI && wifi.getState() === TransportState.CLOSED_IDLE) {
    return wifi.connect(3500);
  }
  return proxy.isConnected();
});

app.on("before-quit", () => {
  void uartS.disconnect(DisconnectReason.USER_CLOSE);
  void wifi.disconnect(DisconnectReason.USER_CLOSE);
});

type TuplaEstadoRed = [
  boolean,
  string,
  string,
  string,
  string,
  number,
  boolean,
  string,
  string,
  string,
  number,
  number,
  number,
  number
];

function mapearEstadoRed(t: TuplaEstadoRed): EstadoRedFirmware {
  const estado: EstadoRedFirmware = {
    staConectado: Boolean(t[0]),
    staSsid: String(t[1] ?? ""),
    ipDhcpAsignada: String(t[2] ?? "0.0.0.0"),
    gatewayDhcp: String(t[3] ?? "0.0.0.0"),
    mascaraSubred: String(t[4] ?? "0.0.0.0"),
    rssiDbm: Number(t[5] ?? 0),
    apActivo: Boolean(t[6]),
    apSsid: String(t[7] ?? ""),
    ipApPropia: String(t[8] ?? "192.168.4.1"),
    macAddress: String(t[9] ?? ""),
    bootId: Number(t[10] ?? 0),
    uptimeMs: Number(t[11] ?? 0),
    resetReason: Number(t[12] ?? 0),
    channel: Number(t[13] ?? 1),
  };
  if (estado.staConectado && estado.ipDhcpAsignada !== "0.0.0.0") {
    wifi.configureTarget(estado.ipDhcpAsignada);
  }
  console.table(estado);
  return estado;
}

function resolverClienteExplicitado(clientId?: number): number {
  if (typeof clientId === "number" && clientId > 0) return clientId;
  if (
    proxy.isMediumConnected(MediumType.WIFI) ||
    wifi.getState() === TransportState.CLOSED_IDLE
  ) {
    return CLIENT_ID_WIFI;
  }
  if (
    proxy.isMediumConnected(MediumType.UART_S) ||
    uartS.getState() === TransportState.CLOSED_IDLE
  ) {
    return CLIENT_ID_UART;
  }
  return CLIENT_ID_UART;
}

async function ejecutarStreamTemperaturas(
  clientId: number,
  cantidadMuestras = 5
): Promise<LecturaTemperaturaStream[]> {
  const recibidas: LecturaTemperaturaStream[] = [];
  await c.stream<[number, number, number]>(
    "sensors.watch_temperature",
    { sensorId: 1, sampleCount: cantidadMuestras },
    (tupla) => {
      recibidas.push({
        sensorId: Number(tupla[0]),
        celsius: Number(Number(tupla[1]).toFixed(2)),
        timestampMs: Number(tupla[2]),
      });
    },
    clientId
  );
  return recibidas;
}

srv.bind("add", (a: number, b: number) => a + b);
srv.bind("system.echo", (mensaje: string) => mensaje);

setInterval(() => {
  srv.run();
}, 10);

export const conexionesBackend = {
  listarPuertosSerial: async (): Promise<SerialPortInfo[]> => {
    return UartPhy.listarPuertos();
  },

  conectarSerial: async (pathManual?: string): Promise<boolean> => {
    uartS.configureSerial(pathManual);
    return uartS.connect(3500);
  },

  conectarSerialDetallado: async (pathManual?: string): Promise<ResultadoConexion> => {
    uartS.configureSerial(pathManual);
    const ok = await uartS.connect(3500);
    const s = proxy.getSessionByClientId(CLIENT_ID_UART);
    return {
      ok,
      medio: "UART-S",
      clientId: CLIENT_ID_UART,
      sessionId: uartS.getSessionId(),
      bootId: uartS.getPeerBootId(),
      resetReason: uartS.getPeerResetReason(),
      rebootDetected: Boolean(s?.rebootDetected),
      motivo: ok ? DisconnectReason.NONE : uartS.getLastDisconnectReason(),
    };
  },

  desconectarSerial: async (): Promise<void> => {
    await uartS.disconnect(DisconnectReason.USER_CLOSE);
  },

  conectarWiFi: async (hostIp?: string): Promise<boolean> => {
    wifi.configureTarget(hostIp);
    return wifi.connect(3500);
  },

  conectarWiFiDetallado: async (hostIp?: string): Promise<ResultadoConexion> => {
    wifi.configureTarget(hostIp);
    const ok = await wifi.connect(3500);
    const s = proxy.getSessionByClientId(CLIENT_ID_WIFI);
    return {
      ok,
      medio: "WIFI",
      clientId: CLIENT_ID_WIFI,
      sessionId: wifi.getSessionId(),
      bootId: wifi.getPeerBootId(),
      resetReason: wifi.getPeerResetReason(),
      rebootDetected: Boolean(s?.rebootDetected),
      motivo: ok ? DisconnectReason.NONE : wifi.getLastDisconnectReason(),
    };
  },

  desconectarWiFi: async (): Promise<void> => {
    await wifi.disconnect(DisconnectReason.USER_CLOSE);
  },

  // H-11 / T17: Getter puro sin efectos secundarios (nunca cierra ni abre puertos)
  estaAutenticado: (clientId?: number): boolean => {
    if (clientId === CLIENT_ID_UART) return proxy.isMediumConnected(MediumType.UART_S);
    if (clientId === CLIENT_ID_WIFI) return proxy.isMediumConnected(MediumType.WIFI);
    return proxy.isConnected();
  },

  obtenerSesion: (medio: "UART-S" | "WIFI"): SessionHandle | null => {
    return proxy.getSessionHandle(medio === "UART-S" ? MediumType.UART_S : MediumType.WIFI);
  },

  obtenerDiagnosticoConexion: () => {
    const sUart = proxy.getSessionByClientId(CLIENT_ID_UART);
    const sWifi = proxy.getSessionByClientId(CLIENT_ID_WIFI);
    return {
      uart: {
        clientId: CLIENT_ID_UART,
        estado: TransportState[uartS.getState()],
        autenticado: proxy.isMediumConnected(MediumType.UART_S),
        puertoConfigurado: uartS.getConfiguredPath(),
        sessionId: sUart?.sessionId ?? 0,
        peerBootId: sUart?.peerBootId ?? 0,
        peerResetReason: sUart?.peerResetReason ?? 0,
        rebootDetected: sUart?.rebootDetected ?? false,
        idleNegociadoMs: sUart?.negotiatedIdleMs ?? 0,
        ultimoMotivoCierre: sUart?.lastDisconnectReason ?? uartS.getLastDisconnectReason(),
      },
      wifi: {
        clientId: CLIENT_ID_WIFI,
        estado: TransportState[wifi.getState()],
        autenticado: proxy.isMediumConnected(MediumType.WIFI),
        ipDestino: wifi.getTargetIp(),
        sessionId: sWifi?.sessionId ?? 0,
        peerBootId: sWifi?.peerBootId ?? 0,
        peerResetReason: sWifi?.peerResetReason ?? 0,
        rebootDetected: sWifi?.rebootDetected ?? false,
        idleNegociadoMs: sWifi?.negotiatedIdleMs ?? 0,
        ultimoMotivoCierre: sWifi?.lastDisconnectReason ?? wifi.getLastDisconnectReason(),
      },
    };
  },

  sumarEnFirmware: async (a: number, b: number, clientId?: number): Promise<number> => {
    const target = resolverClienteExplicitado(clientId);
    return (await c.callTo(target, "add", a, b)).as<number>();
  },

  decirHolaAlFirmware: async (texto = "hola", clientId?: number): Promise<string> => {
    const target = resolverClienteExplicitado(clientId);
    return (await c.callTo(target, "system.echo", texto)).as<string>();
  },

  verEstadoRed: async (clientId?: number): Promise<EstadoRedFirmware> => {
    const target = resolverClienteExplicitado(clientId);
    const tupla = (await c.callTo(target, "network.get_status")).as<TuplaEstadoRed>();
    return mapearEstadoRed(tupla);
  },

  // T05 / T11 / T15: soporta 2 argumentos (solo STA) o 4 argumentos (STA + AP), valida y aplica en forma diferida
  configurarWiFiCasa: async (
    staSsid: string,
    staPassword: string,
    apSsid = "",
    apPassword = "",
    clientId?: number
  ): Promise<EstadoRedFirmware> => {
    if (staSsid.trim().length > 32) {
      throw new Error("El SSID STA no puede superar 32 caracteres");
    }
    if (apSsid.trim().length > 32) {
      throw new Error("El SSID AP no puede superar 32 caracteres");
    }
    if (apPassword.length > 0 && (apPassword.length < 8 || apPassword.length > 64)) {
      throw new Error("La clave WPA2 del AP debe tener entre 8 y 64 caracteres");
    }

    const target = resolverClienteExplicitado(clientId);
    // zona base de datos
    const endpoint = apSsid.trim().length > 0 || apPassword.length > 0
      ? "network.configure"
      : "network.configure_sta";

    const res =
      endpoint === "network.configure"
        ? await c.callTo(target, "network.configure", staSsid.trim(), staPassword, apSsid.trim(), apPassword)
        : await c.callTo(target, "network.configure_sta", staSsid.trim(), staPassword);

    return mapearEstadoRed(res.as<TuplaEstadoRed>());
  },

  // T14: Configuración externa de cada directorio (Proxy, Reliability, RPC)
  configurarEnlace: async (cfg: Partial<LinkConfig>, clientId?: number): Promise<boolean> => {
    if (!proxy.applyConfig(cfg)) {
      throw new Error(
        "Parámetros de enlace inválidos (verificar linkTimeoutMs >= 2 * probeIntervalMs e idleTimeoutMs >= 5000)"
      );
    }
    const applied = proxy.getConfig();
    uartS.setLinkParameters(applied.idleTimeoutMs, applied.linkTimeoutMs);
    wifi.setLinkParameters(applied.idleTimeoutMs, applied.linkTimeoutMs);

    if (proxy.isConnected()) {
      const target = resolverClienteExplicitado(clientId);
      return (
        await c.callTo(
          target,
          "link.configure",
          applied.probeIntervalMs,
          applied.linkTimeoutMs,
          applied.idleTimeoutMs,
          applied.gracefulWaitMs
        )
      ).as<boolean>();
    }
    return true;
  },

  configurarFiabilidadSerial: async (
    cfg: Partial<ReliabilityConfig>,
    clientId?: number
  ): Promise<boolean> => {
    if (!uartS.reliability().applyConfig(cfg, proxy.getConfig().linkTimeoutMs)) {
      throw new Error(
        "Parámetros de fiabilidad inválidos (ackTimeoutMs * maxRetries debe ser <= linkTimeoutMs)"
      );
    }
    const r = uartS.reliability().getConfig();
    if (proxy.isConnected()) {
      const target = resolverClienteExplicitado(clientId);
      return (
        await c.callTo(target, "reliability.configure", r.ackTimeoutMs, r.maxRetries, r.maxChunkSize)
      ).as<boolean>();
    }
    return true;
  },

  configurarRpc: async (cfg: Partial<RpcConfig>, clientId?: number): Promise<boolean> => {
    const okSkel = srv.skeleton().applyConfig(cfg);
    const okStub = c.stub().applyConfig(cfg);
    if (!okSkel || !okStub) {
      throw new Error("Parámetros RPC fuera de rango");
    }
    const sk = srv.skeleton().getConfig();
    const st = c.stub().getConfig();
    if (proxy.isConnected()) {
      const target = resolverClienteExplicitado(clientId);
      return (
        await c.callTo(
          target,
          "rpc.configure",
          sk.maxHistoryEntries,
          sk.historyTtlMs,
          st.retryTimeoutMs,
          st.maxRpcRetries
        )
      ).as<boolean>();
    }
    return true;
  },

  obtenerConfiguraciones: async (clientId?: number) => {
    let firmwareCfg: unknown = null;
    if (proxy.isConnected()) {
      const target = resolverClienteExplicitado(clientId);
      firmwareCfg = (await c.callTo(target, "config.get")).as<unknown>();
    }
    return {
      backend: {
        link: proxy.getConfig(),
        reliability: uartS.reliability().getConfig(),
        rpcSkeleton: srv.skeleton().getConfig(),
        rpcStub: c.stub().getConfig(),
      },
      firmware: firmwareCfg,
    };
  },

  testUartS: async (pathManual?: string): Promise<ResultadoTestMedio> => {
    const t0 = performance.now();
    if (!proxy.isMediumConnected(MediumType.UART_S)) {
      const ok = await conexionesBackend.conectarSerial(pathManual);
      if (!ok) {
        throw new Error(
          `No se pudo autenticar UART-S (motivo=${uartS.getLastDisconnectReason()})`
        );
      }
    }

    const [resSuma, resEcho] = await Promise.all([
      c.callTo(CLIENT_ID_UART, "add", 2, 3),
      c.callTo(CLIENT_ID_UART, "system.echo", "hola desde UART-S"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await c.callTo(CLIENT_ID_UART, "network.get_status")).as<TuplaEstadoRed>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(CLIENT_ID_UART, 5);

    return {
      medio: "UART-S",
      autenticado: true,
      sessionId: uartS.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },

  testWiFiSta: async (ipDhcpManual?: string): Promise<ResultadoTestMedio> => {
    const t0 = performance.now();
    let targetIp = ipDhcpManual?.trim() || "";

    if (!targetIp && proxy.isMediumConnected(MediumType.UART_S)) {
      const red = mapearEstadoRed(
        (await c.callTo(CLIENT_ID_UART, "network.get_status")).as<TuplaEstadoRed>()
      );
      if (red.staConectado && red.ipDhcpAsignada !== "0.0.0.0") {
        targetIp = red.ipDhcpAsignada;
      }
    }

    if (!targetIp && wifi.getTargetIp() !== WiFiTransport.DEFAULT_AP_IP) {
      targetIp = wifi.getTargetIp();
    }

    if (!targetIp) {
      throw new Error(
        "El ESP32 aún no tiene IP DHCP conocida. Pasá la IP a testWiFiSta('192.168.1.X') o consultá verEstadoRed() tras configurarWiFiCasa()."
      );
    }

    const ok = await conexionesBackend.conectarWiFi(targetIp);
    if (!ok) {
      throw new Error(
        `Fallo autenticando WiFi STA contra ${targetIp} (motivo=${wifi.getLastDisconnectReason()})`
      );
    }

    const [resSuma, resEcho] = await Promise.all([
      c.callTo(CLIENT_ID_WIFI, "add", 20, 22),
      c.callTo(CLIENT_ID_WIFI, "system.echo", "hola desde WiFi-STA"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await c.callTo(CLIENT_ID_WIFI, "network.get_status")).as<TuplaEstadoRed>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(CLIENT_ID_WIFI, 5);

    return {
      medio: "WIFI-STA",
      autenticado: true,
      sessionId: wifi.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },

  testWiFiAp: async (): Promise<ResultadoTestMedio> => {
    const t0 = performance.now();
    const ipPrevia = wifi.getTargetIp();
    const ok = await conexionesBackend.conectarWiFi(WiFiTransport.DEFAULT_AP_IP);
    if (!ok) {
      if (ipPrevia && ipPrevia !== WiFiTransport.DEFAULT_AP_IP) {
        wifi.configureTarget(ipPrevia);
      }
      throw new Error(
        `No se pudo conectar al AP ${WiFiTransport.DEFAULT_AP_IP} (motivo=${wifi.getLastDisconnectReason()}). Conectá el WiFi de tu Mac al AP del ESP32.`
      );
    }

    const [resSuma, resEcho] = await Promise.all([
      c.callTo(CLIENT_ID_WIFI, "add", 10, 5),
      c.callTo(CLIENT_ID_WIFI, "system.echo", "hola desde WiFi-AP"),
    ]);

    const estadoRed = mapearEstadoRed(
      (await c.callTo(CLIENT_ID_WIFI, "network.get_status")).as<TuplaEstadoRed>()
    );
    const muestrasStreamRecibidas = await ejecutarStreamTemperaturas(CLIENT_ID_WIFI, 5);

    return {
      medio: "WIFI-AP",
      autenticado: true,
      sessionId: wifi.getSessionId(),
      sumaRpc: resSuma.as<number>(),
      echoRecibido: resEcho.as<string>(),
      muestrasStreamRecibidas,
      estadoRed,
      latenciaMs: Math.round(performance.now() - t0),
    };
  },
};