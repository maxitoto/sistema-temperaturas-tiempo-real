#ifndef RP_CONFIGURACION_H
#define RP_CONFIGURACION_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/repositories/repoConfiguracionSistema.hpp"
#include "../../../../database/repositories/repoConfiguracionSmtp.hpp"
#include "../../../../database/repositories/repoCorreos.hpp"
#include "../../../../database/repositories/repoServidorNtp.hpp"
#include "../../../../rtos/RelojSistema.hpp"
#include "../../../../rtos/EventosYColas.hpp"
#include "../../../smtp/DespachadorCorreosSmtp.hpp"

struct ActualizarConfigSistemaInputDTO {
    PgInt    zonaHorariaMin;
    PgPosInt retencionCensoDias;
    PgPosInt retencionEventosDias;
    PgPosInt retencionAuditoriaDias;
    PgPosInt sdLibreMinimoKb;
    PgPosInt correoCooldownS;
    PgBool   activa;
};

struct ActualizarConfigSmtpInputDTO {
    PgChar<40>  hostSmtp;
    PgPosInt    puerto;
    PgBool      usarTls;
    PgChar<48>  usuario;
    PgChar<160> passwordCifrado;
    PgChar<48>  emailRemitente;
    PgChar<32>  nombreRemitente;
    PgBool      activa;
};

struct CorreoItemDTO {
    PgAuto              idCorreo;
    PgChar<24>          nombre;
    PgChar<24>          apellido;
    PgChar<48>          correoElectronico;
    TipoRecepcionCorreo tipoRecepcionMensajes;
    EstadoEnvioCorreo   ultimoEstadoEnvio;
    PgBool              activa;
};

struct CrearOActualizarCorreoDTO {
    PgInt               idCorreo; // 0 para crear, >0 para actualizar
    PgChar<24>          nombre;
    PgChar<24>          apellido;
    PgChar<48>          correoElectronico;
    TipoRecepcionCorreo tipoRecepcionMensajes;
    PgBool              activa;
};

struct ServidorNtpItemDTO {
    PgAuto     idServer;
    PgChar<40> server;
    PgSmallInt prioridad;
    PgBool     activa;
};

struct CrearOActualizarServidorNtpDTO {
    PgInt      idServer; // 0 para crear, >0 para actualizar
    PgChar<40> server;
    PgSmallInt prioridad;
    PgBool     activa;
};

struct IdRegistroConfigDTO {
    PgInt id;
};

struct SolicitudCorreoPruebaDTO {
    PgChar<48> destinatario; // 0..47 (vacío = primer destinatario activo o remitente)
};
static_assert(sizeof(SolicitudCorreoPruebaDTO) == 48, "SolicitudCorreoPruebaDTO debe medir 48 B");

struct ResultadoEnvioSmtpDTO {
    PgPosSmallInt codigoSmtp;          // 0..1
    PgBool        exito;               // 2
    PgUInt8       correosEnviados;     // 3
    PgPosSmallInt eventosActualizados; // 4..5
    PgChar<128>   detalle;             // 6..133
};
static_assert(sizeof(ResultadoEnvioSmtpDTO) == 134, "ResultadoEnvioSmtpDTO debe medir 134 B");

using ListaCorreosDTO       = PgList<CorreoItemDTO, 12>;
using ListaServidoresNtpDTO = PgList<ServidorNtpItemDTO, 16>;

namespace RPConfiguracion {

    using DbRuntime::Repositories::RepoConfiguracionSistema;
    using DbRuntime::Repositories::ConfiguracionSistemaDTO;
    using DbRuntime::Repositories::RepoConfiguracionSmtp;
    using DbRuntime::Repositories::ConfiguracionSmtpDTO;
    using DbRuntime::Repositories::RepoCorreos;
    using DbRuntime::Repositories::CorreoDTO;
    using DbRuntime::Repositories::RepoServidoresNtp;
    using DbRuntime::Repositories::ServidorNtpDTO;

    inline CorreoItemDTO aCorreoItem(const CorreoDTO& c) {
        CorreoItemDTO d{};
        d.idCorreo              = c.idCorreo;
        d.nombre                = c.nombre;
        d.apellido              = c.apellido;
        d.correoElectronico     = c.correoElectronico;
        d.tipoRecepcionMensajes = c.tipoRecepcionMensajes;
        d.ultimoEstadoEnvio     = c.ultimoEstadoEnvio;
        d.activa                = c.activa;
        return d;
    }

