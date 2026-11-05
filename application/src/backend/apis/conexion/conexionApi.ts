import { comm } from "../../connections/ContenedorComunicacion";
import {
  DisconnectReason,
  MediumType,
  SessionHandle,
  TransportState,
} from "../../connections/proxy/ITransport";
import { UartPhy, SerialPortInfo } from "../../connections/interfaces/UART-S/UART/UartPhy";
import {
  ConfigSnapshotFirmwareDTO,
  EstadoRedFirmwareDTO,
  LinkConfig,
  ReliabilityConfig,
  ResultadoConexionDTO,
  RpcConfig,
  TuplaConfigSnapshotFirmwareDTO,
  TuplaEstadoRedDTO,
} from "./conexionDTO";

export function mapearEstadoRed(t: TuplaEstadoRedDTO): EstadoRedFirmwareDTO {
  const estado: EstadoRedFirmwareDTO = {
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
    comm.wifi.configureTarget(estado.ipDhcpAsignada);
  }
  console.table(estado);
  return estado;
}

function mapearConfigSnapshot(t: TuplaConfigSnapshotFirmwareDTO): ConfigSnapshotFirmwareDTO {
  return {
    staSsid: String(t[0] ?? ""),
    apSsid: String(t[1] ?? ""),
    apChannel: Number(t[2] ?? 1),
    probeIntervalMs: Number(t[3] ?? 1000),
    linkTimeoutMs: Number(t[4] ?? 2800),
    idleTimeoutMs: Number(t[5] ?? 600000),
    gracefulWaitMs: Number(t[6] ?? 1000),
    ackTimeoutMs: Number(t[7] ?? 250),
    maxRetries: Number(t[8] ?? 6),
    maxChunkSize: Number(t[9] ?? 240),
    maxHistoryEntries: Number(t[10] ?? 64),
    rpcRetryTimeoutMs: Number(t[11] ?? 2500),
    rpcMaxRetries: Number(t[12] ?? 2),
  };
}

