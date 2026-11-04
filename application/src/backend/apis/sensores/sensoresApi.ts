import { comm } from "../../connections/ContenedorComunicacion";
import {
  LecturaTemperaturaStreamDTO,
  SolicitudStreamSensorDTO,
  TuplaLecturaTemperaturaDTO,
} from "./sensoresDTO";

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
  escucharTemperaturasEnVivo: async (
    cantidadMuestras = 5,
    sensorId = 1,
    clientId?: number
  ): Promise<LecturaTemperaturaStreamDTO[]> => {
    const target = comm.resolverClienteExplicitado(clientId);
    return ejecutarStreamTemperaturas(target, cantidadMuestras, sensorId);
  },
};