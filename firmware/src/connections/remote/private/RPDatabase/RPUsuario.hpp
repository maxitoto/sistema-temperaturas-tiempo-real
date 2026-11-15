#ifndef RP_USUARIO_H
#define RP_USUARIO_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/repositories/repoUsuario.hpp"

// --- DTOs de Entrada ---

struct ConsultaUsuarioNombreDTO {
    char usuario[20];
};

struct FiltroListadoUsuariosDTO {
    uint16_t limiteRegistros;
    uint16_t offsetRegistros;
    uint8_t  soloActivos;
    uint8_t  reservado[3];
};

struct CrearUsuarioDTO {
    char    usuario[20];
    char    clavePlana[28];
    uint8_t rol;
    uint8_t reservado[3];
};

struct ActualizarUsuarioDTO {
    char    usuario[20];
    uint8_t nuevoRol;
    uint8_t activa;
    uint8_t reservado[2];
};

struct CambiarContrasenaDTO {
    char usuario[20];
    char claveActual[28];
    char claveNueva[28];
};

// --- DTOs de Salida (Alineados a 4 bytes sin exponer hash ni salt) ---

struct PerfilUsuarioDTO {
    int32_t idUsuario;
    char    usuario[20];
    uint8_t rol;
    uint8_t intentosFallidos;
    uint8_t bloqueado;
    uint8_t activa;
    char    creadoEl[20];
    char    actualizadoEl[20];
};

struct ResumenUsuarioDTO {
    int32_t  idUsuario;
    char     usuario[16];
    uint8_t  rol;
    uint8_t  intentosFallidos;
    uint8_t  bloqueado;
    uint8_t  activa;
    uint32_t actualizadoEpoch;
};

struct ListaUsuariosDTO {
    uint8_t           cantidadUsuarios;
    uint8_t           reservado[3];
    ResumenUsuarioDTO usuarios[32];
};

namespace RPUsuario {

    using DbRuntime::Repositories::RepoUsuario;
    using DbRuntime::Repositories::UsuarioDTO;
    using DbRuntime::Repositories::EstadisticasUsuariosDTO;

    inline PerfilUsuarioDTO aPerfilPublico(const UsuarioDTO& u) {
        PerfilUsuarioDTO out{};
        out.idUsuario        = u.idUsuario;
        strncpy(out.usuario, u.usuario, sizeof(out.usuario) - 1);
        out.rol              = static_cast<uint8_t>(u.rol);
        out.intentosFallidos = static_cast<uint8_t>(u.intentosFallidos);
        out.bloqueado        = u.bloqueado ? 1 : 0;
        out.activa           = u.activa ? 1 : 0;
        strncpy(out.creadoEl, u.creadoEl, sizeof(out.creadoEl) - 1);
        strncpy(out.actualizadoEl, u.actualizadoEl, sizeof(out.actualizadoEl) - 1);
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

    // 2. Listar usuarios con paginación (si limiteRegistros > 32, el SGBD arroja error SQLSTATE 22023)
    inline auto listarUsuarios = llamadoSemantico.alMenosUnaVez<FiltroListadoUsuariosDTO, ListaUsuariosDTO>(
        "listarUsuarios",
        [](const FiltroListadoUsuariosDTO& filtro) -> ListaUsuariosDTO {
            auto filas = RepoUsuario::listarUsuariosPaginados(
                static_cast<int32_t>(filtro.limiteRegistros),
                static_cast<int32_t>(filtro.offsetRegistros),
                filtro.soloActivos != 0
            );

            ListaUsuariosDTO salida{};
            salida.cantidadUsuarios = static_cast<uint8_t>(filas.size());
            for (size_t i = 0; i < filas.size() && i < 32; ++i) {
                salida.usuarios[i].idUsuario        = filas[i].idUsuario;
                strncpy(salida.usuarios[i].usuario, filas[i].usuario, sizeof(salida.usuarios[i].usuario) - 1);
                salida.usuarios[i].rol              = static_cast<uint8_t>(filas[i].rol);
                salida.usuarios[i].intentosFallidos = static_cast<uint8_t>(filas[i].intentosFallidos);
                salida.usuarios[i].bloqueado        = filas[i].bloqueado ? 1 : 0;
                salida.usuarios[i].activa           = filas[i].activa ? 1 : 0;
                salida.usuarios[i].actualizadoEpoch = filas[i].actualizadoEpoch;
            }
            return salida;
        },
        RolUsuario::ADMIN
    );

    // 3. Crear un nuevo usuario (solo ADMIN)
    inline auto crearUsuario = llamadoSemantico.exactamenteUnaVez<CrearUsuarioDTO, PerfilUsuarioDTO>(
        "crearUsuario",
        [](const CrearUsuarioDTO& dto) -> PerfilUsuarioDTO {
            RolUsuario rol = (dto.rol == static_cast<uint8_t>(RolUsuario::ADMIN))
                           ? RolUsuario::ADMIN
                           : RolUsuario::NO_ADMIN;
            UsuarioDTO creado = RepoUsuario::crearUsuario(dto.usuario, dto.clavePlana, rol);
            return aPerfilPublico(creado);
        },
        RolUsuario::ADMIN
    );

    // 4. Actualizar rol y estado activo/inactivo de un usuario (solo ADMIN)
    inline auto actualizarRolYEstadoUsuario = llamadoSemantico.exactamenteUnaVez<ActualizarUsuarioDTO, PerfilUsuarioDTO>(
        "actualizarRolYEstadoUsuario",
        [](const ActualizarUsuarioDTO& dto) -> PerfilUsuarioDTO {
            RolUsuario rol = (dto.nuevoRol == static_cast<uint8_t>(RolUsuario::ADMIN))
                           ? RolUsuario::ADMIN
                           : RolUsuario::NO_ADMIN;
            UsuarioDTO actualizado = RepoUsuario::actualizarRolYEstado(dto.usuario, rol, dto.activa != 0);
            return aPerfilPublico(actualizado);
        },
        RolUsuario::ADMIN
    );

    // 5. Cambiar contraseña (si el cliente no es ADMIN, exige que claveActual coincida)
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

    // 8. Obtener estadísticas globales de la tabla Usuario (COUNT + FILTER WHERE + MAX)
    inline auto obtenerEstadisticasUsuarios = llamadoSemantico.alMenosUnaVez<void, EstadisticasUsuariosDTO>(
        "obtenerEstadisticasUsuarios",
        []() -> EstadisticasUsuariosDTO {
            return RepoUsuario::obtenerEstadisticasGlobales();
        },
        RolUsuario::ADMIN
    );

} // namespace RPUsuario

#endif