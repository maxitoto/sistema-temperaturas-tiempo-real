#pragma once
#include <Arduino.h>
#include <time.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::AuditoriaAuth;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct AuditoriaAuthDTO {
        PgAuto             idAuditoriaAuth  = 0;
        PgPosBigInt        seq              = 0;
        PgInt              idUsuario        = 0;
        PgChar<20>         usuarioIntentado = "";
        PgChar<32>         accion           = "";
        PgChar<32>         tablaAfectada    = "NINGUNA";
        PgChar<64>         valorPrevio      = "";
        PgChar<64>         valorNuevo       = "";
        PgChar<120>        detalle          = "";
        MedioConexion      medioConexion    = MedioConexion::INTERNO;
        PgChar<46>         origenRemoto     = "LOCAL";
        ResultadoAuditoria resultado        = ResultadoAuditoria::EXITO;
        CriticidadEvento   criticidad       = CriticidadEvento::BAJA;
        PgPosInt           bootId           = 0;
        PgPosInt           uptimeS          = 0;
        FuenteTiempo       fuenteTiempo     = FuenteTiempo::SIN_SINCRONIZAR;
        PgBigInt           fechaCreacion    = 0;
        PgChar<64>         hashPrevio       = "";
        PgChar<64>         hashRegistro     = "";
    };

    class RepoAuditoriaAuth {
    private:
        static constexpr const char* HASH_GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";

       static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

        static void alimentarHash64(uint64_t& h1, uint64_t& h2, const char* str) {
            if (!str) return;
            while (*str) {
                const uint8_t b = static_cast<uint8_t>(*str++);
                h1 = (h1 ^ b) * 1099511628211ULL;
                h2 = (h2 ^ (b + 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL;
            }
        }

        static String calcularHmacEncadenado(const AuditoriaAuthDTO& dto) {
            uint64_t h1 = 0xCBF29CE484222325ULL;
            uint64_t h2 = 0x84222325CBF29CE4ULL;

            char numBuf[32]{};
            snprintf(numBuf, sizeof(numBuf), "%llu|%d|%lld",
                     static_cast<unsigned long long>(dto.seq),
                     static_cast<int>(dto.idUsuario),
                     static_cast<long long>(dto.fechaCreacion));

            alimentarHash64(h1, h2, dto.hashPrevio.c_str());
            alimentarHash64(h1, h2, numBuf);
            alimentarHash64(h1, h2, dto.usuarioIntentado.c_str());
            alimentarHash64(h1, h2, dto.accion.c_str());
            alimentarHash64(h1, h2, dto.tablaAfectada.c_str());
            alimentarHash64(h1, h2, dto.valorPrevio.c_str());
            alimentarHash64(h1, h2, dto.valorNuevo.c_str());
            alimentarHash64(h1, h2, dto.detalle.c_str());

            uint64_t h3 = (h1 ^ (h2 >> 29)) * 0x94D049BB133111EBULL;
            uint64_t h4 = (h2 ^ (h1 >> 31)) * 0x2545F4914F6CDD1DULL;

            char hex[65]{};
            snprintf(hex, sizeof(hex), "%016llX%016llX%016llX%016llX",
                     static_cast<unsigned long long>(h1),
                     static_cast<unsigned long long>(h2),
                     static_cast<unsigned long long>(h3),
                     static_cast<unsigned long long>(h4));
            return String(hex);
        }

    public:
        static AuditoriaAuthDTO mapear(const PgTuple& r) {
            AuditoriaAuthDTO dto{};
            r["id_auditoria_auth"] >> dto.idAuditoriaAuth;
            r["seq"]               >> dto.seq;
            r["id_usuario"]        >> dto.idUsuario;
            r["usuario_intentado"] >> dto.usuarioIntentado;
            r["accion"]            >> dto.accion;
            r["tabla_afectada"]    >> dto.tablaAfectada;
            r["valor_previo"]      >> dto.valorPrevio;
            r["valor_nuevo"]       >> dto.valorNuevo;
            r["detalle"]           >> dto.detalle;
            r["medio_conexion"]    >> dto.medioConexion;
            r["origen_remoto"]     >> dto.origenRemoto;
            r["resultado"]         >> dto.resultado;
            r["criticidad"]        >> dto.criticidad;
            r["boot_id"]           >> dto.bootId;
            r["uptime_s"]          >> dto.uptimeS;
            r["fuente_tiempo"]     >> dto.fuenteTiempo;
            r["fecha_creacion"]    >> dto.fechaCreacion;
            r["hash_previo"]       >> dto.hashPrevio;
            r["hash_registro"]     >> dto.hashRegistro;
            return dto;
        }

        static AuditoriaAuthDTO registrarEvento(AuditoriaAuthDTO entrada) {
            asegurarCatalogo();

            PgTuple ultimaFila;
            if (AuditoriaAuth.objects().order_by("-id_auditoria_auth").first(ultimaFila)) {
                AuditoriaAuthDTO ant = mapear(ultimaFila);
                entrada.seq        = ant.seq + 1;
                entrada.hashPrevio = ant.hashRegistro;
            } else {
                entrada.seq        = 1;
                entrada.hashPrevio = HASH_GENESIS;
            }

            if (entrada.fechaCreacion <= 0) {
                entrada.fechaCreacion = epochActual();
            }
            if (entrada.uptimeS == 0) {
                entrada.uptimeS = static_cast<uint32_t>(millis() / 1000UL);
            }

            const String firma = calcularHmacEncadenado(entrada);
            entrada.hashRegistro = firma.c_str();

            std::vector<PgCell> celdas = {
                PgCell("seq",               entrada.seq),
                PgCell("usuario_intentado", entrada.usuarioIntentado.c_str()),
                PgCell("accion",            entrada.accion.c_str()),
                PgCell("tabla_afectada",    entrada.tablaAfectada.c_str()),
                PgCell("valor_previo",      entrada.valorPrevio.c_str()),
                PgCell("valor_nuevo",       entrada.valorNuevo.c_str()),
                PgCell("detalle",           entrada.detalle.c_str()),
                PgCell("medio_conexion",    entrada.medioConexion),
                PgCell("origen_remoto",     entrada.origenRemoto.c_str()),
                PgCell("resultado",         entrada.resultado),
                PgCell("criticidad",        entrada.criticidad),
                PgCell("boot_id",           entrada.bootId),
                PgCell("uptime_s",          entrada.uptimeS),
                PgCell("fuente_tiempo",     entrada.fuenteTiempo),
                PgCell("fecha_creacion",    entrada.fechaCreacion),
                PgCell("hash_previo",       entrada.hashPrevio.c_str()),
                PgCell("hash_registro",     entrada.hashRegistro.c_str())
            };

            if (entrada.idUsuario > 0) {
                celdas.push_back(PgCell("id_usuario", entrada.idUsuario));
            }

            auto res = AuditoriaAuth.create(celdas, {"*"});
            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static std::vector<AuditoriaAuthDTO> listarRecientes(uint16_t limite = 32, uint16_t offset = 0) {
            asegurarCatalogo();

            return AuditoriaAuth.objects()
                                .order_by("-id_auditoria_auth")
                                .limit(static_cast<int32_t>(limite))
                                .offset(static_cast<int32_t>(offset))
                                .all_as<AuditoriaAuthDTO>(&RepoAuditoriaAuth::mapear);
        }

        static std::vector<AuditoriaAuthDTO> listarPorUsuario(int32_t idUsuario, uint16_t limite = 32) {
            asegurarCatalogo();

            return AuditoriaAuth.filter_eq("id_usuario", idUsuario)
                                .order_by("-fecha_creacion")
                                .limit(static_cast<int32_t>(limite))
                                .all_as<AuditoriaAuthDTO>(&RepoAuditoriaAuth::mapear);
        }

        static bool verificarIntegridadReciente(uint16_t cantidad = 32) {
            asegurarCatalogo();

            auto filas = AuditoriaAuth.objects()
                                      .order_by("seq")
                                      .limit(static_cast<int32_t>(cantidad))
                                      .all_as<AuditoriaAuthDTO>(&RepoAuditoriaAuth::mapear);

            for (size_t i = 0; i < filas.size(); ++i) {
                const String esperado = calcularHmacEncadenado(filas[i]);
                if (!esperado.equalsIgnoreCase(filas[i].hashRegistro.c_str())) {
                    return false;
                }
                if (i > 0 && strcmp(filas[i].hashPrevio.c_str(), filas[i - 1].hashRegistro.c_str()) != 0) {
                    return false;
                }
            }
            return true;
        }
        static uint16_t purgarAnterioresA(int64_t timestampLimite) {
            asegurarCatalogo();

            char bufLimite[24]{};
            snprintf(bufLimite, sizeof(bufLimite), "%lld", static_cast<long long>(timestampLimite));

            auto res = AuditoriaAuth.filter("fecha_creacion", FilterOp::LT, String(bufLimite)).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            return res.affectedRows;
        }
    };

}