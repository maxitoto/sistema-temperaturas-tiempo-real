#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::AuditoriaSistema;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

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
        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

    public:
        static AuditoriaSistemaDTO mapear(const PgTuple& r) {
            AuditoriaSistemaDTO dto{};
            r["id_auditoria_sistema"] >> dto.idAuditoriaSistema;
            r["criticidad"]           >> dto.criticidad;
            r["tipo_evento_sistema"]  >> dto.tipoEventoSistema;
            r["estado_bus"]           >> dto.estadoBus;
            r["espacio_sd_libre_kb"]  >> dto.espacioSdLibreKb;
            r["heap_libre_bytes"]     >> dto.heapLibreBytes;
            r["reset_reason"]         >> dto.resetReason;
            r["version_firmware"]     >> dto.versionFirmware;
            r["detalle"]              >> dto.detalle;
            r["internet"]             >> dto.internet;
            r["estado_reporte"]       >> dto.estadoReporte;
            r["boot_id"]              >> dto.bootId;
            r["uptime_s"]             >> dto.uptimeS;
            r["fuente_tiempo"]        >> dto.fuenteTiempo;
            r["activa"]               >> dto.activa;
            r["fecha_creacion"]       >> dto.fechaCreacion;
            return dto;
        }

        static AuditoriaSistemaDTO registrarEvento(const AuditoriaSistemaDTO& entrada) {
            asegurarCatalogo();

            const uint32_t heapActual   = entrada.heapLibreBytes > 0 ? entrada.heapLibreBytes : ESP.getFreeHeap();
            const uint32_t uptimeActual = entrada.uptimeS > 0 ? entrada.uptimeS : static_cast<uint32_t>(millis() / 1000UL);

            auto res = AuditoriaSistema.create(
                {
                    PgCell("criticidad",          entrada.criticidad),
                    PgCell("tipo_evento_sistema", entrada.tipoEventoSistema),
                    PgCell("estado_bus",          entrada.estadoBus),
                    PgCell("espacio_sd_libre_kb", entrada.espacioSdLibreKb),
                    PgCell("heap_libre_bytes",    heapActual),
                    PgCell("reset_reason",        entrada.resetReason.c_str()),
                    PgCell("version_firmware",    entrada.versionFirmware.vacio() ? "1.0.0" : entrada.versionFirmware.c_str()),
                    PgCell("detalle",             entrada.detalle.c_str()),
                    PgCell("internet",            entrada.internet),
                    PgCell("estado_reporte",      entrada.estadoReporte),
                    PgCell("boot_id",             entrada.bootId),
                    PgCell("uptime_s",            uptimeActual),
                    PgCell("fuente_tiempo",       entrada.fuenteTiempo),
                    PgCell("activa",              true)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static std::vector<AuditoriaSistemaDTO> listarRecientes(uint16_t limite = 32, uint16_t offset = 0) {
            asegurarCatalogo();

            return AuditoriaSistema.objects()
                                   .order_by("-id_auditoria_sistema")
                                   .limit(static_cast<int32_t>(limite))
                                   .offset(static_cast<int32_t>(offset))
                                   .all_as<AuditoriaSistemaDTO>(&RepoAuditoriaSistema::mapear);
        }

        static std::vector<AuditoriaSistemaDTO> listarPendientesReporte(uint16_t limite = 32) {
            asegurarCatalogo();

            return AuditoriaSistema.filter_eq("estado_reporte", "PENDIENTE")
                                   .order_by("id_auditoria_sistema")
                                   .limit(static_cast<int32_t>(limite))
                                   .all_as<AuditoriaSistemaDTO>(&RepoAuditoriaSistema::mapear);
        }

        static AuditoriaSistemaDTO actualizarEstadoReporte(int32_t idAuditoriaSistema, EstadoEnvioCorreo nuevoEstado) {
            asegurarCatalogo();

            auto res = AuditoriaSistema.filter_eq("id_auditoria_sistema", idAuditoriaSistema)
                .returning({"*"})
                .update({ PgCell("estado_reporte", nuevoEstado) });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }
    };

}