#pragma once
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_cpu.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace DbRuntime::Telemetry {

    // Identifica el camino de acceso físico elegido por el optimizador de consultas.
    enum class AccessScanType : uint8_t {
        NONE = 0,
        SEQ_SCAN,           // Escaneo secuencial completo del archivo .tbl
        INDEX_SCAN,         // Salto directo por índice secundario declarado en el DDL
        PRIMARY_KEY_LOOKUP, // Salto O(log N) por índice de clave primaria
        UNIQUE_INDEX_LOOKUP // Salto O(log N) por restricción UNIQUE del DDL
    };

    // Estructura plana (POD) que almacena las métricas físicas de costo de una operación runtime.
    struct OperationCostMetrics {
        const char* operationKind = "NONE";
        char targetTable[32]      = {0};
        char indexUsed[40]        = {0};
        AccessScanType scanType   = AccessScanType::NONE;

        uint32_t cpuCycles        = 0;
        uint32_t elapsedMicros    = 0;
        uint32_t stackFreeMinBytes= 0;
        uint32_t stackUsedEstimate= 0;
        int32_t  heapDeltaBytes   = 0;
        uint16_t rowsExamined     = 0;
        uint16_t rowsProduced     = 0;

        // Convierte el tipo de escaneo a su nombre estándar en EXPLAIN ANALYZE de PostgreSQL.
        const char* scanTypeName() const {
            switch (scanType) {
                case AccessScanType::SEQ_SCAN:            return "Seq Scan";
                case AccessScanType::INDEX_SCAN:          return "Index Scan";
                case AccessScanType::PRIMARY_KEY_LOOKUP:  return "Index Scan (PK)";
                case AccessScanType::UNIQUE_INDEX_LOOKUP: return "Index Scan (UNIQUE)";
                default:                                  return "Metadata/No-Scan";
            }
        }

        // Formatea el reporte como la salida de EXPLAIN (ANALYZE, BUFFERS, COSTS) de PostgreSQL.
        String toExplainAnalyzeString() const {
            String out = "QUERY PLAN: " + String(scanTypeName()) + " on " + String(targetTable);
            if (indexUsed[0] != '\0') {
                out += " using index \"" + String(indexUsed) + "\"";
            }
            out += " (rows_examined=" + String(rowsExamined) + " rows_out=" + String(rowsProduced) + ")\n";
            out += "  Execution Time: " + String(elapsedMicros) + " us | CPU Cycles: " + String(cpuCycles)
                + " | Stack Free Min: " + String(stackFreeMinBytes) + " B | Stack Peak Probe: "
                + String(stackUsedEstimate) + " B | Heap Delta: " + String(heapDeltaBytes) + " B";
            return out;
        }
    };

    // Sonda RAII sin heap que captura contadores de hardware al iniciar y finalizar una operación.
    class ScopedCostProbe {
    private:
        OperationCostMetrics& target;
        uint32_t startCycles;
        int64_t  startMicros;
        uint32_t startHeap;
        uint32_t startStackHwm;
        bool finished = false;

    public:
        // Inicia los contadores hardware del Xtensa LX6 y planta un canario local en la pila para medir presión.
        ScopedCostProbe(OperationCostMetrics& metricsOut, const char* opKind, const String& table)
            : target(metricsOut) {
            target = OperationCostMetrics{};
            target.operationKind = opKind;
            strncpy(target.targetTable, table.c_str(), sizeof(target.targetTable) - 1);

            startStackHwm = uxTaskGetStackHighWaterMark(nullptr);
            startHeap     = ESP.getFreeHeap();
            startMicros   = esp_timer_get_time();
            startCycles   = esp_cpu_get_cycle_count();
        }
        // Cierra la medición y graba los resultados; se puede llamar antes de copiar el costo.
        void finish() {
            if (finished) return;
            finished = true;
            uint32_t endCycles   = esp_cpu_get_cycle_count();
            int64_t  endMicros   = esp_timer_get_time();
            uint32_t endHeap     = ESP.getFreeHeap();
            uint32_t endStackHwm = uxTaskGetStackHighWaterMark(nullptr);

            target.cpuCycles         = endCycles - startCycles;
            target.elapsedMicros     = static_cast<uint32_t>(endMicros - startMicros);
            target.stackFreeMinBytes = endStackHwm;
            target.stackUsedEstimate = (startStackHwm > endStackHwm) ? (startStackHwm - endStackHwm) : 0;
            target.heapDeltaBytes    = static_cast<int32_t>(startHeap) - static_cast<int32_t>(endHeap);
        }

        ~ScopedCostProbe() { finish(); }
    };

} // namespace DbRuntime::Telemetry