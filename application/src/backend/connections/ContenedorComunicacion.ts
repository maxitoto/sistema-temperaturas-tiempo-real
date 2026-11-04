import { app } from "electron";
import {
  DisconnectReason,
  MediumType,
  SessionHandle,
  TransportState,
} from "./proxy/ITransport";
import { TransportProxy } from "./proxy/TransportProxy";
import { UartSTransport } from "./interfaces/UART-S/UartSTransport";
import { WiFiTransport } from "./interfaces/WiFi/WiFiTransport";
import { rpc } from "./messages/RPC/RpcApp";

class CommunicationContainer {
  private static instance_: CommunicationContainer | null = null;

  public readonly proxy = new TransportProxy();
  public readonly uartS = new UartSTransport();
  public readonly wifi = new WiFiTransport();
  public readonly srv = new rpc.server(this.proxy);
  public readonly c = new rpc.client(this.proxy, this.srv);

  public readonly CLIENT_ID_UART: number;
  public readonly CLIENT_ID_WIFI: number;

  private constructor() {
    this.proxy.loadConfig();
    this.uartS.loadConfig();
    this.wifi.loadConfig();

    const initLinkCfg = this.proxy.getConfig();
    this.uartS.setLinkParameters(initLinkCfg.idleTimeoutMs, initLinkCfg.linkTimeoutMs);
    this.wifi.setLinkParameters(initLinkCfg.idleTimeoutMs, initLinkCfg.linkTimeoutMs);

    this.proxy.registerMedium(this.uartS);
    this.proxy.registerMedium(this.wifi);

    this.CLIENT_ID_UART = this.proxy.bindDefaultEndpoint(MediumType.UART_S, 1);
    this.CLIENT_ID_WIFI = this.proxy.bindDefaultEndpoint(MediumType.WIFI, 1);

    // Reconexión perezosa bajo demanda cuando el medio entró en reposo (CLOSED_IDLE)
    this.c.setEnsureConnectedHook(async (clientId: number): Promise<boolean> => {
      if (
        clientId === this.CLIENT_ID_UART &&
        this.uartS.getState() === TransportState.CLOSED_IDLE
      ) {
        return this.uartS.connect(3500);
      }
      if (
        clientId === this.CLIENT_ID_WIFI &&
        this.wifi.getState() === TransportState.CLOSED_IDLE
      ) {
        return this.wifi.connect(3500);
      }
      return this.proxy.isConnected();
    });

    app.on("before-quit", () => {
      void this.uartS.disconnect(DisconnectReason.USER_CLOSE);
      void this.wifi.disconnect(DisconnectReason.USER_CLOSE);
    });

    setInterval(() => {
      this.srv.run();
    }, 10);
  }

  public static instance(): CommunicationContainer {
    if (!CommunicationContainer.instance_) {
      CommunicationContainer.instance_ = new CommunicationContainer();
    }
    return CommunicationContainer.instance_;
  }

  public resolverClienteExplicitado(clientId?: number): number {
    if (typeof clientId === "number" && clientId > 0) return clientId;
    if (
      this.proxy.isMediumConnected(MediumType.WIFI) ||
      this.wifi.getState() === TransportState.CLOSED_IDLE
    ) {
      return this.CLIENT_ID_WIFI;
    }
    if (
      this.proxy.isMediumConnected(MediumType.UART_S) ||
      this.uartS.getState() === TransportState.CLOSED_IDLE
    ) {
      return this.CLIENT_ID_UART;
    }
    return this.CLIENT_ID_UART;
  }

  public obtenerSesion(medio: "UART-S" | "WIFI"): SessionHandle | null {
    return this.proxy.getSessionHandle(
      medio === "UART-S" ? MediumType.UART_S : MediumType.WIFI
    );
  }
}

export const comm = CommunicationContainer.instance();
export const { proxy, uartS, wifi, srv, c, CLIENT_ID_UART, CLIENT_ID_WIFI } = comm;