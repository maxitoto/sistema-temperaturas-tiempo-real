#pragma once
#include <Arduino.h>
#include <functional>

namespace SGBD {

    // --- 1. PREDICADOS Y CONSTANTES DESCRIPTIVAS ---
    enum Orden : uint8_t { ASC = 0, DESC = 1 };

    enum AccionFK : uint8_t {
        RESTRICT  = 0, // Impide borrar el padre si tiene hijos
        CASCADE   = 1, // Borra en cascada todos los hijos relacionados
        SET_NULL  = 2  // Pone en NULL la FK de los hijos
    };

    enum Operador : uint8_t {
        EQ = 0, NEQ, GT, GTE, LT, LTE, LIKE
    };

    enum TipoDato : uint8_t {
        TYPE_INT = 0, TYPE_FLOAT, TYPE_STR, TYPE_BOOL
    };

    // --- 2. ROLES Y PERMISOS DCL (Bitmask) ---
    enum RolSGBD : uint8_t {
        ROL_ANONIMO  = 0,
        ROL_LECTOR   = 1,
        ROL_OPERADOR = 2,
        ROL_ADMIN    = 3
    };

    enum PermisoDCL : uint8_t {
        PERM_NONE   = 0,
        PERM_SELECT = 1 << 0,
        PERM_INSERT = 1 << 1,
        PERM_UPDATE = 1 << 2,
        PERM_DELETE = 1 << 3,
        PERM_DDL    = 1 << 4,
        PERM_ALL    = 0x1F
    };

    using CheckPredicado = std::function<bool(const String&)>;

    // --- 3. MOTOR DE DIAGNÓSTICO ESTILO TERMINAL SQLITE ---
    class ConsolaSQL {
    private:
        String ultimoError = "";
        ConsolaSQL() = default;

    public:
        static ConsolaSQL& getInstance() {
            static ConsolaSQL instance;
            return instance;
        }

        void reportarError(const char* codigoSqlState,
                           const char* tipoError,
                           const String& contexto,
                           const String& detalle) {
            ultimoError = "[" + String(codigoSqlState) + "] " + String(tipoError) + ": " + detalle;
            Serial.println("\n============================================================");
            Serial.printf("SQL ERROR [%s] %s\n", codigoSqlState, tipoError);
            Serial.printf("  -> Contexto : %s\n", contexto.c_str());
            Serial.printf("  -> Detalle  : %s\n", detalle.c_str());
            Serial.println("============================================================");
        }

        void reportarInfo(const String& comando, const String& mensaje) {
            Serial.printf("SQL OK [%s] -> %s\n", comando.c_str(), mensaje.c_str());
        }

        String obtenerUltimoError() const { return ultimoError; }
        void limpiarError() { ultimoError = ""; }
    };

}

// Exposición de constantes descriptivas para uso directo en consultas y esquemas
using SGBD::ASC;
using SGBD::DESC;
using SGBD::RESTRICT;
using SGBD::CASCADE;
using SGBD::SET_NULL;
using SGBD::EQ;
using SGBD::NEQ;
using SGBD::GT;
using SGBD::GTE;
using SGBD::LT;
using SGBD::LTE;
using SGBD::LIKE;
using SGBD::ROL_ANONIMO;
using SGBD::ROL_LECTOR;
using SGBD::ROL_OPERADOR;
using SGBD::ROL_ADMIN;
using SGBD::PERM_SELECT;
using SGBD::PERM_INSERT;
using SGBD::PERM_UPDATE;
using SGBD::PERM_DELETE;
using SGBD::PERM_DDL;
using SGBD::PERM_ALL;