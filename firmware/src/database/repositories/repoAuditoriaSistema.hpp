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

    struct AuditoriaSistemaDTO {
        PgAuto            idAuditoriaSistema = 0;
        CriticidadEvento  criticidad         = CriticidadEvento::NULA;
        TipoEvento        tipoEventoSistema  = TipoEvento::INFORMATIVO;
        EstadoBus         estadoBus          = EstadoBus::OK;
        PgPosInt          espacioSdLibreKb   = 0;
        PgPosInt          heapLibreBytes     = 0;
        PgChar<24>        resetReason        = "";
        PgChar<16>        versionFirmware    = "1.0.0";
        PgChar<96>        detalle            = "";
        PgBool            internet           = false;
        EstadoEnvioCorreo estadoReporte      = EstadoEnvioCorreo::PENDIENTE;
        PgPosInt          bootId             = 0;
        PgPosInt          uptimeS            = 0;
        FuenteTiempo      fuenteTiempo       = FuenteTiempo::SIN_SINCRONIZAR;
        PgBool            activa             = true;
        PgDateTime        fechaCreacion      = "";
    };

    class RepoAuditoriaSistema {
    private:
        static constexpr const char* TABLA = "AuditoriaSistema";

    public:
        static AuditoriaSistemaDTO mapear(const PgRowView& r) {
            AuditoriaSistemaDTO dto{};
            dto.idAuditoriaSistema = r.getInt32("id_auditoria_sistema");
            dto.criticidad         = r.getEnum<CriticidadEvento>("criticidad");
            dto.tipoEventoSistema  = r.getEnum<TipoEvento>("tipo_evento_sistema");
            dto.estadoBus          = r.getEnum<EstadoBus>("estado_bus");
            dto.espacioSdLibreKb   = r.getUInt32("espacio_sd_libre_kb");
            dto.heapLibreBytes     = r.getUInt32("heap_libre_bytes");
            dto.resetReason        = r.getCStr("reset_reason");
            dto.versionFirmware    = r.getCStr("version_firmware", "1.0.0");
            dto.detalle            = r.getCStr("detalle");
            dto.internet           = r.getBool("internet", false);
            dto.estadoReporte      = r.getEnum<EstadoEnvioCorreo>("estado_reporte");
            dto.bootId             = r.getUInt32("boot_id");
            dto.uptimeS            = r.getUInt32("uptime_s");
            dto.fuenteTiempo       = r.getEnum<FuenteTiempo>("fuente_tiempo");
            dto.activa             = r.getBool("activa", true);
            dto.fechaCreacion      = r.getCStr("fecha_creacion");
            return dto;
        }

        static AuditoriaSistemaDTO registrarEvento(const AuditoriaSistemaDTO& entrada) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            const uint32_t heapActual   = entrada.heapLibreBytes > 0 ? entrada.heapLibreBytes : ESP.getFreeHeap();
            const uint32_t uptimeActual = entrada.uptimeS > 0 ? entrada.uptimeS : static_cast<uint32_t>(millis() / 1000UL);

            PgRowBuffer row(schema);
            row.setEnum("criticidad",           entrada.criticidad);
            row.setEnum("tipo_evento_sistema",  entrada.tipoEventoSistema);
            row.setEnum("estado_bus",           entrada.estadoBus);
            row.setUInt32("espacio_sd_libre_kb",entrada.espacioSdLibreKb);
            row.setUInt32("heap_libre_bytes",   heapActual);
            row.setText("reset_reason",         entrada.resetReason.c_str());
            row.setText("version_firmware",     entrada.versionFirmware.vacio() ? "1.0.0" : entrada.versionFirmware.c_str());
            row.setText("detalle",              entrada.detalle.c_str());
            row.setBool("internet",             entrada.internet);
            row.setEnum("estado_reporte",       entrada.estadoReporte);
            row.setUInt32("boot_id",            entrada.bootId);
            row.setUInt32("uptime_s",           uptimeActual);
            row.setEnum("fuente_tiempo",        entrada.fuenteTiempo);
            row.setBool("activa",               true);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar AuditoriaSistema");
            }
            return mapear(row.view());
        }

        // Recorrido inverso cero-heap (usado por RPAuditoria para llenar ListaAuditoriaSistemaDTO sin vector)
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

        static std::vector<AuditoriaSistemaDTO> listarRecientes(uint16_t limite = 6, uint16_t offset = 0) {
            if (limite == 0 || limite > 16) limite = 16;
            std::vector<AuditoriaSistemaDTO> out;
            out.reserve(limite);
            forEachReciente(limite, offset, [&](const PgRowView& r) -> bool {
                out.push_back(mapear(r));
                return true;
            });
            return out;
        }

        static std::vector<AuditoriaSistemaDTO> listarPendientesReporte(uint16_t limite = 16) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0 || limite > 16) limite = 16;
            std::vector<AuditoriaSistemaDTO> out;

            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getEnum<EstadoEnvioCorreo>("estado_reporte") == EstadoEnvioCorreo::PENDIENTE) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                return true;
            });
            return out;
        }

        static AuditoriaSistemaDTO actualizarEstadoReporte(int32_t idAuditoriaSistema, EstadoEnvioCorreo nuevoEstado) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_auditoria_sistema") == idAuditoriaSistema; },
                [&](PgRowBuffer& row) { row.setEnum("estado_reporte", nuevoEstado); },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: id_auditoria_sistema no existe");
            }
            return mapear(updated.view());
        }

        static void eliminarPorId(int32_t idAuditoriaSistema) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_auditoria_sistema") == idAuditoriaSistema; },
                true
            );
        }

        static uint16_t purgarAnterioresA(const char* fechaHoraIsoLimite) {
            asegurarEsquemaTabla(TABLA);
            if (!fechaHoraIsoLimite || fechaHoraIsoLimite[0] == '\0') return 0;

            return PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool {
                    const char* fc = r.getCStr("fecha_creacion");
                    return fc[0] != '\0' && strcmp(fc, fechaHoraIsoLimite) < 0;
                },
                true
            );
        }
    };

} // namespace DbRuntime::Repositories