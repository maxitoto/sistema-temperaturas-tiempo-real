#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct ConfiguracionSistemaDTO {
        PgInt    idConfiguracionSistema = 1;
        PgInt    zonaHorariaMin         = -180;
        PgPosInt retencionSensoDias     = 365;
        PgPosInt retencionEventosDias   = 730;
        PgPosInt retencionAuditoriaDias = 1095;
        PgPosInt sdLibreMinimoKb        = 51200;
        PgPosInt correoCooldownS        = 900;
        PgBool   activa                 = true;
        PgBigInt fechaModificacion      = 0;
    };

    class RepoConfiguracionSistema {
    private:
        static constexpr const char* TABLA = "ConfiguracionSistema";

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

    public:
        static ConfiguracionSistemaDTO mapear(const PgRowView& r) {
            ConfiguracionSistemaDTO dto{};
            dto.idConfiguracionSistema = r.getInt32("id_configuracion_sistema", 1);
            dto.zonaHorariaMin         = r.getInt32("zona_horaria_min", -180);
            dto.retencionSensoDias     = r.getUInt32("retencion_senso_dias", 365);
            dto.retencionEventosDias   = r.getUInt32("retencion_eventos_dias", 730);
            dto.retencionAuditoriaDias = r.getUInt32("retencion_auditoria_dias", 1095);
            dto.sdLibreMinimoKb        = r.getUInt32("sd_libre_minimo_kb", 51200);
            dto.correoCooldownS        = r.getUInt32("correo_cooldown_s", 900);
            dto.activa                 = r.getBool("activa", true);
            dto.fechaModificacion      = r.getInt64("fecha_modificacion", 0);
            return dto;
        }

        static bool obtenerOInicializar(ConfiguracionSistemaDTO& outDto) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            bool encontrado = false;
            st.scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_configuracion_sistema") == 1) {
                    outDto = mapear(r);
                    encontrado = true;
                    return false;
                }
                return true;
            });
            if (encontrado) return true;

            PgRowBuffer row(schema);
            row.setInt32("id_configuracion_sistema", 1);
            row.setInt32("zona_horaria_min",         -180);
            row.setUInt32("retencion_senso_dias",     365);
            row.setUInt32("retencion_eventos_dias",   730);
            row.setUInt32("retencion_auditoria_dias", 1095);
            row.setUInt32("sd_libre_minimo_kb",       51200);
            row.setUInt32("correo_cooldown_s",        900);
            row.setBool("activa",                     true);
            row.setInt64("fecha_modificacion",        epochActual());

            if (!st.appendRecord(TABLA, row)) {
                return false;
            }
            outDto = mapear(row.view());
            return true;
        }

        static ConfiguracionSistemaDTO exigirConfiguracion() {
            ConfiguracionSistemaDTO dto{};
            if (!obtenerOInicializar(dto)) {
                throw std::runtime_error("IOError: no se pudo leer ConfiguracionSistema");
            }
            return dto;
        }

        static ConfiguracionSistemaDTO actualizarTodo(const ConfiguracionSistemaDTO& dto) {
            ConfiguracionSistemaDTO actual{};
            obtenerOInicializar(actual);

            const int64_t ahora = epochActual();
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [](const PgRowView& r) -> bool { return r.getInt32("id_configuracion_sistema") == 1; },
                [&](PgRowBuffer& row) {
                    row.setInt32("zona_horaria_min",          dto.zonaHorariaMin);
                    row.setUInt32("retencion_senso_dias",     dto.retencionSensoDias);
                    row.setUInt32("retencion_eventos_dias",   dto.retencionEventosDias);
                    row.setUInt32("retencion_auditoria_dias", dto.retencionAuditoriaDias);
                    row.setUInt32("sd_libre_minimo_kb",       dto.sdLibreMinimoKb);
                    row.setUInt32("correo_cooldown_s",        dto.correoCooldownS);
                    row.setBool("activa",                     dto.activa);
                    row.setInt64("fecha_modificacion",        ahora);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("IOError: no se pudo actualizar ConfiguracionSistema");
            }
            return mapear(updated.view());
        }
    };

} // namespace DbRuntime::Repositories