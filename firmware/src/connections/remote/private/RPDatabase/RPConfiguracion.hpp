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

struct ActualizarConfigSistemaInputDTO {
    PgInt    zonaHorariaMin;
    PgPosInt retencionSensoDias;
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

using ListaCorreosDTO       = PgList<CorreoItemDTO, 12>;
using ListaServidoresNtpDTO = PgList<ServidorNtpItemDTO, 16>;

namespace RPConfiguracion {

    using DbRuntime::Repositories::RepoConfiguracionSistema;
    using DbRuntime::Repositories::ConfiguracionSistemaDTO;
    using DbRuntime::Repositories::ConfiguracionSmtpDTO;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;

    inline CorreoItemDTO mapearCorreo(const PgTuple& r) {
        CorreoItemDTO d{};
        r["id_correo"]               >> d.idCorreo;
        r["nombre"]                  >> d.nombre;
        r["apellido"]                >> d.apellido;
        r["correo_electronico"]      >> d.correoElectronico;
        r["tipo_recepcion_mensajes"] >> d.tipoRecepcionMensajes;
        r["ultimo_estado_envio"]     >> d.ultimoEstadoEnvio;
        r["activa"]                  >> d.activa;
        return d;
    }

    inline ServidorNtpItemDTO mapearNtp(const PgTuple& r) {
        ServidorNtpItemDTO d{};
        r["id_server"] >> d.idServer;
        r["server"]    >> d.server;
        r["prioridad"] >> d.prioridad;
        r["activa"]    >> d.activa;
        return d;
    }

    inline ConfiguracionSmtpDTO mapearSmtp(const PgTuple& r) {
        ConfiguracionSmtpDTO d{};
        r["id_configuracion_smtp"] >> d.idConfiguracionSmtp;
        r["host_smtp"]             >> d.hostSmtp;
        r["puerto"]                >> d.puerto;
        r["usar_tls"]              >> d.usarTls;
        r["usuario"]               >> d.usuario;
        r["password_cifrado"]      >> d.passwordCifrado;
        r["email_remitente"]       >> d.emailRemitente;
        r["nombre_remitente"]      >> d.nombreRemitente;
        r["activa"]                >> d.activa;
        r["fecha_creacion"]        >> d.fechaCreacion;
        r["fecha_modificacion"]    >> d.fechaModificacion;
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
            dto.retencionSensoDias     = in.retencionSensoDias;
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
            PgTuple fila;
            if (DbRuntime::Orm::ConfiguracionSmtp.filter_eq("id_configuracion_smtp", 1).first(fila)) {
                return mapearSmtp(fila);
            }
            auto res = DbRuntime::Orm::ConfiguracionSmtp.create({
                PgCell("id_configuracion_smtp", static_cast<int32_t>(1)),
                PgCell("host_smtp", ""),
                PgCell("puerto", static_cast<uint32_t>(465)),
                PgCell("usar_tls", true),
                PgCell("usuario", ""),
                PgCell("password_cifrado", ""),
                PgCell("email_remitente", ""),
                PgCell("nombre_remitente", "Laboratorio UNTDF"),
                PgCell("activa", true)
            }, {"*"});
            if (res.success && !res.returningRows.empty()) {
                return mapearSmtp(res.returningRows[0]);
            }
            return ConfiguracionSmtpDTO{};
        },
        RolUsuario::ADMIN
    );

    // 4. Actualizar ConfiguracionSmtp (ADMIN)
    inline auto actualizarConfiguracionSmtp = llamadoSemantico.exactamenteUnaVez<ActualizarConfigSmtpInputDTO, ConfiguracionSmtpDTO>(
        "actualizarConfiguracionSmtp",
        [](const ActualizarConfigSmtpInputDTO& in) -> ConfiguracionSmtpDTO {
            PgTuple fila;
            if (!DbRuntime::Orm::ConfiguracionSmtp.filter_eq("id_configuracion_smtp", 1).first(fila)) {
                DbRuntime::Orm::ConfiguracionSmtp.create({
                    PgCell("id_configuracion_smtp", static_cast<int32_t>(1))
                });
            }
            auto res = DbRuntime::Orm::ConfiguracionSmtp.filter_eq("id_configuracion_smtp", 1)
                .returning({"*"})
                .update({
                    PgCell("host_smtp",        in.hostSmtp.c_str()),
                    PgCell("puerto",           in.puerto),
                    PgCell("usar_tls",         in.usarTls),
                    PgCell("usuario",          in.usuario.c_str()),
                    PgCell("password_cifrado", in.passwordCifrado.c_str()),
                    PgCell("email_remitente",  in.emailRemitente.c_str()),
                    PgCell("nombre_remitente", in.nombreRemitente.c_str()),
                    PgCell("activa",           in.activa)
                });
            if (!res.success || res.returningRows.empty()) {
                throw std::runtime_error("RuntimeError: no se pudo actualizar ConfiguracionSmtp");
            }
            return mapearSmtp(res.returningRows[0]);
        },
        RolUsuario::ADMIN
    );

