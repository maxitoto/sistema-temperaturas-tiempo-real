//./src/backend/communication/layers/CapaAplicacion.ts
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
  private readonly rutasRequest = new Map<string, ManejadorProcedimientoRPC>();
  private readonly rutasStream = new Map<string, ManejadorProcedimientoRPC>();
  private readonly oyentesEventos = new Map<string, ManejadorEventoRPC[]>();
  private readonly oyentesStream = new Map<string, ManejadorEventoRPC[]>();
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

  // =========================================================================
  // CAMINO 1: TRANSACCIONAL FIABLE (REQUEST / REPLY / EVENTO)
  // =========================================================================
  public onRequest(comando: string, manejador: ManejadorProcedimientoRPC): void {
    this.rutasRequest.set(comando, manejador);
  }

  // Alias de compatibilidad con registrarSkeletonBackend
  public on(comando: string, manejador: ManejadorProcedimientoRPC): void {
    this.onRequest(comando, manejador);
  }

  public async request<T = unknown[]>(
    comando: string,
    parametros: unknown[] = [],
    opts: OpcionesLlamada = {}
  ): Promise<T> {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    const timeoutMs = opts.timeoutMs ?? 5000;

    const respuesta = await this.presentacion.invocarRequest(
      idNodo,
      comando,
      parametros,
      timeoutMs
    );
    const [estado, ...datos] = respuesta;
    if (estado !== "OK") {
      throw new Error(`RPC Request Error [${comando}]: ${String(datos[0] ?? estado)}`);
    }
    return datos as unknown as T;
  }

  // Alias de compatibilidad con métodos existentes en clientExample.ts
  public async call<T = unknown[]>(
    comando: string,
    parametros: unknown[] = [],
    opts: OpcionesLlamada = {}
  ): Promise<T> {
    return this.request<T>(comando, parametros, opts);
  }

  public onEvento(evento: string, manejador: ManejadorEventoRPC): void {
    const lista = this.oyentesEventos.get(evento) ?? [];
    lista.push(manejador);
    this.oyentesEventos.set(evento, lista);
  }

  public emitirEvento(
    evento: string,
    argumentos: unknown[] = [],
    opts: Pick<OpcionesLlamada, "preferir"> = {}
  ): boolean {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    return this.presentacion.bajarEventoFormateado(idNodo, evento, argumentos);
  }

  // =========================================================================
  // CAMINO 2: STREAMING TIEMPO REAL (UDP / DATAGRAMA)
  // =========================================================================
  public onStream(canal: string, manejador: ManejadorProcedimientoRPC): void {
    this.rutasStream.set(canal, manejador);
  }

  public async stream<T = unknown[]>(
    canal: string,
    parametros: unknown[] = [],
    opts: OpcionesLlamada = {}
  ): Promise<T> {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    const timeoutMs = opts.timeoutMs ?? 1500;

    const respuesta = await this.presentacion.invocarStream(
      idNodo,
      canal,
      parametros,
      timeoutMs
    );
    const [estado, ...datos] = respuesta;
    if (estado !== "OK") {
      throw new Error(`RPC Stream Error [${canal}]: ${String(datos[0] ?? estado)}`);
    }
    return datos as unknown as T;
  }

  public onStreamData(canal: string, manejador: ManejadorEventoRPC): void {
    const lista = this.oyentesStream.get(canal) ?? [];
    lista.push(manejador);
    this.oyentesStream.set(canal, lista);
  }

  public emitirStream(
    canal: string,
    argumentos: unknown[] = [],
    opts: Pick<OpcionesLlamada, "preferir"> = {}
  ): boolean {
    const idNodo = this.resolverNodo(opts.preferir ?? "auto");
    return this.presentacion.bajarStreamFormateado(idNodo, canal, argumentos);
  }

  // =========================================================================
  // DESPACHO DE SUBIDA (RX)
  // =========================================================================
  private async despacharMensajeEntrante(msj: MensajePresentacion): Promise<void> {
    const meta: MetaCall = {
      idNodo: msj.idNodo,
      reqId: msj.reqId,
      lector: msj.lector,
    };

    // CASO 1: Request Transaccional entrante (PROTO_TCP)
    if (msj.tipoSesion === CapaSesion.TIPO_RPC_REQ) {
      const manejador = this.rutasRequest.get(msj.cabecera);
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
          meta,
        });

        if (!resultado.permitido) {
          if (msj.reqId !== 0) {
            this.presentacion.bajarRespuestaFormateada(msj.idNodo, msj.reqId, "ERR", [
              resultado.motivo ?? "FORBIDDEN",
            ]);
          }
          return;
        }
      }

      try {
        const datosRetorno = await manejador(msj.argumentos, meta);
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

    // CASO 2: Petición de Stream entrante (PROTO_UDP)
    if (msj.tipoSesion === CapaSesion.TIPO_STREAM_REQ) {
      const manejador = this.rutasStream.get(msj.cabecera);
      if (!manejador) {
        if (msj.reqId !== 0) {
          this.presentacion.bajarRespuestaStreamFormateada(msj.idNodo, msj.reqId, "ERR", [
            "STREAM_NOT_FOUND",
          ]);
        }
        return;
      }

      try {
        const datosRetorno = await manejador(msj.argumentos, meta);
        if (msj.reqId !== 0) {
          this.presentacion.bajarRespuestaStreamFormateada(
            msj.idNodo,
            msj.reqId,
            "OK",
            Array.isArray(datosRetorno) ? datosRetorno : [datosRetorno]
          );
        }
      } catch (err) {
        if (msj.reqId !== 0) {
          const detalle = err instanceof Error ? err.message : "STREAM_EXEC_ERR";
          this.presentacion.bajarRespuestaStreamFormateada(msj.idNodo, msj.reqId, "ERR", [detalle]);
        }
      }
      return;
    }

    // CASO 3: Evento asíncrono o Cuadro de Stream unidireccional
    if (msj.tipoSesion === CapaSesion.TIPO_EVENTO) {
      const oyentes = this.oyentesEventos.get(msj.cabecera) ?? [];
      for (const cb of oyentes) {
        cb(msj.argumentos, msj.cabecera, meta);
      }
      return;
    }

    if (msj.tipoSesion === CapaSesion.TIPO_STREAM_DATA) {
      const oyentes = this.oyentesStream.get(msj.cabecera) ?? [];
      for (const cb of oyentes) {
        cb(msj.argumentos, msj.cabecera, meta);
      }
    }
  }
}