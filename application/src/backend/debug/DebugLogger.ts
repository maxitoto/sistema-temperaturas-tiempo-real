// Archivo: ./src/backend/debug/DebugLogger.ts
// MÓDULO DE DIAGNÓSTICO BORRABLE: Para limpiar, borrar este archivo y su import en servicosConectivos.ts
import * as fs from "node:fs";
import * as path from "node:path";
import { pila } from "../communication/ContenedorComunicacion";

const RUTA_LOG = path.join(process.cwd(), "debug-osi.log");

function ts(): string {
  const d = new Date();
  return d.toISOString().slice(11, 23); // HH:mm:ss.SSS
}

export function logOsi(capa: string, mensaje: string): void {
  const linea = `[${ts()}] [${capa}] ${mensaje}\n`;
  try {
    fs.appendFileSync(RUTA_LOG, linea, "utf-8");
  } catch {
    // Ignorar errores de FS
  }
}

export function activarDebugPilaOSI(): void {
  fs.writeFileSync(RUTA_LOG, `=== INICIO DEBUG OSI (${new Date().toISOString()}) ===\n`, "utf-8");

  // 1. Hook Capa 1 Física (RX y TX)
  pila.fisica.alSubirBytes((idMedio, bytes) => {
    logOsi("L1-RX", `medio=${idMedio} len=${bytes.length}B hex=${Buffer.from(bytes).toString("hex")}`);
  });

  const enviarOriginal = pila.fisica.enviarBytes.bind(pila.fisica);
  pila.fisica.enviarBytes = (idMedio: number, datos: Uint8Array): number => {
    logOsi("L1-TX", `medio=${idMedio} len=${datos.length}B hex=${Buffer.from(datos).toString("hex")}`);
    return enviarOriginal(idMedio, datos);
  };

  // 2. Hook Capa 3 Red (Paquetes ensamblados por Capa 2)
  pila.red.alSubirPaquete((pkt) => {
    logOsi("L3-RX", `nodo=${pkt.idNodo} len=${pkt.longitud}B`);
  });

  // 3. Hook Capa 4 Transporte (Segmentos validados y flags TCP)
  pila.transporte.alSubirSegmento((pkt, meta) => {
    logOsi(
      "L4-RX",
      `nodo=${meta.idNodo} flags=0x${meta.flags.toString(16)} seq=${meta.numSecuencia} ack=${meta.numAcuse} len=${pkt.longitud}B`
    );
  });

  // 4. Hook Capa 5 Sesión (Sobres RPC y Eventos)
  pila.sesion.alSubirSesion((_pkt, meta) => {
    logOsi(
      "L5-RX",
      `nodo=${meta.idNodo} tipo=0x${meta.tipo.toString(16)} sesId=${meta.sessionId} reqId=${meta.reqId} len=${meta.payloadLen}B`
    );
  });

  // 5. Hook Capa 6 Presentación (Mensajes decodificados MessagePack)
  pila.presentacion.alSubirMensaje((msj) => {
    logOsi(
      "L6-RX",
      `nodo=${msj.idNodo} reqId=${msj.reqId} cmd="${msj.cabecera}" args=${JSON.stringify(msj.argumentos)}`
    );
  });
}