    // 5. Listar destinatarios de Correos (NO_ADMIN)
    inline auto listarCorreos = llamadoSemantico.alMenosUnaVez<void, ListaCorreosDTO>(
        "listarCorreos",
        []() -> ListaCorreosDTO {
            auto filas = DbRuntime::Orm::Correos.objects()
                .order_by("id_correo")
                .limit(12)
                .all_as<CorreoItemDTO>(&mapearCorreo);
            ListaCorreosDTO out{};
            for (const auto& c : filas) {
                if (!out.agregar(c)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 6. Crear o actualizar destinatario en Correos (ADMIN)
    inline auto guardarCorreo = llamadoSemantico.exactamenteUnaVez<CrearOActualizarCorreoDTO, CorreoItemDTO>(
        "guardarCorreo",
        [](const CrearOActualizarCorreoDTO& in) -> CorreoItemDTO {
            if (in.correoElectronico.vacio() || in.nombre.vacio()) {
                throw std::invalid_argument("ValueError: nombre y correo_electronico son obligatorios");
            }
            if (in.idCorreo > 0) {
                auto res = DbRuntime::Orm::Correos.filter_eq("id_correo", in.idCorreo)
                    .returning({"*"})
                    .update({
                        PgCell("nombre",                  in.nombre.c_str()),
                        PgCell("apellido",                in.apellido.c_str()),
                        PgCell("correo_electronico",      in.correoElectronico.c_str()),
                        PgCell("tipo_recepcion_mensajes", in.tipoRecepcionMensajes),
                        PgCell("activa",                  in.activa)
                    });
                if (!res.success || res.returningRows.empty()) {
                    throw std::runtime_error("RuntimeError: no se encontro el id_correo a actualizar");
                }
                return mapearCorreo(res.returningRows[0]);
            }

            auto res = DbRuntime::Orm::Correos.create({
                PgCell("nombre",                  in.nombre.c_str()),
                PgCell("apellido",                in.apellido.c_str()),
                PgCell("correo_electronico",      in.correoElectronico.c_str()),
                PgCell("tipo_recepcion_mensajes", in.tipoRecepcionMensajes),
                PgCell("ultimo_estado_envio",     EstadoEnvioCorreo::PENDIENTE),
                PgCell("activa",                  in.activa)
            }, {"*"});
            if (!res.success || res.returningRows.empty()) {
                throw std::runtime_error("RuntimeError: no se pudo crear el correo");
            }
            return mapearCorreo(res.returningRows[0]);
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar destinatario de Correos por ID (ADMIN)
    inline auto eliminarCorreoPorId = llamadoSemantico.exactamenteUnaVez<IdRegistroConfigDTO, void>(
        "eliminarCorreoPorId",
        [](const IdRegistroConfigDTO& dto) {
            DbRuntime::Orm::Correos.filter_eq("id_correo", dto.id).remove();
        },
        RolUsuario::ADMIN
    );

    // 8. Listar ServidoresNtp (NO_ADMIN)
    inline auto listarServidoresNtp = llamadoSemantico.alMenosUnaVez<void, ListaServidoresNtpDTO>(
        "listarServidoresNtp",
        []() -> ListaServidoresNtpDTO {
            auto filas = DbRuntime::Orm::ServidoresNtp.objects()
                .order_by("prioridad")
                .limit(16)
                .all_as<ServidorNtpItemDTO>(&mapearNtp);
            ListaServidoresNtpDTO out{};
            for (const auto& s : filas) {
                if (!out.agregar(s)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 9. Crear o actualizar ServidorNtp (ADMIN)
    inline auto guardarServidorNtp = llamadoSemantico.exactamenteUnaVez<CrearOActualizarServidorNtpDTO, ServidorNtpItemDTO>(
        "guardarServidorNtp",
        [](const CrearOActualizarServidorNtpDTO& in) -> ServidorNtpItemDTO {
            if (in.server.vacio()) {
                throw std::invalid_argument("ValueError: el host del servidor NTP no puede estar vacio");
            }
            int16_t prio = (in.prioridad < 1) ? 1 : ((in.prioridad > 10) ? 10 : in.prioridad);
            const int64_t ahora = static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());

            if (in.idServer > 0) {
                auto res = DbRuntime::Orm::ServidoresNtp.filter_eq("id_server", in.idServer)
                    .returning({"*"})
                    .update({
                        PgCell("server",             in.server.c_str()),
                        PgCell("prioridad",          prio),
                        PgCell("activa",             in.activa),
                        PgCell("fecha_modificacion", ahora)
                    });
                if (!res.success || res.returningRows.empty()) {
                    throw std::runtime_error("RuntimeError: no se pudo actualizar el servidor NTP");
                }
                return mapearNtp(res.returningRows[0]);
            }

            auto res = DbRuntime::Orm::ServidoresNtp.create({
                PgCell("server",             in.server.c_str()),
                PgCell("prioridad",          prio),
                PgCell("activa",             in.activa),
                PgCell("fecha_creacion",     ahora),
                PgCell("fecha_modificacion", ahora)
            }, {"*"});
            if (!res.success || res.returningRows.empty()) {
                throw std::runtime_error("RuntimeError: no se pudo crear el servidor NTP");
            }
            return mapearNtp(res.returningRows[0]);
        },
        RolUsuario::ADMIN
    );

    // 10. Eliminar ServidorNtp por ID (ADMIN)
    inline auto eliminarServidorNtpPorId = llamadoSemantico.exactamenteUnaVez<IdRegistroConfigDTO, void>(
        "eliminarServidorNtpPorId",
        [](const IdRegistroConfigDTO& dto) {
            DbRuntime::Orm::ServidoresNtp.filter_eq("id_server", dto.id).remove();
        },
        RolUsuario::ADMIN
    );

} // namespace RPConfiguracion

#endif