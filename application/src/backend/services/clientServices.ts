import { UARTSClient } from "../connections/interfaces/UART-S/UARTSClient";
import { FirmwareRPCClient } from "../connections/rpc/FirmwareRPCClient";
import { LecturaSensorCliente } from "../connections/rpc/CodecRPC";

const clienteFirmware = new FirmwareRPCClient();
let ultimoLoteTiempoReal: LecturaSensorCliente[] = [];

export const clientServices = {
    obtenerUsuarioPorNombre: (usuario: string) =>
        clienteFirmware.obtenerUsuarioPorNombre(usuario),

    listarUsuarios: (limiteRegistros = 32, offsetRegistros = 0, soloActivos = false) =>
        clienteFirmware.listarUsuarios(limiteRegistros, offsetRegistros, soloActivos),

    crearUsuario: (usuario: string, clavePlana: string, rol = 1) =>
        clienteFirmware.crearUsuario(usuario, clavePlana, rol),

    actualizarRolYEstadoUsuario: (usuario: string, nuevoRol: number, activa: boolean) =>
        clienteFirmware.actualizarRolYEstadoUsuario(usuario, nuevoRol, activa),

    cambiarContrasenaUsuario: (usuario: string, claveActual: string, claveNueva: string) =>
        clienteFirmware.cambiarContrasenaUsuario(usuario, claveActual, claveNueva),

    desbloquearUsuario: (usuario: string) =>
        clienteFirmware.desbloquearUsuario(usuario),

    eliminarUsuario: (usuario: string) =>
        clienteFirmware.eliminarUsuario(usuario),

    obtenerEstadisticasUsuarios: () =>
        clienteFirmware.obtenerEstadisticasUsuarios(),

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

    suscribirSensoresTiempoReal: async () => {
        const loteInicial = await clienteFirmware.suscribirSensoresTiempoReal(lecturas => {
            ultimoLoteTiempoReal = lecturas;
        });
        ultimoLoteTiempoReal = loteInicial;
        return loteInicial;
    },

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

    obtenerConfiguracionWiFi: () =>
        clienteFirmware.obtenerConfiguracionWiFi(),

    ejecutarDiagnosticoSistema: () =>
        clienteFirmware.ejecutarDiagnosticoSistema(),

    reiniciarHardware: () =>
        clienteFirmware.reiniciarHardware(),

    obtenerLogsVisor: () =>
        clienteFirmware.obtenerLogsVisor(),

    limpiarLogsVisor: () =>
        clienteFirmware.limpiarLogsVisor()  
};