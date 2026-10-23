// Archivo: ./src/backend/stubs/serverExample.ts
import { pila } from "../communication/ContenedorComunicacion";
import { transformarMensajeCanalTLS } from "./clientExample";

interface RegistroSyncDBBackend {
  id: number;
  idNodo: number;
  uptimeEspMs: number;
  motivo: string;
  certificadoRecibido: string;
  fechaIso: string;
}

// Simulación de tabla de auditoría en el Backend
const dbAuditoriaSyncBackend: RegistroSyncDBBackend[] = [];

let registrado = false;

export function registrarStubsServidorBackend(): void {
  if (registrado) return;
  registrado = true;

  // 1. Activar Encriptación de Mensaje Completo en Capa 6 (simétrica con el Firmware)
  pila.presentacion.configurarTransformadores(
    transformarMensajeCanalTLS,
    transformarMensajeCanalTLS
  );

  // 2. Activar Middleware de Política en el Backend (verifica certificado del ESP)
  pila.appRpc.establecerEvaluadorPolitica(({ comando, argumentos }) => {
    if (comando === "RPC_SYNC_RELOJ") {
      const certEsp = String(argumentos[2] ?? "");
      if (certEsp !== "CERT-TLS-UNTDF-2026-VALIDO") {
        return { permitido: false, motivo: "CERT_DISPOSITIVO_ESP_INVALIDO" };
      }
    }
    return { permitido: true };
  });

  // 3. Manejador RPC_SYNC_RELOJ
  pila.appRpc.on("RPC_SYNC_RELOJ", (args, meta) => {
    const uptimeEspMs = Number(args[0] ?? 0);
    const motivo = String(args[1] ?? "DESCONOCIDO");
    const certEsp = String(args[2] ?? "SIN_CERT");

    dbAuditoriaSyncBackend.push({
      id: dbAuditoriaSyncBackend.length + 1,
      idNodo: meta.idNodo,
      uptimeEspMs,
      motivo,
      certificadoRecibido: certEsp,
      fechaIso: new Date().toISOString(),
    });

    console.log(
      `[RPC-SERVER-DB] Sync #${dbAuditoriaSyncBackend.length} | Nodo=${meta.idNodo} | Cert=${certEsp} | Motivo=${motivo}`
    );

    const ahoraUnixSeg = Math.floor(Date.now() / 1000);
    const zonaHoraria = "UTC-3";

    return [ahoraUnixSeg, zonaHoraria];
  });
}