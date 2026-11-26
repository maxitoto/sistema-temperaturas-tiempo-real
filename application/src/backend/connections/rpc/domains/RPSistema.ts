import { T, struct, InferDTO } from "../SchemaBuilder";
import { llamadoCliente } from "../FirmwareRPCClient";

// --- 1. DTOs de Entrada y Salida (Espejo 1:1 de RPSistema.hpp) ---

export const ConfiguracionWiFiDTO = struct({
    ssidSta:  T.char(32),
    claveSta: T.char(64),
    ssidAp:   T.char(32),
    claveAp:  T.char(64)
});

export const ResConfiguracionWiFiDTO = struct({
    ssidSta:           T.char(32),
    ssidAp:            T.char(32),
    ipSta:             T.char(16),
    ipAp:              T.char(16),
    conectadoARouter:  T.bool(),
    puntoAccesoActivo: T.bool()
});

export const DiagnosticoSistemaDTO = struct({
    heapLibreBytes: T.posInt(),
    uptimeSegundos: T.posInt()
});

export type ConfiguracionWiFiCliente  = InferDTO<typeof ResConfiguracionWiFiDTO>;
export type DiagnosticoSistemaCliente = InferDTO<typeof DiagnosticoSistemaDTO>;

// --- 2. Procedimientos Remotos Declarativos ---

const rpcActualizarWiFi = llamadoCliente.exactamenteUnaVez("actualizarConfiguracionWiFi", ConfiguracionWiFiDTO, ResConfiguracionWiFiDTO);
const rpcObtenerWiFi = llamadoCliente.alMenosUnaVez("obtenerConfiguracionWiFi", null, ResConfiguracionWiFiDTO, 60000);
const rpcDiagnostico    = llamadoCliente.alMenosUnaVez("ejecutarDiagnosticoSistema", null, DiagnosticoSistemaDTO);
const rpcReiniciar      = llamadoCliente.exactamenteUnaVez("reiniciarHardware", null, null);

export const RPSistema = {
    actualizarConfiguracionWiFi: (ssidSta: string, claveSta: string, ssidAp: string, claveAp: string) =>
        rpcActualizarWiFi({ ssidSta, claveSta, ssidAp, claveAp }),

    obtenerConfiguracionWiFi: () =>
        rpcObtenerWiFi(),

    ejecutarDiagnosticoSistema: () =>
        rpcDiagnostico(),

    reiniciarHardware: () =>
        rpcReiniciar()
};