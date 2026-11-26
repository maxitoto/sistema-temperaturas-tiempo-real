#pragma once
#include <Arduino.h>
#include <SD.h>
#include <esp_timer.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "../common/PostgresSqlStates.hpp"
#include "../common/PostgresDataTypes.hpp"
#include "../common/PostgresConstraints.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../common/PhysicalSchemaCatalog.hpp"
#include "../DB_DDL/standard/PostgresReservedWords.hpp"
#include "../DB_DQL_DML/standard/PostgresDmlDqlKeywords.hpp"
#include "../storage/PgBinaryStorage.hpp"
#include "PgVirtualCatalog.hpp"
#include "PgSqlDqlEngine.hpp"

namespace DbSql {

    using DbCommon::Engine::DdlDiagnosticConsole;
    using DbCommon::Engine::PhysicalSchemaCatalog;
    using DbCommon::Engine::TablePhysicalSchema;
    using DbCommon::Standard::ColumnConstraintDef;
    using DbCommon::Standard::DataTypeUtils;
    using DbCommon::Standard::EnumTypeDefinition;
    using DbCommon::Standard::IdentityGeneration;
    using DbCommon::Standard::IndexDefinition;
    using DbCommon::Standard::PgBinHeader;
    using DbCommon::Standard::PgDataType;
    using DbCommon::Standard::ReferentialAction;
    using DbCommon::Standard::SqlState;
    using DbNotRuntime::Standard::PostgresLexicalValidator;
    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    // Resultado POD (cero heap) 1:1 con ResultadoSqlTerminalDTO
    struct SqlExecResult {
        bool     exito                = false;
        char     sqlState[8]          = "00000";
        char     comandoEjecutado[28] = "";
        uint16_t filasAfectadas       = 0;
        uint32_t tiempoEjecucionUs    = 0;
        char     salidaPsql[1024]     = "";
    };

    class PgSqlEngine {
    private:
        SqlExecResult resBuf{};
        bool deferCatalogFlush = false;

        PgSqlEngine() = default;

        static void copyStr(char* dst, size_t cap, const char* src) {
            if (!dst || cap == 0) return;
            if (!src) { dst[0] = '\0'; return; }
            strncpy(dst, src, cap - 1);
            dst[cap - 1] = '\0';
        }

        static void appendFmt(char* dst, size_t cap, size_t& pos, const char* text) {
            if (!dst || !text || pos + 1 >= cap) return;
            const size_t avail = cap - 1 - pos;
            const size_t len   = strnlen(text, avail);
            memcpy(dst + pos, text, len);
            pos += len;
            dst[pos] = '\0';
        }

        static String stripQuotes(const String& s) {
            String t = s;
            t.trim();
            if (t.length() >= 2 &&
                ((t.startsWith("'") && t.endsWith("'")) || (t.startsWith("\"") && t.endsWith("\"")))) {
                return t.substring(1, t.length() - 1);
            }
            return t;
        }

        static std::vector<String> splitTopLevel(const String& src, char sep) {
            std::vector<String> out;
            String cur = "";
            bool inSingle = false;
            bool inDouble = false;
            int  paren    = 0;

            for (size_t i = 0; i < src.length(); ++i) {
                char c = src[i];
                if (c == '\'' && !inDouble) {
                    inSingle = !inSingle;
                    cur += c;
                } else if (c == '"' && !inSingle) {
                    inDouble = !inDouble;
                    cur += c;
                } else if (!inSingle && !inDouble) {
                    if (c == '(') paren++;
                    else if (c == ')' && paren > 0) paren--;

                    if (c == sep && paren == 0) {
                        cur.trim();
                        if (!cur.isEmpty()) out.push_back(cur);
                        cur = "";
                        continue;
                    }
                    cur += c;
                } else {
                    cur += c;
                }
            }
            cur.trim();
            if (!cur.isEmpty()) out.push_back(cur);
            return out;
        }

        static int findKeywordOutsideQuotes(const String& src, const char* kwUpper, int fromIdx = 0) {
            const size_t kwLen = strlen(kwUpper);
            bool inS = false, inD = false;
            int par = 0;
            for (int i = fromIdx; i + static_cast<int>(kwLen) <= static_cast<int>(src.length()); ++i) {
                char c = src[i];
                if (c == '\'' && !inD) inS = !inS;
                else if (c == '"' && !inS) inD = !inD;
                else if (!inS && !inD) {
                    if (c == '(') par++;
                    else if (c == ')' && par > 0) par--;
                    if (par == 0 && strncasecmp(src.c_str() + i, kwUpper, kwLen) == 0) {
                        return i;
                    }
                }
            }
            return -1;
        }

        static bool fail(
            SqlExecResult& res,
            const char* sqlState,
            const char* cmd,
            const String& table,
            const String& col,
            const String& msg,
            const String& hint = ""
        ) {
            auto& console = DdlDiagnosticConsole::getInstance();
            console.raiseError(sqlState, cmd, table, col, msg, hint);
            res.exito = false;
            copyStr(res.sqlState, sizeof(res.sqlState), sqlState);
            copyStr(res.comandoEjecutado, sizeof(res.comandoEjecutado), cmd);
            copyStr(res.salidaPsql, sizeof(res.salidaPsql), console.last().toPsqlFormat().c_str());
            return false;
        }

