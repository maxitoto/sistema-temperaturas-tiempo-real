#ifndef RP_USUARIO_H
#define RP_USUARIO_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/repositories/repoUsuario.hpp"

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

    // 1. Obtener el perfil completo de un usuario por su nombre
    inline auto obtenerUsuarioPorNombre = llamadoSemantico.alMenosUnaVez<ConsultaUsuarioNombreDTO, PerfilUsuarioDTO>(
        "obtenerUsuarioPorNombre",
        [](const ConsultaUsuarioNombreDTO& consulta) -> PerfilUsuarioDTO {
            UsuarioDTO u = RepoUsuario::exigirPorNombreUsuario(consulta.usuario);
            return aPerfilPublico(u);
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Listar usuarios con paginación
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

    // 3. Crear un nuevo usuario (solo ADMIN)
    inline auto crearUsuario = llamadoSemantico.exactamenteUnaVez<CrearUsuarioDTO, PerfilUsuarioDTO>(
        "crearUsuario",
        [](const CrearUsuarioDTO& dto) -> PerfilUsuarioDTO {
            UsuarioDTO creado = RepoUsuario::crearUsuario(dto.usuario, dto.clavePlana, dto.rol);
            return aPerfilPublico(creado);
        },
        RolUsuario::ADMIN
    );

    // 4. Actualizar rol y estado activo/inactivo de un usuario (solo ADMIN)
    inline auto actualizarRolYEstadoUsuario = llamadoSemantico.exactamenteUnaVez<ActualizarUsuarioDTO, PerfilUsuarioDTO>(
        "actualizarRolYEstadoUsuario",
        [](const ActualizarUsuarioDTO& dto) -> PerfilUsuarioDTO {
            UsuarioDTO actualizado = RepoUsuario::actualizarRolYEstado(dto.usuario, dto.nuevoRol, dto.activa);
            return aPerfilPublico(actualizado);
        },
        RolUsuario::ADMIN
    );

    // 5. Cambiar contraseña
    inline auto cambiarContrasenaUsuario = llamadoSemantico.exactamenteUnaVez<CambiarContrasenaDTO, PerfilUsuarioDTO>(
        "cambiarContrasenaUsuario",
        [](const CambiarContrasenaDTO& dto) -> PerfilUsuarioDTO {
            const bool esAdmin = (RPCBase::contextoActual().rolAutenticado == RolUsuario::ADMIN);
            UsuarioDTO actualizado = RepoUsuario::cambiarContrasena(
                dto.usuario,
                dto.claveActual,
                dto.claveNueva,
                !esAdmin
            );
            return aPerfilPublico(actualizado);
        },
        RolUsuario::NO_ADMIN
    );

    // 6. Desbloquear un usuario bloqueado por intentos fallidos (solo ADMIN)
    inline auto desbloquearUsuario = llamadoSemantico.exactamenteUnaVez<ConsultaUsuarioNombreDTO, PerfilUsuarioDTO>(
        "desbloquearUsuario",
        [](const ConsultaUsuarioNombreDTO& dto) -> PerfilUsuarioDTO {
            UsuarioDTO desbloqueado = RepoUsuario::desbloquearPorNombre(dto.usuario);
            return aPerfilPublico(desbloqueado);
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar usuario por nombre (solo ADMIN)
    inline auto eliminarUsuario = llamadoSemantico.exactamenteUnaVez<ConsultaUsuarioNombreDTO, void>(
        "eliminarUsuario",
        [](const ConsultaUsuarioNombreDTO& dto) {
            RepoUsuario::eliminarPorNombre(dto.usuario);
        },
        RolUsuario::ADMIN
    );

    // 8. Obtener estadísticas globales de la tabla Usuario
    inline auto obtenerEstadisticasUsuarios = llamadoSemantico.alMenosUnaVez<void, EstadisticasUsuariosDTO>(
        "obtenerEstadisticasUsuarios",
        []() -> EstadisticasUsuariosDTO {
            return RepoUsuario::obtenerEstadisticasGlobales();
        },
        RolUsuario::ADMIN
    );

} // namespace RPUsuario

#endif