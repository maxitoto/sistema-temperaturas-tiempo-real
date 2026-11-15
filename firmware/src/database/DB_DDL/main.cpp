#include <Arduino.h>

#include "models/PhysicalModelsDefinition.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../common/PhysicalSchemaCatalog.hpp"

namespace DDL = DbNotRuntime::Engine;
namespace PM  = DbNotRuntime::PhysicalModels;

// -----------------------------------------------------------------------------

static void printBanner(const char* title) {
    Serial.println();
    Serial.println("==============================================");
    Serial.println(title);
    Serial.println("==============================================");
}

static void printLastDiagnostic() {
    Serial.println(DDL::DdlDiagnosticConsole::getInstance().last().toPsqlFormat());
}

// Carga el catálogo existente para que las migraciones sean idempotentes.
// Si NO se llama, el primer flushCatalogToSingleFile() de la corrida
// sobreescribe /db/pg_catalog.cat dejando SOLO las tablas de esta migración.
static void loadExistingCatalog() {
    auto& cat = DDL::PhysicalSchemaCatalog::getInstance();
    bool loaded = cat.loadCatalogFromSingleFile();
    Serial.println(String("[CATALOG] ")
        + (loaded ? "cargado desde SD" : "no existía (se creará nuevo)")
        + " | tablas=" + String(static_cast<int>(cat.getAllTables().size())));
}

static bool runMigrations() {
    Serial.println("[DDL] Registrando ENUMs + migrando modelos físicos...");
    if (!PM::migrateAllPhysicalModels()) {
        Serial.println("[DDL][ERROR] La migración falló:");
        printLastDiagnostic();
        return false;
    }
    return true;
}

static void reportSchema() {
    auto& cat = DDL::PhysicalSchemaCatalog::getInstance();
    Serial.println("[DDL][OK] Esquema en catálogo:");
    for (const auto& kv : cat.getAllTables()) {
        const auto& tb = kv.second;
        Serial.println(String("  - ") + tb.tableName
            + " (" + String(static_cast<int>(tb.columns.size())) + " columnas"
            + (tb.hasPrimaryKey() ? ", PK" : "") + ")");
    }
    Serial.println(String("[DDL] Catálogo: ") + cat.catalogFilePath());
}

// -----------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

    printBanner("PgEmbedded — Bootstrap DDL");

    DDL::DdlDiagnosticConsole::getInstance().setSerialOutput(true);

    loadExistingCatalog();

    if (!runMigrations()) {
        Serial.println("[BOOT] Abortando: esquema incompleto.");
        return;
    }

    reportSchema();
    printBanner("Base de datos creada.");
}

void loop() {
    delay(10000);
}