import { RPConexion } from "../connections/rpc/domains/RPConexion";
import { RPAuth } from "../connections/rpc/domains/RPAuth";
import { RPUsuario } from "../connections/rpc/domains/RPUsuario";
import { RPSensor } from "../connections/rpc/domains/RPSensor";
import { RPSistema } from "../connections/rpc/domains/RPSistema";
import { procedimientosRemotosFirmware } from "../connections/rpc/ProcedimientosRemotosFirmware"
import {
  iniciarRelayLocal,
  detenerRelayLocal,
  obtenerEstadoRelayLocal,
} from "../replay/relayLocalService";

export const clientServices = {
    ...RPConexion,
    ...RPAuth,
    ...RPUsuario,
    ...RPSensor,
    ...RPSistema,
    ...procedimientosRemotosFirmware,
    iniciarRelayLocal,
    detenerRelayLocal,
    obtenerEstadoRelayLocal,
};