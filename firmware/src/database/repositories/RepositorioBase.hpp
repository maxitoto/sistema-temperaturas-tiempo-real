#pragma once
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    // Guardia RAII para ejecutar múltiples operaciones (Batch Insert / Update) en una sola escritura física a MicroSD
    class TransaccionBatch {
    private:
        bool cerrada = false;
    public:
        TransaccionBatch() {
            Storage::RuntimeStorageAndIndexManager::getInstance().beginBatch();
        }
        bool confirmar() {
            if (cerrada) return true;
            cerrada = true;
            return Storage::RuntimeStorageAndIndexManager::getInstance().commitBatch();
        }
        ~TransaccionBatch() {
            confirmar();
        }
    };

} // namespace DbRuntime::Repositories