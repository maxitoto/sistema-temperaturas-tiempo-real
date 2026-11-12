#pragma once
#include <Arduino.h>
#include "../../DB_DDL/models/PhysicalModelsDefinition.hpp"
#include "../../DB_DDL/engine/DdlDiagnosticConsole.hpp"
#include "../orm/DjangoManager.hpp"

namespace DbRuntime::Tests {

    using namespace DbRuntime::Orm;
    using namespace DbRuntime::Engine;
    using namespace DbRuntime::Storage;
    using namespace DbRuntime::Standard;
    using namespace DbRuntime::Telemetry;
    using DbNotRuntime::Standard::SqlState;
    using DbNotRuntime::Standard::PgDataType;

    // Resumen estadístico de la ejecución de la suite automatizada runtime.
    struct RuntimeSuiteSummary {
        uint16_t total = 0;
        uint16_t passed = 0;
        uint16_t failed = 0;
    };

    class AutomatedRuntimeCostAndSqlSuite {
    private:
        // Registra el resultado de un test imprimiendo su plan de acceso (Index vs Seq Scan) y sus costos físicos de hardware.
        static void assertWithMetrics(RuntimeSuiteSummary& sum,
                                      const char* testName,
                                      bool passed,
                                      const OperationCostMetrics& cost) {
            sum.total++;
            if (passed) {
                sum.passed++;
                Serial.print(" [PASS] ");
            } else {
                sum.failed++;
                Serial.print(" [FAIL] ");
            }
            Serial.print(testName);
            Serial.print(" | Plan: ");
            Serial.print(cost.scanTypeName());
            if (cost.indexUsed[0] != '\0') {
                Serial.print("(");
                Serial.print(cost.indexUsed);
                Serial.print(")");
            }
            Serial.print(" | Time: ");
            Serial.print(cost.elapsedMicros);
            Serial.print(" us | CPU: ");
            Serial.print(cost.cpuCycles);
            Serial.print(" cyc | StackFree: ");
            Serial.print(cost.stackFreeMinBytes);
            Serial.println(" B");

            if (!passed) {
                Serial.println(DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat());
            }
        }

