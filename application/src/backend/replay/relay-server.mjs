import net from "node:net";
import crypto from "node:crypto";

const PORT_CLIENTES = Number(process.env.RELAY_CLIENT_PORT ?? 8443);
const PORT_TUNEL    = Number(process.env.RELAY_TUNNEL_PORT ?? 9443);
const RELAY_SECRET  = process.env.RELAY_SECRET ?? "";
const NODE_ID_RAW   = process.env.NODE_ID ?? "ESP32-UNTDF-0001";

if (RELAY_SECRET.length < 16) {
  console.error("[FATAL] Defina RELAY_SECRET en el entorno (minimo 16 caracteres).");
  process.exit(1);
}

// Identificador exacto de 16 bytes con relleno nulo (espejo de WiFiRelayClient.hpp)
const NODE_ID_BUF = Buffer.alloc(16, 0);
Buffer.from(NODE_ID_RAW, "utf8").copy(NODE_ID_BUF, 0, 0, 16);

const MAX_CLIENTES         = 4;
const MAX_PAYLOAD_TO_ESP   = 1024; // Coincide con WiFiRelayClient::MAX_PAYLOAD_RX_MUX
const MAX_PAYLOAD_FROM_ESP = 2048; // Coincide con WiFiRelayClient::MAX_PAYLOAD_TX_MUX
const IDLE_CLIENT_MS       = 150_000;
const TUNNEL_TIMEOUT_MS    = 50_000;

const OP = {
  CHALLENGE: 0x00,
  HELLO:     0x01,
  OPEN:      0x02,
  DATA:      0x03,
  CLOSE:     0x04,
  PING:      0x05,
  PONG:      0x06,
  AUTH_OK:   0x07,
};

function packMux(op, sid, payload = Buffer.alloc(0)) {
  const hdr = Buffer.alloc(4);
  hdr[0] = op;
  hdr[1] = sid;
  hdr.writeUInt16BE(payload.length, 2);
  return payload.length > 0 ? Buffer.concat([hdr, payload]) : hdr;
}

let espSocket = null;
let espAutenticado = false;
let espUltimoRxMs = 0;

const clientesPorSid = new Map();

function asignarSidLibre() {
  for (let sid = 1; sid <= MAX_CLIENTES; sid++) {
    if (!clientesPorSid.has(sid)) return sid;
  }
  return null;
}

function enviarAlEsp(op, sid, payload) {
  if (!espSocket || !espAutenticado || espSocket.destroyed) return false;
  return espSocket.write(packMux(op, sid, payload));
}

function cerrarTodosLosClientes(motivo) {
  for (const [sid, cli] of clientesPorSid.entries()) {
    cli.destroy();
    clientesPorSid.delete(sid);
  }
  console.log(`[RELAY] Clientes cerrados (${motivo})`);
}

// --- 1. TÚNEL SALIENTE DEL ESP32 (:9443) ---
const serverTunel = net.createServer((sock) => {
  sock.setNoDelay(true);
  sock.setKeepAlive(true, 15_000);

  const nonce = crypto.randomBytes(16);
  let autenticado = false;
  let rxBuf = Buffer.alloc(0);

  sock.write(packMux(OP.CHALLENGE, 0, nonce));

  const authTimer = setTimeout(() => {
    if (!autenticado) sock.destroy();
  }, 5_000);

  sock.on("data", (chunk) => {
    rxBuf = Buffer.concat([rxBuf, chunk]);

    while (rxBuf.length >= 4) {
      const op  = rxBuf[0];
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
        const macRx    = payload.subarray(16, 48);
        const esperado = crypto
          .createHmac("sha256", RELAY_SECRET)
          .update(Buffer.concat([nodeIdRx, nonce]))
          .digest();

        if (
          !crypto.timingSafeEqual(nodeIdRx, NODE_ID_BUF) ||
          !crypto.timingSafeEqual(macRx, esperado)
        ) {
          console.warn(`[TUNEL] Auth rechazada desde ${sock.remoteAddress}`);
          sock.destroy();
          return;
        }

        clearTimeout(authTimer);
        autenticado = true;

        if (espSocket && espSocket !== sock) {
          cerrarTodosLosClientes("reemplazo de tunel ESP32");
          espSocket.destroy();
        }
        espSocket = sock;
        espAutenticado = true;

        // Confirma autenticación al ESP32 para activar el túnel
        sock.write(packMux(OP.AUTH_OK, 0));
        console.log(`[TUNEL] ESP32 autenticado (${sock.remoteAddress})`);
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
      cerrarTodosLosClientes("tunel ESP32 desconectado");
    }
  });

  sock.on("error", () => sock.destroy());
});

setInterval(() => {
  if (espSocket && espAutenticado && Date.now() - espUltimoRxMs > TUNNEL_TIMEOUT_MS) {
    console.warn("[TUNEL] Timeout sin PING; cerrando tunel zombi");
    espSocket.destroy();
  }
}, 10_000);

// --- 2. PUERTO PÚBLICO PARA ELECTRON (:8443) ---
const serverClientes = net.createServer((cli) => {
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
  console.log(`[CLIENTE] SID=${sid} conectado (${cli.remoteAddress})`);

  cli.on("data", (chunk) => {
    for (let off = 0; off < chunk.length; off += MAX_PAYLOAD_TO_ESP) {
      const slice = chunk.subarray(off, Math.min(off + MAX_PAYLOAD_TO_ESP, chunk.length));
      if (!enviarAlEsp(OP.DATA, sid, slice)) {
        cli.destroy();
        return;
      }
    }
  });

  const limpiar = () => {
    if (clientesPorSid.delete(sid)) {
      enviarAlEsp(OP.CLOSE, sid);
      console.log(`[CLIENTE] SID=${sid} desconectado`);
    }
  };

  cli.on("timeout", () => cli.destroy());
  cli.on("close", limpiar);
  cli.on("error", () => cli.destroy());
});

serverTunel.listen(PORT_TUNEL, "0.0.0.0", () => {
  console.log(`[RELAY] Tunel ESP32 escuchando en :${PORT_TUNEL}`);
});
serverClientes.listen(PORT_CLIENTES, "0.0.0.0", () => {
  console.log(`[RELAY] Clientes Electron escuchando en :${PORT_CLIENTES}`);
});