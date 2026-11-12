#include <Arduino.h>
#include "hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "DB_DDL/models/PhysicalModelsDefinition.hpp"
#include "DB_DQL_DML/storage/RuntimeIndexAccessPath.hpp"
#include "DB_DQL_DML/orm/DjangoManager.hpp"
#include "DB_DQL_DML/test/AutomatedRuntimeCostAndSqlSuite.hpp"
#include "DB_DQL_DML/test/ManualRuntimeSandboxTemplate.hpp"

// Configura 16 KB de pila en loopTask para auditar con precisión el HighWaterMark de FreeRTOS.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println("\n======================================================================");
    Serial.println(" SGBD RUNTIME ENGINE - MODULO DML Y DQL (POSTGRESQL + DJANGO ORM)");
    Serial.println("======================================================================");

    // 1. Prepara las 12 tablas del catálogo físico (creadas por el módulo DDL)
    DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().setSerialOutput(false);
    if (!DbNotRuntime::PhysicalModels::migrateAllPhysicalModels()) {
        Serial.println("[ERROR] No se pudo inicializar el catalogo fisico base:");
        Serial.println(DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat());
        return;
    }

    // 2. Vincula database_runtime al catálogo y construye los árboles de índices en RAM
    DbRuntime::Orm::runtimeDb.bindToPhysicalCatalog();

    // 3. Ejecuta la suite automatizada midiendo Stack, Ciclos de CPU, Tiempo e Índices en cada test
    DbRuntime::Tests::AutomatedRuntimeCostAndSqlSuite::runAll();

    // 4. Ejecuta tu plantilla de Sandbox Manual con salida EXPLAIN (ANALYZE, COSTS)
    DbRuntime::Tests::ejecutarSandboxManualRuntime();
}

void loop() {
    delay(10000);
}