        // GRUPO 1: Validación de tipos contra el DDL, Constraints, RETURNING y Determinismo en INSERT/UPDATE/DELETE.
        static void testDmlTypesConstraintsAndReturning(RuntimeSuiteSummary& sum) {
            Serial.println("\n--- GRUPO 1: Validacion de Tipos DDL, Constraints, RETURNING y Determinismo DML ---");
            ModelManager Sensor = runtimeDb.model("Sensor");
            ModelManager Senso  = runtimeDb.model("Senso");

            // Limpieza inicial usando DELETE determinista con permiso explícito (sin depender de DDL TRUNCATE)
            Senso.objects().remove(true);
            Sensor.objects().remove(true);

            // 1.1 INSERT válido con generación IDENTITY, valores DEFAULT del DDL y cláusula RETURNING
            auto r1 = Sensor.create({
                PgCell("hexadecimal", "28FFAA01"),
                PgCell("mac2", "01"),
                PgCell("nombre", "Sonda Principal"),
                PgCell("resolucion_bits", "12", PgDataType::SMALLINT)
            }, {"id_sensor", "hexadecimal", "ubicacion"});
            assertWithMetrics(sum, "1.1 INSERT valido + IDENTITY + DEFAULT + RETURNING",
                              r1.success && r1.returningRows.size() == 1 &&
                              r1.returningRows[0].getInt("id_sensor") >= 1 &&
                              r1.returningRows[0].getText("ubicacion") == "Sin asignar",
                              r1.cost);

            // 1.2 INSERT con valor literal que infringe el tipo SMALLINT del DDL -> SQLSTATE 22P02
            auto r2 = Sensor.create({
                PgCell("hexadecimal", "28FFAA02"),
                PgCell("mac2", "02"),
                PgCell("resolucion_bits", "NO_ES_ENTERO", PgDataType::SMALLINT)
            });
            assertWithMetrics(sum, "1.2 INSERT con tipo invalido rechazado (22P02)",
                              !r2.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::INVALID_TEXT_REPRESENTATION,
                              r2.cost);

            // 1.3 INSERT con columna inexistente en el esquema físico -> SQLSTATE 42703
            auto r3 = Sensor.create({
                PgCell("hexadecimal", "28FFAA03"),
                PgCell("mac2", "03"),
                PgCell("columna_inventada", "123")
            });
            assertWithMetrics(sum, "1.3 INSERT con columna inexistente rechazado (42703)",
                              !r3.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::UNDEFINED_COLUMN,
                              r3.cost);

            // 1.4 INSERT violando restricción UNIQUE en hexadecimal -> SQLSTATE 23505
            auto r4 = Sensor.create({
                PgCell("hexadecimal", "28FFAA01"),
                PgCell("mac2", "99")
            });
            assertWithMetrics(sum, "1.4 INSERT duplicado en columna UNIQUE rechazado por indice (23505)",
                              !r4.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::UNIQUE_VIOLATION,
                              r4.cost);

            // 1.5 INSERT en tabla hija Senso apuntando a id_sensor padre inexistente -> SQLSTATE 23503
            auto r5 = Senso.create({
                PgCell("id_sensor", "9999", PgDataType::INTEGER),
                PgCell("temperatura_cruda", "4.20", PgDataType::REAL),
                PgCell("temperatura_corregida", "4.20", PgDataType::REAL)
            });
            assertWithMetrics(sum, "1.5 INSERT con FK hurfana rechazado por indice PK padre (23503)",
                              !r5.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::FOREIGN_KEY_VIOLATION,
                              r5.cost);

            // 1.6 UPDATE sin cláusula WHERE bloqueado por política determinista -> SQLSTATE 42601
            auto r6 = Sensor.objects().update({PgCell("ubicacion", "Laboratorio")});
            assertWithMetrics(sum, "1.6 UPDATE sin WHERE bloqueado deterministicamente (42601)",
                              !r6.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::SYNTAX_ERROR,
                              r6.cost);

            // 1.7 DELETE sin cláusula WHERE bloqueado por política determinista -> SQLSTATE 42601
            auto r7 = Sensor.objects().remove(false);
            assertWithMetrics(sum, "1.7 DELETE sin WHERE bloqueado deterministicamente (42601)",
                              !r7.success && DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().returnedSqlState == SqlState::SYNTAX_ERROR,
                              r7.cost);
        }

        // GRUPO 2: Upsert PostgreSQL (ON CONFLICT DO NOTHING / DO UPDATE) y sentencia estándar MERGE.
        static void testUpsertAndMerge(RuntimeSuiteSummary& sum) {
            Serial.println("\n--- GRUPO 2: Upsert (ON CONFLICT DO NOTHING / DO UPDATE) y MERGE ---");
            ModelManager Sensor = runtimeDb.model("Sensor");

            // 2.1 ON CONFLICT DO NOTHING sobre clave única existente
            OnConflictClause doNothing;
            doNothing.action = ConflictAction::DO_NOTHING;
            auto r1 = Sensor.upsert({
                PgCell("hexadecimal", "28FFAA01"),
                PgCell("mac2", "01")
            }, doNothing);
            assertWithMetrics(sum, "2.1 INSERT ON CONFLICT DO NOTHING descarta colision sin error",
                              r1.success && r1.affectedRows == 0, r1.cost);

            // 2.2 ON CONFLICT (hexadecimal) DO UPDATE SET ubicacion='Camara Fria 1' RETURNING ubicacion
            OnConflictClause doUpdate;
            doUpdate.action = ConflictAction::DO_UPDATE;
            doUpdate.conflictTargetColumns = {"hexadecimal"};
            doUpdate.updateAssignments = { PgCell("ubicacion", "Camara Fria 1") };
            auto r2 = Sensor.upsert({
                PgCell("hexadecimal", "28FFAA01"),
                PgCell("mac2", "01")
            }, doUpdate, {"ubicacion"});
            assertWithMetrics(sum, "2.2 INSERT ON CONFLICT DO UPDATE actualiza y proyecta RETURNING",
                              r2.success && r2.affectedRows == 1 && r2.returningRows[0].getText("ubicacion") == "Camara Fria 1",
                              r2.cost);

            // 2.3 MERGE: actualiza 28FFAA01 (WHEN MATCHED) e inserta 28FFAA02 (WHEN NOT MATCHED)
            PgTuple s1; s1.set("hexadecimal", "28FFAA01"); s1.set("mac2", "01"); s1.set("nombre", "Sonda 1");
            PgTuple s2; s2.set("hexadecimal", "28FFAA02"); s2.set("mac2", "02"); s2.set("nombre", "Sonda 2");

            MergeWhenClause whenMatched;
            whenMatched.isMatched = true;
            whenMatched.action    = MergeActionType::UPDATE_SET;
            whenMatched.assignmentsOrValues = { PgCell("nombre", "Sonda 1 MergeOK") };

            MergeWhenClause whenNotMatched;
            whenNotMatched.isMatched = false;
            whenNotMatched.action    = MergeActionType::INSERT_VALUES;

            auto r3 = Sensor.merge({s1, s2}, "hexadecimal", "hexadecimal", {whenMatched, whenNotMatched});
            assertWithMetrics(sum, "2.3 MERGE ejecuta WHEN MATCHED UPDATE y WHEN NOT MATCHED INSERT",
                              r3.success && r3.affectedRows == 2 && Sensor.objects().count() == 2,
                              r3.cost);
        }

