import { T, struct, listOf, InferDTO } from "../SchemaBuilder";
import { llamadoCliente } from "../FirmwareRPCClient";

// --- 1. DTOs de Entrada y Salida (Espejo 1:1 de RPSensor.hpp) ---

export const FiltroPorMacDTO = struct({
    macSensor:       T.mac(),
    limiteRegistros: T.posSmallInt()
});

export const FiltroPorMacYFechaDTO = struct({
    macSensor:       T.mac(),
    timestampDesde:  T.epoch(),
    timestampHasta:  T.epoch(),
    limiteRegistros: T.posSmallInt()
});

export const LecturaSensorDTO = struct({
    timestampMs:          T.posInt(),
    idSensor:             T.posSmallInt(),
    macHex:               T.mac(),
    temperaturaCorregida: T.float()
});

export const HistorialSensoresDTO = listOf(LecturaSensorDTO, 32);
export type LecturaSensorCliente = InferDTO<typeof LecturaSensorDTO>;

// --- 2. Procedimientos Remotos Declarativos ---

const streamSensoresTiempoReal = llamadoCliente.talVez("suscribirSensoresTiempoReal", HistorialSensoresDTO);
const rpcObtenerPorMac         = llamadoCliente.alMenosUnaVez("obtenerCensosPorMac", FiltroPorMacDTO, HistorialSensoresDTO);
const rpcObtenerPorMacYFecha   = llamadoCliente.alMenosUnaVez("obtenerCensosPorMacYFecha", FiltroPorMacYFechaDTO, HistorialSensoresDTO);

let ultimoLoteTiempoReal: LecturaSensorCliente[] = [];

export const RPSensor = {
    suscribirSensoresTiempoReal: async (): Promise<LecturaSensorCliente[]> => {
        const loteInicial = await streamSensoresTiempoReal(nuevoLote => {
            ultimoLoteTiempoReal = nuevoLote.items;
        });
        ultimoLoteTiempoReal = loteInicial.items;
        return loteInicial.items;
    },

    obtenerUltimoLoteTiempoReal: (): LecturaSensorCliente[] =>
        ultimoLoteTiempoReal,

    obtenerCensosPorMac: async (macHex: string, limiteRegistros = 32): Promise<LecturaSensorCliente[]> => {
        const res = await rpcObtenerPorMac({ macSensor: macHex, limiteRegistros });
        return res.items;
    },

    obtenerCensosPorMacYFecha: async (
        macHex: string,
        timestampDesde: number,
        timestampHasta: number,
        limiteRegistros = 32
    ): Promise<LecturaSensorCliente[]> => {
        const res = await rpcObtenerPorMacYFecha({
            macSensor: macHex,
            timestampDesde,
            timestampHasta,
            limiteRegistros
        });
        return res.items;
    }
};