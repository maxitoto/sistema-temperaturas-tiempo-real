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

    struct AuditoriaSensorDTO {
        PgAuto            idAuditoriaSensor  = 0;
        PgInt             idSensor           = 0;
        PgInt             idCenso            = 0;
        EstadoSensor      estadoPrevioSensor = EstadoSensor::OK;
        EstadoSensor      estadoNuevoSensor  = EstadoSensor::ALARMA_SUPERIOR;
        EstadoBus         estadoBus          = EstadoBus::OK;
        TipoEvento        tipoEventoSensor   = TipoEvento::ADVERTENCIA;
        CriticidadEvento  criticidadSensor   = CriticidadEvento::ALTA;
        EstadoEnvioCorreo estadoReporte      = EstadoEnvioCorreo::PENDIENTE;
        PgBool            activa             = true;
        PgDateTime        fechaCreacion      = "";
    };

    struct EstadisticasAuditoriaSensoresDTO {
        PgInt totalEventos       = 0;
        PgInt eventosCriticos    = 0;
        PgInt reportesPendientes = 0;
    };

    class RepoAuditoriaSensores {
    private:
        static constexpr const char* TABLA = "AuditoriaSensores";

    public:
        static AuditoriaSensorDTO mapear(const PgRowView& r) {
            AuditoriaSensorDTO dto{};
            dto.idAuditoriaSensor  = r.getInt32("id_auditoria_sensor");
            dto.idSensor           = r.getInt32("id_sensor");
            dto.idCenso            = r.getInt32("id_censo");
            dto.estadoPrevioSensor = r.getEnum<EstadoSensor>("estado_previo_sensor");
            dto.estadoNuevoSensor  = r.getEnum<EstadoSensor>("estado_nuevo_sensor");
            dto.estadoBus          = r.getEnum<EstadoBus>("estado_bus");
            dto.tipoEventoSensor   = r.getEnum<TipoEvento>("tipo_evento_sensor");
            dto.criticidadSensor   = r.getEnum<CriticidadEvento>("criticidad_sensor");
            dto.estadoReporte      = r.getEnum<EstadoEnvioCorreo>("estado_reporte");
            dto.activa             = r.getBool("activa", true);
            dto.fechaCreacion      = r.getCStr("fecha_creacion");
            return dto;
        }

        static AuditoriaSensorDTO registrarEvento(
            int32_t idSensor,
            int32_t idCenso,
            EstadoSensor estadoPrevio,
            EstadoSensor estadoNuevo,
            EstadoBus estadoBus,
            TipoEvento tipoEvento,
            CriticidadEvento criticidad
        ) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            PgRowBuffer row(schema);
            row.setInt32("id_sensor",           idSensor);
            row.setInt32("id_censo",            idCenso);
            row.setEnum("estado_previo_sensor", estadoPrevio);
            row.setEnum("estado_nuevo_sensor",  estadoNuevo);
            row.setEnum("estado_bus",           estadoBus);
            row.setEnum("tipo_evento_sensor",   tipoEvento);
            row.setEnum("criticidad_sensor",    criticidad);
            row.setEnum("estado_reporte",       EstadoEnvioCorreo::PENDIENTE);
            row.setBool("activa",               true);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar evento en AuditoriaSensores");
            }
            return mapear(row.view());
        }

        static std::vector<AuditoriaSensorDTO> listarPorIdSensor(
            int32_t idSensor,
            uint16_t limite = 32,
            uint16_t offset = 0
        ) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0 || limite > 32) limite = 32;

            std::vector<AuditoriaSensorDTO> out;
            out.reserve(limite);
            uint32_t matched = 0;

            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_sensor") != idSensor) return true;
                if (matched >= offset) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                matched++;
                return true;
            });
            return out;
        }

        static std::vector<AuditoriaSensorDTO> listarPendientesReporte(uint16_t limite = 32) {
            asegurarEsquemaTabla(TABLA);
            if (limite == 0 || limite > 32) limite = 32;

            std::vector<AuditoriaSensorDTO> out;
            PgBinaryStorage::getInstance().scanBackward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getEnum<EstadoEnvioCorreo>("estado_reporte") == EstadoEnvioCorreo::PENDIENTE) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                return true;
            });
            return out;
        }

        static AuditoriaSensorDTO actualizarEstadoReporte(int32_t idAuditoriaSensor, EstadoEnvioCorreo nuevoEstado) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_auditoria_sensor") == idAuditoriaSensor; },
                [&](PgRowBuffer& row) { row.setEnum("estado_reporte", nuevoEstado); },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: id_auditoria_sensor no existe");
            }
            return mapear(updated.view());
        }

        static EstadisticasAuditoriaSensoresDTO obtenerEstadisticasGlobales() {
            asegurarEsquemaTabla(TABLA);
            EstadisticasAuditoriaSensoresDTO out{};
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                out.totalEventos++;
                if (r.getEnum<CriticidadEvento>("criticidad_sensor") == CriticidadEvento::ALTA) {
                    out.eventosCriticos++;
                }
                if (r.getEnum<EstadoEnvioCorreo>("estado_reporte") == EstadoEnvioCorreo::PENDIENTE) {
                    out.reportesPendientes++;
                }
                return true;
            });
            return out;
        }

        static void eliminarPorId(int32_t idAuditoriaSensor) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_auditoria_sensor") == idAuditoriaSensor; },
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