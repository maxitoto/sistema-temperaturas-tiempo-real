// Archivo: ./src/backend/communication/layers/CapaAplicacion.ts
import { CapaPresentacion, LectorMsgPack, MensajePresentacion } from "./CapaPresentacion";
import { CapaSesion } from "./CapaSesion";

export interface MetaCall {
  readonly idNodo: number;
  readonly reqId: number;
  readonly lector: LectorMsgPack;
}

export interface OpcionesLlamada {
  preferir?: "auto" | "USB" | "WIFI";
  timeoutMs?: number;
}

export type ManejadorProcedimientoRPC = (
  argumentos: unknown[],
  meta: MetaCall
) => Promise<unknown[]> | unknown[];

export type ManejadorEventoRPC = (
  argumentos: unknown[],
  evento: string,
  meta: { readonly idNodo: number; readonly lector: LectorMsgPack }
) => void;

export interface ContextoPoliticaRPC {
  readonly comando: string;
  readonly argumentos: unknown[];
  readonly meta: MetaCall;
}

export type EvaluadorPoliticaRPC = (
  ctx: ContextoPoliticaRPC
) => Promise<{ permitido: boolean; motivo?: string }> | { permitido: boolean; motivo?: string };

export class CapaAplicacion {
  private readonly rutas = new Map<string, ManejadorProcedimientoRPC>();
  private readonly oyentesEventos = new Map<string, ManejadorEventoRPC[]>();
  private evaluadorPolitica: EvaluadorPoliticaRPC | null = null;

  constructor(private readonly presentacion: CapaPresentacion, private idNodoPorDefecto = 0) {
    this.presentacion.alSubirMensaje((msj) => this.despacharMensajeEntrante(msj));
  }

  public cambiarMedioActivo(idNodo: number): void {
    this.idNodoPorDefecto = idNodo;
  }

  public obtenerMedioActivo(): number {
    return this.idNodoPorDefecto;
  }

  public establecerEvaluadorPolitica(evaluador: EvaluadorPoliticaRPC | null): void {
    this.evaluadorPolitica = evaluador;
  }

  private resolverNodo(preferir: "auto" | "USB" | "WIFI" = "auto"): number {
    if (preferir === "USB") return 0;
    if (preferir === "WIFI") return 1;
    return this.idNodoPorDefecto;
  }

  public on(comando: string, manejador: ManejadorProcedimientoRPC): void {
    this.rutas.set(comando, manejador);
  }

  public async call<T = unknown[]>(
    comando: string,
    parametros: unknown[] = [],
    opts: OpcionesLlamada = {}
  ): Promise<T> {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    const timeoutMs = opts.timeoutMs ?? 5000;

    const respuesta = await this.presentacion.invocarSerializado(
      idNodo,
      comando,
      parametros,
      timeoutMs
    );
    const [estado, ...datos] = respuesta;
    if (estado !== "OK") {
      throw new Error(`RPC Error [${comando}]: ${String(datos[0] ?? estado)}`);
    }
    return datos as unknown as T;
  }

  public onEvento(evento: string, manejador: ManejadorEventoRPC): void {
    const lista = this.oyentesEventos.get(evento) ?? [];
    lista.push(manejador);
    this.oyentesEventos.set(evento, lista);
  }

  public emitirEvento(
    evento: string,
    argumentos: unknown[] = [],
    opts: Pick<OpcionesLlamada, "preferir"> & { fiable?: boolean } = {}
  ): boolean {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    return this.presentacion.bajarEventoFormateado(
      idNodo,
      evento,
      argumentos,
      opts.fiable ?? true
    );
  }

  private async despacharMensajeEntrante(msj: MensajePresentacion): Promise<void> {
    if (msj.tipoSesion === CapaSesion.TIPO_RPC_REQ) {
      const manejador = this.rutas.get(msj.cabecera);
      if (!manejador) {
        if (msj.reqId !== 0) {
          this.presentacion.bajarRespuestaFormateada(msj.idNodo, msj.reqId, "ERR", [
            "ROUTE_NOT_FOUND",
          ]);
        }
        return;
      }

      if (this.evaluadorPolitica) {
        const resultado = await this.evaluadorPolitica({
          comando: msj.cabecera,
          argumentos: msj.argumentos,
          meta: {
            idNodo: msj.idNodo,
            reqId: msj.reqId,
            lector: msj.lector,
          },
        });

        if (!resultado.permitido) {
          if (msj.reqId !== 0) {
            this.presentacion.bajarRespuestaFormateada(
              msj.idNodo,
              msj.reqId,
              "ERR",
              [resultado.motivo ?? "FORBIDDEN"]
            );
          }
          return; // ← NO ejecutar el manejador
        }
      }

      try {
        const datosRetorno = await manejador(msj.argumentos, {
          idNodo: msj.idNodo,
          reqId: msj.reqId,
          lector: msj.lector,
        });
        if (msj.reqId !== 0) {
          this.presentacion.bajarRespuestaFormateada(
            msj.idNodo,
            msj.reqId,
            "OK",
            Array.isArray(datosRetorno) ? datosRetorno : [datosRetorno]
          );
        }
      } catch (err) {
        if (msj.reqId !== 0) {
          const detalle = err instanceof Error ? err.message : "HANDLER_EXEC_ERR";
          this.presentacion.bajarRespuestaFormateada(msj.idNodo, msj.reqId, "ERR", [detalle]);
        }
      }
      return;
    }

    if (msj.tipoSesion === CapaSesion.TIPO_EVENTO) {
      const oyentes = this.oyentesEventos.get(msj.cabecera) ?? [];
      for (const cb of oyentes) {
        cb(msj.argumentos, msj.cabecera, { idNodo: msj.idNodo, lector: msj.lector });
      }
    }
  }
}