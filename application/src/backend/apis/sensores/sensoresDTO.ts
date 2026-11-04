export interface SolicitudStreamSensorDTO {
  sensorId: number;
  sampleCount: number;
}

export interface LecturaTemperaturaStreamDTO {
  sensorId: number;
  celsius: number;
  timestampMs: number;
}

export type TuplaLecturaTemperaturaDTO = [number, number, number];