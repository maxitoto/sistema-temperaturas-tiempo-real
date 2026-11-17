#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::AuditoriaSensores;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct AuditoriaSensorDTO {
        PgAuto            idAuditoriaSensor  = 0;
        PgInt             idSensor           = 0;
        PgInt             idSenso            = 0;
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
        PgInt totalEventos        = 0;
        PgInt eventosCriticos     = 0;
        PgInt reportesPendientes  = 0;
    };

    class RepoAuditoriaSensores {
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
        static AuditoriaSensorDTO mapear(const PgTuple& r) {
            AuditoriaSensorDTO dto{};
            r["id_auditoria_sensor"]  >> dto.idAuditoriaSensor;
            r["id_sensor"]            >> dto.idSensor;
            r["id_senso"]             >> dto.idSenso;
            r["estado_previo_sensor"] >> dto.estadoPrevioSensor;
            r["estado_nuevo_sensor"]  >> dto.estadoNuevoSensor;
            r["estado_bus"]           >> dto.estadoBus;
            r["tipo_evento_sensor"]   >> dto.tipoEventoSensor;
            r["criticidad_sensor"]    >> dto.criticidadSensor;
            r["estado_reporte"]       >> dto.estadoReporte;
            r["activa"]               >> dto.activa;
            r["fecha_creacion"]       >> dto.fechaCreacion;
            return dto;
        }

        static AuditoriaSensorDTO registrarEvento(
            int32_t idSensor,
            int32_t idSenso,
            EstadoSensor estadoPrevio,
            EstadoSensor estadoNuevo,
            EstadoBus estadoBus,
            TipoEvento tipoEvento,
            CriticidadEvento criticidad
        ) {
            asegurarCatalogo();

            auto res = AuditoriaSensores.create(
                {
                    PgCell("id_sensor",            idSensor),
                    PgCell("id_senso",             idSenso),
                    PgCell("estado_previo_sensor", estadoPrevio),
                    PgCell("estado_nuevo_sensor",  estadoNuevo),
                    PgCell("estado_bus",           estadoBus),
                    PgCell("tipo_evento_sensor",   tipoEvento),
                    PgCell("criticidad_sensor",    criticidad),
                    PgCell("estado_reporte",       EstadoEnvioCorreo::PENDIENTE),
                    PgCell("activa",               true)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static std::vector<AuditoriaSensorDTO> listarPorIdSensor(
            int32_t idSensor,
            uint16_t limite = 32,
            uint16_t offset = 0
        ) {
            asegurarCatalogo();

            return AuditoriaSensores.filter_eq("id_sensor", idSensor)
                                    .order_by("-id_auditoria_sensor")
                                    .limit(static_cast<int32_t>(limite))
                                    .offset(static_cast<int32_t>(offset))
                                    .all_as<AuditoriaSensorDTO>(&RepoAuditoriaSensores::mapear);
        }

        static std::vector<AuditoriaSensorDTO> listarPendientesReporte(uint16_t limite = 32) {
            asegurarCatalogo();

            return AuditoriaSensores.filter_eq("estado_reporte", "PENDIENTE")
                                    .order_by("id_auditoria_sensor")
                                    .limit(static_cast<int32_t>(limite))
                                    .all_as<AuditoriaSensorDTO>(&RepoAuditoriaSensores::mapear);
        }

        static AuditoriaSensorDTO actualizarEstadoReporte(int32_t idAuditoriaSensor, EstadoEnvioCorreo nuevoEstado) {
            asegurarCatalogo();

            auto res = AuditoriaSensores.filter_eq("id_auditoria_sensor", idAuditoriaSensor)
                .returning({"*"})
                .update({ PgCell("estado_reporte", nuevoEstado) });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static EstadisticasAuditoriaSensoresDTO obtenerEstadisticasGlobales() {
            asegurarCatalogo();

            DbRuntime::Engine::PredicateCondition filtroCriticos;
            filtroCriticos.columnOrExpr = "criticidad_sensor";
            filtroCriticos.op           = FilterOp::EQ;
            filtroCriticos.valueA       = "ALTA";

            DbRuntime::Engine::PredicateCondition filtroPendientes;
            filtroPendientes.columnOrExpr = "estado_reporte";
            filtroPendientes.op           = FilterOp::EQ;
            filtroPendientes.valueA       = "PENDIENTE";

            PgTuple r;
            EstadisticasAuditoriaSensoresDTO out{};
            if (AuditoriaSensores.objects()
                    .annotate("total",      AggregateFunc::COUNT_ALL)
                    .annotate_filtered("criticos",   AggregateFunc::COUNT_COL, "id_auditoria_sensor", filtroCriticos)
                    .annotate_filtered("pendientes", AggregateFunc::COUNT_COL, "id_auditoria_sensor", filtroPendientes)
                    .first(r)) {
                r["total"]      >> out.totalEventos;
                r["criticos"]   >> out.eventosCriticos;
                r["pendientes"] >> out.reportesPendientes;
            }
            return out;
        }
        
        static uint16_t purgarAnterioresA(const char* fechaHoraIsoLimite) {
            asegurarCatalogo();
            if (!fechaHoraIsoLimite || fechaHoraIsoLimite[0] == '\0') return 0;

            auto res = AuditoriaSensores.filter("fecha_creacion", FilterOp::LT, String(fechaHoraIsoLimite)).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            return res.affectedRows;
        }
    };
}