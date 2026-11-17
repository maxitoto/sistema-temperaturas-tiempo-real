#include <Arduino.h>

#include "models/PhysicalModelsDefinition.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../common/PhysicalSchemaCatalog.hpp"

#include "../DB_DQL_DML/orm/DjangoManager.hpp"
#include "../repositories/repoUsuario.hpp"



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


static void seedDefaultAdminUser() {
    using DbRuntime::Repositories::RepoUsuario;
    using DbRuntime::Repositories::UsuarioDTO;

    DbRuntime::Orm::runtimeDb.bindToPhysicalCatalog();

    try {
        UsuarioDTO u{};
        if (!RepoUsuario::obtenerPorNombreUsuario("admin", u)) {
            RepoUsuario::crearUsuario("admin", "admin", RolUsuario::ADMIN);
            Serial.println("[DDL][OK] Usuario por defecto 'admin' creado.");
        } else {
            RepoUsuario::desbloquearPorNombre("admin");
            RepoUsuario::cambiarContrasena("admin", "", "admin", false);
            Serial.println("[DDL][OK] Usuario 'admin' verificado y desbloqueado.");
        }
    } catch (const std::exception& e) {
        Serial.print("[DDL][ERROR] No se pudo crear el usuario 'admin': ");
        Serial.println(e.what());
    }
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

    seedDefaultAdminUser();

    reportSchema();
    printBanner("Base de datos creada.");
}

void loop() {
    delay(10000);
}