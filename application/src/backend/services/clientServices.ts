import { conexionApi, sensoresApi, sistemaApi } from "../apis";

export const clientServices = {
  ...conexionApi,
  ...sensoresApi,
  ...sistemaApi,
};