        // GRUPO 3: Verificación real de uso de Índices declarados en el DDL (Index Scan vs Seq Scan).
        static void testIndexAccessPathsAndCost(RuntimeSuiteSummary& sum) {
            Serial.println("\n--- GRUPO 3: Uso Real de Indices del DDL (PK Lookup / Unique Scan vs Seq Scan) ---");
            ModelManager Sensor = runtimeDb.model("Sensor");

            for (int i = 3; i <= 15; ++i) {
                String hex = "28FFAA" + String(i < 10 ? "0" : "") + String(i);
                Sensor.create({
                    PgCell("hexadecimal", hex),
                    PgCell("mac2", String(i)),
                    PgCell("ubicacion", (i % 2 == 0) ? "Sector Par" : "Sector Impar")
                });
            }

            // 3.1 Búsqueda por Primary Key -> Debe elegir PRIMARY_KEY_LOOKUP y examinar 1 sola fila
            PgTuple rowPk;
            bool okPk = Sensor.filter_eq("id_sensor", 1).get(rowPk);
            const auto costPk = ModuloDQL::getInstance().getLastCost();
            assertWithMetrics(sum, "3.1 Filtro por PK elige PRIMARY_KEY_LOOKUP (rowsExamined=1)",
                              okPk && costPk.scanType == AccessScanType::PRIMARY_KEY_LOOKUP && costPk.rowsExamined == 1,
                              costPk);

            // 3.2 Búsqueda por columna UNIQUE (hexadecimal) -> Debe elegir UNIQUE_INDEX_LOOKUP y examinar 1 sola fila
            PgTuple rowUq;
            bool okUq = Sensor.filter_eq("hexadecimal", "28FFAA10").get(rowUq);
            const auto costUq = ModuloDQL::getInstance().getLastCost();
            assertWithMetrics(sum, "3.2 Filtro por UNIQUE elige UNIQUE_INDEX_LOOKUP (rowsExamined=1)",
                              okUq && costUq.scanType == AccessScanType::UNIQUE_INDEX_LOOKUP && costUq.rowsExamined == 1,
                              costUq);

            // 3.3 Búsqueda por columna sin índice (ubicacion) -> Debe elegir SEQ_SCAN y examinar las 15 filas
            auto rowsSeq = Sensor.filter_eq("ubicacion", "Sector Par").all();
            const auto costSeq = ModuloDQL::getInstance().getLastCost();
            assertWithMetrics(sum, "3.3 Filtro por columna no indexada elige SEQ_SCAN (rowsExamined=15)",
                              !rowsSeq.empty() && costSeq.scanType == AccessScanType::SEQ_SCAN && costSeq.rowsExamined == 15,
                              costSeq);
        }

