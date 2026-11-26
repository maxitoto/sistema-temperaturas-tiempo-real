#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

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
        static constexpr const char* TABLA        = "AuditoriaAuth";
        static constexpr const char* HASH_GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static void alimentarHash64(uint64_t& h1, uint64_t& h2, const char* str) {
            if (!str) return;
            while (*str) {
                const uint8_t b = static_cast<uint8_t>(*str++);
                h1 = (h1 ^ b) * 1099511628211ULL;
                h2 = (h2 ^ (b + 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL;
            }
        }

        static void calcularHmacEncadenado(const AuditoriaAuthDTO& dto, char hexOut[65]) {
            uint64_t h1 = 0xCBF29CE484222325ULL;
            uint64_t h2 = 0x84222325CBF29CE4ULL;

            char numBuf[36]{};
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

            snprintf(hexOut, 65, "%016llX%016llX%016llX%016llX",
                     static_cast<unsigned long long>(h1),
                     static_cast<unsigned long long>(h2),
                     static_cast<unsigned long long>(h3),
                     static_cast<unsigned long long>(h4));
        }

    public:
        static AuditoriaAuthDTO mapear(const PgRowView& r) {
            AuditoriaAuthDTO dto{};
            dto.idAuditoriaAuth  = r.getInt32("id_auditoria_auth");
            dto.seq              = r.getUInt64("seq");
            dto.idUsuario        = r.getInt32("id_usuario", 0);
            dto.usuarioIntentado = r.getCStr("usuario_intentado");
            dto.accion           = r.getCStr("accion");
            dto.tablaAfectada    = r.getCStr("tabla_afectada", "NINGUNA");
            dto.valorPrevio      = r.getCStr("valor_previo");
            dto.valorNuevo       = r.getCStr("valor_nuevo");
            dto.detalle          = r.getCStr("detalle");
            dto.medioConexion    = r.getEnum<MedioConexion>("medio_conexion");
            dto.origenRemoto     = r.getCStr("origen_remoto", "LOCAL");
            dto.resultado        = r.getEnum<ResultadoAuditoria>("resultado");
            dto.criticidad       = r.getEnum<CriticidadEvento>("criticidad");
            dto.bootId           = r.getUInt32("boot_id");
            dto.uptimeS          = r.getUInt32("uptime_s");
            dto.fuenteTiempo     = r.getEnum<FuenteTiempo>("fuente_tiempo");
            dto.fechaCreacion    = r.getInt64("fecha_creacion");
            dto.hashPrevio       = r.getCStr("hash_previo");
            dto.hashRegistro     = r.getCStr("hash_registro");
            return dto;
        }

        static AuditoriaAuthDTO registrarEvento(AuditoriaAuthDTO entrada) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            bool hayAnterior = false;
            st.scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                entrada.seq        = r.getUInt64("seq") + 1;
                entrada.hashPrevio = r.getCStr("hash_registro", HASH_GENESIS);
                hayAnterior = true;
                return false;
            });
            if (!hayAnterior) {
                entrada.seq        = 1;
                entrada.hashPrevio = HASH_GENESIS;
            }

            if (entrada.fechaCreacion <= 0) {
                entrada.fechaCreacion = epochActual();
            }
            if (entrada.uptimeS == 0) {
                entrada.uptimeS = static_cast<uint32_t>(millis() / 1000UL);
            }

            char firmaHex[65]{};
            calcularHmacEncadenado(entrada, firmaHex);
            entrada.hashRegistro = firmaHex;

            PgRowBuffer row(schema);
            row.setUInt64("seq",               entrada.seq);
            if (entrada.idUsuario > 0) row.setInt32("id_usuario", entrada.idUsuario);
            else                       row.setNull("id_usuario");
            row.setText("usuario_intentado",   entrada.usuarioIntentado.c_str());
            row.setText("accion",              entrada.accion.c_str());
            row.setText("tabla_afectada",      entrada.tablaAfectada.c_str());
            row.setText("valor_previo",        entrada.valorPrevio.c_str());
            row.setText("valor_nuevo",         entrada.valorNuevo.c_str());
            row.setText("detalle",             entrada.detalle.c_str());
            row.setEnum("medio_conexion",      entrada.medioConexion);
            row.setText("origen_remoto",       entrada.origenRemoto.c_str());
            row.setEnum("resultado",           entrada.resultado);
            row.setEnum("criticidad",          entrada.criticidad);
            row.setUInt32("boot_id",           entrada.bootId);
            row.setUInt32("uptime_s",          entrada.uptimeS);
            row.setEnum("fuente_tiempo",       entrada.fuenteTiempo);
            row.setInt64("fecha_creacion",     entrada.fechaCreacion);
            row.setText("hash_previo",         entrada.hashPrevio.c_str());
            row.setText("hash_registro",       entrada.hashRegistro.c_str());

            if (!st.appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar AuditoriaAuth");
            }
            return mapear(row.view());
        }

        // Recorrido inverso cero-heap (usado por RPAuditoria::listarAuditoriaAuth para no alocar 16 * 552 B)
        template <typename CallbackFn>
        static void forEachReciente(uint16_t limite, uint16_t offset, CallbackFn fn) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0) return;

            uint32_t matched = 0;
            uint16_t emitted = 0;
            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (matched >= offset) {
                    if (!fn(r)) return false;
                    if (++emitted >= limite) return false;
                }
                matched++;
                return true;
            });
        }

        static bool obtenerPorId(int32_t idAuditoriaAuth, AuditoriaAuthDTO& outDto) {
            asegurarEsquemaTabla(TABLA);
            bool ok = false;
            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_auditoria_auth") == idAuditoriaAuth) {
                    outDto = mapear(r);
                    ok = true;
                    return false;
                }
                return true;
            });
            return ok;
        }

        static AuditoriaAuthDTO exigirPorId(int32_t idAuditoriaAuth) {
            AuditoriaAuthDTO dto{};
            if (!obtenerPorId(idAuditoriaAuth, dto)) {
                throw std::runtime_error("DoesNotExist: id_auditoria_auth no existe");
            }
            return dto;
        }

        static std::vector<AuditoriaAuthDTO> listarRecientes(uint16_t limite = 16, uint16_t offset = 0) {
            if (limite == 0 || limite > 16) limite = 16;
            std::vector<AuditoriaAuthDTO> out;
            out.reserve(limite);
            forEachReciente(limite, offset, [&](const PgRowView& r) -> bool {
                out.push_back(mapear(r));
                return true;
            });
            return out;
        }

        static std::vector<AuditoriaAuthDTO> listarPorUsuario(int32_t idUsuario, uint16_t limite = 16) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0 || limite > 16) limite = 16;
            std::vector<AuditoriaAuthDTO> out;

            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_usuario") == idUsuario) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                return true;
            });
            return out;
        }

        // Verifica la cadena HMAC en streaming (1 solo DTO en stack, 0 heap) y devuelve cuántos bloques revisó
        static bool verificarIntegridadReciente(uint16_t cantidad = 32, uint16_t* verificadosOut = nullptr) {
            asegurarEsquemaTabla(TABLA);
            bool integra = true;
            bool tienePrevio = false;
            uint16_t revisados = 0;
            char prevHashRegistro[65]{};

            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                AuditoriaAuthDTO actual = mapear(r);
                char esperado[65]{};
                calcularHmacEncadenado(actual, esperado);

                const size_t lenGuardado = strlen(actual.hashRegistro.c_str());
                if (lenGuardado < 63 || strncasecmp(esperado, actual.hashRegistro.c_str(), lenGuardado) != 0) {
                    integra = false;
                    return false;
                }
                if (tienePrevio && strncasecmp(actual.hashPrevio.c_str(), prevHashRegistro, lenGuardado) != 0) {
                    integra = false;
                    return false;
                }

                strncpy(prevHashRegistro, actual.hashRegistro.c_str(), sizeof(prevHashRegistro) - 1);
                prevHashRegistro[sizeof(prevHashRegistro) - 1] = '\0';
                tienePrevio = true;
                revisados++;
                return revisados < cantidad;
            });

            if (verificadosOut) *verificadosOut = revisados;
            return integra;
        }

        static void eliminarPorId(int32_t idAuditoriaAuth) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_auditoria_auth") == idAuditoriaAuth; },
                true
            );
        }

        static uint16_t purgarAnterioresA(int64_t timestampLimite) {
            asegurarEsquemaTabla(TABLA);
            return PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt64("fecha_creacion") < timestampLimite; },
                true
            );
        }
    };

} // namespace DbRuntime::Repositories