        static void okResult(SqlExecResult& res, const char* cmd, uint16_t rows, const char* txt) {
            DdlDiagnosticConsole::getInstance().reportSuccess(cmd, "", txt ? txt : "");
            res.exito          = true;
            res.filasAfectadas = rows;
            copyStr(res.sqlState, sizeof(res.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(res.comandoEjecutado, sizeof(res.comandoEjecutado), cmd);
            copyStr(res.salidaPsql, sizeof(res.salidaPsql), txt ? txt : cmd);
        }

        void flushCatalogIfAllowed() {
            if (!deferCatalogFlush) {
                PhysicalSchemaCatalog::getInstance().flushCatalogToSingleFile();
            }
        }

        static String readNextWord(const String& s, int& cursor) {
            while (cursor < static_cast<int>(s.length()) && isspace(static_cast<unsigned char>(s[cursor]))) {
                cursor++;
            }
            if (cursor >= static_cast<int>(s.length())) return "";

            if (s[cursor] == '\'') {
                int start = cursor++;
                while (cursor < static_cast<int>(s.length())) {
                    if (s[cursor] == '\'') {
                        if (cursor + 1 < static_cast<int>(s.length()) && s[cursor + 1] == '\'') {
                            cursor += 2;
                            continue;
                        }
                        cursor++;
                        break;
                    }
                    cursor++;
                }
                return s.substring(start, cursor);
            }

            int start = cursor;
            int paren = 0;
            while (cursor < static_cast<int>(s.length())) {
                char c = s[cursor];
                if (c == '(') paren++;
                else if (c == ')' && paren > 0) paren--;
                else if (isspace(static_cast<unsigned char>(c)) && paren == 0) break;
                cursor++;
            }
            return s.substring(start, cursor);
        }

        static bool parseSingleTableWhere(
            const TablePhysicalSchema* schema,
            const String& whereStr,
            std::vector<BoundTable>& tablesOut,
            std::vector<ExprNode>& nodesOut,
            int16_t& rootOut,
            const char* cmd,
            SqlExecResult& errRes
        ) {
            tablesOut.clear();
            BoundTable bt{};
            bt.isVirtual = false;
            bt.schema    = schema;
            bt.fullName  = schema->tableName;
            bt.shortName = schema->tableName;
            bt.alias     = schema->tableName;
            tablesOut.push_back(bt);

            String w = whereStr;
            w.trim();
            if (w.isEmpty()) { rootOut = -1; return true; }

            auto toks = tokenizeSql(w);
            SqlExprParser p(toks, tablesOut, nodesOut, nullptr);
            rootOut = p.parseExpr();
            if (rootOut < 0) {
                return fail(errRes, p.errorState, cmd, schema->tableName, "", p.errorMsg);
            }
            return true;
        }

        __attribute__((noinline))
        static bool parseColumnDef(
            const String& rawDef,
            const TablePhysicalSchema& draft,
            ColumnConstraintDef& col,
            const char* cmd,
            SqlExecResult& res
        ) {
            String d = rawDef;
            d.trim();
            int cursor = 0;

            String colNameRaw = readNextWord(d, cursor);
            if (colNameRaw.isEmpty()) return false;

            String reason;
            if (!PostgresLexicalValidator::isValidIdentifierFormat(colNameRaw, reason)) {
                return fail(res, SqlState::INVALID_NAME, cmd, draft.tableName, colNameRaw,
                            "invalid column name \"" + colNameRaw + "\": " + reason);
            }
            if (PostgresLexicalValidator::isForbiddenUnquotedIdentifier(colNameRaw, false)) {
                return fail(res, SqlState::SYNTAX_ERROR, cmd, draft.tableName, colNameRaw,
                            "syntax error at or near \"" + colNameRaw + "\" (PostgreSQL RESERVED word)");
            }

            col.columnName = stripQuotes(colNameRaw);
            if (draft.findColumn(col.columnName) != nullptr) {
                return fail(res, SqlState::DUPLICATE_COLUMN, cmd, draft.tableName, col.columnName,
                            "column \"" + col.columnName + "\" specified more than once");
            }

            String typeTok = readNextWord(d, cursor);
            if (typeTok.isEmpty()) {
                return fail(res, SqlState::SYNTAX_ERROR, cmd, draft.tableName, col.columnName,
                            "missing data type for column \"" + col.columnName + "\"");
            }

            String typeUp = typeTok;
            typeUp.toUpperCase();

            if (typeUp == "DOUBLE") {
                int saved = cursor;
                String nxt = readNextWord(d, cursor);
                if (nxt.equalsIgnoreCase("PRECISION")) typeUp = "DOUBLE PRECISION";
                else cursor = saved;
            } else if (typeUp == "CHARACTER") {
                int saved = cursor;
                String nxt = readNextWord(d, cursor);
                String nxtUp = nxt; nxtUp.toUpperCase();
                if (nxtUp.startsWith("VARYING")) {
                    typeUp = nxtUp;
                    typeTok = nxt;
                } else {
                    cursor = saved;
                }
            }

            uint16_t maxLen = 0;
            int pOpen  = typeUp.indexOf('(');
            int pClose = typeUp.indexOf(')');
            if (pOpen > 0 && pClose > pOpen) {
                String inside = typeUp.substring(pOpen + 1, pClose);
                int comma = inside.indexOf(',');
                if (comma > 0) {
                    col.precision = static_cast<uint8_t>(inside.substring(0, comma).toInt());
                    col.scale     = static_cast<uint8_t>(inside.substring(comma + 1).toInt());
                } else {
                    maxLen = static_cast<uint16_t>(inside.toInt());
                }
                typeUp = typeUp.substring(0, pOpen);
                typeUp.trim();
            }

            auto& cat = PhysicalSchemaCatalog::getInstance();
            bool esSerial = false;

            if (typeUp == "SMALLSERIAL") {
                col.dataType = PgDataType::SMALLINT; esSerial = true;
            } else if (typeUp == "SERIAL") {
                col.dataType = PgDataType::INTEGER; esSerial = true;
            } else if (typeUp == "BIGSERIAL") {
                col.dataType = PgDataType::BIGINT; esSerial = true;
            } else if (typeUp == "SMALLINT" || typeUp == "INT2") {
                col.dataType = PgDataType::SMALLINT;
            } else if (typeUp == "INTEGER" || typeUp == "INT" || typeUp == "INT4") {
                col.dataType = PgDataType::INTEGER;
            } else if (typeUp == "BIGINT" || typeUp == "INT8") {
                col.dataType = PgDataType::BIGINT;
            } else if (typeUp == "REAL" || typeUp == "FLOAT4" || typeUp == "FLOAT") {
                col.dataType = PgDataType::REAL;
            } else if (typeUp == "DOUBLE PRECISION" || typeUp == "FLOAT8") {
                col.dataType = PgDataType::DOUBLE_PRECISION;
            } else if (typeUp == "NUMERIC" || typeUp == "DECIMAL") {
                col.dataType = PgDataType::NUMERIC;
            } else if (typeUp == "BOOLEAN" || typeUp == "BOOL") {
                col.dataType = PgDataType::BOOLEAN;
            } else if (typeUp == "CHAR" || typeUp == "CHARACTER") {
                col.dataType  = PgDataType::CHAR;
                col.maxLength = (maxLen > 0) ? maxLen : 1;
            } else if (typeUp == "VARCHAR" || typeUp == "VARYING") {
                col.dataType  = PgDataType::VARCHAR;
                col.maxLength = (maxLen > 0) ? maxLen : 64;
            } else if (typeUp == "TEXT") {
                col.dataType  = PgDataType::TEXT;
                col.maxLength = (maxLen > 0) ? maxLen : 128;
            } else if (typeUp == "DATE") {
                col.dataType = PgDataType::DATE;
            } else if (typeUp == "TIME") {
                col.dataType = PgDataType::TIME;
            } else if (typeUp == "TIMETZ") {
                col.dataType = PgDataType::TIME_TZ;
            } else if (typeUp == "TIMESTAMP") {
                col.dataType = PgDataType::TIMESTAMP;
            } else if (typeUp == "TIMESTAMPTZ") {
                col.dataType = PgDataType::TIMESTAMP_TZ;
            } else if (typeUp == "INTERVAL") {
                col.dataType = PgDataType::INTERVAL;
            } else if (typeUp == "JSON") {
                col.dataType  = PgDataType::JSON;
                col.maxLength = 128;
            } else if (const auto* enumDef = cat.getEnum(stripQuotes(typeTok))) {
                col.dataType       = PgDataType::ENUM;
                col.customTypeName = enumDef->typeName;
            } else {
                return fail(res, SqlState::UNDEFINED_OBJECT, cmd, draft.tableName, col.columnName,
                            "type \"" + typeTok + "\" does not exist");
            }

            if (esSerial) {
                col.identityMode      = IdentityGeneration::BY_DEFAULT;
                col.identityStart     = 1;
                col.identityIncrement = 1;
                col.isNotNull         = true;
            }

            while (cursor < static_cast<int>(d.length())) {
                String tok = readNextWord(d, cursor);
                if (tok.isEmpty()) break;
                String u = tok;
                u.toUpperCase();

                if (u == "PRIMARY") {
                    String nxt = readNextWord(d, cursor);
                    if (nxt.equalsIgnoreCase("KEY")) {
                        if (draft.hasPrimaryKey()) {
                            return fail(res, SqlState::TUPLE_ALREADY_HAS_PRIMARY_KEY, cmd, draft.tableName, col.columnName,
                                        "multiple primary keys for table \"" + draft.tableName + "\" are not allowed");
                        }
                        col.isPrimaryKey = true;
                        col.isNotNull    = true;
                        col.isUnique     = true;
                    }
                } else if (u == "NOT") {
                    String nxt = readNextWord(d, cursor);
                    if (nxt.equalsIgnoreCase("NULL")) col.isNotNull = true;
                } else if (u == "NULL") {
                    col.isNotNull = false;
                } else if (u == "UNIQUE") {
                    col.isUnique = true;
                } else if (u == "GENERATED") {
                    String t1 = readNextWord(d, cursor);
                    if (t1.equalsIgnoreCase("BY")) readNextWord(d, cursor);
                    readNextWord(d, cursor);
                    readNextWord(d, cursor);
                    col.identityMode      = t1.equalsIgnoreCase("ALWAYS") ? IdentityGeneration::ALWAYS : IdentityGeneration::BY_DEFAULT;
                    col.identityStart     = 1;
                    col.identityIncrement = 1;
                    col.isNotNull         = true;
                } else if (u == "DEFAULT") {
                    String defTok = readNextWord(d, cursor);
                    String defVal = stripQuotes(defTok);
                    col.hasDefault   = true;
                    col.defaultValue = defVal;
                    if (defVal.equalsIgnoreCase("CURRENT_TIMESTAMP") ||
                        defVal.equalsIgnoreCase("CURRENT_DATE") ||
                        defVal.equalsIgnoreCase("CURRENT_TIME")) {
                        col.autoNowAdd = true;
                    }
                } else if (u == "ON") {
                    String act = readNextWord(d, cursor);
                    if (act.equalsIgnoreCase("UPDATE")) {
                        String val = readNextWord(d, cursor);
                        if (val.equalsIgnoreCase("CURRENT_TIMESTAMP") || val.equalsIgnoreCase("NOW()")) {
                            col.autoNow = true;
                        }
                    } else if (act.equalsIgnoreCase("DELETE")) {
                        String pol = readNextWord(d, cursor);
                        if (pol.equalsIgnoreCase("CASCADE")) col.onDelete = ReferentialAction::CASCADE;
                        else if (pol.equalsIgnoreCase("RESTRICT")) col.onDelete = ReferentialAction::RESTRICT;
                        else if (pol.equalsIgnoreCase("SET")) {
                            String sub = readNextWord(d, cursor);
                            col.onDelete = sub.equalsIgnoreCase("NULL") ? ReferentialAction::SET_NULL : ReferentialAction::SET_DEFAULT;
                        } else if (pol.equalsIgnoreCase("NO")) {
                            readNextWord(d, cursor);
                            col.onDelete = ReferentialAction::NO_ACTION;
                        }
                    }
                } else if (u == "REFERENCES") {
                    String refTok = readNextWord(d, cursor);
                    String parentTbl = refTok;
                    String parentCol = "";
                    int po = refTok.indexOf('(');
                    int pc = refTok.indexOf(')');
                    if (po > 0 && pc > po) {
                        parentTbl = stripQuotes(refTok.substring(0, po));
                        parentCol = stripQuotes(refTok.substring(po + 1, pc));
                    } else {
                        String colParen = readNextWord(d, cursor);
                        if (colParen.startsWith("(") && colParen.endsWith(")")) {
                            parentCol = stripQuotes(colParen.substring(1, colParen.length() - 1));
                        }
                    }

                    const TablePhysicalSchema* pSchema = parentTbl.equalsIgnoreCase(draft.tableName)
                        ? &draft
                        : cat.getTable(parentTbl);
                    if (!pSchema) {
                        return fail(res, SqlState::UNDEFINED_TABLE, cmd, draft.tableName, col.columnName,
                                    "referenced relation \"" + parentTbl + "\" does not exist");
                    }
                    const auto* pCol = pSchema->findColumn(parentCol);
                    if (!pCol) {
                        return fail(res, SqlState::UNDEFINED_COLUMN, cmd, draft.tableName, col.columnName,
                                    "column \"" + parentCol + "\" referenced in foreign key does not exist in \"" + parentTbl + "\"");
                    }
                    if (!pCol->isPrimaryKey && !pCol->isUnique) {
                        return fail(res, SqlState::INVALID_FOREIGN_KEY, cmd, draft.tableName, col.columnName,
                                    "there is no unique constraint matching given keys for referenced table \"" + parentTbl + "\"");
                    }
                    if (!DataTypeUtils::areForeignKeyCompatible(col.dataType, col.customTypeName, pCol->dataType, pCol->customTypeName)) {
                        return fail(res, SqlState::DATATYPE_MISMATCH, cmd, draft.tableName, col.columnName,
                                    "foreign key columns \"" + col.columnName + "\" and \"" + parentCol + "\" are of incompatible types");
                    }

                    col.isForeignKey     = true;
                    col.referencedTable  = pSchema->tableName;
                    col.referencedColumn = pCol->columnName;
                    col.fkConstraintName = draft.tableName + "_" + col.columnName + "_fkey";
                } else if (u.startsWith("CHECK")) {
                    col.hasCheck = true;
                    if (tok.indexOf('(') > 0) {
                        col.checkExpression = tok;
                    } else {
                        col.checkExpression = readNextWord(d, cursor);
                    }
                }
            }

            return true;
        }

        // Normaliza SQL multi-línea: elimina comentarios (-- y /* */) y colapsa saltos/tabs/espacios fuera de comillas
        static String normalizarSql(const char* rawSql) {
            String out = "";
            if (!rawSql) return out;
            const size_t n = strlen(rawSql);
            out.reserve(n + 1);

            bool inSingle = false;
            bool inDouble = false;
            bool prevSpace = true;

            for (size_t i = 0; i < n; ++i) {
                const char c    = rawSql[i];
                const char next = (i + 1 < n) ? rawSql[i + 1] : '\0';

                if (!inSingle && !inDouble) {
                    if (c == '-' && next == '-') {
                        i += 2;
                        while (i < n && rawSql[i] != '\n') ++i;
                        if (!prevSpace) { out += ' '; prevSpace = true; }
                        continue;
                    }
                    if (c == '/' && next == '*') {
                        i += 2;
                        while (i + 1 < n && !(rawSql[i] == '*' && rawSql[i + 1] == '/')) ++i;
                        if (i + 1 < n) ++i;
                        if (!prevSpace) { out += ' '; prevSpace = true; }
                        continue;
                    }
                    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                        if (!prevSpace) {
                            out += ' ';
                            prevSpace = true;
                        }
                        continue;
                    }
                    if (c == '\'') inSingle = true;
                    else if (c == '"') inDouble = true;
                    out += c;
                    prevSpace = false;
                } else {
                    out += c;
                    if (inSingle && c == '\'') {
                        if (next == '\'') {
                            out += next;
                            ++i;
                        } else {
                            inSingle = false;
                        }
                    } else if (inDouble && c == '"') {
                        inDouble = false;
                    }
                }
            }

            out.trim();
            while (out.endsWith(";")) {
                out.remove(out.length() - 1);
                out.trim();
            }
            return out;
        }

        // --- A. CREATE / ALTER / DROP TYPE (ENUM) ---
        __attribute__((noinline))
        void execCreateType(const String& sql, const String& upper) {
            int posAsEnum = upper.indexOf(" AS ENUM");
            int pOpen     = sql.indexOf('(', posAsEnum > 0 ? posAsEnum : 0);
            int pClose    = sql.lastIndexOf(')');
            if (posAsEnum < 0 || pOpen < 0 || pClose <= pOpen) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "CREATE TYPE", "", "",
                     "syntax error in CREATE TYPE (expected: CREATE TYPE name AS ENUM ('A', 'B'))");
                return;
            }

