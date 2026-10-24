// Archivo: ./src/backend/services/clientServices.ts
import { firmware } from "../stubs/clientExample";
import {
  conectarPorUSB,
  desconectarPorUSB,
  conectarPorWiFi,
  desconectarPorWiFi,
  estaConectado,
  obtenerEstadoConexion,
  listarPuertosUSB,
} from "../plugins/conectividad/servicosConectivos";

export const clientServices = {
  sumarYLeerEstado: firmware.sumarYLeerEstado,
  guardarConfiguracionSegura: firmware.guardarConfiguracionSegura,
  conectarPorUSB,
  desconectarPorUSB,
  conectarPorWiFi,
  desconectarPorWiFi,
  estaConectado,
  obtenerEstadoConexion,
  listarPuertosUSB,
};