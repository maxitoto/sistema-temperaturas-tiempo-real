// Archivo: ./src/backend/communication/ContenedorComunicacion.ts
import { LimitesPila } from "./common/BufferRed";
import { CapaFisica } from "./layers/CapaFisica";
import { CapaRed } from "./layers/CapaRed";
import { CapaTransporte } from "./layers/CapaTransporte";
import { CapaSesion } from "./layers/CapaSesion";
import { CapaPresentacion } from "./layers/CapaPresentacion";
import { CapaAplicacion } from "./layers/CapaAplicacion";

export const ID_NODO_USB = 0;
export const ID_NODO_WIFI = 1;

const SECRETO_COMPARTIDO = "SECRETO_HMAC_DEFAULT";

const fisica = new CapaFisica();
const red = new CapaRed(fisica, LimitesPila.MAX_NODOS_RED);
const transporte = new CapaTransporte(red);
const sesion = new CapaSesion(transporte, SECRETO_COMPARTIDO, 15000);
const presentacion = new CapaPresentacion(sesion);
const appRpc = new CapaAplicacion(presentacion, ID_NODO_USB);

export const pila = {
  fisica,
  red,
  transporte,
  sesion,
  presentacion,
  appRpc,
} as const;