import { comm } from "../../connections/ContenedorComunicacion";
import {
  CalibracionSensorDTO,
  LecturaTemperaturaStreamDTO,
  SensorInfoDTO,
  SolicitudStreamSensorDTO,
  TuplaCalibracionSensorDTO,
  TuplaLecturaTemperaturaDTO,
  TuplaSensorInfoDTO,
} from "./sensoresDTO";

const MAX_SENSORES = 6;
const r2 = (n: unknown): number => Number(Number(n).toFixed(2));

function validarSensorId(id: number): void {
  if (!Number.isInteger(id) || id < 1 || id > MAX_SENSORES) {
    throw new Error(`sensorId inválido (1..${MAX_SENSORES})`);
  }
}

function mapearInfo(t: TuplaSensorInfoDTO): SensorInfoDTO {
  return {
    sensorId: Number(t[0]), hexadecimal: String(t[1]), mac2: String(t[2]),
    nombre: String(t[3]), descripcion: String(t[4]), ubicacion: String(t[5]),
    offsetCalibracion: r2(t[6]), umbralSuperior: r2(t[7]), umbralInferior: r2(t[8]),
    tempActualC: r2(t[9]), resolucionBits: Number(t[10]),
    alimentacionParasita: Boolean(t[11]), activo: Boolean(t[12]),
    estado: Number(t[13]), fechaModificacion: Number(t[14]),
  };
}

export async function ejecutarStreamTemperaturas(
  clientId: number,
  cantidadMuestras = 5,
  sensorId = 1
): Promise<LecturaTemperaturaStreamDTO[]> {
  const recibidas: LecturaTemperaturaStreamDTO[] = [];
  const req: SolicitudStreamSensorDTO = {
    sensorId,
    sampleCount: cantidadMuestras,
  };

  await comm.c.stream<TuplaLecturaTemperaturaDTO>(
    "sensors.watch_temperature",
    req,
    (tupla) => {
      recibidas.push({
        sensorId: Number(tupla[0]),
        celsius: Number(Number(tupla[1]).toFixed(2)),
        timestampMs: Number(tupla[2]),
      });
    },
    clientId
  );
  return recibidas;
}

export const sensoresApi = {
  obtenerInfoSensor: async (sensorId = 1, clientId?: number): Promise<SensorInfoDTO> => {
    validarSensorId(sensorId);
    const target = comm.resolverClienteExplicitado(clientId);
    const res = await comm.c.callTo(target, "sensors.get_info", sensorId);
    return mapearInfo(res.as<TuplaSensorInfoDTO>());
  },

  obtenerInfoSensores: async (clientId?: number): Promise<SensorInfoDTO[]> => {
    const target = comm.resolverClienteExplicitado(clientId);
    const lista: SensorInfoDTO[] = [];
    for (let id = 1; id <= MAX_SENSORES; id++) {
      const res = await comm.c.callTo(target, "sensors.get_info", id);
      const info = mapearInfo(res.as<TuplaSensorInfoDTO>());
      if (info.activo) lista.push(info);
    }
    return lista;
  },

  configurarUmbrales: async (
    sensorId: number, umbralSuperior: number, umbralInferior: number, clientId?: number
  ): Promise<boolean> => {
    validarSensorId(sensorId);
    if (umbralInferior >= umbralSuperior) throw new Error("umbralInferior debe ser menor que umbralSuperior");
    if (umbralInferior < -55 || umbralSuperior > 125) throw new Error("Umbrales fuera de rango [-55..125]");
    const target = comm.resolverClienteExplicitado(clientId);
    return (await comm.c.callTo(target, "sensors.set_umbrales", sensorId, umbralSuperior, umbralInferior)).as<boolean>();
  },

  obtenerCalibracionSensor: async (sensorId = 1, clientId?: number): Promise<CalibracionSensorDTO> => {
    validarSensorId(sensorId);
    const target = comm.resolverClienteExplicitado(clientId);
    const t = (await comm.c.callTo(target, "sensors.get_calibracion", sensorId)).as<TuplaCalibracionSensorDTO>();
    return { sensorId: Number(t[0]), offsetCalibracion: r2(t[1]), correccionSuperior: r2(t[2]), correccionInferior: r2(t[3]) };
  },

  configurarCalibracion: async (
    sensorId: number, offset: number, correccionSuperior: number, correccionInferior: number, clientId?: number
  ): Promise<boolean> => {
    validarSensorId(sensorId);
    for (const v of [offset, correccionSuperior, correccionInferior]) {
      if (!Number.isFinite(v) || v < -10 || v > 10) throw new Error("Offset/correcciones fuera de rango [-10..10]");
    }
    const target = comm.resolverClienteExplicitado(clientId);
    return (
      await comm.c.callTo(target, "sensors.set_calibracion", sensorId, offset, correccionSuperior, correccionInferior)
    ).as<boolean>();
  },
  escucharTemperaturasEnVivo: async (
    cantidadMuestras = 5,
    sensorId = 1,
    clientId?: number
  ): Promise<LecturaTemperaturaStreamDTO[]> => {
    const target = comm.resolverClienteExplicitado(clientId);
    return ejecutarStreamTemperaturas(target, cantidadMuestras, sensorId);
  },
};