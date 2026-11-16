#pragma once
#include <Arduino.h>
#include <vector>
#include "../interfaces/IRuntimeDbEngine.hpp"
#include "DjangoQuerySet.hpp"

namespace DbRuntime::Orm {

    using DbRuntime::Engine::OnConflictClause;
    using DbRuntime::Engine::MergeWhenClause;
    using DbRuntime::Standard::ConflictAction;
    using DbNotRuntime::Standard::ReferentialAction;

    // Gestor de modelo estilo Django (Model.objects) que centraliza operaciones DML y creación de QuerySets.
    class ModelManager {
    private:
        String tableName;

    public:
        // Vincula el manager con el nombre físico de la tabla declarada en el DDL.
        explicit ModelManager(const char* table) : tableName(table) {}

        // Sobrecarga para inicializar el manager desde un objeto String.
        explicit ModelManager(const String& table) : tableName(table) {}

        // Devuelve un QuerySet limpio apuntando a la tabla administrada (Model.objects).
        QuerySet objects() const {
            return QuerySet(tableName);
        }

        // Inicia un QuerySet aplicando inmediatamente una condición de filtrado WHERE.
        QuerySet filter(const String& col, FilterOp op, const String& valA = "", const String& valB = "") const {
            return QuerySet(tableName).filter(col, op, valA, valB);
        }

        // Inicia un QuerySet con una condición de igualdad exacta (aprovecha índices PK / UNIQUE / BTREE).
        QuerySet filter_eq(const String& col, const String& val) const {
            return QuerySet(tableName).filter_eq(col, val);
        }

        // Sobrecarga de igualdad exacta para claves enteras (ej: Sensor.filter_eq("id_sensor", 1)).
        QuerySet filter_eq(const String& col, int32_t val) const {
            return QuerySet(tableName).filter_eq(col, val);
        }

        // Devuelve todas las filas de la tabla equivaliendo a Model.objects.all().
        std::vector<PgTuple> all() const {
            return QuerySet(tableName).all();
        }

        // Inserta un nuevo registro validando tipos y constraints del DDL (equivale a Model.objects.create(...)).
        DmlExecutionResult create(const std::vector<PgCell>& cells,
                                  const std::vector<String>& returningCols = {}) const {
            PgTuple tuple;
            tuple.reserve(cells.size());
            for (const auto& c : cells) {
                tuple.set(c.columnName, c.rawText, c.type);
            }
            return ModuloDML::getInstance().insertRow(tableName, std::move(tuple), {}, returningCols);
        }

        // Ejecuta un INSERT ... ON CONFLICT recibiendo directamente la estructura OnConflictClause y columnas RETURNING.
        DmlExecutionResult upsert(const std::vector<PgCell>& insertCells,
                                  const OnConflictClause& conflict,
                                  const std::vector<String>& returningCols = {}) const {
            PgTuple tuple;
            tuple.reserve(insertCells.size());
            for (const auto& c : insertCells) {
                tuple.set(c.columnName, c.rawText, c.type);
            }
            return ModuloDML::getInstance().insertRow(tableName, std::move(tuple), conflict, returningCols);
        }

        // Sobrecarga ergonómica de INSERT ... ON CONFLICT indicando ConflictAction y celdas de actualización.
        DmlExecutionResult upsert(const std::vector<PgCell>& insertCells,
                                  ConflictAction action,
                                  const std::vector<PgCell>& updateCells = {},
                                  const std::vector<String>& returningCols = {}) const {
            OnConflictClause conflict;
            conflict.action            = action;
            conflict.updateAssignments = updateCells;
            return upsert(insertCells, conflict, returningCols);
        }

        // Ejecuta la sentencia estándar MERGE INTO sincronizando un lote de tuplas origen contra esta tabla.
        DmlExecutionResult merge(const std::vector<PgTuple>& sourceRows,
                                 const String& targetKeyCol,
                                 const String& sourceKeyCol,
                                 const std::vector<MergeWhenClause>& rules) const {
            return ModuloDML::getInstance().mergeRows(tableName, sourceRows, targetKeyCol, sourceKeyCol, rules);
        }

