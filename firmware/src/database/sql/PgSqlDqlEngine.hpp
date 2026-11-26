#pragma once
#include <Arduino.h>
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
#include "../common/PhysicalSchemaCatalog.hpp"
#include "../storage/PgBinaryStorage.hpp"
#include "PgVirtualCatalog.hpp"

namespace DbSql {

    using DbCommon::Engine::TablePhysicalSchema;
    using DbCommon::Standard::SqlState;
    using DbRuntime::Storage::PgRowView;

    struct SqlVal {
        bool   isNull = true;
        bool   isNum  = false;
        double num    = 0.0;
        String str    = "";

        static SqlVal makeNull() { return SqlVal{}; }
        static SqlVal makeStr(const String& s) {
            SqlVal v; v.isNull = false; v.isNum = false; v.str = s; return v;
        }
        static SqlVal makeNum(double d, bool isInt = false) {
            SqlVal v; v.isNull = false; v.isNum = true; v.num = d;
            if (isInt || fabs(d - static_cast<double>(static_cast<int64_t>(d))) < 1e-7) {
                char b[32]{};
                snprintf(b, sizeof(b), "%lld", static_cast<long long>(llround(d)));
                v.str = b;
            } else {
                char b[32]{};
                snprintf(b, sizeof(b), "%.4f", d);
                // Recorta ceros decimales innecesarios dejando al menos 2 decimales
                int len = static_cast<int>(strlen(b));
                int dot = -1;
                for (int i = 0; i < len; ++i) if (b[i] == '.') { dot = i; break; }
                while (dot >= 0 && len > dot + 3 && b[len - 1] == '0') b[--len] = '\0';
                v.str = b;
            }
            return v;
        }
        static SqlVal makeBool(bool b) {
            return makeStr(b ? "TRUE" : "FALSE");
        }
        bool toBool() const {
            if (isNull) return false;
            if (isNum) return fabs(num) > 1e-9;
            return str.equalsIgnoreCase("TRUE") || str == "1" || str.equalsIgnoreCase("T");
        }
        bool asNumber(double& out) const {
            if (isNull) return false;
            if (isNum) { out = num; return true; }
            const char* s = str.c_str();
            if (!s || s[0] == '\0') return false;
            char* end = nullptr;
            double d = strtod(s, &end);
            if (end != s && *end == '\0' && isfinite(d)) { out = d; return true; }
            return false;
        }
    };

    // Normaliza SQL multi-línea: elimina comentarios (-- y /* */), convierte saltos/tabs y colapsa espacios fuera de comillas
    inline String normalizeSqlStatement(const char* rawSql) {
        if (!rawSql) return "";
        String out;
        out.reserve(strlen(rawSql) + 4);
        bool inS = false, inD = false, lastSpace = true;

        for (size_t i = 0; rawSql[i] != '\0'; ++i) {
            const char c = rawSql[i];
            const char n = rawSql[i + 1];

            if (!inS && !inD) {
                if (c == '-' && n == '-') {
                    i += 2;
                    while (rawSql[i] != '\0' && rawSql[i] != '\n') ++i;
                    if (!lastSpace) { out += ' '; lastSpace = true; }
                    if (rawSql[i] == '\0') break;
                    continue;
                }
                if (c == '/' && n == '*') {
                    i += 2;
                    while (rawSql[i] != '\0' && !(rawSql[i] == '*' && rawSql[i + 1] == '/')) ++i;
                    if (rawSql[i] != '\0') ++i;
                    if (!lastSpace) { out += ' '; lastSpace = true; }
                    continue;
                }
                if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
                    if (!lastSpace) { out += ' '; lastSpace = true; }
                    continue;
                }
            }

            if (c == '\'' && !inD) {
                if (inS && n == '\'') {
                    out += "''";
                    ++i;
                    lastSpace = false;
                    continue;
                }
                inS = !inS;
            } else if (c == '"' && !inS) {
                inD = !inD;
            }

            out += c;
            lastSpace = false;
        }

