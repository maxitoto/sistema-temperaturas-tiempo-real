// Archivo: ./src/backend/services/clientServices.ts
import { sumarYLeerEstado, guardarConfiguracionSegura } from "../stubs/clientExample";
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
  sumarYLeerEstado,
  guardarConfiguracionSegura,
  conectarPorUSB,
  desconectarPorUSB,
  conectarPorWiFi,
  desconectarPorWiFi,
  estaConectado,
  obtenerEstadoConexion,
  listarPuertosUSB,
};