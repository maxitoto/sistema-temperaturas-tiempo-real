import { EstadoRedFirmwareDTO } from "../conexion/conexionDTO";
import { LecturaTemperaturaStreamDTO } from "../sensores/sensoresDTO";

export interface ResultadoTestMedioDTO {
  medio: "UART-S" | "WIFI-STA" | "WIFI-AP";
  autenticado: boolean;
  sessionId: number;
  sumaRpc: number;
  echoRecibido: string;
  muestrasStreamRecibidas: LecturaTemperaturaStreamDTO[];
  estadoRed: EstadoRedFirmwareDTO;
  latenciaMs: number;
}