        // Vacía la tabla de forma instantánea verificando políticas referenciales (TRUNCATE TABLE).
        DmlExecutionResult truncate(ReferentialAction behavior = ReferentialAction::RESTRICT) const {
            return ModuloDML::getInstance().truncateTable(tableName, behavior);
        }
    };

    // Implementación concreta de la interfaz IRuntimeDbEngine para desacoplamiento y control del motor en runtime.
    class RuntimeDbEngine final : public Interfaces::IRuntimeDbEngine {
    public:
        // Obtiene la instancia única de la fachada RuntimeDbEngine.
        static RuntimeDbEngine& getInstance() {
            static RuntimeDbEngine instance;
            return instance;
        }

        // Inicializa los caminos de acceso e índices leyendo el catálogo generado por database_not_runtime.
        bool bindToPhysicalCatalog() override {
            return Storage::RuntimeStorageAndIndexManager::getInstance().synchronizeWithDdlCatalog();
        }

        // Reconstruye los índices físicos en RAM/SD para una tabla específica.
        bool rebuildTableIndexes(const String& tableName) override {
            return Storage::RuntimeStorageAndIndexManager::getInstance().rebuildIndexesForTable(tableName);
        }

        // Devuelve la telemetría de costo de la última operación DQL ejecutada.
        const Telemetry::OperationCostMetrics& lastOperationCost() const override {
            return ModuloDQL::getInstance().getLastCost();
        }

        // Verifica de forma ultrarrápida (usando índice si existe) si un valor existe en una columna.
        bool existsRecord(const String& tableName, const String& columnName, const String& value) override {
            return QuerySet(tableName).filter_eq(columnName, value).exists();
        }

        // Devuelve un ModelManager asociado al nombre físico de una tabla del catálogo.
        ModelManager model(const String& tableName) const {
            return ModelManager(tableName);
        }

        // Ejecuta TRUNCATE TABLE sobre la tabla indicada con opción CASCADE.
        DmlExecutionResult truncate(const String& tableName, bool cascade = false) {
            return ModuloDML::getInstance().truncateTable(
                tableName, cascade ? ReferentialAction::CASCADE : ReferentialAction::RESTRICT
            );
        }

        // Devuelve la telemetría de costo de la última operación DQL.
        const Telemetry::OperationCostMetrics& lastDqlCost() const {
            return ModuloDQL::getInstance().getLastCost();
        }

        // Devuelve la telemetría de costo de la última operación DML.
        const Telemetry::OperationCostMetrics& lastDmlCost() const {
            return ModuloDML::getInstance().getLastCost();
        }
    };

    // Instancia global de la fachada del motor runtime consumida por main.cpp y la suite de tests.
    inline RuntimeDbEngine& runtimeDb = RuntimeDbEngine::getInstance();

    // Instancias globales de Managers para las 12 tablas del modelo físico (escritura directa como en Django).
    inline const ModelManager ConfiguracionRed("ConfiguracionRed");
    inline const ModelManager ConfiguracionConectividad("ConfiguracionConectividad");
    inline const ModelManager ConfiguracionSmtp("ConfiguracionSmtp");
    inline const ModelManager Calibracion("Calibracion");
    inline const ModelManager ServidoresNtp("ServidoresNtp");
    inline const ModelManager ConfiguracionSistema("ConfiguracionSistema");
    inline const ModelManager Usuario("Usuario");
    inline const ModelManager Sensor("Sensor");
    inline const ModelManager Senso("Senso");
    inline const ModelManager AuditoriaSensores("AuditoriaSensores");
    inline const ModelManager AuditoriaSistema("AuditoriaSistema");
    inline const ModelManager Correos("Correos");
    inline const ModelManager CorreoEnvio("CorreoEnvio");
    inline const ModelManager AuditoriaConexion("AuditoriaConexion");
    inline const ModelManager AuditoriaAuth("AuditoriaAuth");
    inline const ModelManager Sesiones("Sesiones");

} // namespace DbRuntime::Orm