import { T, struct, listOf, InferDTO } from "../SchemaBuilder";
import { llamadoCliente } from "../FirmwareRPCClient";

// --- 1. DTOs de Entrada (Espejo 1:1 de RPUsuario.hpp) ---

export const ConsultaUsuarioNombreDTO = struct({
    usuario: T.char(20)
});

export const FiltroListadoUsuariosDTO = struct({
    limiteRegistros: T.posSmallInt(),
    offsetRegistros: T.posSmallInt(),
    soloActivos:     T.bool()
});

export const CrearUsuarioDTO = struct({
    usuario:    T.char(20),
    clavePlana: T.char(28),
    rol:        T.enum()
});

export const ActualizarUsuarioDTO = struct({
    usuario:  T.char(20),
    nuevoRol: T.enum(),
    activa:   T.bool()
});

export const CambiarContrasenaDTO = struct({
    usuario:     T.char(20),
    claveActual: T.char(28),
    claveNueva:  T.char(28)
});

// --- 2. DTOs de Salida (Espejo 1:1 de RPUsuario.hpp) ---
export enum RolUsuario {
    NINGUNO  = 0,
    NO_ADMIN = 1,
    ADMIN    = 2
}

export const PerfilUsuarioDTO = struct({
    idUsuario:        T.auto(),
    usuario:          T.char(20),
    rol:              T.enum(),
    intentosFallidos: T.smallInt(),
    bloqueado:        T.bool(),
    activa:           T.bool(),
    creadoEl:         T.dateTime(),
    actualizadoEl:    T.dateTime()
});

export const ResumenUsuarioDTO = struct({
    idUsuario:        T.auto(),
    usuario:          T.char(16),
    rol:              T.enum(),
    intentosFallidos: T.smallInt(),
    bloqueado:        T.bool(),
    activa:           T.bool(),
    actualizadoEpoch: T.epoch()
});

export const ListaUsuariosDTO = listOf(ResumenUsuarioDTO, 32);

export const EstadisticasUsuariosDTO = struct({
    totalUsuarios:       T.int(),
    usuariosActivos:     T.int(),
    usuariosBloqueados:  T.int(),
    maxIntentosFallidos: T.int()
});

export type PerfilUsuarioCliente        = InferDTO<typeof PerfilUsuarioDTO>;
export type ResumenUsuarioCliente       = InferDTO<typeof ResumenUsuarioDTO>;
export type EstadisticasUsuariosCliente = InferDTO<typeof EstadisticasUsuariosDTO>;

// --- 3. Procedimientos Remotos Declarativos ---

const rpcObtenerPorNombre     = llamadoCliente.alMenosUnaVez("obtenerUsuarioPorNombre", ConsultaUsuarioNombreDTO, PerfilUsuarioDTO);
const rpcListarUsuarios       = llamadoCliente.alMenosUnaVez("listarUsuarios", FiltroListadoUsuariosDTO, ListaUsuariosDTO);
const rpcCrearUsuario         = llamadoCliente.exactamenteUnaVez("crearUsuario", CrearUsuarioDTO, PerfilUsuarioDTO);
const rpcActualizarRolYEstado = llamadoCliente.exactamenteUnaVez("actualizarRolYEstadoUsuario", ActualizarUsuarioDTO, PerfilUsuarioDTO);
const rpcCambiarContrasena    = llamadoCliente.exactamenteUnaVez("cambiarContrasenaUsuario", CambiarContrasenaDTO, PerfilUsuarioDTO);
const rpcDesbloquearUsuario   = llamadoCliente.exactamenteUnaVez("desbloquearUsuario", ConsultaUsuarioNombreDTO, PerfilUsuarioDTO);
const rpcEliminarUsuario      = llamadoCliente.exactamenteUnaVez("eliminarUsuario", ConsultaUsuarioNombreDTO, null);
const rpcObtenerEstadisticas  = llamadoCliente.alMenosUnaVez("obtenerEstadisticasUsuarios", null, EstadisticasUsuariosDTO);

export const RPUsuario = {
    obtenerUsuarioPorNombre: (usuario: string) =>
        rpcObtenerPorNombre({ usuario }),

    listarUsuarios: async (limiteRegistros = 32, offsetRegistros = 0, soloActivos = false): Promise<ResumenUsuarioCliente[]> => {
        const res = await rpcListarUsuarios({ limiteRegistros, offsetRegistros, soloActivos });
        return res.items;
    },

    crearUsuario: (usuario: string, clavePlana: string, rol = 1) =>
        rpcCrearUsuario({ usuario, clavePlana, rol }),

    actualizarRolYEstadoUsuario: (usuario: string, nuevoRol: number, activa: boolean) =>
        rpcActualizarRolYEstado({ usuario, nuevoRol, activa }),

    cambiarContrasenaUsuario: (usuario: string, claveActual: string, claveNueva: string) =>
        rpcCambiarContrasena({ usuario, claveActual, claveNueva }),

    desbloquearUsuario: (usuario: string) =>
        rpcDesbloquearUsuario({ usuario }),

    eliminarUsuario: (usuario: string) =>
        rpcEliminarUsuario({ usuario }),

    obtenerEstadisticasUsuarios: () =>
        rpcObtenerEstadisticas()
};