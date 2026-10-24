// ===== src/backend/stubs/serverExample.ts =====
import { pila } from "../communication/ContenedorComunicacion";
import { transformarMensajeCanalTLS } from "./clientExample";
import { IBackend, RPC, RespuestaReloj } from "./contratos";

interface RegistroSyncDBBackend {
  id: number;
  idNodo: number;
  uptimeEspMs: number;
  motivo: string;
  certificadoRecibido: string;
  fechaIso: string;
}

const dbAuditoriaSyncBackend: RegistroSyncDBBackend[] = [];

// =========================================================================
// IMPLEMENTACIÓN REAL (lógica de negocio, no sabe nada de RPC ni MessagePack)
// =========================================================================
export class BackendImpl implements IBackend {
  sincronizarReloj(uptimeEspMs: number, motivo: string, certEsp: string, idNodo: number): RespuestaReloj {
    dbAuditoriaSyncBackend.push({
      id: dbAuditoriaSyncBackend.length + 1,
      idNodo,
      uptimeEspMs,
      motivo,
      certificadoRecibido: certEsp,
      fechaIso: new Date().toISOString(),
    });
    console.log(
      `[RPC-SERVER-DB] Sync #${dbAuditoriaSyncBackend.length} | Nodo=${idNodo} | Cert=${certEsp} | Motivo=${motivo}`
    );
    return { unixSeg: Math.floor(Date.now() / 1000), zonaHoraria: "UTC-3" };
  }
}

// =========================================================================
// SKELETON: traduce RPC entrante -> llamada a IBackend -> respuesta RPC
// =========================================================================
function registrarSkeletonBackend(impl: IBackend): void {
  pila.appRpc.on(RPC.SYNC_RELOJ, (args, meta) => {
    const r = impl.sincronizarReloj(
      Number(args[0] ?? 0),
      String(args[1] ?? "DESCONOCIDO"),
      String(args[2] ?? "SIN_CERT"),
      meta.idNodo
    );
    return [r.unixSeg, r.zonaHoraria];
  });
}

let registrado = false;

export function registrarStubsServidorBackend(impl: IBackend = new BackendImpl()): void {
  if (registrado) return;
  registrado = true;

  pila.presentacion.configurarTransformadores(transformarMensajeCanalTLS, transformarMensajeCanalTLS);

  pila.appRpc.establecerEvaluadorPolitica(({ comando, argumentos }) => {
    if (comando === RPC.SYNC_RELOJ) {
      const certEsp = String(argumentos[2] ?? "");
      if (certEsp !== "CERT-TLS-UNTDF-2026-VALIDO") {
        return { permitido: false, motivo: "CERT_DISPOSITIVO_ESP_INVALIDO" };
      }
    }
    return { permitido: true };
  });

  registrarSkeletonBackend(impl);
}