#ifndef RP_USUARIO_H
#define RP_USUARIO_H

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/repositories/repoUsuario.hpp"
#include "../../../../rtos/EventosYColas.hpp"

// --- DTOs de Entrada (Tipos 1:1 con el DDL y usando RolUsuario directo) ---

struct ConsultaUsuarioNombreDTO {
    PgChar<20> usuario;
};

struct FiltroListadoUsuariosDTO {
    PgPosSmallInt limiteRegistros;
    PgPosSmallInt offsetRegistros;
    PgBool        soloActivos;
};

struct CrearUsuarioDTO {
    PgChar<20> usuario;
    PgChar<28> clavePlana;
    RolUsuario rol;
};

struct ActualizarUsuarioDTO {
    PgChar<20> usuario;
    RolUsuario nuevoRol;
    PgBool     activa;
};

struct CambiarContrasenaDTO {
    PgChar<20> usuario;
    PgChar<28> claveActual;
    PgChar<28> claveNueva;
};

// --- DTOs de Salida (Sin casts, sin 'reservado', 1:1 con UsuarioModel) ---

struct PerfilUsuarioDTO {
    PgAuto     idUsuario;
    PgChar<20> usuario;
    RolUsuario rol;
    PgSmallInt intentosFallidos;
    PgBool     bloqueado;
    PgBool     activa;
    PgDateTime creadoEl;
    PgDateTime actualizadoEl;
};

struct ResumenUsuarioDTO {
    PgAuto     idUsuario;
    PgChar<16> usuario;
    RolUsuario rol;
    PgSmallInt intentosFallidos;
    PgBool     bloqueado;
    PgBool     activa;
    PgEpoch    actualizadoEpoch;
};

using ListaUsuariosDTO = PgList<ResumenUsuarioDTO, 32>;

namespace RPUsuario {

    using DbRuntime::Repositories::RepoUsuario;
    using DbRuntime::Repositories::UsuarioDTO;
    using DbRuntime::Repositories::EstadisticasUsuariosDTO;

    inline void auditarAccionUsuario(
        const char* accion,
        const char* usuarioObjetivo,
        ResultadoAuditoria resultado = ResultadoAuditoria::EXITO,
        CriticidadEvento criticidad = CriticidadEvento::MEDIA
    ) {
        const uint8_t slot = RPCBase::contextoActual().slotClienteOrigen;
        int32_t idActor    = 0;
        char nombreActor[20]{};
        RPAuth::obtenerSnapshotUsuarioSlot(slot, idActor, nombreActor, sizeof(nombreActor));

        RTOS::encolarAuditoria(
            RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
            idActor,
            nombreActor[0] != '\0' ? nombreActor : "SISTEMA",
            (usuarioObjetivo && usuarioObjetivo[0] != '\0') ? usuarioObjetivo : "USUARIO",
            accion,
            static_cast<uint8_t>(resultado),
            static_cast<uint8_t>(criticidad)
        );
    }

    inline PerfilUsuarioDTO aPerfilPublico(const UsuarioDTO& u) {
        PerfilUsuarioDTO out{};
        out.idUsuario        = u.idUsuario;
        out.usuario          = u.usuario;
        out.rol              = u.rol;
        out.intentosFallidos = u.intentosFallidos;
        out.bloqueado        = u.bloqueado;
        out.activa           = u.activa;
        out.creadoEl         = u.creadoEl;
        out.actualizadoEl    = u.actualizadoEl;
        return out;
    }

