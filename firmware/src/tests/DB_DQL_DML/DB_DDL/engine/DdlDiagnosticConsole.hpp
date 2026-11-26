#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include "../standard/PostgresSqlStates.hpp"

namespace DbNotRuntime::Engine {

    // Almacena los campos de diagnóstico estándar del diccionario de PostgreSQL (Sección 7 de informe.md).
    struct PgDiagnosticRecord {
        bool success = true;
        String returnedSqlState = Standard::SqlState::SUCCESSFUL_COMPLETION;
        String commandFunction = "";  // Ej: "CREATE TABLE", "ALTER TABLE"
        String schemaName = "public";
        String tableName = "";
        String columnName = "";
        String constraintName = "";
        String messageText = "";
        String hintText = "";
        uint32_t timestampMs = 0;

        // Formatea el registro exactamente como la salida estándar de psql en PostgreSQL.
        String toPsqlFormat() const {
            if (success) {
                return commandFunction + (messageText.isEmpty() ? "" : (" (" + messageText + ")"));
            }
            String out = "ERROR:  " + messageText + " [SQLSTATE " + returnedSqlState + "]\n";
            out += "CONTEXT:  DDL statement \"" + commandFunction + "\" on schema \"" + schemaName + "\"";
            if (!tableName.isEmpty()) out += ", table \"" + tableName + "\"";
            if (!columnName.isEmpty()) out += ", column \"" + columnName + "\"";
            if (!constraintName.isEmpty()) out += ", constraint \"" + constraintName + "\"";
            if (!hintText.isEmpty()) {
                out += "\nHINT:  " + hintText;
            }
            return out;
        }
    };

    class DdlDiagnosticConsole {
    public:
        using ListenerFn = std::function<void(const PgDiagnosticRecord&)>;

    private:
        static constexpr size_t MAX_HISTORY = 16;
        PgDiagnosticRecord lastRecord;
        std::vector<PgDiagnosticRecord> history;
        ListenerFn listener = nullptr;
        bool printToSerial = false;

        DdlDiagnosticConsole() = default;

        // Registra el diagnóstico en la cola circular y notifica al listener o puerto serie si está activo.
        void pushRecord(const PgDiagnosticRecord& rec) {
            lastRecord = rec;
            if (history.size() >= MAX_HISTORY) {
                history.erase(history.begin());
            }
            history.push_back(rec);
            if (listener) listener(rec);
            if (printToSerial) Serial.println(rec.toPsqlFormat());
        }

    public:
        // Obtiene la instancia única del motor de diagnóstico DDL.
        static DdlDiagnosticConsole& getInstance() {
            static DdlDiagnosticConsole instance;
            return instance;
        }

        // Activa o desactiva el volcado directo por UART0 para sesiones de depuración o tests.
        void setSerialOutput(bool enabled) { printToSerial = enabled; }

        // Registra un callback para redirigir los errores de PostgreSQL hacia RPC/Electron.
        void setRemoteListener(ListenerFn fn) { listener = fn; }

        // Emite un error estructurado de PostgreSQL con SQLSTATE, contexto de objeto y sugerencia.
        bool raiseError(const char* sqlState,
                        const char* command,
                        const String& table,
                        const String& column,
                        const String& message,
                        const String& hint = "",
                        const String& constraint = "") {
            PgDiagnosticRecord r;
            r.success          = false;
            r.returnedSqlState = sqlState;
            r.commandFunction  = command;
            r.tableName        = table;
            r.columnName       = column;
            r.constraintName   = constraint;
            r.messageText      = message;
            r.hintText         = hint;
            r.timestampMs      = millis();
            pushRecord(r);
            return false;
        }

        // Emite una confirmación de comando DDL exitoso con SQLSTATE 00000.
        bool reportSuccess(const char* command, const String& table, const String& details = "") {
            PgDiagnosticRecord r;
            r.success          = true;
            r.returnedSqlState = Standard::SqlState::SUCCESSFUL_COMPLETION;
            r.commandFunction  = command;
            r.tableName        = table;
            r.messageText      = details;
            r.timestampMs      = millis();
            pushRecord(r);
            return true;
        }

        // Devuelve el último diagnóstico registrado por el motor DDL.
        const PgDiagnosticRecord& last() const { return lastRecord; }

        // Devuelve el historial reciente de diagnósticos DDL.
        const std::vector<PgDiagnosticRecord>& getHistory() const { return history; }

        // Limpia el historial de diagnósticos en RAM.
        void clearHistory() { history.clear(); }
    };

} // namespace DbNotRuntime::Engine