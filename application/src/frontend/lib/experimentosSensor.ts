import { client, safeRpc } from "./service";
import type { PerfilSensorCliente } from "#types/rpc.types";

export interface VentanaExperimento {
  id: string;
  nombre: string;
  inicioSeg: number;
  finSeg: number;
  enCurso: boolean;
}

function parsearFechaPgASeg(texto: string): number {
  if (!texto) return 0;
  const iso = texto.includes("T") ? texto : texto.replace(" ", "T");
  const ms = Date.parse(iso);
  return Number.isFinite(ms) ? Math.floor(ms / 1000) : 0;
}

function extraerNombreDeTexto(raw: string): string {
  if (!raw) return "";
  // Soporta formatos como "nombre=Carne de Cerdo; ...", "nombre: ...", o texto plano
  const m = raw.match(/nombre\s*[:=]\s*([^|;,}\n]+)/i);
  if (m?.[1]) return m[1].trim().replace(/^["']|["']$/g, "");
  return raw.trim().slice(0, 32);
}

/**
 * Reconstruye intervalos de experimentos [inicio, fin] a partir de cambios de nombre
 * del sensor en AuditoriaAuth, sin alterar la estructura de la base de datos.
 */
export async function detectarExperimentosDesdeAuditoria(
  perfil: PerfilSensorCliente,
  esAdmin: boolean
): Promise<VentanaExperimento[]> {
  const ahoraSeg = Math.floor(Date.now() / 1000);
  const inicioCreacion = Math.max(1, parsearFechaPgASeg(perfil.fechaCreacion));
  const nombreActual = perfil.nombre || `Sensor #${perfil.idSensor}`;
  const macUpper = perfil.mac.toUpperCase();

  if (!esAdmin) {
    return [
      {
        id: "exp-actual",
        nombre: `${nombreActual} (actual)`,
        inicioSeg: inicioCreacion,
        finSeg: ahoraSeg,
        enCurso: true,
      },
    ];
  }

  interface CambioNombre {
    ts: number;
    previo: string;
    nuevo: string;
  }

  const cambios: CambioNombre[] = [];

  // Revisa hasta 4 páginas (64 registros recientes) de AuditoriaAuth sin sobrecargar el ESP32
  for (let off = 0; off < 64; off += 16) {
    const pagina = await safeRpc(
      () => client.listarAuditoriaAuth(16, off),
      "listarAuditoriaAuthExperimentos",
      { silent: true }
    );
    if (!pagina || pagina.length === 0) break;

    const candidatos = pagina.filter((r) => {
      const acc = r.accion.toUpperCase();
      return (
        acc.includes("SENSOR") ||
        acc.includes("META") ||
        acc.includes("UPDATE")
      );
    });

    for (const cand of candidatos) {
      const det = await safeRpc(
        () => client.obtenerAuditoriaAuthPorId(cand.idAuditoriaAuth),
        "obtenerAuditoriaAuthExp",
        { silent: true }
      );
      if (!det) continue;

      const tablaOk =
        !det.tablaAfectada ||
        det.tablaAfectada.toUpperCase().includes("SENSOR");
      const bolsa = `${det.detalle} ${det.valorPrevio} ${det.valorNuevo}`.toUpperCase();
      const correspondeAlSensor =
        bolsa.includes(macUpper) ||
        bolsa.includes(`ID=${perfil.idSensor}`) ||
        bolsa.includes(`#${perfil.idSensor}`);

      if (!tablaOk || !correspondeAlSensor) continue;

      const previo = extraerNombreDeTexto(det.valorPrevio);
      const nuevo = extraerNombreDeTexto(det.valorNuevo);
      if (nuevo && nuevo !== previo) {
        cambios.push({
          ts: det.fechaCreacion,
          previo,
          nuevo,
        });
      }
    }

    if (pagina.length < 16) break;
  }

  cambios.sort((a, b) => a.ts - b.ts);

  if (cambios.length === 0) {
    return [
      {
        id: "exp-actual",
        nombre: nombreActual,
        inicioSeg: inicioCreacion,
        finSeg: ahoraSeg,
        enCurso: true,
      },
    ];
  }

  const ventanas: VentanaExperimento[] = [];

  // Tramo previo al primer cambio de nombre detectado
  if (cambios[0].previo && cambios[0].ts > inicioCreacion) {
    ventanas.push({
      id: `exp-0`,
      nombre: cambios[0].previo,
      inicioSeg: inicioCreacion,
      finSeg: cambios[0].ts,
      enCurso: false,
    });
  }

  for (let i = 0; i < cambios.length; i++) {
    const esUltimo = i === cambios.length - 1;
    const fin = esUltimo ? ahoraSeg : cambios[i + 1].ts;
    ventanas.push({
      id: `exp-${i + 1}`,
      nombre: cambios[i].nuevo,
      inicioSeg: cambios[i].ts,
      finSeg: fin,
      enCurso: esUltimo,
    });
  }

  // Mostrar el experimento más reciente primero
  return ventanas.reverse();
}