            String header = sql.substring(12, posAsEnum);
            header.trim();
            bool ifNotExists = false;
            String hUp = header; hUp.toUpperCase();
            if (hUp.startsWith("IF NOT EXISTS ")) {
                ifNotExists = true;
                header = header.substring(14);
                header.trim();
            }

            String typeName = stripQuotes(header);
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.hasEnum(typeName)) {
                if (ifNotExists) {
                    okResult(resBuf, "CREATE TYPE", 0, "CREATE TYPE (already exists, skipping)");
                    return;
                }
                fail(resBuf, SqlState::DUPLICATE_OBJECT, "CREATE TYPE", "", "",
                     "type \"" + typeName + "\" already exists");
                return;
            }

            auto rawLabels = splitTopLevel(sql.substring(pOpen + 1, pClose), ',');
            if (rawLabels.empty()) {
                fail(resBuf, SqlState::INVALID_OBJECT_DEFINITION, "CREATE TYPE", "", "",
                     "enum type \"" + typeName + "\" must have at least one label");
                return;
            }

            EnumTypeDefinition def;
            def.typeName = typeName;
            for (const auto& l : rawLabels) {
                String clean = stripQuotes(l);
                if (def.containsLabel(clean)) {
                    fail(resBuf, SqlState::INVALID_OBJECT_DEFINITION, "CREATE TYPE", "", "",
                         "enum label \"" + clean + "\" specified more than once");
                    return;
                }
                def.labels.push_back(clean);
            }

