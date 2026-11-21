import { T, struct, listOf, InferDTO } from "../SchemaBuilder";
import { llamadoCliente } from "../FirmwareRPCClient";

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

export const ConsultaSensorMacDTO = struct({
    mac: T.char(25)
});

export const FiltroListadoSensoresDTO = struct({
    limiteRegistros: T.posSmallInt(),
    offsetRegistros: T.posSmallInt(),
    soloActivos:     T.bool()
});

export const ActualizarMetadatosSensorDTO = struct({
    mac:         T.char(25),
    nombre:      T.char(32),
    ubicacion:   T.char(32),
    descripcion: T.char(128),
    activa:      T.bool()
});

export const ActualizarCalibracionSensorDTO = struct({
    mac:       T.char(25),
    umbralSup: T.float(),
    umbralInf: T.float(),
    errorSup:  T.float(),
    errorInf:  T.float(),
    offset:    T.float()
});

export const PerfilSensorDTO = struct({
    idSensor:          T.auto(),
    mac:               T.char(25),
    nombre:            T.char(32),
    ubicacion:         T.char(32),
    descripcion:       T.char(128),
    umbralSup:         T.float(),
    umbralInf:         T.float(),
    errorSup:          T.float(),
    errorInf:          T.float(),
    offset:            T.float(),
    activa:            T.bool(),
    fechaCreacion:     T.dateTime(),
    fechaModificacion: T.dateTime()
});

export const ResumenSensorDTO = struct({
    idSensor:  T.auto(),
    mac:       T.char(25),
    nombre:    T.char(32),
    ubicacion: T.char(32),
    umbralSup: T.float(),
    umbralInf: T.float(),
    offset:    T.float(),
    activa:    T.bool()
});

export const ListaSensoresDTO = listOf(ResumenSensorDTO, 32);

export const EstadisticasSensoresDTO = struct({
    totalSensores:     T.int(),
    sensoresActivos:   T.int(),
    sensoresInactivos: T.int()
});

export const ResumenEstadisticoSensoDTO = struct({
    totalLecturas:       T.int(),
    temperaturaMinima:   T.float(),
    temperaturaMaxima:   T.float(),
    temperaturaPromedio: T.float()
});

export type LecturaSensorCliente           = InferDTO<typeof LecturaSensorDTO>;
export type PerfilSensorCliente            = InferDTO<typeof PerfilSensorDTO>;
export type ResumenSensorCliente           = InferDTO<typeof ResumenSensorDTO>;
export type EstadisticasSensoresCliente    = InferDTO<typeof EstadisticasSensoresDTO>;
export type ResumenEstadisticoSensoCliente = InferDTO<typeof ResumenEstadisticoSensoDTO>;

