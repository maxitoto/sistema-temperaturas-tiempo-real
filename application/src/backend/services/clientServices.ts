import { RPConexion } from "../connections/rpc/domains/RPConexion";
import { RPAuth } from "../connections/rpc/domains/RPAuth";
import { RPUsuario } from "../connections/rpc/domains/RPUsuario";
import { RPSensor } from "../connections/rpc/domains/RPSensor";
import { RPSistema } from "../connections/rpc/domains/RPSistema";

export const clientServices = {
    ...RPConexion,
    ...RPAuth,
    ...RPUsuario,
    ...RPSensor,
    ...RPSistema
};