export const conexionApi = {
  listarPuertosSerial: async (): Promise<SerialPortInfo[]> => {
    return UartPhy.listarPuertos();
  },

  conectarSerial: async (pathManual?: string): Promise<boolean> => {
    comm.uartS.configureSerial(pathManual);
    return comm.uartS.connect(3500);
  },

  conectarSerialDetallado: async (pathManual?: string): Promise<ResultadoConexionDTO> => {
    comm.uartS.configureSerial(pathManual);
    const ok = await comm.uartS.connect(3500);
    const s = comm.proxy.getSessionByClientId(comm.CLIENT_ID_UART);
    return {
      ok,
      medio: "UART-S",
      clientId: comm.CLIENT_ID_UART,
      sessionId: comm.uartS.getSessionId(),
      bootId: comm.uartS.getPeerBootId(),
      resetReason: comm.uartS.getPeerResetReason(),
      rebootDetected: Boolean(s?.rebootDetected),
      motivo: ok ? DisconnectReason.NONE : comm.uartS.getLastDisconnectReason(),
    };
  },

  desconectarSerial: async (): Promise<void> => {
    await comm.uartS.disconnect(DisconnectReason.USER_CLOSE);
  },

  conectarWiFi: async (hostIp?: string): Promise<boolean> => {
    comm.wifi.configureTarget(hostIp);
    return comm.wifi.connect(3500);
  },

  conectarWiFiDetallado: async (hostIp?: string): Promise<ResultadoConexionDTO> => {
    comm.wifi.configureTarget(hostIp);
    const ok = await comm.wifi.connect(3500);
    const s = comm.proxy.getSessionByClientId(comm.CLIENT_ID_WIFI);
    return {
      ok,
      medio: "WIFI",
      clientId: comm.CLIENT_ID_WIFI,
      sessionId: comm.wifi.getSessionId(),
      bootId: comm.wifi.getPeerBootId(),
      resetReason: comm.wifi.getPeerResetReason(),
      rebootDetected: Boolean(s?.rebootDetected),
      motivo: ok ? DisconnectReason.NONE : comm.wifi.getLastDisconnectReason(),
    };
  },

  desconectarWiFi: async (): Promise<void> => {
    await comm.wifi.disconnect(DisconnectReason.USER_CLOSE);
  },

  estaAutenticado: (clientId?: number): boolean => {
    if (clientId === comm.CLIENT_ID_UART) return comm.proxy.isMediumConnected(MediumType.UART_S);
    if (clientId === comm.CLIENT_ID_WIFI) return comm.proxy.isMediumConnected(MediumType.WIFI);
    return comm.proxy.isConnected();
  },

  obtenerSesion: (medio: "UART-S" | "WIFI"): SessionHandle | null => {
    return comm.obtenerSesion(medio);
  },

  obtenerDiagnosticoConexion: () => {
    const sUart = comm.proxy.getSessionByClientId(comm.CLIENT_ID_UART);
    const sWifi = comm.proxy.getSessionByClientId(comm.CLIENT_ID_WIFI);
    return {
      uart: {
        clientId: comm.CLIENT_ID_UART,
        estado: TransportState[comm.uartS.getState()],
        autenticado: comm.proxy.isMediumConnected(MediumType.UART_S),
        puertoConfigurado: comm.uartS.getConfiguredPath(),
        sessionId: sUart?.sessionId ?? 0,
        peerBootId: sUart?.peerBootId ?? 0,
        peerResetReason: sUart?.peerResetReason ?? 0,
        rebootDetected: sUart?.rebootDetected ?? false,
        idleNegociadoMs: sUart?.negotiatedIdleMs ?? 0,
        ultimoMotivoCierre: sUart?.lastDisconnectReason ?? comm.uartS.getLastDisconnectReason(),
      },
      wifi: {
        clientId: comm.CLIENT_ID_WIFI,
        estado: TransportState[comm.wifi.getState()],
        autenticado: comm.proxy.isMediumConnected(MediumType.WIFI),
        ipDestino: comm.wifi.getTargetIp(),
        sessionId: sWifi?.sessionId ?? 0,
        peerBootId: sWifi?.peerBootId ?? 0,
        peerResetReason: sWifi?.peerResetReason ?? 0,
        rebootDetected: sWifi?.rebootDetected ?? false,
        idleNegociadoMs: sWifi?.negotiatedIdleMs ?? 0,
        ultimoMotivoCierre: sWifi?.lastDisconnectReason ?? comm.wifi.getLastDisconnectReason(),
      },
    };
  },

  verEstadoRed: async (clientId?: number): Promise<EstadoRedFirmwareDTO> => {
    const target = comm.resolverClienteExplicitado(clientId);
    const tupla = (await comm.c.callTo(target, "network.get_status")).as<TuplaEstadoRedDTO>();
    return mapearEstadoRed(tupla);
  },

  configurarWiFiCasa: async (
    staSsid: string,
    staPassword: string,
    apSsid = "",
    apPassword = "",
    clientId?: number
  ): Promise<EstadoRedFirmwareDTO> => {
    if (staSsid.trim().length > 32) {
      throw new Error("El SSID STA no puede superar 32 caracteres");
    }
    if (apSsid.trim().length > 32) {
      throw new Error("El SSID AP no puede superar 32 caracteres");
    }
    if (apPassword.length > 0 && (apPassword.length < 8 || apPassword.length > 64)) {
      throw new Error("La clave WPA2 del AP debe tener entre 8 y 64 caracteres");
    }

    for (let i = 0; i < 2 && !comm.proxy.isConnected(); i++) {
      await comm.uartS.connect(8000);
    }
    const target = comm.resolverClienteExplicitado(clientId);
    // zona base de datos
    const usaCuatroArgs = apSsid.trim().length > 0 || apPassword.length > 0;
    const res = usaCuatroArgs
      ? await comm.c.callTo(
          target,
          "network.configure",
          staSsid.trim(),
          staPassword,
          apSsid.trim(),
          apPassword
        )
      : await comm.c.callTo(target, "network.configure_sta", staSsid.trim(), staPassword);

    return mapearEstadoRed(res.as<TuplaEstadoRedDTO>());
  },

  configurarEnlace: async (cfg: Partial<LinkConfig>, clientId?: number): Promise<boolean> => {
    if (!comm.proxy.applyConfig(cfg)) {
      throw new Error(
        "Parámetros de enlace inválidos (verificar linkTimeoutMs >= 2 * probeIntervalMs e idleTimeoutMs >= 5000)"
      );
    }
    const applied = comm.proxy.getConfig();
    comm.uartS.setLinkParameters(applied.idleTimeoutMs, applied.linkTimeoutMs);
    comm.wifi.setLinkParameters(applied.idleTimeoutMs, applied.linkTimeoutMs);

    if (comm.proxy.isConnected()) {
      const target = comm.resolverClienteExplicitado(clientId);
      return (
        await comm.c.callTo(
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
    if (!comm.uartS.reliability().applyConfig(cfg, comm.proxy.getConfig().linkTimeoutMs)) {
      throw new Error(
        "Parámetros de fiabilidad inválidos (ackTimeoutMs * maxRetries debe ser <= linkTimeoutMs)"
      );
    }
    const r = comm.uartS.reliability().getConfig();
    if (comm.proxy.isConnected()) {
      const target = comm.resolverClienteExplicitado(clientId);
      return (
        await comm.c.callTo(
          target,
          "reliability.configure",
          r.ackTimeoutMs,
          r.maxRetries,
          r.maxChunkSize
        )
      ).as<boolean>();
    }
    return true;
  },

  configurarRpc: async (cfg: Partial<RpcConfig>, clientId?: number): Promise<boolean> => {
    const okSkel = comm.srv.skeleton().applyConfig(cfg);
    const okStub = comm.c.stub().applyConfig(cfg);
    if (!okSkel || !okStub) {
      throw new Error("Parámetros RPC fuera de rango");
    }
    const sk = comm.srv.skeleton().getConfig();
    const st = comm.c.stub().getConfig();
    if (comm.proxy.isConnected()) {
      const target = comm.resolverClienteExplicitado(clientId);
      return (
        await comm.c.callTo(
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
    let firmwareCfg: ConfigSnapshotFirmwareDTO | null = null;
    if (comm.proxy.isConnected()) {
      const target = comm.resolverClienteExplicitado(clientId);
      const raw = (await comm.c.callTo(target, "config.get")).as<TuplaConfigSnapshotFirmwareDTO>();
      if (Array.isArray(raw)) {
        firmwareCfg = mapearConfigSnapshot(raw);
      }
    }
    return {
      backend: {
        link: comm.proxy.getConfig(),
        reliability: comm.uartS.reliability().getConfig(),
        rpcSkeleton: comm.srv.skeleton().getConfig(),
        rpcStub: comm.c.stub().getConfig(),
      },
      firmware: firmwareCfg,
    };
  },
};  