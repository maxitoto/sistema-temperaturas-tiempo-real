#ifndef RP_AUDITORIA_H
#define RP_AUDITORIA_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../database/repositories/repoAuditoriaSistema.hpp"
#include "../../../../database/repositories/repoAuditoriaAuth.hpp"
#include "../../../../rtos/RelojSistema.hpp"

struct FiltroPaginacionAuditoriaDTO {
    PgPosSmallInt limiteRegistros;
    PgPosSmallInt offsetRegistros;
};

struct IdAuditoriaDTO {
    PgInt id;
};

struct PurgarAuditoriaTextoFechaDTO {
    PgDateTime fechaHoraLimite;
};

struct PurgarAuditoriaEpochDTO {
    PgEpoch timestampLimite;
};

struct ResumenAuditoriaAuthDTO {
    PgAuto             idAuditoriaAuth;
    PgPosInt           seq;
    PgInt              idUsuario;
    PgChar<20>         usuarioIntentado;
    PgChar<24>         accion;
    MedioConexion      medioConexion;
    PgChar<16>         origenRemoto;
    ResultadoAuditoria resultado;
    CriticidadEvento   criticidad;
    PgEpoch            fechaCreacion;
};

struct ResultadoIntegridadAuthDTO {
    PgBool        cadenaIntegra;
    PgPosSmallInt registrosVerificados;
};

// 6 * 188 B = 1128 B (< 1536 B de MTU RPC)
using ListaAuditoriaSistemaDTO = PgList<DbRuntime::Repositories::AuditoriaSistemaDTO, 6>;
// 16 * 78 B = 1248 B (< 1536 B de MTU RPC)
using ListaResumenAuditoriaAuthDTO = PgList<ResumenAuditoriaAuthDTO, 16>;

namespace RPAuditoria {

    using DbRuntime::Repositories::RepoAuditoriaSistema;
    using DbRuntime::Repositories::AuditoriaSistemaDTO;
    using DbRuntime::Repositories::RepoAuditoriaAuth;
    using DbRuntime::Repositories::AuditoriaAuthDTO;

