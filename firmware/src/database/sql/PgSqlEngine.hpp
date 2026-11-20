#pragma once
#include <Arduino.h>
#include <SD.h>
#include <esp_timer.h>
#include <vector>
#include <algorithm>
#include <cstring>
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
    using DbCommon::Standard::SortDirection;
    using DbCommon::Standard::SqlState;
    using DbNotRuntime::Standard::PostgresLexicalValidator;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::LogicConnector;
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
        struct SimplePred {
            int8_t         colIdx    = -1;
            FilterOp       op        = FilterOp::EQ;
            LogicConnector connector = LogicConnector::AND_OP;
            char           val[64]{};
        };

        struct SlotSortKey {
            uint32_t slotIdx = 0;
            uint8_t  kind    = 0; // 0=NULL, 1=INT64, 2=DOUBLE, 3=TEXT
            union {
                int64_t i64;
                double  f64;
            } num{};
            char txt[20]{};
        };

        // Buffer de salida persistente en la instancia: elimina >5 KB de stack en execute()
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

        static bool matchLike(const char* str, const char* pat, bool caseInsensitive) {
            while (*pat) {
                if (*pat == '%') {
                    while (*pat == '%') pat++;
                    if (!*pat) return true;
                    while (*str) {
                        if (matchLike(str, pat, caseInsensitive)) return true;
                        str++;
                    }
                    return false;
                }
                if (!*str) return false;
                if (*pat != '_') {
                    char a = caseInsensitive ? static_cast<char>(tolower(*str)) : *str;
                    char b = caseInsensitive ? static_cast<char>(tolower(*pat)) : *pat;
                    if (a != b) return false;
                }
                str++;
                pat++;
            }
            return *str == '\0';
        }

        static int findKeywordOutsideQuotes(const String& src, const char* kwUpper, int fromIdx = 0) {
            const size_t kwLen = strlen(kwUpper);
            bool inS = false, inD = false;
            for (int i = fromIdx; i + static_cast<int>(kwLen) <= static_cast<int>(src.length()); ++i) {
                char c = src[i];
                if (c == '\'' && !inD) inS = !inS;
                else if (c == '"' && !inS) inD = !inD;
                if (!inS && !inD) {
                    if (strncasecmp(src.c_str() + i, kwUpper, kwLen) == 0) {
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

        static bool parseWhereClause(
            const TablePhysicalSchema& schema,
            const String& whereStr,
            std::vector<SimplePred>& outPreds,
            const char* cmd,
            SqlExecResult& errRes
        ) {
            String w = whereStr;
            w.trim();
            if (w.isEmpty()) return true;

            int pos = 0;
            LogicConnector nextConn = LogicConnector::AND_OP;

            while (pos < static_cast<int>(w.length())) {
                int pAnd = findKeywordOutsideQuotes(w, " AND ", pos);
                int pOr  = findKeywordOutsideQuotes(w, " OR ",  pos);
                int cut  = w.length();
                LogicConnector afterConn = LogicConnector::AND_OP;
                int skipLen = 0;

                if (pAnd >= 0 && pAnd < cut) { cut = pAnd; afterConn = LogicConnector::AND_OP; skipLen = 5; }
                if (pOr  >= 0 && pOr  < cut) { cut = pOr;  afterConn = LogicConnector::OR_OP;  skipLen = 4; }

                String part = w.substring(pos, cut);
                part.trim();
                pos = (skipLen > 0) ? (cut + skipLen) : static_cast<int>(w.length());

                if (part.isEmpty()) {
                    nextConn = afterConn;
                    continue;
                }

                SimplePred pred{};
                pred.connector = nextConn;
                nextConn = afterConn;

                String partUp = part;
                partUp.toUpperCase();

                int idxNull = partUp.indexOf(" IS NOT NULL");
                if (idxNull > 0) {
                    String col = stripQuotes(part.substring(0, idxNull));
                    pred.colIdx = schema.findColumnIndex(col);
                    pred.op     = FilterOp::IS_NOT_NULL;
                } else if ((idxNull = partUp.indexOf(" IS NULL")) > 0) {
                    String col = stripQuotes(part.substring(0, idxNull));
                    pred.colIdx = schema.findColumnIndex(col);
                    pred.op     = FilterOp::IS_NULL;
                } else {
                    const char* ops[]  = { ">=", "<=", "<>", "!=", "=", ">", "<", " ILIKE ", " LIKE " };
                    FilterOp    fOps[] = { FilterOp::GTE, FilterOp::LTE, FilterOp::NEQ, FilterOp::NEQ,
                                           FilterOp::EQ,  FilterOp::GT,  FilterOp::LT,  FilterOp::ILIKE, FilterOp::LIKE };
                    bool matched = false;
                    for (size_t k = 0; k < 9; ++k) {
                        int opPos = findKeywordOutsideQuotes(part, ops[k], 0);
                        if (opPos > 0) {
                            String col = stripQuotes(part.substring(0, opPos));
                            String val = stripQuotes(part.substring(opPos + static_cast<int>(strlen(ops[k]))));
                            pred.colIdx = schema.findColumnIndex(col);
                            pred.op     = fOps[k];
                            copyStr(pred.val, sizeof(pred.val), val.c_str());
                            matched = true;
                            break;
                        }
                    }
                    if (!matched) {
                        return fail(errRes, SqlState::SYNTAX_ERROR, cmd, schema.tableName, "",
                                    "syntax error in WHERE condition: \"" + part + "\"");
                    }
                }

                if (pred.colIdx < 0) {
                    return fail(errRes, SqlState::UNDEFINED_COLUMN, cmd, schema.tableName, "",
                                "column referenced in WHERE clause does not exist: \"" + part + "\"");
                }
                outPreds.push_back(pred);
            }
            return true;
        }

        static bool evalPreds(const PgRowView& row, const std::vector<SimplePred>& preds) {
            if (preds.empty()) return true;
            bool acc = true;
            for (size_t i = 0; i < preds.size(); ++i) {
                const auto& p = preds[i];
                bool cur = false;
                const bool isN = row.isNullIdx(p.colIdx);

                if (p.op == FilterOp::IS_NULL) {
                    cur = isN;
                } else if (p.op == FilterOp::IS_NOT_NULL) {
                    cur = !isN;
                } else if (!isN) {
                    switch (p.op) {
                        case FilterOp::EQ:    cur = (row.compareCellWithLiteral(p.colIdx, p.val) == 0); break;
                        case FilterOp::NEQ:   cur = (row.compareCellWithLiteral(p.colIdx, p.val) != 0); break;
                        case FilterOp::GT:    cur = (row.compareCellWithLiteral(p.colIdx, p.val) > 0);  break;
                        case FilterOp::GTE:   cur = (row.compareCellWithLiteral(p.colIdx, p.val) >= 0); break;
                        case FilterOp::LT:    cur = (row.compareCellWithLiteral(p.colIdx, p.val) < 0);  break;
                        case FilterOp::LTE:   cur = (row.compareCellWithLiteral(p.colIdx, p.val) <= 0); break;
                        case FilterOp::LIKE:  cur = matchLike(row.getCStrIdx(p.colIdx), p.val, false);  break;
                        case FilterOp::ILIKE: cur = matchLike(row.getCStrIdx(p.colIdx), p.val, true);   break;
                        default: cur = false; break;
                    }
                }

                if (i == 0) acc = cur;
                else if (p.connector == LogicConnector::OR_OP) acc = acc || cur;
                else acc = acc && cur;
            }
            return acc;
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

        // --- A. CREATE TYPE [IF NOT EXISTS] <nombre> AS ENUM (...) ---
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

        // --- C. CREATE [UNIQUE] INDEX [IF NOT EXISTS] <idx> ON <tabla> (c1, c2) ---
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

        // --- D. SHOW TABLES / \dt / \d <tabla> ---
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

        // --- E. SELECT ... FROM ... [WHERE ...] [ORDER BY ...] [LIMIT ...] [OFFSET ...] ---
        __attribute__((noinline))
        void execSelect(const String& sql) {
            int posFrom = findKeywordOutsideQuotes(sql, " FROM ", 7);
            if (posFrom < 0) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "SELECT", "", "", "missing FROM-clause entry for SELECT");
                return;
            }

            String colsPart = sql.substring(7, posFrom);
            colsPart.trim();

            String tail = sql.substring(posFrom + 6);
            int posWhere  = findKeywordOutsideQuotes(tail, " WHERE ", 0);
            int posOrder  = findKeywordOutsideQuotes(tail, " ORDER BY ", 0);
            int posLimit  = findKeywordOutsideQuotes(tail, " LIMIT ", 0);
            int posOffset = findKeywordOutsideQuotes(tail, " OFFSET ", 0);

            int endTable = tail.length();
            if (posWhere  >= 0 && posWhere  < endTable) endTable = posWhere;
            if (posOrder  >= 0 && posOrder  < endTable) endTable = posOrder;
            if (posLimit  >= 0 && posLimit  < endTable) endTable = posLimit;
            if (posOffset >= 0 && posOffset < endTable) endTable = posOffset;

            String tableName = stripQuotes(tail.substring(0, endTable));
            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "SELECT", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            std::vector<SimplePred> preds;
            if (posWhere >= 0) {
                int endW = tail.length();
                if (posOrder  > posWhere && posOrder  < endW) endW = posOrder;
                if (posLimit  > posWhere && posLimit  < endW) endW = posLimit;
                if (posOffset > posWhere && posOffset < endW) endW = posOffset;
                if (!parseWhereClause(*schema, tail.substring(posWhere + 7, endW), preds, "SELECT", resBuf)) {
                    return;
                }
            }

            auto& st = PgBinaryStorage::getInstance();

            String colsUp = colsPart; colsUp.toUpperCase();
            if (colsUp == "COUNT(*)") {
                uint32_t total = 0;
                if (preds.empty()) {
                    PgBinHeader hdr{};
                    if (st.getTableHeader(schema->tableName.c_str(), hdr)) total = hdr.activeRows;
                } else {
                    st.scanForward(schema->tableName.c_str(), [&](const PgRowView& r, uint32_t) -> bool {
                        if (evalPreds(r, preds)) total++;
                        return true;
                    });
                }
                char outBuf[96]{};
                snprintf(outBuf, sizeof(outBuf), " count \n-------\n %lu\n(1 row)", static_cast<unsigned long>(total));
                okResult(resBuf, "SELECT", 1, outBuf);
                return;
            }

            int8_t projIdx[TablePhysicalSchema::MAX_COLUMNS]{};
            uint8_t nProj = 0;
            if (colsPart == "*") {
                for (size_t i = 0; i < schema->columns.size() && nProj < TablePhysicalSchema::MAX_COLUMNS; ++i) {
                    projIdx[nProj++] = static_cast<int8_t>(i);
                }
            } else {
                auto reqCols = splitTopLevel(colsPart, ',');
                for (const auto& rc : reqCols) {
                    String clean = stripQuotes(rc);
                    int8_t idx = schema->findColumnIndex(clean);
                    if (idx < 0) {
                        fail(resBuf, SqlState::UNDEFINED_COLUMN, "SELECT", schema->tableName, clean,
                             "column \"" + clean + "\" does not exist");
                        return;
                    }
                    if (nProj < TablePhysicalSchema::MAX_COLUMNS) projIdx[nProj++] = idx;
                }
            }

            int8_t orderColIdx = -1;
            SortDirection orderDir = SortDirection::ASC;
            if (posOrder >= 0) {
                int endO = tail.length();
                if (posLimit  > posOrder && posLimit  < endO) endO = posLimit;
                if (posOffset > posOrder && posOffset < endO) endO = posOffset;
                String obRaw = tail.substring(posOrder + 10, endO);
                obRaw.trim();
                String obUp = obRaw; obUp.toUpperCase();
                if (obUp.endsWith(" DESC")) {
                    orderDir = SortDirection::DESC;
                    obRaw = obRaw.substring(0, obRaw.length() - 5);
                } else if (obUp.endsWith(" ASC")) {
                    orderDir = SortDirection::ASC;
                    obRaw = obRaw.substring(0, obRaw.length() - 4);
                }
                String obCol = stripQuotes(obRaw);
                orderColIdx = schema->findColumnIndex(obCol);
                if (orderColIdx < 0) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "SELECT", schema->tableName, obCol,
                         "column \"" + obCol + "\" in ORDER BY does not exist");
                    return;
                }
            }

            uint16_t limitRows = 16;
            if (posLimit >= 0) {
                int lim = tail.substring(posLimit + 7).toInt();
                if (lim > 0 && lim <= 32) limitRows = static_cast<uint16_t>(lim);
            }
            uint32_t offsetRows = 0;
            if (posOffset >= 0) {
                long off = tail.substring(posOffset + 8).toInt();
                if (off > 0) offsetRows = static_cast<uint32_t>(off);
            }

            size_t outPos = 0;
            bool headerPrinted = false;
            bool truncated     = false;
            uint16_t shownRows = 0;

            auto emitRow = [&](const PgRowView& r) -> bool {
                if (!headerPrinted) {
                    for (uint8_t c = 0; c < nProj; ++c) {
                        if (c > 0) appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos, " | ");
                        appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos,
                                  schema->columns[static_cast<size_t>(projIdx[c])].columnName.c_str());
                    }
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos,
                              "\n------------------------------------------------\n");
                    headerPrinted = true;
                }

                if (outPos > 900) {
                    if (!truncated) {
                        appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos,
                                  "... (truncated to fit 1KB terminal buffer)\n");
                        truncated = true;
                    }
                    return false;
                }

                char cellBuf[96]{};
                for (uint8_t c = 0; c < nProj; ++c) {
                    if (c > 0) appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos, " | ");
                    r.formatCellToBuf(projIdx[c], cellBuf, sizeof(cellBuf));
                    appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos, cellBuf);
                }
                appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos, "\n");
                shownRows++;
                return true;
            };

            const bool esOrdenNatural = (orderColIdx < 0) ||
                                        (orderColIdx == schema->identityColIndex) ||
                                        (orderColIdx == schema->pkColIndex);

            if (esOrdenNatural) {
                uint32_t matched = 0;
                auto visitor = [&](const PgRowView& r, uint32_t) -> bool {
                    if (!evalPreds(r, preds)) return true;
                    if (matched >= offsetRows) {
                        if (!emitRow(r)) return false;
                        if (shownRows >= limitRows) return false;
                    }
                    matched++;
                    return true;
                };
                if (orderColIdx >= 0 && orderDir == SortDirection::DESC) {
                    st.scanBackward(schema->tableName.c_str(), visitor);
                } else {
                    st.scanForward(schema->tableName.c_str(), visitor);
                }
            } else {
                const size_t maxKeep = std::min<size_t>(static_cast<size_t>(offsetRows + limitRows), 64u);
                std::vector<SlotSortKey> keys;
                keys.reserve(std::min<size_t>(maxKeep, 32u));

                const auto& oCol = schema->columns[static_cast<size_t>(orderColIdx)];
                auto keyBefore = [&](const SlotSortKey& a, const SlotSortKey& b) -> bool {
                    if (a.kind == 0 && b.kind == 0) return a.slotIdx < b.slotIdx;
                    if (a.kind == 0) return false;
                    if (b.kind == 0) return true;
                    int cmp = 0;
                    if (a.kind == 1 && b.kind == 1) cmp = (a.num.i64 < b.num.i64) ? -1 : ((a.num.i64 > b.num.i64) ? 1 : 0);
                    else if (a.kind == 2 && b.kind == 2) cmp = (fabs(a.num.f64 - b.num.f64) < 0.00001) ? 0 : ((a.num.f64 < b.num.f64) ? -1 : 1);
                    else cmp = strcmp(a.txt, b.txt);
                    if (cmp != 0) return (orderDir == SortDirection::ASC) ? (cmp < 0) : (cmp > 0);
                    return a.slotIdx < b.slotIdx;
                };

                st.scanForward(schema->tableName.c_str(), [&](const PgRowView& r, uint32_t slotIdx) -> bool {
                    if (!evalPreds(r, preds)) return true;
                    SlotSortKey k{};
                    k.slotIdx = slotIdx;
                    if (!r.isNullIdx(orderColIdx)) {
                        if (DataTypeUtils::isIntegerType(oCol.dataType)) {
                            k.kind = 1; k.num.i64 = r.getInt64Idx(orderColIdx);
                        } else if (DataTypeUtils::isFloatingOrDecimalType(oCol.dataType)) {
                            k.kind = 2; k.num.f64 = r.getDoubleIdx(orderColIdx);
                        } else if (oCol.dataType == PgDataType::BOOLEAN || oCol.dataType == PgDataType::ENUM) {
                            k.kind = 1; k.num.i64 = r.getEnumU8Idx(orderColIdx);
                        } else {
                            k.kind = 3; copyStr(k.txt, sizeof(k.txt), r.getCStrIdx(orderColIdx));
                        }
                    }
                    if (keys.size() == maxKeep && !keyBefore(k, keys.back())) return true;
                    auto posIt = std::lower_bound(keys.begin(), keys.end(), k, keyBefore);
                    if (keys.size() < maxKeep) {
                        keys.insert(posIt, k);
                    } else if (posIt != keys.end()) {
                        keys.pop_back();
                        keys.insert(posIt, k);
                    }
                    return true;
                });

                PgRowBuffer rowBuf;
                for (size_t i = offsetRows; i < keys.size() && shownRows < limitRows; ++i) {
                    if (st.readSlot(schema->tableName.c_str(), keys[i].slotIdx, rowBuf)) {
                        if (!emitRow(rowBuf.view())) break;
                    }
                }
            }

            char foot[32]{};
            snprintf(foot, sizeof(foot), "(%u rows)", static_cast<unsigned>(shownRows));
            appendFmt(resBuf.salidaPsql, sizeof(resBuf.salidaPsql), outPos, foot);

            resBuf.exito          = true;
            resBuf.filasAfectadas = shownRows;
            copyStr(resBuf.sqlState, sizeof(resBuf.sqlState), SqlState::SUCCESSFUL_COMPLETION);
            copyStr(resBuf.comandoEjecutado, sizeof(resBuf.comandoEjecutado), "SELECT");
        }

        // --- F. INSERT INTO <tabla> (cols) VALUES (vals) ---
        __attribute__((noinline))
        void execInsert(const String& sql, const String& upper) {
            int posValues = upper.indexOf(" VALUES");
            int p1Open    = sql.indexOf('(');
            int p1Close   = sql.indexOf(')');
            int p2Open    = (posValues > 0) ? sql.indexOf('(', posValues) : -1;
            int p2Close   = (p2Open > 0)    ? sql.lastIndexOf(')')        : -1;

            if (posValues < 0 || p1Open < 0 || p1Close < p1Open || p2Open < 0 || p2Close < p2Open) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", "", "",
                     "syntax error in INSERT INTO (expected: INSERT INTO table (c1, c2) VALUES (v1, v2))");
                return;
            }

            String tableName = stripQuotes(sql.substring(12, p1Open));
            const TablePhysicalSchema* schema = PhysicalSchemaCatalog::getInstance().getTable(tableName);
            if (!schema) {
                fail(resBuf, SqlState::UNDEFINED_TABLE, "INSERT", tableName, "",
                     "relation \"" + tableName + "\" does not exist");
                return;
            }

            auto cols = splitTopLevel(sql.substring(p1Open + 1, p1Close), ',');
            auto vals = splitTopLevel(sql.substring(p2Open + 1, p2Close), ',');
            if (cols.empty() || cols.size() != vals.size()) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "INSERT", schema->tableName, "",
                     "INSERT has more target columns than expressions or vice versa");
                return;
            }

            PgRowBuffer row(schema);
            for (size_t i = 0; i < cols.size(); ++i) {
                String cName = stripQuotes(cols[i]);
                int8_t idx = schema->findColumnIndex(cName);
                if (idx < 0) {
                    fail(resBuf, SqlState::UNDEFINED_COLUMN, "INSERT", schema->tableName, cName,
                         "column \"" + cName + "\" of relation \"" + schema->tableName + "\" does not exist");
                    return;
                }
                String vLit = stripQuotes(vals[i]);
                String stErr, msgErr;
                if (!row.setFromLiteralIdx(idx, vLit.c_str(), &stErr, &msgErr)) {
                    fail(resBuf, stErr.c_str(), "INSERT", schema->tableName, cName, msgErr);
                    return;
                }
            }

            row.applySchemaDefaults(true);

            auto& st = PgBinaryStorage::getInstance();
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

            okResult(resBuf, "INSERT", 1, "INSERT 0 1");
        }

        // --- G. UPDATE <tabla> SET col=val, ... [WHERE ...] ---
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

            struct Assign { int8_t idx; String val; };
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
                assigns.push_back({ idx, stripQuotes(item.substring(eq + 1)) });
            }

            std::vector<SimplePred> preds;
            if (!parseWhereClause(*schema, wherePart, preds, "UPDATE", resBuf)) {
                return;
            }

            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                schema->tableName.c_str(),
                [&](const PgRowView& r) -> bool { return evalPreds(r, preds); },
                [&](PgRowBuffer& row) {
                    for (const auto& a : assigns) {
                        row.setFromLiteralIdx(a.idx, a.val.c_str());
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

            std::vector<SimplePred> preds;
            if (!parseWhereClause(*schema, wherePart, preds, "DELETE", resBuf)) {
                return;
            }

            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                schema->tableName.c_str(),
                [&](const PgRowView& r) -> bool { return evalPreds(r, preds); },
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
                 "ALTER TABLE clause not supported yet (use ADD COLUMN, DROP COLUMN, RENAME TO)");
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

            String sql = rawSql ? String(rawSql) : String("");
            sql.trim();
            while (sql.endsWith(";")) {
                sql.remove(sql.length() - 1);
                sql.trim();
            }

            if (sql.isEmpty()) {
                fail(resBuf, SqlState::SYNTAX_ERROR, "EMPTY", "", "", "empty query string");
                resBuf.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
                return resBuf;
            }

            for (size_t i = 0; i < sql.length(); ++i) {
                if (sql[i] == '\n' || sql[i] == '\r' || sql[i] == '\t') {
                    sql.setCharAt(i, ' ');
                }
            }

            String upper = sql;
            upper.toUpperCase();

            if (upper == "SHOW TABLES" || upper == "\\DT") {
                execShowTables();
            } else if (upper.startsWith("\\D ")) {
                execDescribeTable(sql.substring(3));
            } else if (upper.startsWith("SELECT ")) {
                execSelect(sql);
            } else if (upper.startsWith("INSERT INTO ")) {
                execInsert(sql, upper);
            } else if (upper.startsWith("UPDATE ")) {
                execUpdate(sql);
            } else if (upper.startsWith("DELETE FROM ")) {
                execDelete(sql);
            } else if (upper.startsWith("CREATE TYPE ")) {
                execCreateType(sql, upper);
            } else if (upper.startsWith("CREATE TABLE ")) {
                execCreateTable(sql);
            } else if (upper.startsWith("CREATE INDEX ") || upper.startsWith("CREATE UNIQUE INDEX ")) {
                execCreateIndex(sql, upper);
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
                     "Activos: SELECT, INSERT INTO, UPDATE, DELETE FROM, CREATE TYPE, CREATE TABLE, CREATE INDEX, ALTER TABLE, DROP TABLE, TRUNCATE, VACUUM, SHOW TABLES, \\d <tabla>.");
            }

            resBuf.tiempoEjecucionUs = static_cast<uint32_t>(esp_timer_get_time() - t0);
            return resBuf;
        }
    };

} // namespace DbSql