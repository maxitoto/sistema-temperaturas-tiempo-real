#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::Sensor;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbRuntime::Standard::ConflictAction;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

    struct SensorDTO {
        PgAuto      idSensor          = 0;
        PgChar<25>  mac               = "";
        PgChar<32>  nombre            = "";
        PgChar<32>  ubicacion         = "";
        PgChar<128> descripcion       = "";
        PgFloat     umbralSup         = 126.0f;
        PgFloat     umbralInf         = -55.0f;
        PgFloat     errorSup          = 0.0f;
        PgFloat     errorInf          = 0.0f;
        PgFloat     offset            = 0.0f;
        PgBool      activa            = true;
        PgDateTime  fechaCreacion     = "";
        PgDateTime  fechaModificacion = "";
    };

    struct EstadisticasSensoresDTO {
        PgInt totalSensores     = 0;
        PgInt sensoresActivos   = 0;
        PgInt sensoresInactivos = 0;
    };

    class RepoSensor {
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
        static SensorDTO mapear(const PgTuple& r) {
            SensorDTO s{};
            r["id_sensor"]          >> s.idSensor;
            r["mac"]                >> s.mac;
            r["nombre"]             >> s.nombre;
            r["ubicacion"]          >> s.ubicacion;
            r["descripcion"]        >> s.descripcion;
            r["umbral_sup"]         >> s.umbralSup;
            r["umbral_inf"]         >> s.umbralInf;
            r["error_sup"]          >> s.errorSup;
            r["error_inf"]          >> s.errorInf;
            r["offset"]             >> s.offset;
            r["activa"]             >> s.activa;
            r["fecha_creacion"]     >> s.fechaCreacion;
            r["fecha_modificacion"] >> s.fechaModificacion;
            return s;
        }

        static bool obtenerPorId(int32_t idSensor, SensorDTO& outDto) {
            asegurarCatalogo();
            PgTuple fila;
            if (!Sensor.filter_eq("id_sensor", idSensor).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
        }

        static SensorDTO exigirPorId(int32_t idSensor) {
            SensorDTO s{};
            if (!obtenerPorId(idSensor, s)) {
                throw std::runtime_error("LookupError: el id_sensor solicitado no existe");
            }
            return s;
        }

        static bool obtenerPorMac(const char* macHex, SensorDTO& outDto) {
            asegurarCatalogo();
            if (macHex == nullptr || macHex[0] == '\0') return false;

            PgTuple fila;
            if (!Sensor.filter_eq("mac", String(macHex)).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
        }

        static SensorDTO exigirPorMac(const char* macHex) {
            SensorDTO s{};
            if (!obtenerPorMac(macHex, s)) {
                throw std::runtime_error("LookupError: no existe un sensor registrado con esa MAC");
            }
            return s;
        }

        static SensorDTO registrarOAsegurarPorMac(
            const char* macHex,
            const char* nombreInicial = "Sensor DS18B20",
            const char* ubicacionInicial = "Sin asignar"
        ) {
            SensorDTO existente{};
            if (obtenerPorMac(macHex, existente)) {
                return existente;
            }

            SensorDTO nuevo{};
            nuevo.mac         = macHex ? macHex : "";
            nuevo.nombre      = nombreInicial ? nombreInicial : "Sensor DS18B20";
            nuevo.ubicacion   = ubicacionInicial ? ubicacionInicial : "Sin asignar";
            nuevo.descripcion = "";
            nuevo.umbralSup   = 126.0f;
            nuevo.umbralInf   = -55.0f;
            nuevo.errorSup    = 0.0f;
            nuevo.errorInf    = 0.0f;
            nuevo.offset      = 0.0f;
            nuevo.activa      = true;
            return crearSensor(nuevo);
        }

        static SensorDTO crearSensor(const SensorDTO& entrada) {
            asegurarCatalogo();

            if (entrada.mac.vacio()) {
                throw std::invalid_argument("ValueError: la direccion MAC del sensor es obligatoria");
            }
            if (entrada.umbralInf > entrada.umbralSup) {
                throw std::invalid_argument("ValueError: umbral_inf no puede ser mayor que umbral_sup");
            }

            auto res = Sensor.create(
                {
                    PgCell("mac",         entrada.mac.c_str()),
                    PgCell("nombre",      entrada.nombre.c_str()),
                    PgCell("ubicacion",   entrada.ubicacion.c_str()),
                    PgCell("descripcion", entrada.descripcion.c_str()),
                    PgCell("umbral_sup",  entrada.umbralSup),
                    PgCell("umbral_inf",  entrada.umbralInf),
                    PgCell("error_sup",   entrada.errorSup),
                    PgCell("error_inf",   entrada.errorInf),
                    PgCell("offset",      entrada.offset),
                    PgCell("activa",      entrada.activa)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static SensorDTO actualizarMetadatosPorMac(
            const char* macHex,
            const char* nombre,
            const char* ubicacion,
            const char* descripcion,
            bool activa
        ) {
            asegurarCatalogo();

            auto res = Sensor.filter_eq("mac", String(macHex ? macHex : ""))
                .returning({"*"})
                .update({
                    PgCell("nombre",      nombre ? nombre : ""),
                    PgCell("ubicacion",   ubicacion ? ubicacion : ""),
                    PgCell("descripcion", descripcion ? descripcion : ""),
                    PgCell("activa",      activa)
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el sensor a actualizar no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static SensorDTO actualizarCalibracionYUmbralesPorMac(
            const char* macHex,
            float umbralSup,
            float umbralInf,
            float errorSup,
            float errorInf,
            float offset
        ) {
            asegurarCatalogo();

            if (umbralInf > umbralSup) {
                throw std::invalid_argument("ValueError: umbral_inf no puede ser mayor que umbral_sup");
            }

            auto res = Sensor.filter_eq("mac", String(macHex ? macHex : ""))
                .returning({"*"})
                .update({
                    PgCell("umbral_sup", umbralSup),
                    PgCell("umbral_inf", umbralInf),
                    PgCell("error_sup",  errorSup),
                    PgCell("error_inf",  errorInf),
                    PgCell("offset",     offset)
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el sensor a calibrar no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static SensorDTO cambiarEstadoActivoPorMac(const char* macHex, bool activa) {
            asegurarCatalogo();

            auto res = Sensor.filter_eq("mac", String(macHex ? macHex : ""))
                .returning({"*"})
                .update({ PgCell("activa", activa) });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el sensor solicitado no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static void eliminarPorMac(const char* macHex) {
            asegurarCatalogo();

            auto res = Sensor.filter_eq("mac", String(macHex ? macHex : "")).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.affectedRows == 0) {
                throw std::runtime_error("LookupError: el sensor a eliminar no existe");
            }
        }

        static std::vector<SensorDTO> listarSensoresPaginados(
            uint16_t limite = 32,
            uint16_t offset = 0,
            bool soloActivos = false
        ) {
            asegurarCatalogo();

            auto q = Sensor.objects();
            if (soloActivos) {
                q.filter_eq("activa", "TRUE");
            }
            return q.order_by("id_sensor")
                    .limit(static_cast<int32_t>(limite))
                    .offset(static_cast<int32_t>(offset))
                    .all_as<SensorDTO>(&RepoSensor::mapear);
        }

        static std::vector<SensorDTO> listarPorUbicacion(const char* ubicacion, uint16_t limite = 32) {
            asegurarCatalogo();

            return Sensor.filter_eq("ubicacion", String(ubicacion ? ubicacion : ""))
                         .order_by("id_sensor")
                         .limit(static_cast<int32_t>(limite))
                         .all_as<SensorDTO>(&RepoSensor::mapear);
        }

        static EstadisticasSensoresDTO obtenerEstadisticasGlobales() {
            asegurarCatalogo();

            DbRuntime::Engine::PredicateCondition filtroActivos;
            filtroActivos.columnOrExpr = "activa";
            filtroActivos.op           = FilterOp::EQ;
            filtroActivos.valueA       = "TRUE";

            DbRuntime::Engine::PredicateCondition filtroInactivos;
            filtroInactivos.columnOrExpr = "activa";
            filtroInactivos.op           = FilterOp::EQ;
            filtroInactivos.valueA       = "FALSE";

            PgTuple r;
            EstadisticasSensoresDTO out{};
            if (Sensor.objects()
                    .annotate("total",     AggregateFunc::COUNT_ALL)
                    .annotate_filtered("activos",   AggregateFunc::COUNT_COL, "id_sensor", filtroActivos)
                    .annotate_filtered("inactivos", AggregateFunc::COUNT_COL, "id_sensor", filtroInactivos)
                    .first(r)) {
                r["total"]     >> out.totalSensores;
                r["activos"]   >> out.sensoresActivos;
                r["inactivos"] >> out.sensoresInactivos;
            }
            return out;
        }
    };

}