    // 1. Obtener el perfil completo de un usuario (NO_ADMIN solo puede consultar su propio perfil)
    inline auto obtenerUsuarioPorNombre = llamadoSemantico.alMenosUnaVez<ConsultaUsuarioNombreDTO, PerfilUsuarioDTO>(
        "obtenerUsuarioPorNombre",
        [](const ConsultaUsuarioNombreDTO& consulta) -> PerfilUsuarioDTO {
            const auto& ctx   = RPCBase::contextoActual();
            const bool esAdmin = (ctx.rolAutenticado == RolUsuario::ADMIN);
            if (!esAdmin) {
                const char* propio = RPAuth::obtenerNombreUsuarioSlot(ctx.slotClienteOrigen);
                if (strcasecmp(consulta.usuario.c_str(), propio) != 0) {
                    auditarAccionUsuario(
                        "PERFIL_AJENO_DENEGADO",
                        consulta.usuario.c_str(),
                        ResultadoAuditoria::DENEGADO_ROL,
                        CriticidadEvento::MEDIA
                    );
                    throw std::runtime_error("PermissionError: un operador solo puede consultar su propio perfil");
                }
            }
            UsuarioDTO u = RepoUsuario::exigirPorNombreUsuario(consulta.usuario.c_str());
            return aPerfilPublico(u);
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Listar usuarios con paginación (solo ADMIN)
    inline auto listarUsuarios = llamadoSemantico.alMenosUnaVez<FiltroListadoUsuariosDTO, ListaUsuariosDTO>(
        "listarUsuarios",
        [](const FiltroListadoUsuariosDTO& filtro) -> ListaUsuariosDTO {
            auto filas = RepoUsuario::listarUsuariosPaginados(
                filtro.limiteRegistros,
                filtro.offsetRegistros,
                filtro.soloActivos
            );

            ListaUsuariosDTO salida{};
            for (const auto& f : filas) {
                ResumenUsuarioDTO item{};
                item.idUsuario        = f.idUsuario;
                item.usuario          = f.usuario;
                item.rol              = f.rol;
                item.intentosFallidos = f.intentosFallidos;
                item.bloqueado        = f.bloqueado;
                item.activa           = f.activa;
                item.actualizadoEpoch = f.actualizadoEpoch;
                if (!salida.agregar(item)) break;
            }
            return salida;
        },
        RolUsuario::ADMIN
    );

    // 3. Crear un nuevo usuario (solo ADMIN) + auditoría inmutable
    inline auto crearUsuario = llamadoSemantico.exactamenteUnaVez<CrearUsuarioDTO, PerfilUsuarioDTO>(
        "crearUsuario",
        [](const CrearUsuarioDTO& dto) -> PerfilUsuarioDTO {
            UsuarioDTO creado = RepoUsuario::crearUsuario(dto.usuario.c_str(), dto.clavePlana.c_str(), dto.rol);
            auditarAccionUsuario(
                "CREAR_USUARIO",
                creado.usuario.c_str(),
                ResultadoAuditoria::EXITO,
                CriticidadEvento::ALTA
            );
            return aPerfilPublico(creado);
        },
        RolUsuario::ADMIN
    );

    // 4. Actualizar rol y estado activo/inactivo (solo ADMIN) + sincronización inmediata de sesiones
    inline auto actualizarRolYEstadoUsuario = llamadoSemantico.exactamenteUnaVez<ActualizarUsuarioDTO, PerfilUsuarioDTO>(
        "actualizarRolYEstadoUsuario",
        [](const ActualizarUsuarioDTO& dto) -> PerfilUsuarioDTO {
            if (strcasecmp(dto.usuario.c_str(), "admin") == 0 &&
                (dto.nuevoRol != RolUsuario::ADMIN || !dto.activa)) {
                auditarAccionUsuario(
                    "MODIF_ADMIN_RAIZ_DENEG",
                    dto.usuario.c_str(),
                    ResultadoAuditoria::ERROR_VALIDACION,
                    CriticidadEvento::ALTA
                );
                throw std::runtime_error("PermissionError: no se puede degradar ni desactivar la cuenta raiz admin");
            }

            UsuarioDTO actualizado = RepoUsuario::actualizarRolYEstado(dto.usuario.c_str(), dto.nuevoRol, dto.activa);
            RPAuth::sincronizarSesionUsuario(actualizado.usuario.c_str(), actualizado.rol, actualizado.activa);

            const char* accion = !actualizado.activa
                ? "DESACTIVAR_USUARIO"
                : (actualizado.rol == RolUsuario::ADMIN
                    ? "CAMBIAR_ROL_A_ADMIN"
                    : (actualizado.rol == RolUsuario::NO_ADMIN ? "CAMBIAR_ROL_A_OPERADOR" : "CAMBIAR_ROL_A_NINGUNO"));

            auditarAccionUsuario(
                accion,
                actualizado.usuario.c_str(),
                ResultadoAuditoria::EXITO,
                CriticidadEvento::ALTA
            );
            return aPerfilPublico(actualizado);
        },
        RolUsuario::ADMIN
    );

    // 5. Cambiar contraseña (NO_ADMIN solo la propia con clave actual; ADMIN puede resetear cualquiera)
    inline auto cambiarContrasenaUsuario = llamadoSemantico.exactamenteUnaVez<CambiarContrasenaDTO, PerfilUsuarioDTO>(
        "cambiarContrasenaUsuario",
        [](const CambiarContrasenaDTO& dto) -> PerfilUsuarioDTO {
            const auto& ctx    = RPCBase::contextoActual();
            const bool esAdmin = (ctx.rolAutenticado == RolUsuario::ADMIN);
            const char* propio = RPAuth::obtenerNombreUsuarioSlot(ctx.slotClienteOrigen);
            const bool esPropio = (strcasecmp(dto.usuario.c_str(), propio) == 0);

            if (!esAdmin && !esPropio) {
                auditarAccionUsuario(
                    "CLAVE_AJENA_DENEGADA",
                    dto.usuario.c_str(),
                    ResultadoAuditoria::DENEGADO_ROL,
                    CriticidadEvento::ALTA
                );
                throw std::runtime_error("PermissionError: solo un administrador puede restablecer contrasenas de otros usuarios");
            }

            UsuarioDTO actualizado = RepoUsuario::cambiarContrasena(
                dto.usuario.c_str(),
                dto.claveActual.c_str(),
                dto.claveNueva.c_str(),
                !esAdmin
            );

            if (esAdmin && !esPropio) {
                RPAuth::revocarSesionesDeUsuario(actualizado.usuario.c_str());
            }

            auditarAccionUsuario(
                esPropio ? "CAMBIAR_CLAVE_PROPIA" : "RESET_CLAVE_ADMIN",
                actualizado.usuario.c_str(),
                ResultadoAuditoria::EXITO,
                esPropio ? CriticidadEvento::MEDIA : CriticidadEvento::ALTA
            );
            return aPerfilPublico(actualizado);
        },
        RolUsuario::NO_ADMIN
    );

    // 6. Desbloquear un usuario bloqueado por intentos fallidos (solo ADMIN)
    inline auto desbloquearUsuario = llamadoSemantico.exactamenteUnaVez<ConsultaUsuarioNombreDTO, PerfilUsuarioDTO>(
        "desbloquearUsuario",
        [](const ConsultaUsuarioNombreDTO& dto) -> PerfilUsuarioDTO {
            UsuarioDTO desbloqueado = RepoUsuario::desbloquearPorNombre(dto.usuario.c_str());
            auditarAccionUsuario(
                "DESBLOQUEAR_USUARIO",
                desbloqueado.usuario.c_str(),
                ResultadoAuditoria::EXITO,
                CriticidadEvento::MEDIA
            );
            return aPerfilPublico(desbloqueado);
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar usuario por nombre (solo ADMIN) + revocación inmediata de sesiones
    inline auto eliminarUsuario = llamadoSemantico.exactamenteUnaVez<ConsultaUsuarioNombreDTO, void>(
        "eliminarUsuario",
        [](const ConsultaUsuarioNombreDTO& dto) {
            RepoUsuario::eliminarPorNombre(dto.usuario.c_str());
            RPAuth::revocarSesionesDeUsuario(dto.usuario.c_str());
            auditarAccionUsuario(
                "ELIMINAR_USUARIO",
                dto.usuario.c_str(),
                ResultadoAuditoria::EXITO,
                CriticidadEvento::ALTA
            );
        },
        RolUsuario::ADMIN
    );

    // 8. Obtener estadísticas globales de la tabla Usuario (solo ADMIN)
    inline auto obtenerEstadisticasUsuarios = llamadoSemantico.alMenosUnaVez<void, EstadisticasUsuariosDTO>(
        "obtenerEstadisticasUsuarios",
        []() -> EstadisticasUsuariosDTO {
            return RepoUsuario::obtenerEstadisticasGlobales();
        },
        RolUsuario::ADMIN
    );

} // namespace RPUsuario

#endif