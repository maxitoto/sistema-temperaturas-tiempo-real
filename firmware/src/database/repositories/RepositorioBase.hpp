#pragma once
#include <stdexcept>
#include "../common/DiagnosticConsole.hpp"
#include "../common/PhysicalSchemaCatalog.hpp"
#include "../storage/PgBinaryStorage.hpp"
#include "../sql/SchemaPostgreSQL.hpp"

namespace DbRuntime::Repositories {

    // Helper común: garantiza catálogo V2 cargado y devuelve el esquema físico de la tabla
    inline const DbCommon::Engine::TablePhysicalSchema* asegurarEsquemaTabla(const char* tableName) {
        auto& cat = DbCommon::Engine::PhysicalSchemaCatalog::getInstance();
        if (cat.getAllTables().empty()) {
            DbSql::Schema::bindOrBootstrapDatabase();
        }
        const auto* schema = cat.getTable(tableName);
        if (!schema) {
            throw std::runtime_error("UndefinedTable: la tabla solicitada no existe en pg_catalog");
        }
        return schema;
    }

    inline void lanzarUltimoErrorDB() {
        throw std::runtime_error(
            DbCommon::Engine::DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str()
        );
    }

    // Guardia RAII para ejecutar múltiples escrituras binarias en una sola ráfaga física a MicroSD
    class TransaccionBatch {
    private:
        bool cerrada = false;
    public:
        TransaccionBatch() {
            Storage::PgBinaryStorage::getInstance().beginBatch();
        }
        bool confirmar() {
            if (cerrada) return true;
            cerrada = true;
            return Storage::PgBinaryStorage::getInstance().commitBatch();
        }
        ~TransaccionBatch() {
            confirmar();
        }
    };

} // namespace DbRuntime::Repositories