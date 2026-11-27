import net from "node:net";
import crypto from "node:crypto";
import os from "node:os";

export interface EstadoRelayLocalDTO {
  activo: boolean;
  ipLan: string;
  puertoTunel: number;
  puertoClientes: number;
  espConectado: boolean;
  clientesConectados: number;
}

export interface ConfigInicioRelayLocalDTO {
  puertoTunel?: number;
  puertoClientes?: number;
  nodeId?: string;
  secreto?: string;
}

const MAX_CLIENTES = 4;
const MAX_PAYLOAD_TO_ESP = 512;
const MAX_PAYLOAD_FROM_ESP = 2048;
const IDLE_CLIENT_MS = 150_000;
const TUNNEL_TIMEOUT_MS = 50_000;

const OP = {
  CHALLENGE: 0x00,
  HELLO: 0x01,
  OPEN: 0x02,
  DATA: 0x03,
  CLOSE: 0x04,
  PING: 0x05,
  PONG: 0x06,
  AUTH_OK: 0x07,
} as const;

let serverTunel: net.Server | null = null;
let serverClientes: net.Server | null = null;
let watchdogTimer: ReturnType<typeof setInterval> | null = null;
let espSocket: net.Socket | null = null;
let espAutenticado = false;
let espUltimoRxMs = 0;
let puertoTunelActivo = 9443;
let puertoClientesActivo = 8443;

const clientesPorSid = new Map<number, net.Socket>();

function obtenerIpLanLocal(): string {
  const ifaces = os.networkInterfaces();
  for (const lista of Object.values(ifaces)) {
    if (!lista) continue;
    for (const info of lista) {
      if (info.family === "IPv4" && !info.internal) {
        return info.address;
      }
    }
  }
  return "127.0.0.1";
}

function packMux(op: number, sid: number, payload: Buffer = Buffer.alloc(0)): Buffer {
  const hdr = Buffer.alloc(4);
  hdr[0] = op;
  hdr[1] = sid;
  hdr.writeUInt16BE(payload.length, 2);
  return payload.length > 0 ? Buffer.concat([hdr, payload]) : hdr;
}

function asignarSidLibre(): number | null {
  for (let sid = 1; sid <= MAX_CLIENTES; sid++) {
    if (!clientesPorSid.has(sid)) return sid;
  }
  return null;
}

function enviarAlEsp(op: number, sid: number, payload?: Buffer): boolean {
  if (!espSocket || !espAutenticado || espSocket.destroyed) return false;
  return espSocket.write(packMux(op, sid, payload));
}

function cerrarTodosLosClientes(): void {
  for (const [sid, cli] of clientesPorSid.entries()) {
    cli.destroy();
    clientesPorSid.delete(sid);
  }
}

export function obtenerEstadoRelayLocal(): EstadoRelayLocalDTO {
  return {
    activo: serverTunel !== null && serverClientes !== null,
    ipLan: obtenerIpLanLocal(),
    puertoTunel: puertoTunelActivo,
    puertoClientes: puertoClientesActivo,
    espConectado: espAutenticado,
    clientesConectados: clientesPorSid.size,
  };
}

export async function detenerRelayLocal(): Promise<EstadoRelayLocalDTO> {
  if (watchdogTimer) {
    clearInterval(watchdogTimer);
    watchdogTimer = null;
  }
  cerrarTodosLosClientes();
  if (espSocket) {
    espSocket.destroy();
    espSocket = null;
  }
  espAutenticado = false;

  const cerrarServer = (srv: net.Server | null) =>
    new Promise<void>((resolve) => {
      if (!srv) return resolve();
      srv.close(() => resolve());
    });

  await Promise.all([cerrarServer(serverTunel), cerrarServer(serverClientes)]);
  serverTunel = null;
  serverClientes = null;

  return obtenerEstadoRelayLocal();
}

