#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "engine/DdlDiagnosticConsole.hpp"
#include "models/PhysicalModelsDefinition.hpp"
#include "test/AutomatedPostgresDdlSuite.hpp"
#include "test/ManualDdlSandboxTemplate.hpp"

// Asigna 16 KB de pila a loopTask en Arduino-ESP32 para operaciones intensivas de validación de esquema.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Imprime el mínimo histórico de bytes libres en la pila de la tarea actual (High Water Mark).
static void logStackMargin(const char* stageLabel) {
    Serial.print(" [STACK MARGIN - ");
    Serial.print(stageLabel);
    Serial.print("]: ");
    Serial.print(uxTaskGetStackHighWaterMark(nullptr));
    Serial.println(" bytes libres minimos");
}

// Inicializa la consola serie y ejecuta en orden: 1) Test del Sistema, 2) Tu Sandbox Manual y 3) Migración del Modelo Físico Django.
void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println("\n======================================================================");
    Serial.println(" MOTOR SGBD - MODULO DDL (POSTGRESQL STANDARD + DJANGO MODELS)");
    Serial.println("======================================================================");
    logStackMargin("BOOT");

    // 1. Ejecuta el test automatizado del sistema (verifica todos los casos posibles de restricciones y SQLSTATEs)
    DbNotRuntime::Tests::AutomatedPostgresDdlSuite::runAll();
    logStackMargin("POST-SUITE");

    // 2. Ejecuta tu plantilla de test manual interactiva (muestra la salida como terminal psql)
    DbNotRuntime::Tests::ejecutarSandboxManual();
    logStackMargin("POST-SANDBOX");

    // 3. Migra las 12 tablas definitivas del proyecto escritas con el patrón Django Models
    Serial.println("\n======================================================================");
    Serial.println(" MIGRANDO LOS 12 MODELOS FISICOS DEL PROYECTO (PhysicalModelsDefinition)");
    Serial.println("======================================================================");
    DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().setSerialOutput(true);

    bool ok = DbNotRuntime::PhysicalModels::migrateAllPhysicalModels();
    logStackMargin("POST-MIGRATE-12-TABLES");

    if (ok) {
        Serial.println("\n[OK] Esquema fisico completo (12 tablas) migrado y persistido en /db/pg_catalog.cat");
    } else {
        Serial.println("\n[ERROR] Fallo en la migracion del esquema fisico:");
        Serial.println(DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat());
    }
}

// Mantiene el bucle principal en reposo tras completar la verificación y migración DDL.
void loop() {
    delay(10000);
}