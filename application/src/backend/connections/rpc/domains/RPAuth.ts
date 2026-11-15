import { T, struct, InferDTO } from "../SchemaBuilder";
import { ProcedimientoSistemaRPC, clienteFirmware } from "../FirmwareRPCClient";

// Espejo 1:1 de CredencialesLoginDTO y SesionIniciadaDTO en RPAuth.hpp
export const CredencialesLoginDTO = struct({
    usuario:    T.char(32),
    clavePlana: T.char(32)
});

export const SesionIniciadaDTO = struct({
    rolAsignado:              T.uint8(),
    tiempoExpiracionSegundos: T.posInt()
});

export type SesionIniciadaCliente = InferDTO<typeof SesionIniciadaDTO>;

export const RPAuth = {
    iniciarSesion: async (usuario: string, clavePlana: string): Promise<SesionIniciadaCliente> => {
        const raw = await clienteFirmware.invocarPorId(
            ProcedimientoSistemaRPC.L2_LOGIN_USUARIO,
            CredencialesLoginDTO.serialize({ usuario, clavePlana })
        );
        return SesionIniciadaDTO.deserialize(raw);
    },

    cerrarSesion: async (): Promise<void> => {
        try {
            await clienteFirmware.invocarPorId(ProcedimientoSistemaRPC.L2_LOGOUT_USUARIO);
        } finally {
            await clienteFirmware.cerrarTransporteLocal();
        }
    }
};