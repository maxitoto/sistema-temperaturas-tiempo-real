export {
    ProcedimientoSistemaRPC,
    BANDERA_ERROR_RPC,
    hashNombreProcedimiento
} from "./FirmwareRPCClient";

export type { PerfilEnlaceUartsCliente } from "./FirmwareRPCClient";
export type { SesionIniciadaCliente } from "./RPAuth";
export type {
    PerfilUsuarioCliente,
    ResumenUsuarioCliente,
    EstadisticasUsuariosCliente
} from "./domains/RPUsuario";
export type {
    LecturaSensorCliente,
    PerfilSensorCliente,
    ResumenSensorCliente,
    EstadisticasSensoresCliente,
    ResumenEstadisticoSensoCliente
} from "./domains/RPSensor";
export type {
    ConfiguracionWiFiCliente,
    DiagnosticoSistemaCliente
} from "./domains/RPSistema";