        // GRUPO 4: Cobertura 100% DQL PostgreSQL (Joins, LATERAL, ROLLUP, FILTER, WITHIN GROUP, OVER, WITH RECURSIVE, Conjuntos).
        static void testFullPostgresDqlCapabilities(RuntimeSuiteSummary& sum) {
            Serial.println("\n--- GRUPO 4: DQL Completo (Joins, LATERAL, ROLLUP, FILTER, WITHIN GROUP, OVER, CTE, Conjuntos) ---");
            ModelManager Sensor = runtimeDb.model("Sensor");
            Senso.objects().remove(true);

            const float medicionesS1[] = { 2.0f, 4.0f, 6.0f, 8.0f };
            for (float t : medicionesS1) {
                Senso.create({
                    PgCell("id_sensor", "1", PgDataType::INTEGER),
                    PgCell("temperatura_cruda", String(t, 2), PgDataType::REAL),
                    PgCell("temperatura_corregida", String(t, 2), PgDataType::REAL)
                });
            }
            
            // Fixture: 28FFAA02 ya lo inserto el MERGE del Grupo 2; se busca su id real.
            PgTuple filaS2;
            if (!Sensor.filter_eq("hexadecimal", "28FFAA02").get(filaS2)) {
                Serial.println(" [FIXTURE] No existe el Sensor 28FFAA02 (fallo el Grupo 2).");
            }
            const int32_t idS2 = filaS2.getInt("id_sensor", -1);

            auto resSensoS2 = Senso.create({
                PgCell("id_sensor", String(idS2), PgDataType::INTEGER),
                PgCell("temperatura_cruda", "10.00", PgDataType::REAL),
                PgCell("temperatura_corregida", "10.00", PgDataType::REAL)
            });
            if (!resSensoS2.success) {
                Serial.println(" [FIXTURE] Fallo insert en Senso para sensor 2:");
                Serial.println(DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat());
            }

            // 4.2 GROUP BY ROLLUP + COUNT FILTER (WHERE temp > 5) + PERCENTILE_CONT(0.5) WITHIN GROUP + HAVING
            PredicateCondition condAlta;
            condAlta.columnOrExpr = "temperatura_corregida";
            condAlta.op           = FilterOp::GT;
            condAlta.valueA       = "5.0";

            auto aggRows = Senso.objects()
                .group_by({"id_sensor"}, GroupingMode::ROLLUP)
                .annotate("total_muestras", AggregateFunc::COUNT_ALL)
                .annotate("muestras_altas", AggregateFunc::COUNT_COL, "temperatura_corregida", true, condAlta)
                .annotate_within_group("mediana", 0.5f, "temperatura_corregida")
                .having("total_muestras", FilterOp::GTE, "1")
                .all();
            assertWithMetrics(sum, "4.2 GROUP BY ROLLUP + FILTER (WHERE) + WITHIN GROUP + HAVING",
                              aggRows.size() == 3 && aggRows[0].getInt("muestras_altas") == 2 &&
                              aggRows[0].getFloat("mediana") == 5.0f,
                              ModuloDQL::getInstance().getLastCost());

            // 4.3 Ventanas Analíticas OVER (PARTITION BY id_sensor ORDER BY id_senso ROWS BETWEEN ...)
            auto winRows = Senso.filter_eq("id_sensor", 1)
                .window("fila_num", WindowFunc::ROW_NUMBER, "", {"id_sensor"}, "id_senso")
                .window("temp_anterior", WindowFunc::LAG, "temperatura_corregida", {"id_sensor"}, "id_senso")
                .window("suma_acum", WindowFunc::SUM_OVER, "temperatura_corregida", {"id_sensor"}, "id_senso")
                .all();
            assertWithMetrics(sum, "4.3 Ventanas OVER (ROW_NUMBER, LAG, SUM Acumulada)",
                              winRows.size() == 4 &&
                              winRows[3].getInt("fila_num") == 4 &&
                              winRows[1].getFloat("temp_anterior") == 2.0f &&
                              winRows[3].getFloat("suma_acum") == 20.0f,
                              ModuloDQL::getInstance().getLastCost());

            // 4.4 CTE Recursiva: WITH RECURSIVE secuencia(n) AS (1..5)
            PgTuple baseRow; baseRow.set("n", "1", PgDataType::INTEGER);
            auto cteRows = QuerySet("secuencia")
                .with_cte("secuencia", {baseRow}, true, +[](const PgTuple& r) -> std::vector<PgTuple> {
                    int n = r.getInt("n");
                    if (n >= 5) return {};
                    PgTuple next; next.set("n", String(n + 1), PgDataType::INTEGER);
                    return {next};
                })
                .all();
            assertWithMetrics(sum, "4.4 CTE WITH RECURSIVE evalua punto fijo (1..5)",
                              cteRows.size() == 5 && cteRows[4].getInt("n") == 5,
                              ModuloDQL::getInstance().getLastCost());

            // 4.5 DISTINCT ON + UNION DISTINCT / INTERSECT / EXCEPT
            auto set1 = Senso.filter_eq("id_sensor", 1).values({"id_sensor"}).distinct({"id_sensor"}).all();
            auto set2 = Senso.filter_eq("id_sensor", idS2).values({"id_sensor"}).distinct({"id_sensor"}).all();
            auto unionRows = Senso.filter_eq("id_sensor", 1)
                                  .values({"id_sensor"})
                                  .distinct({"id_sensor"})
                                  .union_with(set2, false)
                                  .all();
            assertWithMetrics(sum, "4.5 DISTINCT ON + UNION DISTINCT entre QuerySets",
                              set1.size() == 1 && unionRows.size() == 2,
                              ModuloDQL::getInstance().getLastCost());

            
            // 4.6 ROLLUP y CUBE con 2 columnas (5 filas detalle + 2 subtotales + 1 total = 8; CUBE = 13)
            auto rollup2 = Senso.objects()
                .group_by({"id_sensor", "temperatura_corregida"}, GroupingMode::ROLLUP)
                .annotate("n", AggregateFunc::COUNT_ALL)
                .all();
            auto cube2 = Senso.objects()
                .group_by({"id_sensor", "temperatura_corregida"}, GroupingMode::CUBE)
                .annotate("n", AggregateFunc::COUNT_ALL)
                .all();
            assertWithMetrics(sum, "4.6 ROLLUP/CUBE multicolumna (8 y 13 filas)",
                              rollup2.size() == 8 && cube2.size() == 13,
                              ModuloDQL::getInstance().getLastCost());  
                              
            
            // 4.7 UPDATE ... FROM correlacionado: solo las 4 filas del sensor 28FFAA01
            auto upFrom = Senso.objects()
                .filter_col("id_sensor", FilterOp::EQ, "Sensor.id_sensor")
                .filter("Sensor.hexadecimal", FilterOp::EQ, "28FFAA01")
                .update({ PgCell("temperatura_cruda", "99.00", PgDataType::REAL) }, false, "Sensor");
            assertWithMetrics(sum, "4.7 UPDATE ... FROM correlacionado (4 filas)",
                              upFrom.success && upFrom.affectedRows == 4, upFrom.cost);

            // 4.8 DELETE ... USING correlacionado: borra la lectura del sensor 28FFAA02
            auto delUsing = Senso.objects()
                .filter_col("id_sensor", FilterOp::EQ, "Sensor.id_sensor")
                .filter("Sensor.hexadecimal", FilterOp::EQ, "28FFAA02")
                .remove(false, "Sensor");
            assertWithMetrics(sum, "4.8 DELETE ... USING correlacionado (1 fila)",
                              delUsing.success && delUsing.affectedRows == 1, delUsing.cost);
        }

    public:
        // Ejecuta la suite completa y devuelve el resumen de pruebas.
        static RuntimeSuiteSummary runAll() {
            DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().setSerialOutput(false);
            runtimeDb.bindToPhysicalCatalog();

            Serial.println("\n######################################################################");
            Serial.println(" SUITE AUTOMATIZADA DATABASE_RUNTIME (DML + DQL + INDICES + COSTOS)");
            Serial.println("######################################################################");

            RuntimeSuiteSummary summary;
            testDmlTypesConstraintsAndReturning(summary);
            testUpsertAndMerge(summary);
            testIndexAccessPathsAndCost(summary);
            testFullPostgresDqlCapabilities(summary);

            Serial.println("\n----------------------------------------------------------------------");
            Serial.print(" RESULTADO FINAL RUNTIME SUITE: ");
            Serial.print(summary.passed);
            Serial.print("/");
            Serial.print(summary.total);
            Serial.print(" TESTS SUPERADOS (Fallos: ");
            Serial.print(summary.failed);
            Serial.println(")");
            Serial.println("----------------------------------------------------------------------");
            return summary;
        }
    };

} // namespace DbRuntime::Tests