            cat.registerEnum(def);
            flushCatalogIfAllowed();
            okResult(resBuf, "CREATE TYPE", 0, "CREATE TYPE");
        }

        __attribute__((noinline))
        void execAlterType(const String& sql, const String& upper) {
            int posAdd = upper.indexOf(" ADD VALUE ");
            if (posAdd < 0) {
                fail(resBuf, SqlState::FEATURE_NOT_SUPPORTED, "ALTER TYPE", "", "",
                     "only ALTER TYPE <enum> ADD VALUE [IF NOT EXISTS] '<label>' is supported");
                return;
            }

            String typeName = stripQuotes(sql.substring(11, posAdd));
            String valRest  = sql.substring(posAdd + 11);
            valRest.trim();

            bool ifNotExists = false;
            String vUp = valRest; vUp.toUpperCase();
            if (vUp.startsWith("IF NOT EXISTS ")) {
                ifNotExists = true;
                valRest = valRest.substring(14);
                valRest.trim();
            }

            String newLabel = stripQuotes(valRest);
            if (newLabel.isEmpty()) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "ALTER TYPE", "", "", "empty enum label in ADD VALUE");
                return;
            }

            auto& cat = PhysicalSchemaCatalog::getInstance();
            const auto* existing = cat.getEnum(typeName);
            if (!existing) {
                fail(resBuf, SqlState::UNDEFINED_OBJECT, "ALTER TYPE", "", "",
                     "type \"" + typeName + "\" does not exist");
                return;
            }

            if (existing->containsLabel(newLabel)) {
                if (ifNotExists) {
                    okResult(resBuf, "ALTER TYPE", 0, "ALTER TYPE (label already exists, skipping)");
                    return;
                }
                fail(resBuf, SqlState::DUPLICATE_OBJECT, "ALTER TYPE", "", "",
                     "enum label \"" + newLabel + "\" already exists");
                return;
            }

            if (existing->labels.size() >= 254) {
                fail(resBuf, SqlState::INVALID_OBJECT_DEFINITION, "ALTER TYPE", "", "",
                     "enum reached maximum 254 labels in 1-byte binary encoding");
                return;
            }

            EnumTypeDefinition updated = *existing;
            updated.labels.push_back(newLabel);
            cat.registerEnum(updated);
            flushCatalogIfAllowed();
            okResult(resBuf, "ALTER TYPE", 0, "ALTER TYPE");
        }

        __attribute__((noinline))
        void execDropType(const String& sql) {
            String rest = sql.substring(10); rest.trim();
            String rUp  = rest; rUp.toUpperCase();
            bool ifExists = false;
            if (rUp.startsWith("IF EXISTS ")) {
                ifExists = true;
                rest = rest.substring(10); rest.trim();
            }
            String typeName = stripQuotes(rest);

            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasEnum(typeName)) {
                if (ifExists) {
                    okResult(resBuf, "DROP TYPE", 0, "DROP TYPE (does not exist, skipping)");
                    return;
                }
                fail(resBuf, SqlState::UNDEFINED_OBJECT, "DROP TYPE", "", "",
                     "type \"" + typeName + "\" does not exist");
                return;
            }

            for (const auto& kv : cat.getAllTables()) {
                for (const auto& col : kv.second.columns) {
                    if (col.dataType == PgDataType::ENUM && col.customTypeName.equalsIgnoreCase(typeName)) {
                        fail(resBuf, SqlState::DEPENDENT_OBJECTS_STILL_EXIST, "DROP TYPE", kv.second.tableName, col.columnName,
                             "cannot drop type \"" + typeName + "\" because column \"" + col.columnName + "\" depends on it");
                        return;
                    }
                }
            }

            cat.removeEnum(typeName);
            flushCatalogIfAllowed();
            okResult(resBuf, "DROP TYPE", 0, "DROP TYPE");
        }

        // --- B. CREATE TABLE [IF NOT EXISTS] <tabla> (...) ---
        __attribute__((noinline))
        void execCreateTable(const String& sql) {
            int pOpen  = sql.indexOf('(');
            int pClose = sql.lastIndexOf(')');
            if (pOpen < 0 || pClose <= pOpen) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "CREATE TABLE", "", "",
                     "missing column definitions in CREATE TABLE");
                return;
            }

            String header = sql.substring(13, pOpen);
            header.trim();
            bool ifNotExists = false;
            String hUp = header; hUp.toUpperCase();
            if (hUp.startsWith("IF NOT EXISTS ")) {
                ifNotExists = true;
                header = header.substring(14);
                header.trim();
            }

            String tableName = stripQuotes(header);
            String reason;
            if (!PostgresLexicalValidator::isValidIdentifierFormat(header, reason) ||
                PostgresLexicalValidator::isForbiddenUnquotedIdentifier(header, false)) {
                fail(resBuf, SqlState::INVALID_NAME, "CREATE TABLE", tableName, "",
                     "invalid table name \"" + tableName + "\"");
                return;
            }

            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (cat.hasTable(tableName)) {
                if (ifNotExists) {
                    cat.ensurePhysicalTableFileExists(tableName);
                    okResult(resBuf, "CREATE TABLE", 0, "CREATE TABLE (already exists, skipping)");
                    return;
                }
                fail(resBuf, SqlState::DUPLICATE_TABLE, "CREATE TABLE", tableName, "",
                     "relation \"" + tableName + "\" already exists");
                return;
            }

            TablePhysicalSchema draft;
            draft.tableName = tableName;

            auto colDefs = splitTopLevel(sql.substring(pOpen + 1, pClose), ',');
            for (const auto& def : colDefs) {
                ColumnConstraintDef col;
                if (!parseColumnDef(def, draft, col, "CREATE TABLE", resBuf)) {
                    return;
                }
                draft.columns.push_back(std::move(col));
            }

            if (draft.columns.empty()) {
                fail(resBuf, SqlState::INVALID_TABLE_DEFINITION, "CREATE TABLE", tableName, "",
                     "table \"" + tableName + "\" must have at least one column");
                return;
            }

            draft.recomputeBinaryLayout();
            if (draft.recordSize > DbRuntime::Storage::MAX_RECORD_SIZE) {
                fail(resBuf, SqlState::INVALID_TABLE_DEFINITION, "CREATE TABLE", tableName, "",
                     "row binary size exceeds maximum 640 bytes for ESP32 binary storage");
                return;
            }

            const unsigned nCols = static_cast<unsigned>(draft.columns.size());
            const unsigned recSz = static_cast<unsigned>(draft.recordSize);
            cat.registerTable(std::move(draft));
            cat.ensurePhysicalTableFileExists(tableName);
            flushCatalogIfAllowed();

            char msg[64]{};
            snprintf(msg, sizeof(msg), "CREATE TABLE (%u cols, %u B/row)", nCols, recSz);
            okResult(resBuf, "CREATE TABLE", 0, msg);
        }

        // --- C. CREATE / DROP INDEX ---
        __attribute__((noinline))
        void execCreateIndex(const String& sql, const String& upper) {
            bool isUnique = upper.startsWith("CREATE UNIQUE INDEX ");
            int start = isUnique ? 20 : 13;
            int posOn = upper.indexOf(" ON ", start);
            int pOpen = sql.indexOf('(', posOn > 0 ? posOn : 0);
            int pClose = sql.lastIndexOf(')');

            if (posOn < 0 || pOpen < 0 || pClose <= pOpen) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "CREATE INDEX", "", "", "syntax error in CREATE INDEX");
                return;
            }

            String idxPart = sql.substring(start, posOn);
            idxPart.trim();
            bool ifNotExists = false;
            String idxUp = idxPart; idxUp.toUpperCase();
            if (idxUp.startsWith("IF NOT EXISTS ")) {
                ifNotExists = true;
                idxPart = idxPart.substring(14);
                idxPart.trim();
            }

            String idxName   = stripQuotes(idxPart);
            String tableName = stripQuotes(sql.substring(posOn + 4, pOpen));
            auto& cat = PhysicalSchemaCatalog::getInstance();

            if (cat.hasIndex(idxName)) {
                if (ifNotExists) {
                    okResult(resBuf, "CREATE INDEX", 0, "CREATE INDEX (already exists, skipping)");
                    return;
                }
                fail(resBuf, SqlState::DUPLICATE_OBJECT, "CREATE INDEX", tableName, "",
                     "relation \"" + idxName + "\" already exists");
                return;
            }

            const auto* tb = cat.getTable(tableName);
            if (!tb) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "CREATE INDEX", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            IndexDefinition idxDef;
            idxDef.indexName = idxName;
            idxDef.tableName = tb->tableName;
            idxDef.isUnique  = isUnique;

            auto cols = splitTopLevel(sql.substring(pOpen + 1, pClose), ',');
            for (const auto& c : cols) {
                String cName = stripQuotes(c);
                if (!tb->findColumn(cName)) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "CREATE INDEX", tableName, cName,
                         "column \"" + cName + "\" does not exist in table \"" + tableName + "\"");
                    return;
                }
                idxDef.keyColumns.push_back(cName);
            }

            cat.registerIndex(idxDef);
            flushCatalogIfAllowed();
            okResult(resBuf, "CREATE INDEX", 0, "CREATE INDEX");
        }

        __attribute__((noinline))
        void execDropIndex(const String& sql) {
            String rest = sql.substring(11); rest.trim();
            String rUp  = rest; rUp.toUpperCase();
            bool ifExists = false;
            if (rUp.startsWith("IF EXISTS ")) {
                ifExists = true;
                rest = rest.substring(10); rest.trim();
            }
            String idxName = stripQuotes(rest);
            auto& cat = PhysicalSchemaCatalog::getInstance();

            if (!cat.hasIndex(idxName)) {
                if (ifExists) {
                    okResult(resBuf, "DROP INDEX", 0, "DROP INDEX (does not exist, skipping)");
                    return;
                }
                fail(resBuf, SqlState::UNDEFINED_OBJECT, "DROP INDEX", "", "",
                     "index \"" + idxName + "\" does not exist");
                return;
            }

            cat.removeIndex(idxName);
            flushCatalogIfAllowed();
            okResult(resBuf, "DROP INDEX", 0, "DROP INDEX");
        }

        // --- D. META-COMANDOS PSQL (\dt, \d, \dT+, \di, \du, \dn, \?) ---
        __attribute__((noinline))
        void execShowTables() {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            auto& st  = PgBinaryStorage::getInstance();

            size_t pos = 0;
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos,
                      " List of relations (pg_catalog binary)\n"
                      " Schema |          Name          | Cols | RowB | Rows\n"
                      "--------+------------------------+------+------+------\n");

            uint16_t count = 0;
            for (const auto& kv : cat.getAllTables()) {
                PgBinHeader hdr{};
                uint32_t activeRows = 0;
                if (st.getTableHeader(kv.second.tableName.c_str(), hdr)) {
                    activeRows = hdr.activeRows;
                }
                char line[96]{};
                snprintf(line, sizeof(line), " public | %-22s | %4u | %4u | %lu\n",
                         kv.second.tableName.c_str(),
                         static_cast<unsigned>(kv.second.columns.size()),
                         static_cast<unsigned>(kv.second.recordSize),
                         static_cast<unsigned long>(activeRows));
                appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, line);
                count++;
            }
            char tail[32]{};
            snprintf(tail, sizeof(tail), "(%u rows)", static_cast<unsigned>(count));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, tail);

            resBuf.exito          = true;
            resBuf.filasAfectadas = count;
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "SHOW TABLES");
        }

        __attribute__((noinline))
        void execDescribeTable(const String& tableNameRaw) {
            String tName = stripQuotes(tableNameRaw);
            const auto* tb = PhysicalSchemaCatalog::getInstance().getTable(tName);
            if (!tb) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "\\d", tName, "",
                     "Did not find any relation named \"" + tName + "\"");
                return;
            }

            size_t pos = 0;
            char hdrLine[128]{};
            snprintf(hdrLine, sizeof(hdrLine),
                     " Table \"public.%s\" (%u B/row)\n Column | Type | Modifiers\n--------+------+----------\n",
                     tb->tableName.c_str(), static_cast<unsigned>(tb->recordSize));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, hdrLine);

            for (const auto& c : tb->columns) {
                char rowBuf[128]{};
                const char* tStr = (c.dataType == PgDataType::ENUM)
                    ? c.customTypeName.c_str()
                    : DataTypeUtils::toString(c.dataType);
                snprintf(rowBuf, sizeof(rowBuf), " %-20s | %-18s | %s%s%s\n",
                         c.columnName.c_str(),
                         tStr,
                         c.isPrimaryKey ? "PK " : "",
                         c.isNotNull ? "NOT NULL " : "",
                         c.isForeignKey ? "FK" : "");
                appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, rowBuf);
            }

            resBuf.exito          = true;
            resBuf.filasAfectadas = static_cast<uint16_t>(tb->columns.size());
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "DESCRIBE");
        }

        __attribute__((noinline))
        void execDescribeTypes(const String& filtroNombre, bool mostrarElementos) {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            String filtro = stripQuotes(filtroNombre);
            size_t pos = 0;
            uint16_t count = 0;

            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos,
                      mostrarElementos
                        ? " List of data types (ENUM)\n Name | Elements\n------+---------\n"
                        : " List of data types (ENUM)\n Schema | Name | Values\n--------+------+-------\n");

            for (const auto& kv : cat.getAllEnums()) {
                const auto& e = kv.second;
                if (!filtro.isEmpty() && !e.typeName.equalsIgnoreCase(filtro)) continue;

                if (mostrarElementos) {
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, " ");
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, e.typeName.c_str());
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, " | ");
                    for (size_t i = 0; i < e.labels.size(); ++i) {
                        if (i > 0) appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, ", ");
                        appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, e.labels[i].c_str());
                    }
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, "\n");
                } else {
                    char line[96]{};
                    snprintf(line, sizeof(line), " public | %-26s | %u\n",
                             e.typeName.c_str(), static_cast<unsigned>(e.labels.size()));
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, line);
                }
                count++;
            }

            char tail[32]{};
            snprintf(tail, sizeof(tail), "(%u rows)", static_cast<unsigned>(count));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, tail);

            resBuf.exito          = true;
            resBuf.filasAfectadas = count;
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "DESCRIBE TYPES");
        }

        __attribute__((noinline))
        void execShowIndexes() {
            auto& cat = PhysicalSchemaCatalog::getInstance();
            size_t pos = 0;
            uint16_t count = 0;
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos,
                      " List of indexes\n Name | Table | Unique | Columns\n------+-------+--------+--------\n");

            for (const auto& kv : cat.getAllIndexes()) {
                const auto& idx = kv.second;
                char line[128]{};
                String cols = "";
                for (size_t i = 0; i < idx.keyColumns.size(); ++i) {
                    if (i > 0) cols += ",";
                    cols += idx.keyColumns[i];
                }
                snprintf(line, sizeof(line), " %-24s | %-18s | %-3s | %s\n",
                         idx.indexName.c_str(),
                         idx.tableName.c_str(),
                         idx.isUnique ? "YES" : "NO",
                         cols.c_str());
                appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, line);
                count++;
            }

            char tail[32]{};
            snprintf(tail, sizeof(tail), "(%u rows)", static_cast<unsigned>(count));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, tail);

            resBuf.exito          = true;
            resBuf.filasAfectadas = count;
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "SHOW INDEXES");
        }

        __attribute__((noinline))
        void execShowUsers() {
            auto& st = PgBinaryStorage::getInstance();
            size_t pos = 0;
            uint16_t count = 0;
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos,
                      " List of roles (Usuario)\n ID | Username | Role | Active | Locked\n----+----------+------+--------+-------\n");

            st.scanForward("Usuario", [&](const PgRowView& r, uint32_t) -> bool {
                char rolBuf[20]{};
                int8_t idxRol = r.getSchema()->findColumnIndex("rol");
                r.formatCellToBuf(idxRol, rolBuf, sizeof(rolBuf));
                char line[96]{};
                snprintf(line, sizeof(line), " %2ld | %-14s | %-8s | %-5s | %-5s\n",
                         static_cast<long>(r.getInt32("id_usuario")),
                         r.getCStr("usuario"),
                         rolBuf,
                         r.getBool("activa", true) ? "TRUE" : "FALSE",
                         r.getBool("bloqueado", false) ? "TRUE" : "FALSE");
                appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, line);
                count++;
                return pos < 920;
            });

            char tail[32]{};
            snprintf(tail, sizeof(tail), "(%u rows)", static_cast<unsigned>(count));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), pos, tail);

            resBuf.exito          = true;
            resBuf.filasAfectadas = count;
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "SHOW ROLES");
        }

        // --- F. INSERT INTO <tabla> [(cols)] VALUES (v1, ...)[, (v2, ...)] ---
        __attribute__((noinline))
        void execInsert(const String& sql, const String& upper) {
            int posValues = findKeywordOutsideQuotes(upper, " VALUES", 12);
            if (posValues < 0) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", "", "",
                     "syntax error in INSERT INTO (missing VALUES)");
                return;
            }

            String targetPart = sql.substring(12, posValues);
            targetPart.trim();

            String tableName = "";
            std::vector<String> cols;
            int p1Open  = targetPart.indexOf('(');
            int p1Close = targetPart.lastIndexOf(')');

            if (p1Open > 0 && p1Close > p1Open) {
                tableName = stripQuotes(targetPart.substring(0, p1Open));
                cols      = splitTopLevel(targetPart.substring(p1Open + 1, p1Close), ',');
            } else {
                tableName = stripQuotes(targetPart);
            }

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "INSERT", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            std::vector<int8_t> colIndices;
            if (cols.empty()) {
                for (size_t i = 0; i < schema->columns.size(); ++i) {
                    colIndices.push_back(static_cast<int8_t>(i));
                }
            } else {
                for (const auto& c : cols) {
                    String cName = stripQuotes(c);
                    int8_t idx = schema->findColumnIndex(cName);
                    if (idx < 0) {
                        fail(resBuf, SqlState::UNDEFINED_COLUMN, "INSERT", schema->tableName, cName,
                             "column \"" + cName + "\" of relation \"" + schema->tableName + "\" does not exist");
                        return;
                    }
                    colIndices.push_back(idx);
                }
            }

            String valuesTail = sql.substring(posValues + 7);
            valuesTail.trim();
            auto tuples = splitTopLevel(valuesTail, ',');
            if (tuples.empty()) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", schema->tableName, "",
                     "empty VALUES list in INSERT INTO");
                return;
            }

            auto& st = PgBinaryStorage::getInstance();
            uint16_t inserted = 0;

            for (const auto& tupRaw : tuples) {
                String t = tupRaw;
                t.trim();
                if (!t.startsWith("(") || !t.endsWith(")")) {
                    fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", schema->tableName, "",
                         "invalid tuple in VALUES: \"" + t + "\"");
                    return;
                }
                auto vals = splitTopLevel(t.substring(1, t.length() - 1), ',');
                if (vals.size() != colIndices.size()) {
                    fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", schema->tableName, "",
                         "INSERT has more target columns than expressions or vice versa");
                    return;
                }

                PgRowBuffer row(schema);
                for (size_t i = 0; i < vals.size(); ++i) {
                    const int8_t idx = colIndices[i];
                    String vLit = stripQuotes(vals[i]);
                    if (vLit.equalsIgnoreCase("DEFAULT")) continue;
                    String stErr, msgErr;
                    if (!row.setFromLiteralIdx(idx, vLit.c_str(), &stErr, &msgErr)) {
                        fail(resBuf, stErr.c_str(), "INSERT", schema->tableName,
                             schema->columns[static_cast<size_t>(idx)].columnName, msgErr);
                        return;
                    }
                }

                row.applySchemaDefaults(true);
                PgRowView rv = row.view();

                for (size_t i = 0; i < schema->columns.size(); ++i) {
                    const auto& col = schema->columns[i];
                    const int idx = static_cast<int>(i);
                    if (col.identityMode != IdentityGeneration::NONE && !row.wasProvidedIdx(idx)) {
                        continue;
                    }
                    if (col.isNotNull && rv.isNullIdx(idx)) {
                        fail(resBuf, SqlState::NOT_NULL_VIOLATION, "INSERT", schema->tableName, col.columnName,
                             "null value in column \"" + col.columnName + "\" violates not-null constraint");
                        return;
                    }
                    if ((col.isUnique || col.isPrimaryKey) && !rv.isNullIdx(idx)) {
                        char valBuf[96]{};
                        rv.formatCellToBuf(idx, valBuf, sizeof(valBuf));
                        bool dup = false;
                        st.scanForward(schema->tableName.c_str(), [&](const PgRowView& existing, uint32_t) -> bool {
                            if (!existing.isNullIdx(idx) && existing.compareCellWithLiteral(idx, valBuf) == 0) {
                                dup = true;
                                return false;
                            }
                            return true;
                        });
                        if (dup) {
                            fail(resBuf, SqlState::UNIQUE_VIOLATION, "INSERT", schema->tableName, col.columnName,
                                 "duplicate key value violates unique constraint on \"" + col.columnName + "\"");
                            return;
                        }
                    }
                    if (col.isForeignKey && !rv.isNullIdx(idx)) {
                        const TablePhysicalSchema* pSchema = PhysicalSchemaCatalog::getInstance().getTable(col.referencedTable);
                        const int8_t pIdx = pSchema ? pSchema->findColumnIndex(col.referencedColumn) : -1;
                        if (pSchema && pIdx >= 0) {
                            char valBuf[64]{};
                            rv.formatCellToBuf(idx, valBuf, sizeof(valBuf));
                            bool foundParent = false;
                            st.scanForward(pSchema->tableName.c_str(), [&](const PgRowView& pr, uint32_t) -> bool {
                                if (!pr.isNullIdx(pIdx) && pr.compareCellWithLiteral(pIdx, valBuf) == 0) {
                                    foundParent = true;
                                    return false;
                                }
                                return true;
                            });
                            if (!foundParent) {
                                fail(resBuf, SqlState::FOREIGN_KEY_VIOLATION, "INSERT", schema->tableName, col.columnName,
                                     "insert on table \"" + schema->tableName + "\" violates foreign key constraint \"" + col.fkConstraintName + "\"");
                                return;
                            }
                        }
                    }
                }

                if (!st.appendRecord(schema->tableName.c_str(), row)) {
                    fail(resBuf, SqlState::IO_ERROR, "INSERT", schema->tableName, "", "could not append binary record to MicroSD");
                    return;
                }
                inserted++;
            }

            char msg[32]{};
            snprintf(msg, sizeof(msg), "INSERT 0 %u", static_cast<unsigned>(inserted));
            okResult(resBuf, "INSERT", inserted, msg);
        }

        // --- G. UPDATE <tabla> SET col=expr, ... [WHERE ...] ---
        __attribute__((noinline))
        void execUpdate(const String& sql) {
            int posSet = findKeywordOutsideQuotes(sql, " SET ", 7);
            if (posSet < 0) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "UPDATE", "", "", "missing SET clause in UPDATE");
                return;
            }

            String tableName = stripQuotes(sql.substring(7, posSet));
            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "UPDATE", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            int posWhere = findKeywordOutsideQuotes(sql, " WHERE ", posSet + 5);
            String setPart   = (posWhere > 0) ? sql.substring(posSet + 5, posWhere) : sql.substring(posSet + 5);
            String wherePart = (posWhere > 0) ? sql.substring(posWhere + 7) : "";

            struct Assign {
                int8_t idx = -1;
                String rawExpr = "";
                bool   esArit = false;
                int8_t lhsCol = -1;
                char   op     = 0;
                double rhsNum = 0.0;
            };
            std::vector<Assign> assigns;

            for (const auto& item : splitTopLevel(setPart, ',')) {
                int eq = item.indexOf('=');
                if (eq <= 0) {
                    fail(resBuf, SqlState::SYNTAX_ERROR, "UPDATE", schema->tableName, "",
                         "invalid SET assignment: \"" + item + "\"");
                    return;
                }
                String cName = stripQuotes(item.substring(0, eq));
                int8_t idx   = schema->findColumnIndex(cName);
                if (idx < 0) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "UPDATE", schema->tableName, cName,
                         "column \"" + cName + "\" does not exist");
                    return;
                }

                String rhs = item.substring(eq + 1);
                rhs.trim();

                Assign a{};
                a.idx     = idx;
                a.rawExpr = stripQuotes(rhs);

                // Soporte para expresiones aritméticas simples (ej. SET umbral_sup = umbral_sup + 1.5)
                if (!rhs.startsWith("'") && !rhs.startsWith("\"")) {
                    const char ops[4] = { '+', '-', '*', '/' };
                    for (char op : ops) {
                        int p = rhs.indexOf(op, 1);
                        if (p > 0) {
                            String leftTok  = stripQuotes(rhs.substring(0, p));
                            String rightTok = rhs.substring(p + 1);
                            rightTok.trim();
                            int8_t lIdx = schema->findColumnIndex(leftTok);
                            if (lIdx >= 0 && !rightTok.isEmpty()) {
                                char* endP = nullptr;
                                double num = strtod(rightTok.c_str(), &endP);
                                if (endP != rightTok.c_str() && *endP == '\0') {
                                    a.esArit = true;
                                    a.lhsCol = lIdx;
                                    a.op     = op;
                                    a.rhsNum = num;
                                    break;
                                }
                            }
                        }
                    }
                }

                assigns.push_back(std::move(a));
            }

            std::vector<BoundTable> whereTables;
            std::vector<ExprNode>   whereNodes;
            int16_t                 whereRoot = -1;
            if (!parseSingleTableWhere(schema, wherePart, whereTables, whereNodes, whereRoot, "UPDATE", resBuf)) {
                return;
            }

            // Pre-validación de literales (ENUM, NOT NULL, tipos)
            for (const auto& a : assigns) {
                if (a.esArit) continue;
                PgRowBuffer probe(schema);
                String stErr, msgErr;
                if (!probe.setFromLiteralIdx(a.idx, a.rawExpr.c_str(), &stErr, &msgErr)) {
                    fail(resBuf, stErr.c_str(), "UPDATE", schema->tableName,
                         schema->columns[static_cast<size_t>(a.idx)].columnName, msgErr);
                    return;
                }
            }

            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                schema->tableName.c_str(),
                [&](const PgRowView& r) -> bool {
                    if (whereRoot < 0) return true;
                    RowBinding rb{};
                    rb.physRow = &r;
                    return evalExprNode(whereRoot, whereNodes, whereTables, &rb).toBool();
                },
                [&](PgRowBuffer& row) {
                    PgRowView cur = row.view();
                    for (const auto& a : assigns) {
                        if (a.esArit) {
                            const auto& col = schema->columns[static_cast<size_t>(a.idx)];
                            double base = DataTypeUtils::isIntegerType(schema->columns[static_cast<size_t>(a.lhsCol)].dataType)
                                ? static_cast<double>(cur.getInt64Idx(a.lhsCol))
                                : cur.getDoubleIdx(a.lhsCol);
                            double resVal = base;
                            if (a.op == '+') resVal = base + a.rhsNum;
                            else if (a.op == '-') resVal = base - a.rhsNum;
                            else if (a.op == '*') resVal = base * a.rhsNum;
                            else if (a.op == '/' && fabs(a.rhsNum) > 1e-12) resVal = base / a.rhsNum;

                            if (DataTypeUtils::isIntegerType(col.dataType)) {
                                row.setInt64Idx(a.idx, static_cast<int64_t>(llround(resVal)));
                            } else {
                                row.setDoubleIdx(a.idx, resVal);
                            }
                        } else {
                            row.setFromLiteralIdx(a.idx, a.rawExpr.c_str());
                        }
                    }
                }
            );

            char msg[32]{};
            snprintf(msg, sizeof(msg), "UPDATE %u", static_cast<unsigned>(n));
            okResult(resBuf, "UPDATE", n, msg);
        }

        // --- H. DELETE FROM <tabla> [WHERE ...] ---
        __attribute__((noinline))
        void execDelete(const String& sql) {
            int posWhere     = findKeywordOutsideQuotes(sql, " WHERE ", 12);
            String tableName = stripQuotes((posWhere > 0) ? sql.substring(12, posWhere) : sql.substring(12));
            String wherePart = (posWhere > 0) ? sql.substring(posWhere + 7) : "";

            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "DELETE", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            std::vector<BoundTable> whereTables;
            std::vector<ExprNode>   whereNodes;
            int16_t                 whereRoot = -1;
            if (!parseSingleTableWhere(schema, wherePart, whereTables, whereNodes, whereRoot, "DELETE", resBuf)) {
                return;
            }

            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                schema->tableName.c_str(),
                [&](const PgRowView& r) -> bool {
                    if (whereRoot < 0) return true;
                    RowBinding rb{};
                    rb.physRow = &r;
                    return evalExprNode(whereRoot, whereNodes, whereTables, &rb).toBool();
                },
                true
            );

            char msg[32]{};
            snprintf(msg, sizeof(msg), "DELETE %u", static_cast<unsigned>(n));
            okResult(resBuf, "DELETE", n, msg);
        }

        // --- I. DROP TABLE / TRUNCATE / VACUUM / ALTER TABLE ---
        __attribute__((noinline))
        void execDropTable(const String& sql) {
            String rest = sql.substring(11); rest.trim();
            String rUp = rest; rUp.toUpperCase();

            bool ifExists = false;
            if (rUp.startsWith("IF EXISTS ")) {
                ifExists = true;
                rest = rest.substring(10); rest.trim();
                rUp = rest; rUp.toUpperCase();
            }
            bool cascade = false;
            if (rUp.endsWith(" CASCADE")) {
                cascade = true;
                rest = rest.substring(0, rest.length() - 8); rest.trim();
            } else if (rUp.endsWith(" RESTRICT")) {
                rest = rest.substring(0, rest.length() - 9); rest.trim();
            }

            String tName = stripQuotes(rest);
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasTable(tName)) {
                if (ifExists) {
                    okResult(resBuf, "DROP TABLE", 0, "DROP TABLE (does not exist, skipping)");
                    return;
                }
                fail(resBuf, SqlState::UNDEFINED_TABLE, "DROP TABLE", tName, "",
                     "table \"" + tName + "\" does not exist");
                return;
            }

            auto deps = cat.findDependentTables(tName);
            if (!deps.empty() && !cascade) {
                fail(resBuf, SqlState::DEPENDENT_OBJECTS_STILL_EXIST, "DROP TABLE", tName, "",
                     "cannot drop table \"" + tName + "\" because table \"" + deps[0] + "\" depends on it",
                     "Use DROP TABLE ... CASCADE.");
                return;
            }
            if (cascade) {
                for (const auto& dep : deps) {
                    TablePhysicalSchema* child = cat.getTableMutable(dep);
                    if (!child) continue;
                    for (auto& c : child->columns) {
                        if (c.isForeignKey && c.referencedTable.equalsIgnoreCase(tName)) {
                            c.isForeignKey = false;
                            c.referencedTable = "";
                            c.referencedColumn = "";
                        }
                    }
                }
            }

            String dataPath = cat.tableDataFilePath(tName);
            cat.removeIndexesOfTable(tName);
            cat.removeTable(tName);
            {
                auto lk = PgBinaryStorage::lockEngine();
                if (SD.exists(dataPath.c_str())) SD.remove(dataPath.c_str());
            }
            flushCatalogIfAllowed();
            okResult(resBuf, "DROP TABLE", 0, "DROP TABLE");
        }

        __attribute__((noinline))
        void execTruncate(const String& sql) {
            String rest = sql.substring(9); rest.trim();
            String rUp = rest; rUp.toUpperCase();
            if (rUp.startsWith("TABLE ")) {
                rest = rest.substring(6); rest.trim();
                rUp = rest; rUp.toUpperCase();
            }
            bool cascade = rUp.endsWith(" CASCADE");
            if (cascade) { rest = rest.substring(0, rest.length() - 8); rest.trim(); }
            else if (rUp.endsWith(" RESTRICT")) { rest = rest.substring(0, rest.length() - 9); rest.trim(); }

            String tName = stripQuotes(rest);
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasTable(tName)) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "TRUNCATE", tName, "",
                     "table \"" + tName + "\" does not exist");
                return;
            }
            auto deps = cat.findDependentTables(tName);
            if (!deps.empty() && !cascade) {
                fail(resBuf, SqlState::FEATURE_NOT_SUPPORTED, "TRUNCATE", tName, "",
                     "cannot truncate a table referenced in a foreign key constraint",
                     "Use TRUNCATE TABLE ... CASCADE.");
                return;
            }

            auto& st = PgBinaryStorage::getInstance();
            st.truncateTable(tName.c_str());
            if (cascade) {
                for (const auto& dep : deps) st.truncateTable(dep.c_str());
            }
            okResult(resBuf, "TRUNCATE TABLE", 0, "TRUNCATE TABLE");
        }

        __attribute__((noinline))
        void execVacuum(const String& sql) {
            String tName = stripQuotes(sql.substring(7));
            auto& cat = PhysicalSchemaCatalog::getInstance();
            if (!cat.hasTable(tName)) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "VACUUM", tName, "",
                     "table \"" + tName + "\" does not exist");
                return;
            }
            if (!PgBinaryStorage::getInstance().compactTable(tName.c_str())) {
                fail(resBuf, SqlState::IO_ERROR, "VACUUM", tName, "", "could not compact binary table file");
                return;
            }
            PgBinHeader hdr{};
            PgBinaryStorage::getInstance().getTableHeader(tName.c_str(), hdr);
            char msg[64]{};
            snprintf(msg, sizeof(msg), "VACUUM (%lu active rows)", static_cast<unsigned long>(hdr.activeRows));
            okResult(resBuf, "VACUUM", static_cast<uint16_t>(hdr.activeRows), msg);
        }

        __attribute__((noinline))
        void execAlterTable(const String& sql) {
            String rest = sql.substring(12); rest.trim();
            int sp = rest.indexOf(' ');
            if (sp <= 0) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "ALTER TABLE", "", "", "syntax error in ALTER TABLE");
                return;
            }

            String tableName = stripQuotes(rest.substring(0, sp));
            String opRest    = rest.substring(sp + 1); opRest.trim();
            String opUp      = opRest; opUp.toUpperCase();

            auto& cat = PhysicalSchemaCatalog::getInstance();
            TablePhysicalSchema* tb = cat.getTableMutable(tableName);
            if (!tb) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "ALTER TABLE", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            if (opUp.startsWith("ADD COLUMN ")) {
                TablePhysicalSchema oldSchema = *tb;
                ColumnConstraintDef newCol;
                if (!parseColumnDef(opRest.substring(11), *tb, newCol, "ALTER TABLE", resBuf)) {
                    return;
                }
                tb->columns.push_back(std::move(newCol));
                tb->recomputeBinaryLayout();
                if (tb->recordSize > DbRuntime::Storage::MAX_RECORD_SIZE ||
                    !PgBinaryStorage::getInstance().migrateTableSchema(oldSchema, *tb)) {
                    *tb = oldSchema;
                    fail(resBuf, SqlState::IO_ERROR, "ALTER TABLE", tableName, "",
                         "failed migrating binary file after ADD COLUMN");
                    return;
                }
                flushCatalogIfAllowed();
                okResult(resBuf, "ALTER TABLE", 0, "ALTER TABLE");
                return;
            }

            if (opUp.startsWith("DROP COLUMN ")) {
                String colName = stripQuotes(opRest.substring(12));
                int8_t idx = tb->findColumnIndex(colName);
                if (idx < 0) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "ALTER TABLE", tableName, colName,
                         "column \"" + colName + "\" does not exist");
                    return;
                }
                if (tb->columns.size() <= 1) {
                    fail(resBuf, SqlState::INVALID_TABLE_DEFINITION, "ALTER TABLE", tableName, colName,
                         "cannot drop the only column of table \"" + tableName + "\"");
                    return;
                }
                TablePhysicalSchema oldSchema = *tb;
                tb->columns.erase(tb->columns.begin() + idx);
                tb->recomputeBinaryLayout();
                if (!PgBinaryStorage::getInstance().migrateTableSchema(oldSchema, *tb)) {
                    *tb = oldSchema;
                    fail(resBuf, SqlState::IO_ERROR, "ALTER TABLE", tableName, colName,
                         "failed migrating binary file after DROP COLUMN");
                    return;
                }
                flushCatalogIfAllowed();
                okResult(resBuf, "ALTER TABLE", 0, "ALTER TABLE");
                return;
            }

            if (opUp.startsWith("RENAME COLUMN ")) {
                String rem = opRest.substring(14); rem.trim();
                String remUp = rem; remUp.toUpperCase();
                int posTo = remUp.indexOf(" TO ");
                if (posTo <= 0) {
                    fail(resBuf, SqlState::SYNTAX_ERROR, "ALTER TABLE", tableName, "",
                         "expected: ALTER TABLE t RENAME COLUMN old_col TO new_col");
                    return;
                }
                String oldCol = stripQuotes(rem.substring(0, posTo));
                String newCol = stripQuotes(rem.substring(posTo + 4));
                int8_t idx = tb->findColumnIndex(oldCol);
                if (idx < 0) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "ALTER TABLE", tableName, oldCol,
                         "column \"" + oldCol + "\" does not exist");
                    return;
                }
                if (tb->findColumnIndex(newCol) >= 0) {
                    fail(resBuf, SqlState::DUPLICATE_COLUMN, "ALTER TABLE", tableName, newCol,
                         "column \"" + newCol + "\" already exists");
                    return;
                }
                tb->columns[static_cast<size_t>(idx)].columnName = newCol;
                flushCatalogIfAllowed();
                okResult(resBuf, "ALTER TABLE", 0, "ALTER TABLE");
                return;
            }

            if (opUp.startsWith("RENAME TO ")) {
                String newName = stripQuotes(opRest.substring(10));
                if (cat.hasTable(newName)) {
                    fail(resBuf, SqlState::DUPLICATE_TABLE, "ALTER TABLE", newName, "",
                         "relation \"" + newName + "\" already exists");
                    return;
                }
                String oldPath = cat.tableDataFilePath(tb->tableName);
                String newPath = cat.tableDataFilePath(newName);
                TablePhysicalSchema copy = *tb;
                String oldName = tb->tableName;
                copy.tableName = newName;
                cat.removeTable(oldName);
                cat.registerTable(std::move(copy));
                {
                    auto lk = PgBinaryStorage::lockEngine();
                    if (SD.exists(oldPath.c_str())) SD.rename(oldPath.c_str(), newPath.c_str());
                }
                flushCatalogIfAllowed();
                okResult(resBuf, "ALTER TABLE", 0, "ALTER TABLE");
                return;
            }

            fail(resBuf, SqlState::FEATURE_NOT_SUPPORTED, "ALTER TABLE", tableName, "",
                 "ALTER TABLE clause not supported (use ADD COLUMN, DROP COLUMN, RENAME COLUMN, RENAME TO)");
        }

    public:
        static PgSqlEngine& getInstance() {
            static PgSqlEngine instance;
            return instance;
        }

        void setDeferredCatalogFlush(bool defer) { deferCatalogFlush = defer; }

        __attribute__((noinline))
        const SqlExecResult& execute(const char* rawSql) {
            resBuf = SqlExecResult{};
            const int64_t t0 = esp_timer_get_time();

            String sql = normalizarSql(rawSql);
            if (sql.isEmpty()) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "EMPTY", "", "", "empty query string");
                resBuf.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                return resBuf;
            }

            String upper = sql;
            upper.toUpperCase();

            if (upper == "SHOW TABLES" || sql == "\\dt") {
                execShowTables();
            } else if (sql.startsWith("\\dT+")) {
                execDescribeTypes(sql.substring(4), true);
            } else if (sql == "\\dT" || sql.startsWith("\\dT ")) {
                execDescribeTypes(sql.substring(3), false);
            } else if (upper == "\\DI") {
                execShowIndexes();
            } else if (upper == "\\DU") {
                execShowUsers();
            } else if (upper == "\\DN") {
                okResult(resBuf, "SCHEMAS", 3,
                         " List of schemas\n Name\n--------------------\n public\n pg_catalog\n information_schema\n(3 rows)");
            } else if (upper == "\\?") {
                okResult(resBuf, "HELP", 0,
                         "Meta: \\dt (tables), \\d <tbl>, \\dT[+] [enum], \\di (indexes), \\du (roles), \\dn (schemas)\n"
                         "DQL:  SELECT [DISTINCT] ... [FROM t [JOIN ...]] [WHERE ...] [GROUP BY ...] [HAVING ...] [ORDER BY ...] [LIMIT n OFFSET m]\n"
                         "DML:  INSERT INTO ... VALUES (...), (...); UPDATE ... SET c=expr; DELETE FROM ...\n"
                         "DDL:  CREATE/ALTER/DROP TYPE, CREATE/ALTER/DROP TABLE, CREATE/DROP INDEX, TRUNCATE, VACUUM");
            } else if (upper.startsWith("\\D ")) {
                execDescribeTable(sql.substring(3));
            } else if (upper.startsWith("SELECT ")) {
                uint16_t filas = 0;
                const bool ok = PgSqlDqlEngine::executeSelect(
                    sql,
                    resBuf.salidaPsql,
                    sizeof(resBuf.salidaPsql),
                    filas,
                    [&](const char* st, const char* cmd, const String& tbl, const String& col, const String& msg) {
                        fail(resBuf, st, cmd, tbl, col, msg);
                    },
                    &appendFmt
                );
                if (ok) {
                    resBuf.exito          = true;
                    resBuf.filasAfectadas = filas;
                    copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
                    copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "SELECT");
                }
            } else if (upper.startsWith("INSERT INTO ")) {
                execInsert(sql, upper);
            } else if (upper.startsWith("UPDATE ")) {
                execUpdate(sql);
            } else if (upper.startsWith("DELETE FROM ")) {
                execDelete(sql);
            } else if (upper.startsWith("CREATE TYPE ")) {
                execCreateType(sql, upper);
            } else if (upper.startsWith("ALTER TYPE ")) {
                execAlterType(sql, upper);
            } else if (upper.startsWith("DROP TYPE ")) {
                execDropType(sql);
            } else if (upper.startsWith("CREATE TABLE ")) {
                execCreateTable(sql);
            } else if (upper.startsWith("CREATE INDEX ") || upper.startsWith("CREATE UNIQUE INDEX ")) {
                execCreateIndex(sql, upper);
            } else if (upper.startsWith("DROP INDEX ")) {
                execDropIndex(sql);
            } else if (upper.startsWith("DROP TABLE ")) {
                execDropTable(sql);
            } else if (upper.startsWith("TRUNCATE ")) {
                execTruncate(sql);
            } else if (upper.startsWith("VACUUM ")) {
                execVacuum(sql);
            } else if (upper.startsWith("ALTER TABLE ")) {
                execAlterTable(sql);
            } else {
                fail(resBuf, SqlState::FEATURE_NOT_SUPPORTED, "SQL", "", "",
                     "comando SQL no soportado",
                     "Use \\? para listar comandos SQL y meta-comandos psql activos.");
            }

            resBuf.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
            return resBuf;
        }
    };

} // namespace DbSql