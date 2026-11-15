
import { RPConexion } from '../connections/rpc/domains/RPConexion';
import { RPAuth } from '../connections/rpc/domains/RPAuth';
import { RPUsuario } from '../connections/rpc/domains/RPUsuario';
import { RPSensor } from '../connections/rpc/domains/RPSensor';
import { RPSistema } from '../connections/rpc/domains/RPSistema';
import { clientServices } from './clientServices';
import { developerServices } from './developerServices';

export const serviceRegistry = {
  conexion:  RPConexion,
  auth:      RPAuth,
  usuario:   RPUsuario,
  sensor:    RPSensor,
  sistema:   RPSistema,
  client:    clientServices,
  developer: developerServices,
} as const;

export type ServiceRegistry = typeof serviceRegistry;
export type Namespace       = keyof ServiceRegistry;