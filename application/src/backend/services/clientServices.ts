import { UARTSClient } from "../connections/interfaces/UART-S/UARTSClient";
import { FirmwareRPCClient } from "../connections/rpc/FirmwareRPCClient";
import { LecturaSensorCliente } from "../connections/rpc/CodecRPC";

const clienteFirmware = new FirmwareRPCClient();
let ultimoLoteTiempoReal: LecturaSensorCliente[] = [];

export const clientServices = {
    listarPuertosSerie: () => UARTSClient.listarPuertos(),

    conectarPorSerie: (rutaPuerto: string) =>
        clienteFirmware.conectarPorSerie(rutaPuerto),

    conectarPorWiFi: (ipServidor: string) =>
        clienteFirmware.conectarPorWiFi(ipServidor),

    desconectar: () =>
        clienteFirmware.desconectar(),

    estaConectado: () =>
        clienteFirmware.estaConectado(),

    obtenerMedioActivo: () =>
        clienteFirmware.obtenerMedioActivo(),

    iniciarSesion: (usuario: string, clavePlana: string) =>
        clienteFirmware.iniciarSesion(usuario, clavePlana),

    cerrarSesion: () =>
        clienteFirmware.cerrarSesion(),

    suscribirSensoresTiempoReal: () =>
        clienteFirmware.suscribirSensoresTiempoReal(lecturas => {
            ultimoLoteTiempoReal = lecturas;
        }),

    obtenerUltimoLoteTiempoReal: () =>
        ultimoLoteTiempoReal,

    obtenerCensosPorMac: (macHex: string, limiteRegistros = 32) =>
        clienteFirmware.obtenerCensosPorMac(macHex, limiteRegistros),

    obtenerCensosPorMacYFecha: (
        macHex: string,
        timestampDesde: number,
        timestampHasta: number,
        limiteRegistros = 32
    ) => clienteFirmware.obtenerCensosPorMacYFecha(macHex, timestampDesde, timestampHasta, limiteRegistros),

    actualizarConfiguracionWiFi: (
        ssidSta: string,
        claveSta: string,
        ssidAp: string,
        claveAp: string
    ) => clienteFirmware.actualizarConfiguracionWiFi(ssidSta, claveSta, ssidAp, claveAp),

    ejecutarDiagnosticoSistema: () =>
        clienteFirmware.ejecutarDiagnosticoSistema(),

    reiniciarHardware: () =>
        clienteFirmware.reiniciarHardware(),

    obtenerLogsVisor: () =>
        clienteFirmware.obtenerLogsVisor(),

    limpiarLogsVisor: () =>
        clienteFirmware.limpiarLogsVisor()
};