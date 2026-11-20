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
// 16 * 80 B = 1280 B (< 1536 B de MTU RPC)
using ListaResumenAuditoriaAuthDTO = PgList<ResumenAuditoriaAuthDTO, 16>;

namespace RPAuditoria {

    using DbRuntime::Repositories::RepoAuditoriaSistema;
    using DbRuntime::Repositories::AuditoriaSistemaDTO;
    using DbRuntime::Repositories::RepoAuditoriaAuth;
    using DbRuntime::Repositories::AuditoriaAuthDTO;
    using DbRuntime::Storage::PgRowView;

    // 1. Listar AuditoriaSistema paginada (NO_ADMIN) — 0 bytes de heap
    inline auto listarAuditoriaSistema = llamadoSemantico.alMenosUnaVez<FiltroPaginacionAuditoriaDTO, ListaAuditoriaSistemaDTO>(
        "listarAuditoriaSistema",
        [](const FiltroPaginacionAuditoriaDTO& f) -> ListaAuditoriaSistemaDTO {
            const uint16_t lim = (f.limiteRegistros == 0 || f.limiteRegistros > 6) ? 6 : f.limiteRegistros;
            ListaAuditoriaSistemaDTO out{};
            RepoAuditoriaSistema::forEachReciente(lim, f.offsetRegistros, [&](const PgRowView& r) -> bool {
                return out.agregar(RepoAuditoriaSistema::mapear(r));
            });
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Eliminar registro de AuditoriaSistema por ID (ADMIN)
    inline auto eliminarAuditoriaSistemaPorId = llamadoSemantico.exactamenteUnaVez<IdAuditoriaDTO, void>(
        "eliminarAuditoriaSistemaPorId",
        [](const IdAuditoriaDTO& dto) {
            RepoAuditoriaSistema::eliminarPorId(dto.id);
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

    // 4. Listar AuditoriaAuth paginada (NO_ADMIN / ADMIN) — 0 bytes de heap, mapeo directo a Resumen
    inline auto listarAuditoriaAuth = llamadoSemantico.alMenosUnaVez<FiltroPaginacionAuditoriaDTO, ListaResumenAuditoriaAuthDTO>(
        "listarAuditoriaAuth",
        [](const FiltroPaginacionAuditoriaDTO& f) -> ListaResumenAuditoriaAuthDTO {
            const uint16_t lim = (f.limiteRegistros == 0 || f.limiteRegistros > 16) ? 16 : f.limiteRegistros;
            ListaResumenAuditoriaAuthDTO out{};
            RepoAuditoriaAuth::forEachReciente(lim, f.offsetRegistros, [&](const PgRowView& r) -> bool {
                ResumenAuditoriaAuthDTO item{};
                item.idAuditoriaAuth  = r.getInt32("id_auditoria_auth");
                item.seq              = static_cast<uint32_t>(r.getUInt64("seq"));
                item.idUsuario        = r.getInt32("id_usuario", 0);
                item.usuarioIntentado = r.getCStr("usuario_intentado");
                item.accion           = r.getCStr("accion");
                item.medioConexion    = r.getEnum<MedioConexion>("medio_conexion");
                item.origenRemoto     = r.getCStr("origen_remoto", "LOCAL");
                item.resultado        = r.getEnum<ResultadoAuditoria>("resultado");
                item.criticidad       = r.getEnum<CriticidadEvento>("criticidad");
                item.fechaCreacion    = static_cast<uint32_t>(r.getInt64("fecha_creacion"));
                return out.agregar(item);
            });
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 5. Obtener detalle completo de un registro de AuditoriaAuth incluyendo firmas HMAC (ADMIN)
    inline auto obtenerAuditoriaAuthPorId = llamadoSemantico.alMenosUnaVez<IdAuditoriaDTO, AuditoriaAuthDTO>(
        "obtenerAuditoriaAuthPorId",
        [](const IdAuditoriaDTO& dto) -> AuditoriaAuthDTO {
            return RepoAuditoriaAuth::exigirPorId(dto.id);
        },
        RolUsuario::ADMIN
    );

    // 6. Verificar la cadena criptográfica HMAC de AuditoriaAuth (ADMIN)
    inline auto verificarIntegridadAuditoriaAuth = llamadoSemantico.alMenosUnaVez<void, ResultadoIntegridadAuthDTO>(
        "verificarIntegridadAuditoriaAuth",
        []() -> ResultadoIntegridadAuthDTO {
            ResultadoIntegridadAuthDTO out{};
            uint16_t verificados = 0;
            out.cadenaIntegra        = RepoAuditoriaAuth::verificarIntegridadReciente(32, &verificados);
            out.registrosVerificados = verificados;
            return out;
        },
        RolUsuario::ADMIN
    );

    // 7. Eliminar registro de AuditoriaAuth por ID (ADMIN)
    inline auto eliminarAuditoriaAuthPorId = llamadoSemantico.exactamenteUnaVez<IdAuditoriaDTO, void>(
        "eliminarAuditoriaAuthPorId",
        [](const IdAuditoriaDTO& dto) {
            RepoAuditoriaAuth::eliminarPorId(dto.id);
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