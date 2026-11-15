#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include <cstring>
#include "PostgresSqlStates.hpp"

namespace DbCommon::Engine {

    struct PgDiagnosticRecord {
        bool     success               = true;
        char     returnedSqlState[8]   = "00000";
        char     commandFunction[28]   = "";
        char     schemaName[16]        = "public";
        char     tableName[32]         = "";
        char     columnName[32]        = "";
        char     constraintName[40]    = "";
        char     messageText[128]      = "";
        char     hintText[96]          = "";
        uint32_t timestampMs           = 0;

        static void copySafe(char* dst, size_t cap, const char* src) {
            if (!dst || cap == 0) return;
            if (!src) { dst[0] = '\0'; return; }
            strncpy(dst, src, cap - 1);
            dst[cap - 1] = '\0';
        }

        String toPsqlFormat() const {
            if (success) {
                if (messageText[0] == '\0') return String(commandFunction);
                String out = commandFunction;
                out += " (";
                out += messageText;
                out += ")";
                return out;
            }
            String out = "";
            out.reserve(220);
            out += "ERROR:  ";
            out += messageText;
            out += " [SQLSTATE ";
            out += returnedSqlState;
            out += "]\nCONTEXT:  statement \"";
            out += commandFunction;
            out += "\" on schema \"";
            out += schemaName;
            out += "\"";
            if (tableName[0] != '\0')      { out += ", table \"";      out += tableName;      out += "\""; }
            if (columnName[0] != '\0')     { out += ", column \"";     out += columnName;     out += "\""; }
            if (constraintName[0] != '\0') { out += ", constraint \""; out += constraintName; out += "\""; }
            if (hintText[0] != '\0')       { out += "\nHINT:  ";       out += hintText; }
            return out;
        }
    };

    class DdlDiagnosticConsole {
    public:
        using ListenerFn = std::function<void(const PgDiagnosticRecord&)>;

    private:
        static constexpr size_t MAX_HISTORY = 8;
        PgDiagnosticRecord lastRecord{};
        PgDiagnosticRecord ringHistory[MAX_HISTORY]{};
        uint8_t historyCount = 0;
        uint8_t historyHead  = 0;
        ListenerFn listener  = nullptr;
        bool printToSerial   = false;

        DdlDiagnosticConsole() = default;

        void pushRecord(const PgDiagnosticRecord& rec) {
            lastRecord = rec;
            ringHistory[historyHead] = rec;
            historyHead = static_cast<uint8_t>((historyHead + 1) % MAX_HISTORY);
            if (historyCount < MAX_HISTORY) {
                historyCount++;
            }
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
            PgDiagnosticRecord r{};
            r.success = false;
            PgDiagnosticRecord::copySafe(r.returnedSqlState, sizeof(r.returnedSqlState), sqlState ? sqlState : "XX000");
            PgDiagnosticRecord::copySafe(r.commandFunction,  sizeof(r.commandFunction),  command ? command : "SQL");
            PgDiagnosticRecord::copySafe(r.tableName,        sizeof(r.tableName),        table.c_str());
            PgDiagnosticRecord::copySafe(r.columnName,       sizeof(r.columnName),       column.c_str());
            PgDiagnosticRecord::copySafe(r.constraintName,   sizeof(r.constraintName),   constraint.c_str());
            PgDiagnosticRecord::copySafe(r.messageText,      sizeof(r.messageText),      message.c_str());
            PgDiagnosticRecord::copySafe(r.hintText,         sizeof(r.hintText),         hint.c_str());
            r.timestampMs = millis();
            pushRecord(r);
            return false;
        }

        bool reportSuccess(const char* command, const String& table, const String& details = "") {
            return reportSuccess(command, table.c_str(), details.c_str());
        }

        bool reportSuccess(const char* command, const char* table, const char* details = "") {
            lastRecord.success = true;
            PgDiagnosticRecord::copySafe(lastRecord.returnedSqlState, sizeof(lastRecord.returnedSqlState), "00000");
            PgDiagnosticRecord::copySafe(lastRecord.commandFunction,  sizeof(lastRecord.commandFunction),  command ? command : "OK");
            PgDiagnosticRecord::copySafe(lastRecord.tableName,        sizeof(lastRecord.tableName),        table ? table : "");
            lastRecord.columnName[0]     = '\0';
            lastRecord.constraintName[0] = '\0';
            PgDiagnosticRecord::copySafe(lastRecord.messageText,      sizeof(lastRecord.messageText),      details ? details : "");
            lastRecord.hintText[0]       = '\0';
            lastRecord.timestampMs       = millis();

            if (listener || printToSerial) {
                pushRecord(lastRecord);
            }
            return true;
        }

        const PgDiagnosticRecord& last() const { return lastRecord; }

        std::vector<PgDiagnosticRecord> getHistory() const {
            std::vector<PgDiagnosticRecord> out;
            out.reserve(historyCount);
            uint8_t start = (historyCount == MAX_HISTORY) ? historyHead : 0;
            for (uint8_t i = 0; i < historyCount; ++i) {
                out.push_back(ringHistory[(start + i) % MAX_HISTORY]);
            }
            return out;
        }

        void clearHistory() {
            historyCount = 0;
            historyHead  = 0;
        }
    };

} // namespace DbCommon::Engine

namespace DbNotRuntime::Engine {
    using namespace DbCommon::Engine;
}