export async function iniciarRelayLocal(
  cfg: ConfigInicioRelayLocalDTO = {}
): Promise<EstadoRelayLocalDTO> {
  const secreto = (cfg.secreto ?? "").trim();
  if (secreto.length < 16) {
    throw new Error("ValueError: el secreto del Relay local debe tener al menos 16 caracteres");
  }

  await detenerRelayLocal();

  puertoTunelActivo = cfg.puertoTunel && cfg.puertoTunel > 0 ? cfg.puertoTunel : 9443;
  puertoClientesActivo = cfg.puertoClientes && cfg.puertoClientes > 0 ? cfg.puertoClientes : 8443;
  const nodeIdRaw = (cfg.nodeId ?? "ESP32-UNTDF-0001").trim() || "ESP32-UNTDF-0001";
  const nodeIdBuf = Buffer.alloc(16, 0);
  Buffer.from(nodeIdRaw, "utf8").copy(nodeIdBuf, 0, 0, 16);

  serverTunel = net.createServer((sock) => {
    sock.setNoDelay(true);
    sock.setKeepAlive(true, 15_000);

    const nonce = crypto.randomBytes(16);
    let autenticado = false;
    let rxBuf: Buffer = Buffer.alloc(0);

    sock.write(packMux(OP.CHALLENGE, 0, nonce));

    const authTimer = setTimeout(() => {
      if (!autenticado) sock.destroy();
    }, 5_000);

    sock.on("data", (chunk) => {
      const data = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
      rxBuf = Buffer.concat([rxBuf, data]);

      while (rxBuf.length >= 4) {
        const op = rxBuf[0];
        const sid = rxBuf[1];
        const len = rxBuf.readUInt16BE(2);

        if (len > MAX_PAYLOAD_FROM_ESP) {
          sock.destroy();
          return;
        }
        if (rxBuf.length < 4 + len) break;

        const payload = rxBuf.subarray(4, 4 + len);
        rxBuf = rxBuf.subarray(4 + len);
        espUltimoRxMs = Date.now();

        if (!autenticado) {
          if (op !== OP.HELLO || payload.length !== 48) {
            sock.destroy();
            return;
          }
          const nodeIdRx = payload.subarray(0, 16);
          const macRx = payload.subarray(16, 48);
          const esperado = crypto
            .createHmac("sha256", secreto)
            .update(Buffer.concat([nodeIdRx, nonce]))
            .digest();

          if (
            !crypto.timingSafeEqual(nodeIdRx, nodeIdBuf) ||
            !crypto.timingSafeEqual(macRx, esperado)
          ) {
            sock.destroy();
            return;
          }

          clearTimeout(authTimer);
          autenticado = true;

          if (espSocket && espSocket !== sock) {
            cerrarTodosLosClientes();
            espSocket.destroy();
          }
          espSocket = sock;
          espAutenticado = true;
          sock.write(packMux(OP.AUTH_OK, 0));
          continue;
        }

        if (op === OP.PING) {
          sock.write(packMux(OP.PONG, 0));
        } else if (op === OP.DATA) {
          const cli = clientesPorSid.get(sid);
          if (cli && !cli.destroyed) cli.write(payload);
        } else if (op === OP.CLOSE) {
          const cli = clientesPorSid.get(sid);
          if (cli) {
            clientesPorSid.delete(sid);
            cli.end();
          }
        }
      }
    });

    sock.on("close", () => {
      clearTimeout(authTimer);
      if (espSocket === sock) {
        espSocket = null;
        espAutenticado = false;
        cerrarTodosLosClientes();
      }
    });

    sock.on("error", () => sock.destroy());
  });

  serverClientes = net.createServer((cli) => {
    cli.setNoDelay(true);
    cli.setTimeout(IDLE_CLIENT_MS);

    if (!espAutenticado) {
      cli.destroy();
      return;
    }

    const sid = asignarSidLibre();
    if (sid === null) {
      cli.destroy();
      return;
    }

    clientesPorSid.set(sid, cli);
    enviarAlEsp(OP.OPEN, sid);

    cli.on("data", (chunk) => {
      const data = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
      for (let off = 0; off < data.length; off += MAX_PAYLOAD_TO_ESP) {
        const slice = data.subarray(off, Math.min(off + MAX_PAYLOAD_TO_ESP, data.length));
        if (!enviarAlEsp(OP.DATA, sid, slice)) {
          cli.destroy();
          return;
        }
      }
    });

    const limpiar = () => {
      if (clientesPorSid.delete(sid)) {
        enviarAlEsp(OP.CLOSE, sid);
      }
    };

    cli.on("timeout", () => cli.destroy());
    cli.on("close", limpiar);
    cli.on("error", () => cli.destroy());
  });

  const escuchar = (srv: net.Server, puerto: number) =>
    new Promise<void>((resolve, reject) => {
      srv.once("error", reject);
      srv.listen(puerto, "0.0.0.0", () => {
        srv.removeListener("error", reject);
        resolve();
      });
    });

  try {
    await Promise.all([
      escuchar(serverTunel, puertoTunelActivo),
      escuchar(serverClientes, puertoClientesActivo),
    ]);
  } catch (err) {
    await detenerRelayLocal();
    throw err;
  }

  watchdogTimer = setInterval(() => {
    if (espSocket && espAutenticado && Date.now() - espUltimoRxMs > TUNNEL_TIMEOUT_MS) {
      espSocket.destroy();
    }
  }, 10_000);

  return obtenerEstadoRelayLocal();
}