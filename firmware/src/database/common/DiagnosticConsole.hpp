#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include "PostgresSqlStates.hpp"

namespace DbCommon::Engine {

    struct PgDiagnosticRecord {
        bool success = true;
        String returnedSqlState = Standard::SqlState::SUCCESSFUL_COMPLETION;
        String commandFunction = "";
        String schemaName = "public";
        String tableName = "";
        String columnName = "";
        String constraintName = "";
        String messageText = "";
        String hintText = "";
        uint32_t timestampMs = 0;

        String toPsqlFormat() const {
            if (success) {
                return commandFunction + (messageText.isEmpty() ? "" : (" (" + messageText + ")"));
            }
            String out = "ERROR:  " + messageText + " [SQLSTATE " + returnedSqlState + "]\n";
            out += "CONTEXT:  statement \"" + commandFunction + "\" on schema \"" + schemaName + "\"";
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
        static DdlDiagnosticConsole& getInstance() {
            static DdlDiagnosticConsole instance;
            return instance;
        }

        void setSerialOutput(bool enabled) { printToSerial = enabled; }
        void setRemoteListener(ListenerFn fn) { listener = fn; }

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

        const PgDiagnosticRecord& last() const { return lastRecord; }
        const std::vector<PgDiagnosticRecord>& getHistory() const { return history; }
        void clearHistory() { history.clear(); }
    };

} // namespace DbCommon::Engine

namespace DbNotRuntime::Engine {
    using namespace DbCommon::Engine;
}