const streamSensoresTiempoReal      = llamadoCliente.talVez("suscribirSensoresTiempoReal", HistorialSensoresDTO);
const rpcObtenerPorMac              = llamadoCliente.alMenosUnaVez("obtenerCensosPorMac", FiltroPorMacDTO, HistorialSensoresDTO, 15000);
const rpcObtenerPorMacYFecha        = llamadoCliente.alMenosUnaVez("obtenerCensosPorMacYFecha", FiltroPorMacYFechaDTO, HistorialSensoresDTO, 15000);
const rpcListarSensores              = llamadoCliente.alMenosUnaVez("listarSensores", FiltroListadoSensoresDTO, ListaSensoresDTO, 30000);
const rpcObtenerSensorPorMac         = llamadoCliente.alMenosUnaVez("obtenerSensorPorMac", ConsultaSensorMacDTO, PerfilSensorDTO, 30000);
const rpcActualizarMetadatosSensor  = llamadoCliente.exactamenteUnaVez("actualizarMetadatosSensor", ActualizarMetadatosSensorDTO, PerfilSensorDTO);
const rpcActualizarCalibracion      = llamadoCliente.exactamenteUnaVez("actualizarCalibracionSensor", ActualizarCalibracionSensorDTO, PerfilSensorDTO);
const rpcEliminarSensorPorMac       = llamadoCliente.exactamenteUnaVez("eliminarSensorPorMac", ConsultaSensorMacDTO, null);
const rpcObtenerEstadisticasSensores = llamadoCliente.alMenosUnaVez("obtenerEstadisticasSensores", null, EstadisticasSensoresDTO, 30000);
const rpcObtenerEstadisticasSenso   = llamadoCliente.alMenosUnaVez("obtenerEstadisticasSensoPorMac", ConsultaSensorMacDTO, ResumenEstadisticoSensoDTO, 20000);

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
    },

    obtenerCensosPorMacYFechaFragmentado: async (
        macHex: string,
        timestampDesde: number,
        timestampHasta: number,
        maxRegistros = 256
    ): Promise<LecturaSensorCliente[]> => {
        if (timestampDesde > timestampHasta) {
            throw new Error("ValueError: la fecha 'Desde' no puede ser mayor que 'Hasta'");
        }

        const tope = Math.max(1, Math.min(maxRegistros, 320));
        const acumulados: LecturaSensorCliente[] = [];
        const vistos = new Set<string>();
        const desdeFijo = Math.floor(timestampDesde);
        let hastaCursor = Math.floor(timestampHasta);

        for (let bloque = 0; bloque < 10 && acumulados.length < tope && hastaCursor >= desdeFijo; bloque++) {
            const faltan = Math.min(32, tope - acumulados.length);
            const res = await rpcObtenerPorMacYFecha({
                macSensor: macHex,
                timestampDesde: desdeFijo,
                timestampHasta: hastaCursor,
                limiteRegistros: faltan
            });

            const items = res.items;
            if (items.length === 0) break;

            let minTs = hastaCursor;
            for (const it of items) {
                const clave = `${it.timestampMs}:${it.temperaturaCorregida}`;
                if (!vistos.has(clave)) {
                    vistos.add(clave);
                    acumulados.push(it);
                }
                if (it.timestampMs < minTs) {
                    minTs = it.timestampMs;
                }
            }

            if (items.length < faltan) break;
            const siguienteHasta = minTs - 1;
            if (siguienteHasta >= hastaCursor) break;
            hastaCursor = siguienteHasta;
        }

        acumulados.sort((a, b) => a.timestampMs - b.timestampMs);
        return acumulados;
    },

    listarSensores: async (limiteRegistros = 12, offsetRegistros = 0, soloActivos = false): Promise<ResumenSensorCliente[]> => {
        const res = await rpcListarSensores({ limiteRegistros, offsetRegistros, soloActivos });
        return res.items;
    },

    obtenerSensorPorMac: (mac: string): Promise<PerfilSensorCliente> =>
        rpcObtenerSensorPorMac({ mac }),

    actualizarMetadatosSensor: (
        mac: string,
        nombre: string,
        ubicacion: string,
        descripcion: string,
        activa = true
    ): Promise<PerfilSensorCliente> =>
        rpcActualizarMetadatosSensor({ mac, nombre, ubicacion, descripcion, activa }),

    actualizarCalibracionSensor: (
        mac: string,
        umbralSup: number,
        umbralInf: number,
        errorSup: number,
        errorInf: number,
        offset: number
    ): Promise<PerfilSensorCliente> =>
        rpcActualizarCalibracion({ mac, umbralSup, umbralInf, errorSup, errorInf, offset }),

    eliminarSensorPorMac: (mac: string): Promise<void> =>
        rpcEliminarSensorPorMac({ mac }),

    obtenerEstadisticasSensores: (): Promise<EstadisticasSensoresCliente> =>
        rpcObtenerEstadisticasSensores(),

    obtenerEstadisticasSensoPorMac: (mac: string): Promise<ResumenEstadisticoSensoCliente> =>
        rpcObtenerEstadisticasSenso({ mac })
};