        out.trim();
        while (out.endsWith(";")) {
            out.remove(out.length() - 1);
            out.trim();
        }
        return out;
    }

    inline bool matchSqlLike(const char* str, const char* pat, bool caseInsensitive) {
        while (*pat) {
            if (*pat == '%') {
                while (*pat == '%') pat++;
                if (!*pat) return true;
                while (*str) {
                    if (matchSqlLike(str, pat, caseInsensitive)) return true;
                    str++;
                }
                return false;
            }
            if (!*str) return false;
            if (*pat != '_') {
                char a = caseInsensitive ? static_cast<char>(tolower(static_cast<unsigned char>(*str))) : *str;
                char b = caseInsensitive ? static_cast<char>(tolower(static_cast<unsigned char>(*pat))) : *pat;
                if (a != b) return false;
            }
            str++;
            pat++;
        }
        return *str == '\0';
    }

    inline int compareSqlVals(const SqlVal& a, const SqlVal& b, bool caseInsensitiveStr = false) {
        if (a.isNull && b.isNull) return 0;
        if (a.isNull) return 1;
        if (b.isNull) return -1;

        double da = 0.0, db = 0.0;
        if (a.asNumber(da) && b.asNumber(db)) {
            if (fabs(da - db) < 1e-6) return 0;
            return (da < db) ? -1 : 1;
        }
        if (caseInsensitiveStr) {
            return strcasecmp(a.str.c_str(), b.str.c_str());
        }
        return strcmp(a.str.c_str(), b.str.c_str());
    }

    // ========================================================================
    // TOKENIZADOR SQL LIGERO
    // ========================================================================
    enum class SqlTokKind : uint8_t {
        K_EOF = 0,
        K_IDENT,
        K_STR,
        K_NUM,
        K_OP,
        K_LP,
        K_RP,
        K_COMMA,
        K_DOT,
        K_STAR,
        K_CAST
    };

    struct SqlTok {
        SqlTokKind kind = SqlTokKind::K_EOF;
        String     text = "";
        bool       quoted = false;
    };

    inline std::vector<SqlTok> tokenizeSql(const String& src) {
        std::vector<SqlTok> out;
        size_t i = 0;
        const size_t n = src.length();

        while (i < n) {
            char c = src[i];
            if (isspace(static_cast<unsigned char>(c))) { ++i; continue; }

            if (c == '\'') {
                ++i;
                String s;
                while (i < n) {
                    if (src[i] == '\'') {
                        if (i + 1 < n && src[i + 1] == '\'') { s += '\''; i += 2; continue; }
                        ++i; break;
                    }
                    s += src[i++];
                }
                out.push_back({ SqlTokKind::K_STR, s, true });
                continue;
            }

            if (c == '"') {
                ++i;
                String s;
                while (i < n && src[i] != '"') s += src[i++];
                if (i < n) ++i;
                out.push_back({ SqlTokKind::K_IDENT, s, true });
                continue;
            }

            if (isdigit(static_cast<unsigned char>(c))) {
                size_t start = i;
                bool hasDot = false;
                while (i < n && (isdigit(static_cast<unsigned char>(src[i])) || (src[i] == '.' && !hasDot))) {
                    if (src[i] == '.') hasDot = true;
                    ++i;
                }
                out.push_back({ SqlTokKind::K_NUM, src.substring(start, i), false });
                continue;
            }

            if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
                size_t start = i;
                while (i < n && (isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_' || src[i] == '$')) ++i;
                out.push_back({ SqlTokKind::K_IDENT, src.substring(start, i), false });
                continue;
            }

            if (c == ':' && i + 1 < n && src[i + 1] == ':') {
                out.push_back({ SqlTokKind::K_CAST, "::", false }); i += 2; continue;
            }
            if ((c == '<' && i + 1 < n && (src[i + 1] == '=' || src[i + 1] == '>')) ||
                (c == '>' && i + 1 < n && src[i + 1] == '=') ||
                (c == '!' && i + 1 < n && src[i + 1] == '=') ||
                (c == '|' && i + 1 < n && src[i + 1] == '|')) {
                out.push_back({ SqlTokKind::K_OP, src.substring(i, i + 2), false });
                i += 2;
                continue;
            }

            if (c == '(') { out.push_back({ SqlTokKind::K_LP, "(", false }); ++i; continue; }
            if (c == ')') { out.push_back({ SqlTokKind::K_RP, ")", false }); ++i; continue; }
            if (c == ',') { out.push_back({ SqlTokKind::K_COMMA, ",", false }); ++i; continue; }
            if (c == '.') { out.push_back({ SqlTokKind::K_DOT, ".", false }); ++i; continue; }
            if (c == '*') { out.push_back({ SqlTokKind::K_STAR, "*", false }); ++i; continue; }
            if (c == '=' || c == '<' || c == '>' || c == '+' || c == '-' || c == '/') {
                out.push_back({ SqlTokKind::K_OP, String(c), false }); ++i; continue;
            }
            ++i;
        }
        out.push_back({ SqlTokKind::K_EOF, "", false });
        return out;
    }

    // ========================================================================
    // CONTEXTO DE TABLAS Y AST DE EXPRESIONES (REUTILIZABLE EN SELECT/UPDATE/DELETE)
    // ========================================================================
    struct BoundTable {
        bool                       isVirtual = false;
        PgVirtualCatalog::Kind     vKind     = PgVirtualCatalog::Kind::NONE;
        const VirtualColDef*       vCols     = nullptr;
        size_t                     vColCount = 0;
        const TablePhysicalSchema* schema    = nullptr;
        String                     fullName  = "";
        String                     shortName = "";
        String                     alias     = "";
        bool                       isLeftJoin = false;
        int16_t                    onExprRoot = -1;
    };

    struct RowBinding {
        const PgRowView* physRow  = nullptr;
        const String*    vVals    = nullptr;
        const bool*      vNulls   = nullptr;
        size_t           vCount   = 0;
    };

    enum class AggKind : uint8_t {
        NONE = 0,
        COUNT_ALL,
        COUNT_COL,
        COUNT_DIST,
        SUM_VAL,
        AVG_VAL,
        MIN_VAL,
        MAX_VAL
    };

    enum class ExprKind : uint8_t {
        LITERAL_NULL = 0,
        LITERAL_STR,
        LITERAL_NUM,
        LITERAL_BOOL,
        COL_REF,
        AGG_REF,
        UNARY_NEG,
        UNARY_NOT,
        BIN_OP,        // +, -, *, /, ||, =, <>, <, <=, >, >=, AND, OR, LIKE, ILIKE
        IS_NULL,
        IS_NOT_NULL,
        BETWEEN_OP,
        IN_LIST_OP,
        FUNC_CALL
    };

    struct ExprNode {
        ExprKind kind     = ExprKind::LITERAL_NULL;
        int16_t  left     = -1;
        int16_t  right    = -1;
        int16_t  third    = -1;   // high en BETWEEN
        int8_t   tblIdx   = -1;
        int8_t   colIdx   = -1;
        int8_t   aggIdx   = -1;
        bool     negated  = false;
        bool     colCaseInsensitive = false;
        double   numVal   = 0.0;
        String   text     = "";
        std::vector<int16_t> args;
    };

    struct AggSpec {
        AggKind kind      = AggKind::NONE;
        int16_t argExpr   = -1;
        String  signature = "";
    };

    struct AggState {
        uint32_t count    = 0;
        double   sum      = 0.0;
        bool     hasMinMax = false;
        SqlVal   minVal   = SqlVal::makeNull();
        SqlVal   maxVal   = SqlVal::makeNull();
        std::vector<String> seenDistinct;

        void accumulate(AggKind kind, const SqlVal& v) {
            if (kind == AggKind::COUNT_ALL) { count++; return; }
            if (v.isNull) return;
            if (kind == AggKind::COUNT_COL) { count++; return; }
            if (kind == AggKind::COUNT_DIST) {
                if (seenDistinct.size() < 96) {
                    bool found = false;
                    for (const auto& s : seenDistinct) {
                        if (s == v.str) { found = true; break; }
                    }
                    if (!found) { seenDistinct.push_back(v.str); count++; }
                }
                return;
            }
            if (kind == AggKind::SUM_VAL || kind == AggKind::AVG_VAL) {
                double d = 0.0;
                if (v.asNumber(d)) { sum += d; count++; }
                return;
            }
            if (kind == AggKind::MIN_VAL) {
                if (!hasMinMax || compareSqlVals(v, minVal) < 0) { minVal = v; hasMinMax = true; }
                return;
            }
            if (kind == AggKind::MAX_VAL) {
                if (!hasMinMax || compareSqlVals(v, maxVal) > 0) { maxVal = v; hasMinMax = true; }
                return;
            }
        }

        SqlVal finalize(AggKind kind) const {
            switch (kind) {
                case AggKind::COUNT_ALL:
                case AggKind::COUNT_COL:
                case AggKind::COUNT_DIST: return SqlVal::makeNum(static_cast<double>(count), true);
                case AggKind::SUM_VAL:    return (count > 0) ? SqlVal::makeNum(sum) : SqlVal::makeNull();
                case AggKind::AVG_VAL:    return (count > 0) ? SqlVal::makeNum(sum / static_cast<double>(count)) : SqlVal::makeNull();
                case AggKind::MIN_VAL:    return hasMinMax ? minVal : SqlVal::makeNull();
                case AggKind::MAX_VAL:    return hasMinMax ? maxVal : SqlVal::makeNull();
                default:                  return SqlVal::makeNull();
            }
        }
    };

    class SqlExprParser {
    public:
        const std::vector<SqlTok>&    toks;
        const std::vector<BoundTable>& tables;
        std::vector<ExprNode>&        nodes;
        std::vector<AggSpec>*         aggs;
        size_t                        pos = 0;
        String                        errorMsg = "";
        const char*                   errorState = SqlState::SYNTAX_ERROR;

        SqlExprParser(
            const std::vector<SqlTok>& t,
            const std::vector<BoundTable>& tb,
            std::vector<ExprNode>& nd,
            std::vector<AggSpec>* ag = nullptr,
            size_t startPos = 0
        ) : toks(t), tables(tb), nodes(nd), aggs(ag), pos(startPos) {}

        const SqlTok& cur() const { return toks[pos < toks.size() ? pos : toks.size() - 1]; }
        bool isKw(const char* kw) const {
            return cur().kind == SqlTokKind::K_IDENT && !cur().quoted && cur().text.equalsIgnoreCase(kw);
        }
        bool isOp(const char* op) const {
            return cur().kind == SqlTokKind::K_OP && cur().text == op;
        }
        bool matchKw(const char* kw) {
            if (isKw(kw)) { ++pos; return true; }
            return false;
        }

        int16_t addNode(ExprNode n) {
            nodes.push_back(std::move(n));
            return static_cast<int16_t>(nodes.size() - 1);
        }

        bool resolveColumn(const String& qualifier, const String& colName, int8_t& outTbl, int8_t& outCol, bool& outCI) {
            outTbl = -1; outCol = -1; outCI = false;
            int matches = 0;
            for (size_t ti = 0; ti < tables.size(); ++ti) {
                const auto& t = tables[ti];
                if (!qualifier.isEmpty()) {
                    bool qMatch = t.alias.equalsIgnoreCase(qualifier) ||
                                  t.fullName.equalsIgnoreCase(qualifier) ||
                                  t.shortName.equalsIgnoreCase(qualifier);
                    if (!qMatch) continue;
                }
                if (t.isVirtual) {
                    for (size_t ci = 0; ci < t.vColCount; ++ci) {
                        if (strcasecmp(t.vCols[ci].name, colName.c_str()) == 0) {
                            outTbl = static_cast<int8_t>(ti);
                            outCol = static_cast<int8_t>(ci);
                            outCI  = false;
                            matches++;
                            break;
                        }
                    }
                } else if (t.schema) {
                    int8_t ci = t.schema->findColumnIndex(colName.c_str());
                    if (ci >= 0) {
                        outTbl = static_cast<int8_t>(ti);
                        outCol = ci;
                        const auto dt = t.schema->columns[static_cast<size_t>(ci)].dataType;
                        outCI = (dt == PgDataType::ENUM || dt == PgDataType::BOOLEAN);
                        matches++;
                    }
                }
            }
            if (matches == 0) {
                errorState = SqlState::UNDEFINED_COLUMN;
                errorMsg = "column \"" + (qualifier.isEmpty() ? colName : (qualifier + "." + colName)) + "\" does not exist";
                return false;
            }
            if (matches > 1 && qualifier.isEmpty()) {
                errorState = SqlState::AMBIGUOUS_COLUMN;
                errorMsg = "column reference \"" + colName + "\" is ambiguous (use table.column)";
                return false;
            }
            return true;
        }

        int16_t parseExpr() { return parseOr(); }

        int16_t parseOr() {
            int16_t lhs = parseAnd();
            if (lhs < 0) return -1;
            while (matchKw("OR")) {
                int16_t rhs = parseAnd();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = "OR"; n.left = lhs; n.right = rhs;
                lhs = addNode(std::move(n));
            }
            return lhs;
        }

        int16_t parseAnd() {
            int16_t lhs = parseNot();
            if (lhs < 0) return -1;
            while (matchKw("AND")) {
                int16_t rhs = parseNot();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = "AND"; n.left = lhs; n.right = rhs;
                lhs = addNode(std::move(n));
            }
            return lhs;
        }

        int16_t parseNot() {
            if (matchKw("NOT")) {
                int16_t sub = parseNot();
                if (sub < 0) return -1;
                ExprNode n; n.kind = ExprKind::UNARY_NOT; n.left = sub;
                return addNode(std::move(n));
            }
            return parseComparison();
        }

        int16_t parseComparison() {
            int16_t lhs = parseAddSub();
            if (lhs < 0) return -1;

            if (matchKw("IS")) {
                bool isNot = matchKw("NOT");
                if (!matchKw("NULL")) {
                    errorMsg = "expected NULL after IS"; return -1;
                }
                ExprNode n;
                n.kind = isNot ? ExprKind::IS_NOT_NULL : ExprKind::IS_NULL;
                n.left = lhs;
                return addNode(std::move(n));
            }

            bool neg = false;
            size_t saved = pos;
            if (matchKw("NOT")) neg = true;

            if (matchKw("BETWEEN")) {
                int16_t lo = parseAddSub();
                if (lo < 0) return -1;
                if (!matchKw("AND")) { errorMsg = "expected AND in BETWEEN"; return -1; }
                int16_t hi = parseAddSub();
                if (hi < 0) return -1;
                ExprNode n; n.kind = ExprKind::BETWEEN_OP; n.left = lhs; n.right = lo; n.third = hi; n.negated = neg;
                return addNode(std::move(n));
            }

            if (matchKw("IN")) {
                if (cur().kind != SqlTokKind::K_LP) { errorMsg = "expected '(' after IN"; return -1; }
                ++pos;
                ExprNode n; n.kind = ExprKind::IN_LIST_OP; n.left = lhs; n.negated = neg;
                if (cur().kind != SqlTokKind::K_RP) {
                    while (true) {
                        int16_t item = parseAddSub();
                        if (item < 0) return -1;
                        n.args.push_back(item);
                        if (cur().kind == SqlTokKind::K_COMMA) { ++pos; continue; }
                        break;
                    }
                }
                if (cur().kind != SqlTokKind::K_RP) { errorMsg = "expected ')' closing IN list"; return -1; }
                ++pos;
                return addNode(std::move(n));
            }

            if (matchKw("LIKE") || matchKw("ILIKE")) {
                const bool isIlike = toks[pos - 1].text.equalsIgnoreCase("ILIKE");
                int16_t rhs = parseAddSub();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = isIlike ? "ILIKE" : "LIKE";
                n.left = lhs; n.right = rhs; n.negated = neg;
                return addNode(std::move(n));
            }

            if (neg) pos = saved;

            if (cur().kind == SqlTokKind::K_OP &&
                (cur().text == "=" || cur().text == "<>" || cur().text == "!=" ||
                 cur().text == "<" || cur().text == "<=" || cur().text == ">" || cur().text == ">=")) {
                String op = cur().text;
                if (op == "!=") op = "<>";
                ++pos;
                int16_t rhs = parseAddSub();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = op; n.left = lhs; n.right = rhs;
                return addNode(std::move(n));
            }

            return lhs;
        }

        int16_t parseAddSub() {
            int16_t lhs = parseMulDiv();
            if (lhs < 0) return -1;
            while (cur().kind == SqlTokKind::K_OP &&
                   (cur().text == "+" || cur().text == "-" || cur().text == "||")) {
                String op = cur().text;
                ++pos;
                int16_t rhs = parseMulDiv();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = op; n.left = lhs; n.right = rhs;
                lhs = addNode(std::move(n));
            }
            return lhs;
        }

        int16_t parseMulDiv() {
            int16_t lhs = parseUnary();
            if (lhs < 0) return -1;
            while ((cur().kind == SqlTokKind::K_STAR) ||
                   (cur().kind == SqlTokKind::K_OP && cur().text == "/")) {
                String op = cur().text;
                ++pos;
                int16_t rhs = parseUnary();
                if (rhs < 0) return -1;
                ExprNode n; n.kind = ExprKind::BIN_OP; n.text = op; n.left = lhs; n.right = rhs;
                lhs = addNode(std::move(n));
            }
            return lhs;
        }

        int16_t parseUnary() {
            if (isOp("-")) {
                ++pos;
                int16_t sub = parseUnary();
                if (sub < 0) return -1;
                ExprNode n; n.kind = ExprKind::UNARY_NEG; n.left = sub;
                return addNode(std::move(n));
            }
            if (isOp("+")) {
                ++pos;
                return parseUnary();
            }
            int16_t base = parsePrimary();
            if (base < 0) return -1;
            // Soporte PostgreSQL ::type (ej. col::text, col::int)
            while (cur().kind == SqlTokKind::K_CAST) {
                ++pos;
                if (cur().kind == SqlTokKind::K_IDENT) {
                    String targetType = cur().text;
                    targetType.toUpperCase();
                    ++pos;
                    ExprNode n; n.kind = ExprKind::FUNC_CALL; n.text = "CAST_" + targetType;
                    n.args.push_back(base);
                    base = addNode(std::move(n));
                }
            }
            return base;
        }

        int16_t parsePrimary() {
            if (cur().kind == SqlTokKind::K_LP) {
                ++pos;
                int16_t inner = parseExpr();
                if (inner < 0) return -1;
                if (cur().kind != SqlTokKind::K_RP) { errorMsg = "missing closing ')'"; return -1; }
                ++pos;
                return inner;
            }

            if (cur().kind == SqlTokKind::K_STR) {
                ExprNode n; n.kind = ExprKind::LITERAL_STR; n.text = cur().text; ++pos;
                return addNode(std::move(n));
            }
            if (cur().kind == SqlTokKind::K_NUM) {
                ExprNode n; n.kind = ExprKind::LITERAL_NUM; n.text = cur().text;
                n.numVal = strtod(cur().text.c_str(), nullptr);
                ++pos;
                return addNode(std::move(n));
            }
            if (isKw("NULL"))  { ++pos; ExprNode n; n.kind = ExprKind::LITERAL_NULL; return addNode(std::move(n)); }
            if (isKw("TRUE"))  { ++pos; ExprNode n; n.kind = ExprKind::LITERAL_BOOL; n.text = "TRUE";  return addNode(std::move(n)); }
            if (isKw("FALSE")) { ++pos; ExprNode n; n.kind = ExprKind::LITERAL_BOOL; n.text = "FALSE"; return addNode(std::move(n)); }

            if (isKw("CURRENT_TIMESTAMP") || isKw("CURRENT_DATE") || isKw("CURRENT_TIME")) {
                ExprNode n; n.kind = ExprKind::FUNC_CALL; n.text = cur().text; n.text.toUpperCase(); ++pos;
                return addNode(std::move(n));
            }

            if (cur().kind == SqlTokKind::K_IDENT) {
                String first = cur().text;
                bool firstQuoted = cur().quoted;
                ++pos;

                // Llamada a función o agregación: NOMBRE(...)
                if (!firstQuoted && cur().kind == SqlTokKind::K_LP) {
                    ++pos;
                    String fnUp = first;
                    fnUp.toUpperCase();

                    // Agregaciones: COUNT, SUM, AVG, MIN, MAX
                    if (aggs != nullptr &&
                        (fnUp == "COUNT" || fnUp == "SUM" || fnUp == "AVG" || fnUp == "MIN" || fnUp == "MAX")) {
                        AggKind ak = AggKind::NONE;
                        int16_t argIdx = -1;
                        String sig = fnUp + "(";

                        if (fnUp == "COUNT" && cur().kind == SqlTokKind::K_STAR) {
                            ++pos;
                            ak = AggKind::COUNT_ALL;
                            sig += "*";
                        } else {
                            bool isDist = matchKw("DISTINCT");
                            if (isDist) sig += "DISTINCT ";
                            size_t argStart = pos;
                            argIdx = parseExpr();
                            if (argIdx < 0) return -1;
                            for (size_t k = argStart; k < pos; ++k) sig += toks[k].text;

                            if (fnUp == "COUNT") ak = isDist ? AggKind::COUNT_DIST : AggKind::COUNT_COL;
                            else if (fnUp == "SUM") ak = AggKind::SUM_VAL;
                            else if (fnUp == "AVG") ak = AggKind::AVG_VAL;
                            else if (fnUp == "MIN") ak = AggKind::MIN_VAL;
                            else if (fnUp == "MAX") ak = AggKind::MAX_VAL;
                        }
                        if (cur().kind != SqlTokKind::K_RP) { errorMsg = "expected ')' in aggregate " + fnUp; return -1; }
                        ++pos;
                        sig += ")";

                        int8_t foundIdx = -1;
                        for (size_t k = 0; k < aggs->size(); ++k) {
                            if ((*aggs)[k].signature.equalsIgnoreCase(sig)) { foundIdx = static_cast<int8_t>(k); break; }
                        }
                        if (foundIdx < 0) {
                            foundIdx = static_cast<int8_t>(aggs->size());
                            aggs->push_back({ ak, argIdx, sig });
                        }
                        ExprNode n; n.kind = ExprKind::AGG_REF; n.aggIdx = foundIdx; n.text = sig;
                        return addNode(std::move(n));
                    }

                    // Función escalar
                    ExprNode n; n.kind = ExprKind::FUNC_CALL; n.text = fnUp;
                    if (cur().kind != SqlTokKind::K_RP) {
                        while (true) {
                            int16_t a = parseExpr();
                            if (a < 0) return -1;
                            n.args.push_back(a);
                            if (cur().kind == SqlTokKind::K_COMMA) { ++pos; continue; }
                            break;
                        }
                    }
                    if (cur().kind != SqlTokKind::K_RP) { errorMsg = "expected ')' in function " + fnUp; return -1; }
                    ++pos;
                    return addNode(std::move(n));
                }

                // Cadena de identificadores calificados: col | tbl.col | schema.tbl.col
                std::vector<String> parts;
                parts.push_back(first);
                while (cur().kind == SqlTokKind::K_DOT) {
                    ++pos;
                    if (cur().kind != SqlTokKind::K_IDENT) {
                        errorMsg = "expected column name after '.'"; return -1;
                    }
                    parts.push_back(cur().text);
                    ++pos;
                }

                String colName = parts.back();
                String qualifier = "";
                for (size_t k = 0; k + 1 < parts.size(); ++k) {
                    if (k > 0) qualifier += ".";
                    qualifier += parts[k];
                }

                int8_t tIdx = -1, cIdx = -1;
                bool ci = false;
                if (!resolveColumn(qualifier, colName, tIdx, cIdx, ci)) return -1;

                ExprNode n;
                n.kind   = ExprKind::COL_REF;
                n.tblIdx = tIdx;
                n.colIdx = cIdx;
                n.colCaseInsensitive = ci;
                n.text   = colName;
                return addNode(std::move(n));
            }

            errorMsg = "unexpected token \"" + cur().text + "\" in expression";
            return -1;
        }
    };

    inline SqlVal readBoundCell(const BoundTable& tb, const RowBinding& rb, int8_t colIdx) {
        if (colIdx < 0) return SqlVal::makeNull();
        if (tb.isVirtual) {
            if (!rb.vVals || static_cast<size_t>(colIdx) >= rb.vCount) return SqlVal::makeNull();
            if (rb.vNulls && rb.vNulls[colIdx]) return SqlVal::makeNull();
            const String& s = rb.vVals[colIdx];
            if (tb.vCols[colIdx].isNumeric) {
                double d = strtod(s.c_str(), nullptr);
                return SqlVal::makeNum(d);
            }
            return SqlVal::makeStr(s);
        }

        if (!rb.physRow || !tb.schema || rb.physRow->isNullIdx(colIdx)) return SqlVal::makeNull();
        const auto& col = tb.schema->columns[static_cast<size_t>(colIdx)];
        if (DataTypeUtils::isIntegerType(col.dataType)) {
            return SqlVal::makeNum(static_cast<double>(rb.physRow->getInt64Idx(colIdx)), true);
        }
        if (DataTypeUtils::isFloatingOrDecimalType(col.dataType)) {
            return SqlVal::makeNum(rb.physRow->getDoubleIdx(colIdx), false);
        }
        char buf[96]{};
        rb.physRow->formatCellToBuf(colIdx, buf, sizeof(buf));
        return SqlVal::makeStr(String(buf));
    }

    inline SqlVal evalExprNode(
        int16_t nodeIdx,
        const std::vector<ExprNode>& nodes,
        const std::vector<BoundTable>& tables,
        const RowBinding* rows,
        const std::vector<SqlVal>* aggResults = nullptr
    ) {
        if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= nodes.size()) return SqlVal::makeNull();
        const ExprNode& n = nodes[static_cast<size_t>(nodeIdx)];

        switch (n.kind) {
            case ExprKind::LITERAL_NULL: return SqlVal::makeNull();
            case ExprKind::LITERAL_STR:  return SqlVal::makeStr(n.text);
            case ExprKind::LITERAL_NUM:  return SqlVal::makeNum(n.numVal);
            case ExprKind::LITERAL_BOOL: return SqlVal::makeBool(n.text == "TRUE");

            case ExprKind::COL_REF:
                if (!rows || n.tblIdx < 0 || static_cast<size_t>(n.tblIdx) >= tables.size()) return SqlVal::makeNull();
                return readBoundCell(tables[static_cast<size_t>(n.tblIdx)], rows[n.tblIdx], n.colIdx);

            case ExprKind::AGG_REF:
                if (aggResults && n.aggIdx >= 0 && static_cast<size_t>(n.aggIdx) < aggResults->size()) {
                    return (*aggResults)[static_cast<size_t>(n.aggIdx)];
                }
                return SqlVal::makeNull();

            case ExprKind::UNARY_NEG: {
                SqlVal v = evalExprNode(n.left, nodes, tables, rows, aggResults);
                double d = 0.0;
                return v.asNumber(d) ? SqlVal::makeNum(-d) : SqlVal::makeNull();
            }

            case ExprKind::UNARY_NOT: {
                SqlVal v = evalExprNode(n.left, nodes, tables, rows, aggResults);
                return SqlVal::makeBool(!v.toBool());
            }

            case ExprKind::IS_NULL:
                return SqlVal::makeBool(evalExprNode(n.left, nodes, tables, rows, aggResults).isNull);

            case ExprKind::IS_NOT_NULL:
                return SqlVal::makeBool(!evalExprNode(n.left, nodes, tables, rows, aggResults).isNull);

            case ExprKind::BETWEEN_OP: {
                SqlVal v  = evalExprNode(n.left,  nodes, tables, rows, aggResults);
                SqlVal lo = evalExprNode(n.right, nodes, tables, rows, aggResults);
                SqlVal hi = evalExprNode(n.third, nodes, tables, rows, aggResults);
                if (v.isNull || lo.isNull || hi.isNull) return SqlVal::makeBool(false);
                bool ok = (compareSqlVals(v, lo) >= 0) && (compareSqlVals(v, hi) <= 0);
                return SqlVal::makeBool(n.negated ? !ok : ok);
            }

            case ExprKind::IN_LIST_OP: {
                SqlVal v = evalExprNode(n.left, nodes, tables, rows, aggResults);
                if (v.isNull) return SqlVal::makeBool(false);
                const bool ci = (n.left >= 0 && nodes[static_cast<size_t>(n.left)].colCaseInsensitive);
                bool found = false;
                for (int16_t aIdx : n.args) {
                    SqlVal item = evalExprNode(aIdx, nodes, tables, rows, aggResults);
                    if (!item.isNull && compareSqlVals(v, item, ci) == 0) { found = true; break; }
                }
                return SqlVal::makeBool(n.negated ? !found : found);
            }

            case ExprKind::BIN_OP: {
                if (n.text == "AND") {
                    if (!evalExprNode(n.left, nodes, tables, rows, aggResults).toBool()) return SqlVal::makeBool(false);
                    return SqlVal::makeBool(evalExprNode(n.right, nodes, tables, rows, aggResults).toBool());
                }
                if (n.text == "OR") {
                    if (evalExprNode(n.left, nodes, tables, rows, aggResults).toBool()) return SqlVal::makeBool(true);
                    return SqlVal::makeBool(evalExprNode(n.right, nodes, tables, rows, aggResults).toBool());
                }

                SqlVal l = evalExprNode(n.left,  nodes, tables, rows, aggResults);
                SqlVal r = evalExprNode(n.right, nodes, tables, rows, aggResults);

                if (n.text == "||") {
                    if (l.isNull || r.isNull) return SqlVal::makeNull();
                    return SqlVal::makeStr(l.str + r.str);
                }
                if (n.text == "+" || n.text == "-" || n.text == "*" || n.text == "/") {
                    double a = 0.0, b = 0.0;
                    if (!l.asNumber(a) || !r.asNumber(b)) return SqlVal::makeNull();
                    if (n.text == "+") return SqlVal::makeNum(a + b);
                    if (n.text == "-") return SqlVal::makeNum(a - b);
                    if (n.text == "*") return SqlVal::makeNum(a * b);
                    return (fabs(b) > 1e-12) ? SqlVal::makeNum(a / b) : SqlVal::makeNull();
                }

                if (l.isNull || r.isNull) return SqlVal::makeBool(false);

                if (n.text == "LIKE" || n.text == "ILIKE") {
                    bool m = matchSqlLike(l.str.c_str(), r.str.c_str(), n.text == "ILIKE");
                    return SqlVal::makeBool(n.negated ? !m : m);
                }

                const bool ci = (n.left >= 0 && nodes[static_cast<size_t>(n.left)].colCaseInsensitive) ||
                                (n.right >= 0 && nodes[static_cast<size_t>(n.right)].colCaseInsensitive);
                const int cmp = compareSqlVals(l, r, ci);
                if (n.text == "=")  return SqlVal::makeBool(cmp == 0);
                if (n.text == "<>") return SqlVal::makeBool(cmp != 0);
                if (n.text == "<")  return SqlVal::makeBool(cmp < 0);
                if (n.text == "<=") return SqlVal::makeBool(cmp <= 0);
                if (n.text == ">")  return SqlVal::makeBool(cmp > 0);
                if (n.text == ">=") return SqlVal::makeBool(cmp >= 0);
                return SqlVal::makeBool(false);
            }

            case ExprKind::FUNC_CALL: {
                if (n.text == "VERSION") return SqlVal::makeStr("PostgreSQL 16 (ESP32-BinaryEngine UNTDF)");
                if (n.text == "NOW" || n.text == "CURRENT_TIMESTAMP") return SqlVal::makeStr(RTOS::RelojSistema::fechaHoraActualPg().toString());
                if (n.text == "CURRENT_DATE") return SqlVal::makeStr(RTOS::RelojSistema::fechaActualPg().toString());
                if (n.text == "CURRENT_TIME") {
                    String dt = RTOS::RelojSistema::fechaHoraActualPg().toString();
                    return SqlVal::makeStr(dt.length() >= 19 ? dt.substring(11, 19) : String("00:00:00"));
                }
                if (n.text == "COALESCE") {
                    for (int16_t a : n.args) {
                        SqlVal v = evalExprNode(a, nodes, tables, rows, aggResults);
                        if (!v.isNull) return v;
                    }
                    return SqlVal::makeNull();
                }
                if (n.args.empty()) return SqlVal::makeNull();
                SqlVal a0 = evalExprNode(n.args[0], nodes, tables, rows, aggResults);
                if (a0.isNull) return SqlVal::makeNull();

                if (n.text == "UPPER")  { String s = a0.str; s.toUpperCase(); return SqlVal::makeStr(s); }
                if (n.text == "LOWER")  { String s = a0.str; s.toLowerCase(); return SqlVal::makeStr(s); }
                if (n.text == "LENGTH") { return SqlVal::makeNum(static_cast<double>(a0.str.length()), true); }
                if (n.text == "ABS")    { double d = 0.0; return a0.asNumber(d) ? SqlVal::makeNum(fabs(d)) : SqlVal::makeNull(); }
                if (n.text == "ROUND")  {
                    double d = 0.0;
                    if (!a0.asNumber(d)) return SqlVal::makeNull();
                    int dec = 0;
                    if (n.args.size() >= 2) {
                        double p = 0.0;
                        if (evalExprNode(n.args[1], nodes, tables, rows, aggResults).asNumber(p)) dec = static_cast<int>(p);
                    }
                    double f = pow(10.0, dec);
                    return SqlVal::makeNum(round(d * f) / f, dec <= 0);
                }
                if (n.text.startsWith("CAST_")) {
                    if (n.text == "CAST_TEXT" || n.text == "CAST_VARCHAR") return SqlVal::makeStr(a0.str);
                    if (n.text == "CAST_INT" || n.text == "CAST_INTEGER" || n.text == "CAST_BIGINT") {
                        double d = 0.0; return a0.asNumber(d) ? SqlVal::makeNum(static_cast<double>(static_cast<int64_t>(d)), true) : SqlVal::makeNull();
                    }
                    if (n.text == "CAST_REAL" || n.text == "CAST_FLOAT" || n.text == "CAST_NUMERIC") {
                        double d = 0.0; return a0.asNumber(d) ? SqlVal::makeNum(d) : SqlVal::makeNull();
                    }
                    return a0;
                }
                return a0;
            }
        }
        return SqlVal::makeNull();
    }

    // ========================================================================
    // EJECUTOR DQL COMPLETO (SELECT + JOIN + GROUP BY + HAVING + ORDER BY)
    // ========================================================================
    class PgSqlDqlEngine {
    public:
        static constexpr size_t   MAX_JOIN_TABLES    = 4;
        static constexpr size_t   MAX_MAT_ROWS       = 64;
        static constexpr size_t   MAX_GROUPS         = 64;
        static constexpr size_t   DEFAULT_LIMIT_ROWS = 48;
        static constexpr int64_t  QUERY_TIMEOUT_US   = 2500000LL; // 2.5 s guardarraíl RTOS

        struct SelectItem {
            int16_t exprIdx = -1;
            String  label   = "";
        };

        struct OrderItem {
            int16_t exprIdx   = -1;
            int8_t  aliasCol  = -1; // si ORDER BY referencia un alias del SELECT
            bool    desc      = false;
        };

        struct OutRow {
            std::vector<String> cells;
            std::vector<SqlVal> sortKeys;
        };

        struct GroupBucket {
            String                key;
            std::vector<SqlVal>   projSampleVals;
            std::vector<AggState> aggStates;
        };

        template <typename FailFn, typename AppendFn>
        __attribute__((noinline))
        static bool executeSelect(
            const String& sql,
            char* outBuf,
            size_t outCap,
            uint16_t& filasOut,
            FailFn failFn,
            AppendFn appendFn
        ) {
            const int64_t deadlineUs = esp_timer_get_time() + QUERY_TIMEOUT_US;
            std::vector<SqlTok> toks = tokenizeSql(sql);

            auto isKwAt = [&](size_t idx, const char* kw) -> bool {
                return idx < toks.size() && toks[idx].kind == SqlTokKind::K_IDENT &&
                       !toks[idx].quoted && toks[idx].text.equalsIgnoreCase(kw);
            };

            auto findTopKw = [&](const char* kw, size_t from, size_t to) -> int {
                int par = 0;
                for (size_t i = from; i < to && i < toks.size(); ++i) {
                    if (toks[i].kind == SqlTokKind::K_LP) par++;
                    else if (toks[i].kind == SqlTokKind::K_RP && par > 0) par--;
                    else if (par == 0 && isKwAt(i, kw)) return static_cast<int>(i);
                }
                return -1;
            };

            const size_t totalToks = toks.size() - 1; // excluye K_EOF
            size_t selStart = 1; // después de SELECT
            bool isDistinct = false;
            if (isKwAt(selStart, "DISTINCT")) { isDistinct = true; selStart++; }

            const int pFrom   = findTopKw("FROM",   selStart, totalToks);
            const size_t scanAfter = (pFrom >= 0) ? static_cast<size_t>(pFrom + 1) : selStart;
            const int pWhere  = findTopKw("WHERE",  scanAfter, totalToks);
            const int pGroup  = findTopKw("GROUP",  scanAfter, totalToks);
            const int pHaving = findTopKw("HAVING", scanAfter, totalToks);
            const int pOrder  = findTopKw("ORDER",  scanAfter, totalToks);
            const int pLimit  = findTopKw("LIMIT",  scanAfter, totalToks);
            const int pOffset = findTopKw("OFFSET", scanAfter, totalToks);

            auto nextClauseAfter = [&](int startPos) -> size_t {
                size_t end = totalToks;
                int cands[] = { pWhere, pGroup, pHaving, pOrder, pLimit, pOffset };
                for (int c : cands) {
                    if (c > startPos && static_cast<size_t>(c) < end) end = static_cast<size_t>(c);
                }
                return end;
            };

            std::vector<BoundTable> tables;
            std::vector<ExprNode>   nodes;
            std::vector<AggSpec>    aggs;

            // 1. Parsear FROM + JOINs (soporta 0..4 tablas físicas o virtuales)
            if (pFrom >= 0) {
                const size_t fromEnd = nextClauseAfter(pFrom);
                size_t cur = static_cast<size_t>(pFrom + 1);
                bool nextLeft = false;

                while (cur < fromEnd) {
                    if (tables.size() >= MAX_JOIN_TABLES) {
                        failFn(SqlState::FEATURE_NOT_SUPPORTED, "SELECT", "", "", "maximum 4 tables per JOIN on ESP32");
                        return false;
                    }

                    // Leer nombre de tabla (tabla o schema.tabla)
                    if (toks[cur].kind != SqlTokKind::K_IDENT) {
                        failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "", "expected table name in FROM/JOIN");
                        return false;
                    }
                    String tName = toks[cur++].text;
                    if (cur + 1 < fromEnd && toks[cur].kind == SqlTokKind::K_DOT && toks[cur + 1].kind == SqlTokKind::K_IDENT) {
                        tName += ".";
                        tName += toks[cur + 1].text;
                        cur += 2;
                    }

                    BoundTable bt{};
                    bt.fullName   = tName;
                    int dotIdx    = tName.lastIndexOf('.');
                    bt.shortName  = (dotIdx >= 0) ? tName.substring(dotIdx + 1) : tName;
                    bt.alias      = bt.fullName;
                    bt.isLeftJoin = nextLeft;
                    nextLeft      = false;

                    // Alias opcional: [AS] alias
                    if (cur < fromEnd && isKwAt(cur, "AS")) {
                        ++cur;
                        if (cur < fromEnd && toks[cur].kind == SqlTokKind::K_IDENT) bt.alias = toks[cur++].text;
                    } else if (cur < fromEnd && toks[cur].kind == SqlTokKind::K_IDENT &&
                               !isKwAt(cur, "ON") && !isKwAt(cur, "JOIN") && !isKwAt(cur, "INNER") &&
                               !isKwAt(cur, "LEFT") && !isKwAt(cur, "RIGHT") && !isKwAt(cur, "CROSS") &&
                               !isKwAt(cur, "OUTER")) {
                        bt.alias = toks[cur++].text;
                    }

                    PgVirtualCatalog::Kind vk = PgVirtualCatalog::resolve(tName);
                    if (vk != PgVirtualCatalog::Kind::NONE) {
                        bt.isVirtual = true;
                        bt.vKind     = vk;
                        bt.vCols     = PgVirtualCatalog::getColumns(vk, bt.vColCount);
                    } else {
                        String physName = tName;
                        if (physName.startsWith("public.") || physName.startsWith("PUBLIC.")) {
                            physName = physName.substring(7);
                        }
                        bt.schema = PhysicalSchemaCatalog::getInstance().getTable(physName);
                        if (!bt.schema) {
                            failFn(SqlState::UNDEFINED_TABLE, "SELECT", tName, "",
                                   "relation \"" + tName + "\" does not exist");
                            return false;
                        }
                        bt.shortName = bt.schema->tableName;
                    }

                    tables.push_back(bt);

                    // Condición ON opcional del JOIN actual
                    if (cur < fromEnd && isKwAt(cur, "ON")) {
                        ++cur;
                        size_t onEnd = cur;
                        int par = 0;
                        while (onEnd < fromEnd) {
                            if (toks[onEnd].kind == SqlTokKind::K_LP) par++;
                            else if (toks[onEnd].kind == SqlTokKind::K_RP && par > 0) par--;
                            else if (par == 0 && (toks[onEnd].kind == SqlTokKind::K_COMMA ||
                                                  isKwAt(onEnd, "JOIN") || isKwAt(onEnd, "INNER") ||
                                                  isKwAt(onEnd, "LEFT") || isKwAt(onEnd, "CROSS"))) {
                                break;
                            }
                            ++onEnd;
                        }
                        std::vector<SqlTok> subToks(toks.begin() + cur, toks.begin() + onEnd);
                        subToks.push_back({ SqlTokKind::K_EOF, "", false });
                        SqlExprParser onParser(subToks, tables, nodes, nullptr);
                        int16_t onRoot = onParser.parseExpr();
                        if (onRoot < 0) {
                            failFn(onParser.errorState, "SELECT", bt.fullName, "", onParser.errorMsg);
                            return false;
                        }
                        tables.back().onExprRoot = onRoot;
                        cur = onEnd;
                    }

                    if (cur >= fromEnd) break;

                    // Conector al próximo JOIN
                    if (toks[cur].kind == SqlTokKind::K_COMMA) {
                        ++cur;
                    } else if (isKwAt(cur, "JOIN")) {
                        ++cur;
                    } else if (isKwAt(cur, "INNER") && isKwAt(cur + 1, "JOIN")) {
                        cur += 2;
                    } else if (isKwAt(cur, "CROSS") && isKwAt(cur + 1, "JOIN")) {
                        cur += 2;
                    } else if (isKwAt(cur, "LEFT")) {
                        nextLeft = true;
                        ++cur;
                        if (isKwAt(cur, "OUTER")) ++cur;
                        if (!isKwAt(cur, "JOIN")) {
                            failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "", "expected JOIN after LEFT");
                            return false;
                        }
                        ++cur;
                    } else {
                        failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "",
                               "unexpected token in FROM clause: \"" + toks[cur].text + "\"");
                        return false;
                    }
                }
            }

            // 2. Parsear lista de proyección SELECT
            const size_t selEnd = (pFrom >= 0) ? static_cast<size_t>(pFrom) : nextClauseAfter(0);
            std::vector<SelectItem> selectItems;
            {
                size_t i = selStart;
                while (i < selEnd) {
                    size_t itemEnd = i;
                    int par = 0;
                    while (itemEnd < selEnd) {
                        if (toks[itemEnd].kind == SqlTokKind::K_LP) par++;
                        else if (toks[itemEnd].kind == SqlTokKind::K_RP && par > 0) par--;
                        else if (par == 0 && toks[itemEnd].kind == SqlTokKind::K_COMMA) break;
                        itemEnd++;
                    }

                    // Expandir * o alias.*
                    if (itemEnd == i + 1 && toks[i].kind == SqlTokKind::K_STAR) {
                        for (size_t ti = 0; ti < tables.size(); ++ti) {
                            const auto& t = tables[ti];
                            if (t.isVirtual) {
                                for (size_t ci = 0; ci < t.vColCount; ++ci) {
                                    ExprNode cn; cn.kind = ExprKind::COL_REF; cn.tblIdx = static_cast<int8_t>(ti);
                                    cn.colIdx = static_cast<int8_t>(ci); cn.text = t.vCols[ci].name;
                                    nodes.push_back(cn);
                                    selectItems.push_back({ static_cast<int16_t>(nodes.size() - 1), t.vCols[ci].name });
                                }
                            } else if (t.schema) {
                                for (size_t ci = 0; ci < t.schema->columns.size(); ++ci) {
                                    ExprNode cn; cn.kind = ExprKind::COL_REF; cn.tblIdx = static_cast<int8_t>(ti);
                                    cn.colIdx = static_cast<int8_t>(ci); cn.text = t.schema->columns[ci].columnName;
                                    nodes.push_back(cn);
                                    selectItems.push_back({ static_cast<int16_t>(nodes.size() - 1), t.schema->columns[ci].columnName });
                                }
                            }
                        }
                    } else if (itemEnd == i + 3 && toks[i].kind == SqlTokKind::K_IDENT &&
                               toks[i + 1].kind == SqlTokKind::K_DOT && toks[i + 2].kind == SqlTokKind::K_STAR) {
                        String q = toks[i].text;
                        bool matched = false;
                        for (size_t ti = 0; ti < tables.size(); ++ti) {
                            const auto& t = tables[ti];
                            if (!t.alias.equalsIgnoreCase(q) && !t.shortName.equalsIgnoreCase(q) && !t.fullName.equalsIgnoreCase(q)) continue;
                            matched = true;
                            if (t.isVirtual) {
                                for (size_t ci = 0; ci < t.vColCount; ++ci) {
                                    ExprNode cn; cn.kind = ExprKind::COL_REF; cn.tblIdx = static_cast<int8_t>(ti);
                                    cn.colIdx = static_cast<int8_t>(ci); cn.text = t.vCols[ci].name;
                                    nodes.push_back(cn);
                                    selectItems.push_back({ static_cast<int16_t>(nodes.size() - 1), t.vCols[ci].name });
                                }
                            } else if (t.schema) {
                                for (size_t ci = 0; ci < t.schema->columns.size(); ++ci) {
                                    ExprNode cn; cn.kind = ExprKind::COL_REF; cn.tblIdx = static_cast<int8_t>(ti);
                                    cn.colIdx = static_cast<int8_t>(ci); cn.text = t.schema->columns[ci].columnName;
                                    nodes.push_back(cn);
                                    selectItems.push_back({ static_cast<int16_t>(nodes.size() - 1), t.schema->columns[ci].columnName });
                                }
                            }
                            break;
                        }
                        if (!matched) {
                            failFn(SqlState::UNDEFINED_TABLE, "SELECT", q, "", "table alias \"" + q + "\" not found");
                            return false;
                        }
                    } else {
                        // Detectar alias opcional al final: [AS] alias
                        size_t exprEnd = itemEnd;
                        String aliasLabel = "";
                        if (itemEnd >= i + 3 && isKwAt(itemEnd - 2, "AS") && toks[itemEnd - 1].kind == SqlTokKind::K_IDENT) {
                            aliasLabel = toks[itemEnd - 1].text;
                            exprEnd = itemEnd - 2;
                        } else if (itemEnd >= i + 2 && toks[itemEnd - 1].kind == SqlTokKind::K_IDENT &&
                                   (toks[itemEnd - 2].kind == SqlTokKind::K_RP || toks[itemEnd - 2].kind == SqlTokKind::K_IDENT ||
                                    toks[itemEnd - 2].kind == SqlTokKind::K_NUM || toks[itemEnd - 2].kind == SqlTokKind::K_STR)) {
                            // Evitar confundir operadores o palabras clave con alias
                            if (!isKwAt(itemEnd - 2, "DISTINCT") && !isKwAt(itemEnd - 2, "NOT")) {
                                aliasLabel = toks[itemEnd - 1].text;
                                exprEnd = itemEnd - 1;
                            }
                        }

                        std::vector<SqlTok> subToks(toks.begin() + i, toks.begin() + exprEnd);
                        subToks.push_back({ SqlTokKind::K_EOF, "", false });
                        SqlExprParser ep(subToks, tables, nodes, &aggs);
                        int16_t eIdx = ep.parseExpr();
                        if (eIdx < 0) {
                            failFn(ep.errorState, "SELECT", "", "", ep.errorMsg);
                            return false;
                        }
                        if (aliasLabel.isEmpty()) {
                            aliasLabel = nodes[static_cast<size_t>(eIdx)].text;
                            if (aliasLabel.isEmpty()) aliasLabel = "col" + String(selectItems.size() + 1);
                        }
                        selectItems.push_back({ eIdx, aliasLabel });
                    }

                    i = (itemEnd < selEnd && toks[itemEnd].kind == SqlTokKind::K_COMMA) ? (itemEnd + 1) : itemEnd;
                }
            }

            if (selectItems.empty()) {
                failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "", "empty SELECT projection list");
                return false;
            }

            // 3. Parsear WHERE
            int16_t whereRoot = -1;
            if (pWhere >= 0) {
                const size_t wEnd = nextClauseAfter(pWhere);
                std::vector<SqlTok> subToks(toks.begin() + pWhere + 1, toks.begin() + wEnd);
                subToks.push_back({ SqlTokKind::K_EOF, "", false });
                SqlExprParser wp(subToks, tables, nodes, nullptr);
                whereRoot = wp.parseExpr();
                if (whereRoot < 0) {
                    failFn(wp.errorState, "SELECT", "", "", wp.errorMsg);
                    return false;
                }
            }

            // 4. Parsear GROUP BY
            std::vector<int16_t> groupExprs;
            if (pGroup >= 0) {
                size_t gStart = static_cast<size_t>(pGroup + 1);
                if (!isKwAt(gStart, "BY")) {
                    failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "", "expected BY after GROUP");
                    return false;
                }
                gStart++;
                const size_t gEnd = nextClauseAfter(pGroup);
                size_t i = gStart;
                while (i < gEnd) {
                    size_t ie = i;
                    int par = 0;
                    while (ie < gEnd) {
                        if (toks[ie].kind == SqlTokKind::K_LP) par++;
                        else if (toks[ie].kind == SqlTokKind::K_RP && par > 0) par--;
                        else if (par == 0 && toks[ie].kind == SqlTokKind::K_COMMA) break;
                        ie++;
                    }
                    std::vector<SqlTok> subToks(toks.begin() + i, toks.begin() + ie);
                    subToks.push_back({ SqlTokKind::K_EOF, "", false });
                    SqlExprParser gp(subToks, tables, nodes, nullptr);
                    int16_t gi = gp.parseExpr();
                    if (gi < 0) { failFn(gp.errorState, "SELECT", "", "", gp.errorMsg); return false; }
                    groupExprs.push_back(gi);
                    i = (ie < gEnd && toks[ie].kind == SqlTokKind::K_COMMA) ? (ie + 1) : ie;
                }
            }

            // 5. Parsear HAVING
            int16_t havingRoot = -1;
            if (pHaving >= 0) {
                const size_t hEnd = nextClauseAfter(pHaving);
                std::vector<SqlTok> subToks(toks.begin() + pHaving + 1, toks.begin() + hEnd);
                subToks.push_back({ SqlTokKind::K_EOF, "", false });
                SqlExprParser hp(subToks, tables, nodes, &aggs);
                havingRoot = hp.parseExpr();
                if (havingRoot < 0) { failFn(hp.errorState, "SELECT", "", "", hp.errorMsg); return false; }
            }

            // 6. Parsear ORDER BY (soporta expresiones, alias del SELECT y columnas no proyectadas)
            std::vector<OrderItem> orderItems;
            if (pOrder >= 0) {
                size_t oStart = static_cast<size_t>(pOrder + 1);
                if (!isKwAt(oStart, "BY")) {
                    failFn(SqlState::SYNTAX_ERROR, "SELECT", "", "", "expected BY after ORDER");
                    return false;
                }
                oStart++;
                const size_t oEnd = nextClauseAfter(pOrder);
                size_t i = oStart;
                while (i < oEnd) {
                    size_t ie = i;
                    int par = 0;
                    while (ie < oEnd) {
                        if (toks[ie].kind == SqlTokKind::K_LP) par++;
                        else if (toks[ie].kind == SqlTokKind::K_RP && par > 0) par--;
                        else if (par == 0 && toks[ie].kind == SqlTokKind::K_COMMA) break;
                        ie++;
                    }
                    bool desc = false;
                    size_t exprEnd = ie;
                    if (exprEnd > i && isKwAt(exprEnd - 1, "DESC")) { desc = true; exprEnd--; }
                    else if (exprEnd > i && isKwAt(exprEnd - 1, "ASC")) { desc = false; exprEnd--; }

                    OrderItem oi{};
                    oi.desc = desc;

                    // Permitir referencia a alias de SELECT (ej. ORDER BY media DESC)
                    if (exprEnd == i + 1 && toks[i].kind == SqlTokKind::K_IDENT) {
                        for (size_t s = 0; s < selectItems.size(); ++s) {
                            if (selectItems[s].label.equalsIgnoreCase(toks[i].text)) {
                                oi.aliasCol = static_cast<int8_t>(s);
                                oi.exprIdx  = selectItems[s].exprIdx;
                                break;
                            }
                        }
                    }

                    if (oi.exprIdx < 0) {
                        std::vector<SqlTok> subToks(toks.begin() + i, toks.begin() + exprEnd);
                        subToks.push_back({ SqlTokKind::K_EOF, "", false });
                        SqlExprParser op(subToks, tables, nodes, &aggs);
                        oi.exprIdx = op.parseExpr();
                        if (oi.exprIdx < 0) { failFn(op.errorState, "SELECT", "", "", op.errorMsg); return false; }
                    }

                    orderItems.push_back(oi);
                    i = (ie < oEnd && toks[ie].kind == SqlTokKind::K_COMMA) ? (ie + 1) : ie;
                }
            }

            // 7. Parsear LIMIT y OFFSET
            uint16_t limitRows  = DEFAULT_LIMIT_ROWS;
            uint32_t offsetRows = 0;
            if (pLimit >= 0 && static_cast<size_t>(pLimit + 1) < totalToks) {
                int lim = toks[static_cast<size_t>(pLimit + 1)].text.toInt();
                if (lim > 0) limitRows = static_cast<uint16_t>(std::min<int>(lim, MAX_MAT_ROWS));
            }
            if (pOffset >= 0 && static_cast<size_t>(pOffset + 1) < totalToks) {
                long off = toks[static_cast<size_t>(pOffset + 1)].text.toInt();
                if (off > 0) offsetRows = static_cast<uint32_t>(off);
            }

            const bool isAggQuery = !aggs.empty() || !groupExprs.empty();

            // FAST PATH 1: SELECT COUNT(*) FROM <tabla_fisica> sin WHERE ni GROUP BY -> O(1) header
            if (tables.size() == 1 && !tables[0].isVirtual && whereRoot < 0 && groupExprs.empty() &&
                selectItems.size() == 1 && aggs.size() == 1 && aggs[0].kind == AggKind::COUNT_ALL &&
                nodes[static_cast<size_t>(selectItems[0].exprIdx)].kind == ExprKind::AGG_REF) {
                PgBinHeader hdr{};
                uint32_t total = 0;
                if (PgBinaryStorage::getInstance().getTableHeader(tables[0].schema->tableName.c_str(), hdr)) {
                    total = hdr.activeRows;
                }
                char tmp[96]{};
                snprintf(tmp, sizeof(tmp), " %s \n-------\n %lu\n(1 row)",
                         selectItems[0].label.c_str(), static_cast<unsigned long>(total));
                size_t p = 0;
                appendFn(outBuf, outCap, p, tmp);
                filasOut = 1;
                return true;
            }

            // Motor unificado de recorrido (0 tablas, 1 tabla con fast-backward o N tablas con JOIN)
            std::vector<OutRow>      outRows;
            std::vector<GroupBucket> buckets;
            bool timedOut = false;

            auto rowBefore = [&](const OutRow& a, const OutRow& b) -> bool {
                for (size_t k = 0; k < orderItems.size() && k < a.sortKeys.size() && k < b.sortKeys.size(); ++k) {
                    int cmp = compareSqlVals(a.sortKeys[k], b.sortKeys[k]);
                    if (cmp != 0) return orderItems[k].desc ? (cmp > 0) : (cmp < 0);
                }
                return false;
            };

            const size_t maxKeep = std::min<size_t>(static_cast<size_t>(offsetRows) + limitRows, MAX_MAT_ROWS);
            uint32_t skippedNoOrder = 0;

            auto onJoinedRow = [&](const RowBinding* rowBindings) -> bool {
                if (esp_timer_get_time() > deadlineUs) { timedOut = true; return false; }
                if (whereRoot >= 0 && !evalExprNode(whereRoot, nodes, tables, rowBindings).toBool()) {
                    return true;
                }

                if (isAggQuery) {
                    String gKey = "";
                    for (size_t g = 0; g < groupExprs.size(); ++g) {
                        if (g > 0) gKey += "\x1F";
                        SqlVal gv = evalExprNode(groupExprs[g], nodes, tables, rowBindings);
                        gKey += gv.isNull ? "\x01NULL" : gv.str;
                    }

                    GroupBucket* bkt = nullptr;
                    for (auto& b : buckets) {
                        if (b.key == gKey) { bkt = &b; break; }
                    }
                    if (!bkt) {
                        if (buckets.size() >= MAX_GROUPS) return true;
                        GroupBucket nb;
                        nb.key = gKey;
                        nb.aggStates.resize(aggs.size());
                        // Guardar una muestra de columnas no agregadas para proyectarlas al final
                        nb.projSampleVals.reserve(selectItems.size());
                        for (const auto& si : selectItems) {
                            if (nodes[static_cast<size_t>(si.exprIdx)].kind == ExprKind::COL_REF) {
                                nb.projSampleVals.push_back(evalExprNode(si.exprIdx, nodes, tables, rowBindings));
                            } else {
                                nb.projSampleVals.push_back(SqlVal::makeNull());
                            }
                        }
                        buckets.push_back(std::move(nb));
                        bkt = &buckets.back();
                    }

                    for (size_t a = 0; a < aggs.size(); ++a) {
                        SqlVal av = (aggs[a].kind == AggKind::COUNT_ALL)
                            ? SqlVal::makeNum(1, true)
                            : evalExprNode(aggs[a].argExpr, nodes, tables, rowBindings);
                        bkt->aggStates[a].accumulate(aggs[a].kind, av);
                    }
                    return true;
                }

                // Fila normal (sin agregación)
                OutRow r;
                r.cells.reserve(selectItems.size());
                for (const auto& si : selectItems) {
                    SqlVal v = evalExprNode(si.exprIdx, nodes, tables, rowBindings);
                    r.cells.push_back(v.isNull ? String("NULL") : v.str);
                }

                if (isDistinct) {
                    for (const auto& ex : outRows) {
                        if (ex.cells == r.cells) return true;
                    }
                }

                if (!orderItems.empty()) {
                    r.sortKeys.reserve(orderItems.size());
                    for (const auto& oi : orderItems) {
                        r.sortKeys.push_back(evalExprNode(oi.exprIdx, nodes, tables, rowBindings));
                    }
                    if (outRows.size() == maxKeep && !rowBefore(r, outRows.back())) return true;
                    auto it = std::lower_bound(outRows.begin(), outRows.end(), r, rowBefore);
                    if (outRows.size() < maxKeep) {
                        outRows.insert(it, std::move(r));
                    } else if (it != outRows.end()) {
                        outRows.pop_back();
                        outRows.insert(it, std::move(r));
                    }
                    return true;
                }

                if (skippedNoOrder < offsetRows) {
                    skippedNoOrder++;
                    return true;
                }
                outRows.push_back(std::move(r));
                return outRows.size() < limitRows;
            };

            if (tables.empty()) {
                // SELECT sin FROM (ej. SELECT version(), SELECT NOW(), SELECT 2 + 2)
                onJoinedRow(nullptr);
            } else if (tables.size() == 1 && !tables[0].isVirtual && !isAggQuery && !isDistinct &&
                       orderItems.size() == 1 && orderItems[0].desc &&
                       nodes[static_cast<size_t>(orderItems[0].exprIdx)].kind == ExprKind::COL_REF &&
                       (nodes[static_cast<size_t>(orderItems[0].exprIdx)].colIdx == tables[0].schema->pkColIndex ||
                        nodes[static_cast<size_t>(orderItems[0].exprIdx)].colIdx == tables[0].schema->identityColIndex)) {
                // FAST PATH 2: 1 tabla física + ORDER BY pk DESC -> scanBackward directo (< 5 ms)
                orderItems.clear(); // ya viene ordenado DESC desde disco
                PgBinaryStorage::getInstance().scanBackward(
                    tables[0].schema->tableName.c_str(),
                    [&](const PgRowView& rv, uint32_t) -> bool {
                        RowBinding rb{}; rb.physRow = &rv;
                        return onJoinedRow(&rb);
                    }
                );
            } else {
                // Recorrido recursivo de 1..4 tablas (físicas y/o virtuales) con buffers independientes por nivel
                std::vector<std::vector<uint8_t>> levelBufs(tables.size());
                for (size_t t = 0; t < tables.size(); ++t) {
                    if (!tables[t].isVirtual) {
                        levelBufs[t].resize(640);
                    }
                }
                RowBinding bindings[MAX_JOIN_TABLES]{};

                auto runLevel = [&](auto&& self, size_t level) -> bool {
                    if (level >= tables.size()) {
                        return onJoinedRow(bindings);
                    }
                    const BoundTable& tb = tables[level];
                    bool matchedAny = false;
                    bool keepGoing  = true;

                    auto step = [&]() -> bool {
                        if (esp_timer_get_time() > deadlineUs) { timedOut = true; keepGoing = false; return false; }
                        if (tb.onExprRoot >= 0 && !evalExprNode(tb.onExprRoot, nodes, tables, bindings).toBool()) {
                            return true;
                        }
                        matchedAny = true;
                        keepGoing = self(self, level + 1);
                        return keepGoing;
                    };

                    if (tb.isVirtual) {
                        PgVirtualCatalog::scanRows(tb.vKind, [&](const String* vals, const bool* nulls, size_t nCols) -> bool {
                            bindings[level].physRow = nullptr;
                            bindings[level].vVals   = vals;
                            bindings[level].vNulls  = nulls;
                            bindings[level].vCount  = nCols;
                            return step();
                        });
                    } else {
                        PgBinaryStorage::getInstance().scanForwardWithBuf(
                            tb.schema->tableName.c_str(),
                            levelBufs[level].data(),
                            levelBufs[level].size(),
                            [&](const PgRowView& rv, uint32_t) -> bool {
                                bindings[level].physRow = &rv;
                                bindings[level].vVals   = nullptr;
                                bindings[level].vNulls  = nullptr;
                                bindings[level].vCount  = 0;
                                return step();
                            }
                        );
                    }

                    if (keepGoing && tb.isLeftJoin && !matchedAny) {
                        bindings[level] = RowBinding{};
                        keepGoing = self(self, level + 1);
                    }
                    return keepGoing;
                };

                runLevel(runLevel, 0);
            }

            if (timedOut) {
                failFn("57014", "SELECT", "", "",
                       "query canceled: exceeded 2.5s real-time safety budget (add WHERE or LIMIT)");
                return false;
            }

            // Finalizar agregaciones y HAVING
            if (isAggQuery) {
                if (buckets.empty() && groupExprs.empty()) {
                    GroupBucket emptyBkt;
                    emptyBkt.aggStates.resize(aggs.size());
                    buckets.push_back(std::move(emptyBkt));
                }

                for (const auto& b : buckets) {
                    std::vector<SqlVal> aggRes;
                    aggRes.reserve(aggs.size());
                    for (size_t a = 0; a < aggs.size(); ++a) {
                        aggRes.push_back(b.aggStates[a].finalize(aggs[a].kind));
                    }

                    // Para evaluar HAVING / ORDER BY usamos los valores agregados + muestras del grupo
                    if (havingRoot >= 0 && !evalExprNode(havingRoot, nodes, tables, nullptr, &aggRes).toBool()) {
                        continue;
                    }

                    OutRow r;
                    r.cells.reserve(selectItems.size());
                    std::vector<SqlVal> projVals;
                    projVals.reserve(selectItems.size());

                    for (size_t s = 0; s < selectItems.size(); ++s) {
                        const auto& si = selectItems[s];
                        SqlVal v = (nodes[static_cast<size_t>(si.exprIdx)].kind == ExprKind::COL_REF && s < b.projSampleVals.size())
                            ? b.projSampleVals[s]
                            : evalExprNode(si.exprIdx, nodes, tables, nullptr, &aggRes);
                        projVals.push_back(v);
                        r.cells.push_back(v.isNull ? String("NULL") : v.str);
                    }

                    if (!orderItems.empty()) {
                        r.sortKeys.reserve(orderItems.size());
                        for (const auto& oi : orderItems) {
                            if (oi.aliasCol >= 0 && static_cast<size_t>(oi.aliasCol) < projVals.size()) {
                                r.sortKeys.push_back(projVals[static_cast<size_t>(oi.aliasCol)]);
                            } else {
                                r.sortKeys.push_back(evalExprNode(oi.exprIdx, nodes, tables, nullptr, &aggRes));
                            }
                        }
                    }
                    outRows.push_back(std::move(r));
                }

                if (!orderItems.empty()) {
                    std::stable_sort(outRows.begin(), outRows.end(), rowBefore);
                }
            }

            // Formateo estilo psql en salidaPsql[1024]
            size_t outPos = 0;
            for (size_t c = 0; c < selectItems.size(); ++c) {
                if (c > 0) appendFn(outBuf, outCap, outPos, " | ");
                appendFn(outBuf, outCap, outPos, selectItems[c].label.c_str());
            }
            appendFn(outBuf, outCap, outPos, "\n------------------------------------------------\n");

            const size_t startIdx = (!orderItems.empty() || isAggQuery) ? offsetRows : 0;
            uint16_t shown = 0;
            bool truncated = false;

            for (size_t i = startIdx; i < outRows.size() && shown < limitRows; ++i) {
                if (outPos > outCap - 96) {
                    appendFn(outBuf, outCap, outPos, "... (truncated to fit 1KB RPC buffer; use LIMIT/OFFSET)\n");
                    truncated = true;
                    break;
                }
                for (size_t c = 0; c < outRows[i].cells.size(); ++c) {
                    if (c > 0) appendFn(outBuf, outCap, outPos, " | ");
                    appendFn(outBuf, outCap, outPos, outRows[i].cells[c].c_str());
                }
                appendFn(outBuf, outCap, outPos, "\n");
                shown++;
            }

            char foot[40]{};
            snprintf(foot, sizeof(foot), "(%u row%s%s)",
                     static_cast<unsigned>(shown),
                     shown == 1 ? "" : "s",
                     truncated ? ", partial" : "");
            appendFn(outBuf, outCap, outPos, foot);
            filasOut = shown;
            return true;
        }
    };

} // namespace DbSql