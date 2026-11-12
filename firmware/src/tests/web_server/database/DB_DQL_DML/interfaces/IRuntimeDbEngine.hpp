#pragma once
#include <Arduino.h>
#include "../telemetry/RuntimeCostProfiler.hpp"
#include "../storage/RuntimeTypedTuple.hpp"

namespace DbRuntime::Interfaces {

    // Define el contrato de bajo acoplamiento para cualquier motor de ejecución DML/DQL presente o futuro.
    class IRuntimeDbEngine {
    public:
        virtual ~IRuntimeDbEngine() = default;

        // Inicializa los caminos de acceso e índices leyendo el catálogo generado por database_not_runtime.
        virtual bool bindToPhysicalCatalog() = 0;

        // Reconstruye los índices físicos en RAM/SD para una tabla específica o para todo el esquema.
        virtual bool rebuildTableIndexes(const String& tableName) = 0;

        // Devuelve la telemetría de costo (Stack, CPU cycles, Tiempo us, Plan de acceso) de la última operación.
        virtual const Telemetry::OperationCostMetrics& lastOperationCost() const = 0;

        //Verifica de forma ultrarrápida (usando índice si existe) si un valor existe en una columna.
        virtual bool existsRecord(const String& tableName, const String& columnName, const String& value) = 0;
    };

} // namespace DbRuntime::Interfaces