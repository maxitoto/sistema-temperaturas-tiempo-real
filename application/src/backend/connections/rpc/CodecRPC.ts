export {
    ProcedimientoSistemaRPC,
    BANDERA_ERROR_RPC,
    hashNombreProcedimiento
} from "./FirmwareRPCClient";

export type { PerfilEnlaceUartsCliente } from "./FirmwareRPCClient";
export type { SesionIniciadaCliente } from "./domains/RPAuth";
export type {
    PerfilUsuarioCliente,
    ResumenUsuarioCliente,
    EstadisticasUsuariosCliente
} from "./domains/RPUsuario";
export type { LecturaSensorCliente } from "./domains/RPSensor";
export type {
    ConfiguracionWiFiCliente,
    DiagnosticoSistemaCliente
} from "./domains/RPSistema";