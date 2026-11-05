export type TuplaLecturaTemperaturaDTO = [number, number, number];

export interface SensorInfoDTO {
  sensorId: number;
  hexadecimal: string;
  mac2: string;
  nombre: string;
  descripcion: string;
  ubicacion: string;
  offsetCalibracion: number;
  umbralSuperior: number;
  umbralInferior: number;
  tempActualC: number;
  resolucionBits: number;
  alimentacionParasita: boolean;
  activo: boolean;
  estado: number;
  fechaModificacion: number;
}
export type TuplaSensorInfoDTO = [
  number, string, string, string, string, string,
  number, number, number, number, number,
  boolean, boolean, number, number
];

export interface CalibracionSensorDTO {
  sensorId: number;
  offsetCalibracion: number;
  correccionSuperior: number;
  correccionInferior: number;
}
export type TuplaCalibracionSensorDTO = [number, number, number, number];