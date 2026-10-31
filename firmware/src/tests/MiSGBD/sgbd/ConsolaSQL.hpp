// Archivo: src/tests/SQLite/sgbd/ConsolaSQL.hpp
#pragma once
#include <Arduino.h>

namespace SGBD {

    class ConsolaSQL {
    private:
        String ultimoError = "";
        ConsolaSQL() = default;

    public:
        static ConsolaSQL& getInstance() {
            static ConsolaSQL instance;
            return instance;
        }

        void reportarError(const char* sqlState,
                           const char* nombreError,
                           const String& origen,
                           const String& detalle,
                           const String& sugerencia = "") {
            ultimoError = "[" + String(sqlState) + "] " + String(nombreError) + ": " + detalle;
            Serial.println("\n============================================================");
            Serial.printf("SQL ERROR [%s] %s\n", sqlState, nombreError);
            Serial.printf("  -> Origen     : %s\n", origen.c_str());
            Serial.printf("  -> Detalle    : %s\n", detalle.c_str());
            if (!sugerencia.isEmpty()) {
                Serial.printf("  -> Sugerencia : %s\n", sugerencia.c_str());
            }
            Serial.println("============================================================");
        }

        void reportarInfo(const String& comando, const String& detalle) {
            Serial.printf("SQL OK [%s] -> %s\n", comando.c_str(), detalle.c_str());
        }

        String obtenerUltimoError() const { return ultimoError; }
    };

}