    // 1. Listar AuditoriaSistema paginada (NO_ADMIN)
    inline auto listarAuditoriaSistema = llamadoSemantico.alMenosUnaVez<FiltroPaginacionAuditoriaDTO, ListaAuditoriaSistemaDTO>(
        "listarAuditoriaSistema",
        [](const FiltroPaginacionAuditoriaDTO& f) -> ListaAuditoriaSistemaDTO {
            const uint16_t lim = (f.limiteRegistros == 0 || f.limiteRegistros > 6) ? 6 : f.limiteRegistros;
            auto filas = RepoAuditoriaSistema::listarRecientes(lim, f.offsetRegistros);
            ListaAuditoriaSistemaDTO out{};
            for (const auto& r : filas) {
                if (!out.agregar(r)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Eliminar registro de AuditoriaSistema por ID (ADMIN)
    inline auto eliminarAuditoriaSistemaPorId = llamadoSemantico.exactamenteUnaVez<IdAuditoriaDTO, void>(
        "eliminarAuditoriaSistemaPorId",
        [](const IdAuditoriaDTO& dto) {
            DbRuntime::Orm::AuditoriaSistema.filter_eq("id_auditoria_sistema", dto.id).remove();
        },
        RolUsuario::ADMIN
    );

    // 3. Purgar AuditoriaSistema anterior a una fecha ISO (ADMIN)
    inline auto purgarAuditoriaSistema = llamadoSemantico.exactamenteUnaVez<PurgarAuditoriaTextoFechaDTO, PgPosSmallInt>(
        "purgarAuditoriaSistema",
        [](const PurgarAuditoriaTextoFechaDTO& dto) -> PgPosSmallInt {
            return RepoAuditoriaSistema::purgarAnterioresA(dto.fechaHoraLimite.c_str());
        },
        RolUsuario::ADMIN
    );

    // 4. Listar AuditoriaAuth paginada (NO_ADMIN / ADMIN)
    inline auto listarAuditoriaAuth = llamadoSemantico.alMenosUnaVez<FiltroPaginacionAuditoriaDTO, ListaResumenAuditoriaAuthDTO>(
        "listarAuditoriaAuth",
        [](const FiltroPaginacionAuditoriaDTO& f) -> ListaResumenAuditoriaAuthDTO {
            const uint16_t lim = (f.limiteRegistros == 0 || f.limiteRegistros > 16) ? 16 : f.limiteRegistros;
            auto filas = RepoAuditoriaAuth::listarRecientes(lim, f.offsetRegistros);
            ListaResumenAuditoriaAuthDTO out{};
            for (const auto& a : filas) {
                ResumenAuditoriaAuthDTO item{};
                item.idAuditoriaAuth  = a.idAuditoriaAuth;
                item.seq              = static_cast<uint32_t>(a.seq);
                item.idUsuario        = a.idUsuario;
                item.usuarioIntentado = a.usuarioIntentado;
                item.accion           = a.accion;
                item.medioConexion    = a.medioConexion;
                item.origenRemoto     = a.origenRemoto;
                item.resultado        = a.resultado;
                item.criticidad       = a.criticidad;
                item.fechaCreacion    = static_cast<uint32_t>(a.fechaCreacion);
                if (!out.agregar(item)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 5. Obtener detalle completo de un registro de AuditoriaAuth incluyendo firmas HMAC de 64 chars (ADMIN)
    inline auto obtenerAuditoriaAuthPorId = llamadoSemantico.alMenosUnaVez<IdAuditoriaDTO, AuditoriaAuthDTO>(
        "obtenerAuditoriaAuthPorId",
        [](const IdAuditoriaDTO& dto) -> AuditoriaAuthDTO {
            DbRuntime::Storage::PgTuple fila;
            if (!DbRuntime::Orm::AuditoriaAuth.filter_eq("id_auditoria_auth", dto.id).first(fila)) {
                throw std::runtime_error("DoesNotExist: id_auditoria_auth no existe");
            }
            return RepoAuditoriaAuth::mapear(fila);
        },
        RolUsuario::ADMIN
    );

    // 6. Verificar la cadena criptográfica HMAC de AuditoriaAuth (ADMIN)
    inline auto verificarIntegridadAuditoriaAuth = llamadoSemantico.alMenosUnaVez<void, ResultadoIntegridadAuthDTO>(
        "verificarIntegridadAuditoriaAuth",
        []() -> ResultadoIntegridadAuthDTO {
            ResultadoIntegridadAuthDTO out{};
            out.registrosVerificados = 32;
            out.cadenaIntegra        = RepoAuditoriaAuth::verificarIntegridadReciente(32);
            return out;
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar registro de AuditoriaAuth por ID (ADMIN)
    inline auto eliminarAuditoriaAuthPorId = llamadoSemantico.exactamenteUnaVez<IdAuditoriaDTO, void>(
        "eliminarAuditoriaAuthPorId",
        [](const IdAuditoriaDTO& dto) {
            DbRuntime::Orm::AuditoriaAuth.filter_eq("id_auditoria_auth", dto.id).remove();
        },
        RolUsuario::ADMIN
    );

    // 8. Purgar AuditoriaAuth anterior a un timestamp Unix (ADMIN)
    inline auto purgarAuditoriaAuth = llamadoSemantico.exactamenteUnaVez<PurgarAuditoriaEpochDTO, PgPosSmallInt>(
        "purgarAuditoriaAuth",
        [](const PurgarAuditoriaEpochDTO& dto) -> PgPosSmallInt {
            return RepoAuditoriaAuth::purgarAnterioresA(static_cast<int64_t>(dto.timestampLimite));
        },
        RolUsuario::ADMIN
    );

} // namespace RPAuditoria

#endif