    inline ServidorNtpItemDTO aNtpItem(const ServidorNtpDTO& s) {
        ServidorNtpItemDTO d{};
        d.idServer  = s.idServer;
        d.server    = s.server;
        d.prioridad = s.prioridad;
        d.activa    = s.activa;
        return d;
    }

    // 1. Obtener ConfiguracionSistema (NO_ADMIN)
    inline auto obtenerConfiguracionSistema = llamadoSemantico.alMenosUnaVez<void, ConfiguracionSistemaDTO>(
        "obtenerConfiguracionSistema",
        []() -> ConfiguracionSistemaDTO {
            return RepoConfiguracionSistema::exigirConfiguracion();
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Actualizar ConfiguracionSistema (ADMIN)
    inline auto actualizarConfiguracionSistema = llamadoSemantico.exactamenteUnaVez<ActualizarConfigSistemaInputDTO, ConfiguracionSistemaDTO>(
        "actualizarConfiguracionSistema",
        [](const ActualizarConfigSistemaInputDTO& in) -> ConfiguracionSistemaDTO {
            ConfiguracionSistemaDTO dto{};
            dto.idConfiguracionSistema = 1;
            dto.zonaHorariaMin         = in.zonaHorariaMin;
            dto.retencionCensoDias     = in.retencionCensoDias;
            dto.retencionEventosDias   = in.retencionEventosDias;
            dto.retencionAuditoriaDias = in.retencionAuditoriaDias;
            dto.sdLibreMinimoKb        = in.sdLibreMinimoKb;
            dto.correoCooldownS        = in.correoCooldownS;
            dto.activa                 = in.activa;

            ConfiguracionSistemaDTO act = RepoConfiguracionSistema::actualizarTodo(dto);
            RTOS::RelojSistema::configurarParametrosRed(act.zonaHorariaMin);
            return act;
        },
        RolUsuario::ADMIN
    );

    // 3. Obtener ConfiguracionSmtp (ADMIN)
    inline auto obtenerConfiguracionSmtp = llamadoSemantico.alMenosUnaVez<void, ConfiguracionSmtpDTO>(
        "obtenerConfiguracionSmtp",
        []() -> ConfiguracionSmtpDTO {
            return RepoConfiguracionSmtp::exigirConfiguracion();
        },
        RolUsuario::ADMIN
    );

    // 4. Actualizar ConfiguracionSmtp (ADMIN)
    inline auto actualizarConfiguracionSmtp = llamadoSemantico.exactamenteUnaVez<ActualizarConfigSmtpInputDTO, ConfiguracionSmtpDTO>(
        "actualizarConfiguracionSmtp",
        [](const ActualizarConfigSmtpInputDTO& in) -> ConfiguracionSmtpDTO {
            ConfiguracionSmtpDTO dto{};
            dto.idConfiguracionSmtp = 1;
            dto.hostSmtp            = in.hostSmtp;
            dto.puerto              = in.puerto;
            dto.usarTls             = in.usarTls;
            dto.usuario             = in.usuario;
            dto.passwordCifrado     = in.passwordCifrado;
            dto.emailRemitente      = in.emailRemitente;
            dto.nombreRemitente     = in.nombreRemitente;
            dto.activa              = in.activa;
            return RepoConfiguracionSmtp::actualizarTodo(dto);
        },
        RolUsuario::ADMIN
    );

    // 5. Listar destinatarios de Correos (NO_ADMIN)
    inline auto listarCorreos = llamadoSemantico.alMenosUnaVez<void, ListaCorreosDTO>(
        "listarCorreos",
        []() -> ListaCorreosDTO {
            auto filas = RepoCorreos::listarPaginados(12, 0, false);
            ListaCorreosDTO out{};
            for (const auto& c : filas) {
                if (!out.agregar(aCorreoItem(c))) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 6. Crear o actualizar destinatario en Correos (ADMIN)
    inline auto guardarCorreo = llamadoSemantico.exactamenteUnaVez<CrearOActualizarCorreoDTO, CorreoItemDTO>(
        "guardarCorreo",
        [](const CrearOActualizarCorreoDTO& in) -> CorreoItemDTO {
            if (in.idCorreo > 0) {
                return aCorreoItem(RepoCorreos::actualizarPorId(
                    in.idCorreo,
                    in.nombre.c_str(),
                    in.apellido.c_str(),
                    in.correoElectronico.c_str(),
                    in.tipoRecepcionMensajes,
                    in.activa
                ));
            }
            return aCorreoItem(RepoCorreos::crearCorreo(
                in.nombre.c_str(),
                in.apellido.c_str(),
                in.correoElectronico.c_str(),
                in.tipoRecepcionMensajes,
                in.activa
            ));
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar destinatario de Correos por ID (ADMIN)
    inline auto eliminarCorreoPorId = llamadoSemantico.exactamenteUnaVez<IdRegistroConfigDTO, void>(
        "eliminarCorreoPorId",
        [](const IdRegistroConfigDTO& dto) {
            RepoCorreos::eliminarPorId(dto.id);
        },
        RolUsuario::ADMIN
    );

    // 8. Listar ServidoresNtp (NO_ADMIN)
    inline auto listarServidoresNtp = llamadoSemantico.alMenosUnaVez<void, ListaServidoresNtpDTO>(
        "listarServidoresNtp",
        []() -> ListaServidoresNtpDTO {
            auto filas = RepoServidoresNtp::listarPaginados(16, 0);
            ListaServidoresNtpDTO out{};
            for (const auto& s : filas) {
                if (!out.agregar(aNtpItem(s))) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 9. Crear o actualizar ServidorNtp (ADMIN)
    inline auto guardarServidorNtp = llamadoSemantico.exactamenteUnaVez<CrearOActualizarServidorNtpDTO, ServidorNtpItemDTO>(
        "guardarServidorNtp",
        [](const CrearOActualizarServidorNtpDTO& in) -> ServidorNtpItemDTO {
            if (in.idServer > 0) {
                return aNtpItem(RepoServidoresNtp::actualizarPorId(
                    in.idServer,
                    in.server.c_str(),
                    in.prioridad,
                    in.activa
                ));
            }
            return aNtpItem(RepoServidoresNtp::crearServidor(
                in.server.c_str(),
                in.prioridad,
                in.activa
            ));
        },
        RolUsuario::ADMIN
    );

    // 10. Eliminar ServidorNtp por ID (ADMIN)
    inline auto eliminarServidorNtpPorId = llamadoSemantico.exactamenteUnaVez<IdRegistroConfigDTO, void>(
        "eliminarServidorNtpPorId",
        [](const IdRegistroConfigDTO& dto) {
            RepoServidoresNtp::eliminarPorId(dto.id);
        },
        RolUsuario::ADMIN
    );

    // 11. Enviar un correo de prueba inmediato con la configuración SMTP guardada (ADMIN)
    inline auto enviarCorreoPruebaSmtp = llamadoSemantico.exactamenteUnaVez<SolicitudCorreoPruebaDTO, ResultadoEnvioSmtpDTO>(
        "enviarCorreoPruebaSmtp",
        [](const SolicitudCorreoPruebaDTO& in) -> ResultadoEnvioSmtpDTO {
            const auto r = SmtpNet::DespachadorCorreosSmtp::enviarCorreoDePrueba(in.destinatario.c_str());
            ResultadoEnvioSmtpDTO out{};
            out.codigoSmtp          = r.codigoSmtp;
            out.exito               = r.exito;
            out.correosEnviados     = r.correosEnviados;
            out.eventosActualizados = r.eventosActualizados;
            out.detalle             = r.detalle;
            return out;
        },
        RolUsuario::ADMIN
    );

    // 12. Forzar despacho inmediato de avisos PENDIENTE (saltea cooldown para prueba en vivo) (ADMIN)
    inline auto despacharAlertasCorreoAhora = llamadoSemantico.exactamenteUnaVez<void, ResultadoEnvioSmtpDTO>(
        "despacharAlertasCorreoAhora",
        []() -> ResultadoEnvioSmtpDTO {
            const auto r = SmtpNet::DespachadorCorreosSmtp::despacharPendientes(true);
            ResultadoEnvioSmtpDTO out{};
            out.codigoSmtp          = r.codigoSmtp;
            out.exito               = r.exito;
            out.correosEnviados     = r.correosEnviados;
            out.eventosActualizados = r.eventosActualizados;
            out.detalle             = r.detalle;
            return out;
        },
        RolUsuario::ADMIN
    );

} // namespace RPConfiguracion

#endif