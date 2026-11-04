import { DisconnectReason } from "../../connections/proxy/ITransport";
import { LinkConfig } from "../../connections/proxy/TransportProxy";
import { ReliabilityConfig } from "../../connections/interfaces/UART-S/Reliability/UartReliability";
import { RpcConfig } from "../../connections/messages/RPC/RpcApp";

export type { LinkConfig, ReliabilityConfig, RpcConfig };

export interface EstadoRedFirmwareDTO {
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

export type TuplaEstadoRedDTO = [
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

export interface ResultadoConexionDTO {
  ok: boolean;
  medio: "UART-S" | "WIFI";
  clientId: number;
  sessionId: number;
  bootId: number;
  resetReason: number;
  rebootDetected: boolean;
  motivo: DisconnectReason;
}

export interface ConfigSnapshotFirmwareDTO {
  staSsid: string;
  apSsid: string;
  apChannel: number;
  probeIntervalMs: number;
  linkTimeoutMs: number;
  idleTimeoutMs: number;
  gracefulWaitMs: number;
  ackTimeoutMs: number;
  maxRetries: number;
  maxChunkSize: number;
  maxHistoryEntries: number;
  rpcRetryTimeoutMs: number;
  rpcMaxRetries: number;
}

export type TuplaConfigSnapshotFirmwareDTO = [
  string,
  string,
  number,
  number,
  number,
  number,
  number,
  number,
  number,
  number,
  number,
  number,
  number
];