import { conexionApi } from "./conexion/conexionApi";
import { sensoresApi } from "./sensores/sensoresApi";
import { registrarSistemaEndpointsLocales, sistemaApi } from "./sistema/sistemaApi";

registrarSistemaEndpointsLocales();

export { conexionApi, sensoresApi, sistemaApi };
export * from "./conexion/conexionDTO";
export * from "./sensores/sensoresDTO";
export * from "./sistema/sistemaDTO";