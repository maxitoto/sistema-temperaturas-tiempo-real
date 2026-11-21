import { UARTSClient } from "../../interfaces/UART-S/UARTSClient";
import { clienteFirmware } from "../FirmwareRPCClient";

export const RPConexion = {
    listarPuertosSerie: () =>
        UARTSClient.listarPuertos(),

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

    obtenerLogsVisor: () =>
        clienteFirmware.obtenerLogsVisor(),

    limpiarLogsVisor: () =>
        clienteFirmware.limpiarLogsVisor(),

    invalidarCache: () =>
        clienteFirmware.invalidarCache()
    
};