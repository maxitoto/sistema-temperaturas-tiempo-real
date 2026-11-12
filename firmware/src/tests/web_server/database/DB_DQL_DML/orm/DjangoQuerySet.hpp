#pragma once
#include <Arduino.h>
#include <vector>
#include "../../DB_DDL/engine/DdlDiagnosticConsole.hpp"
#include "../engine/ModuloDQL.hpp"
#include "../engine/ModuloDML.hpp"

namespace DbRuntime::Orm {

    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::LogicConnector;
    using DbRuntime::Standard::JoinType;
    using DbRuntime::Standard::AggregateFunc;
    using DbRuntime::Standard::WindowFunc;
    using DbRuntime::Standard::WindowFrameUnit;
    using DbRuntime::Standard::WindowBoundType;
    using DbRuntime::Standard::SetOpType;
    using DbRuntime::Standard::GroupingMode;
    using DbRuntime::Engine::PredicateCondition;
    using DbRuntime::Engine::JoinClauseDef;
    using DbRuntime::Engine::AggregateSpec;
    using DbRuntime::Engine::WindowSpec;
    using DbRuntime::Engine::OrderBySpec;
    using DbRuntime::Engine::CteDefinition;
    using DbRuntime::Engine::SetOperationAttachment;
    using DbRuntime::Engine::SelectQueryPlan;
    using DbRuntime::Engine::DmlExecutionResult;
    using DbRuntime::Engine::ModuloDQL;
    using DbRuntime::Engine::ModuloDML;
    using DbRuntime::Engine::LateralGeneratorFn;
    using DbRuntime::Engine::SubqueryEvaluatorFn;
    using DbNotRuntime::Standard::SortDirection;
    using DbNotRuntime::Standard::NullsOrder;
    using DbNotRuntime::Standard::SqlState;

    // Constructor fluido de consultas estilo Django QuerySet que compila hacia SelectQueryPlan / ModuloDML sin SQL manual.
    class DjangoQuerySet {
    private:
        SelectQueryPlan plan;
        std::vector<String> returningCols;

    public:
        using QuerySet = DjangoQuerySet;

        // Inicializa un QuerySet apuntando a una tabla física del catálogo o a una CTE.
        explicit DjangoQuerySet(const String& tableName) {
            plan.fromTable = tableName;
        }

        // ---------------- Proyección y Deduplicación (.values / .distinct) ----------------

        // Equivale a .values("col1", "col2") en Django proyectando únicamente las columnas indicadas.
        QuerySet& values(const std::vector<String>& cols) {
            plan.selectColumns = cols;
            return *this;
        }

        // Activa SELECT DISTINCT o SELECT DISTINCT ON (cols...) de PostgreSQL si se reciben columnas.
        QuerySet& distinct(const std::vector<String>& distinctOnCols = {}) {
            if (distinctOnCols.empty()) {
                plan.selectDistinct = true;
            } else {
                plan.distinctOnColumns = distinctOnCols;
            }
            return *this;
        }

        // ---------------- Filtrado Determinista (.filter / .exclude / .or_filter) ----------------

        // Agrega una condición WHERE unida por AND con operador explícito de PostgreSQL.
        QuerySet& filter(const String& col, FilterOp op, const String& valA = "", const String& valB = "") {
            PredicateCondition c;
            c.columnOrExpr = col;
            c.op           = op;
            c.valueA       = valA;
            c.valueB       = valB;
            c.connector    = LogicConnector::AND_OP;
            c.negated      = false;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // Compara una columna contra otra (necesario para correlacionar en UPDATE ... FROM / DELETE ... USING).
        QuerySet& filter_col(const String& col, FilterOp op, const String& otherCol) {
            PredicateCondition c;
            c.columnOrExpr  = col;
            c.op            = op;
            c.valueA        = otherCol;
            c.valueIsColumn = true;
            c.connector     = LogicConnector::AND_OP;
            c.negated       = false;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // Atajo ergonómico para igualdad exacta col = val (aprovecha índices PK / UNIQUE / BTREE).
        QuerySet& filter_eq(const String& col, const String& val) {
            return filter(col, FilterOp::EQ, val);
        }

        // Sobrecarga de igualdad exacta para enteros sin construir Strings innecesarios en el llamador.
        QuerySet& filter_eq(const String& col, int32_t val) {
            return filter(col, FilterOp::EQ, String(val));
        }

        // Agrega una condición WHERE negada (equivale a .exclude(...) en Django).
        QuerySet& exclude(const String& col, FilterOp op, const String& valA = "", const String& valB = "") {
            PredicateCondition c;
            c.columnOrExpr = col;
            c.op           = op;
            c.valueA       = valA;
            c.valueB       = valB;
            c.connector    = LogicConnector::AND_OP;
            c.negated      = true;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // Agrega una condición WHERE unida mediante el conector lógico OR.
        QuerySet& or_filter(const String& col, FilterOp op, const String& valA = "", const String& valB = "") {
            PredicateCondition c;
            c.columnOrExpr = col;
            c.op           = op;
            c.valueA       = valA;
            c.valueB       = valB;
            c.connector    = LogicConnector::OR_OP;
            c.negated      = false;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // Filtra verificando pertenencia a una lista de valores: WHERE col IN (v1, v2, ...).
        QuerySet& filter_in(const String& col, const std::vector<String>& list, bool negate = false) {
            PredicateCondition c;
            c.columnOrExpr = col;
            c.op           = negate ? FilterOp::NOT_IN_LIST : FilterOp::IN_LIST;
            c.valueList    = list;
            c.connector    = LogicConnector::AND_OP;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // Evalúa una subconsulta correlacionada mediante puntero a función ligero (EXISTS / ANY / ALL).
        QuerySet& filter_subquery(FilterOp subOp, SubqueryEvaluatorFn fn, bool negate = false) {
            PredicateCondition c;
            c.op         = subOp;
            c.subqueryFn = fn;
            c.negated    = negate;
            c.connector  = LogicConnector::AND_OP;
            plan.wherePredicates.push_back(std::move(c));
            return *this;
        }

        // ---------------- Uniones Relacionales (.join / .select_related / .join_lateral) ----------------

        // Agrega un JOIN explícito (INNER, LEFT, RIGHT, FULL OUTER, CROSS o NATURAL).
        QuerySet& join(const String& rightTable,
                       const String& leftCol,
                       const String& rightCol,
                       JoinType type = JoinType::INNER) {
            JoinClauseDef j;
            j.joinType    = type;
            j.rightTable  = rightTable;
            j.leftColumn  = leftCol;
            j.rightColumn = rightCol;
            plan.joins.push_back(std::move(j));
            return *this;
        }

        // Equivale a .select_related() en Django realizando un JOIN (INNER por defecto) sobre la clave foránea indicada.
        QuerySet& select_related(const String& parentTable,
                                 const String& fkCol,
                                 const String& pkCol = "",
                                 JoinType type = JoinType::INNER) {
            return join(parentTable, fkCol, pkCol.isEmpty() ? fkCol : pkCol, type);
        }

        // Agrega un JOIN ... USING (col1, col2) estándar de PostgreSQL.
        QuerySet& join_using(const String& rightTable, const std::vector<String>& commonCols, JoinType type = JoinType::INNER) {
            JoinClauseDef j;
            j.joinType     = type;
            j.rightTable   = rightTable;
            j.usingColumns = commonCols;
            plan.joins.push_back(std::move(j));
            return *this;
        }

        // Agrega una subconsulta correlacionada JOIN LATERAL (PostgreSQL).
        QuerySet& join_lateral(const String& alias, LateralGeneratorFn generator) {
            JoinClauseDef j;
            j.joinType     = JoinType::LATERAL_JOIN;
            j.lateralAlias = alias;
            j.lateralFn    = generator;
            plan.joins.push_back(std::move(j));
            return *this;
        }

        // ---------------- Agrupación y Agregación (.group_by / .annotate / .having) ----------------

        // Define las columnas de agrupación GROUP BY (con soporte para SIMPLE, ROLLUP y CUBE).
        QuerySet& group_by(const std::vector<String>& cols, GroupingMode mode = GroupingMode::SIMPLE) {
            plan.groupByColumns = cols;
            plan.groupingMode   = mode;
            return *this;
        }

        // Equivale a .annotate(alias=Func("col")) en Django con soporte opcional para cláusula FILTER (WHERE ...).
        QuerySet& annotate(const String& alias,
                           AggregateFunc func,
                           const String& targetCol = "*",
                           bool hasFilter = false,
                           const PredicateCondition& filterCond = {}) {
            AggregateSpec ag;
            ag.alias           = alias;
            ag.func            = func;
            ag.targetColumn    = targetCol;
            ag.hasFilter       = hasFilter;
            ag.filterCondition = filterCond;
            plan.aggregates.push_back(std::move(ag));
            return *this;
        }

        // Agrega una función de agregación con cláusula FILTER (WHERE ...) nativa de PostgreSQL.
        QuerySet& annotate_filtered(const String& alias,
                                    AggregateFunc func,
                                    const String& targetCol,
                                    const PredicateCondition& filterCond) {
            return annotate(alias, func, targetCol, true, filterCond);
        }

        // Agrega una agregación de conjunto ordenado PERCENTILE_CONT(p) WITHIN GROUP (ORDER BY col).
        QuerySet& annotate_within_group(const String& alias,
                                        float fraction,
                                        const String& orderByCol,
                                        SortDirection dir = SortDirection::ASC) {
            AggregateSpec ag;
            ag.alias              = alias;
            ag.func               = AggregateFunc::PERCENTILE_CONT;
            ag.targetColumn       = orderByCol;
            ag.percentileFraction = fraction;
            ag.withinGroupDir     = dir;
            plan.aggregates.push_back(std::move(ag));
            return *this;
        }

        // Alias ergonómico para PERCENTILE_CONT(p) WITHIN GROUP (ORDER BY col).
        QuerySet& annotate_percentile(const String& alias,
                                      float fraction,
                                      const String& orderByCol,
                                      SortDirection dir = SortDirection::ASC) {
            return annotate_within_group(alias, fraction, orderByCol, dir);
        }

        // Agrega una condición de filtrado post-agregación HAVING.
        QuerySet& having(const String& aggAliasOrCol, FilterOp op, const String& valA) {
            PredicateCondition c;
            c.columnOrExpr = aggAliasOrCol;
            c.op           = op;
            c.valueA       = valA;
            plan.havingPredicates.push_back(std::move(c));
            return *this;
        }

        // ---------------- Funciones de Ventana OVER (...) ----------------

        // Registra una especificación completa de ventana analítica OVER (...).
        QuerySet& window(const WindowSpec& winSpec) {
            plan.windows.push_back(winSpec);
            return *this;
        }

        // Sobrecarga ergonómica para declarar funciones de ventana con PARTITION BY, ORDER BY y ROWS BETWEEN.
        QuerySet& window(const String& alias,
                         WindowFunc func,
                         const String& targetCol,
                         const std::vector<String>& partitionBy,
                         const String& orderByCol,
                         SortDirection dir = SortDirection::ASC,
                         WindowBoundType startBound = WindowBoundType::UNBOUNDED_PRECEDING,
                         WindowBoundType endBound = WindowBoundType::CURRENT_ROW,
                         int16_t startOff = 0,
                         int16_t endOff = 0) {
            WindowSpec w;
            w.alias              = alias;
            w.func               = func;
            w.targetColumn       = targetCol;
            w.partitionByColumns = partitionBy;
            w.orderByColumn      = orderByCol;
            w.orderDir           = dir;
            w.startBound         = startBound;
            w.endBound           = endBound;
            w.startOffset        = startOff;
            w.endOffset          = endOff;
            plan.windows.push_back(std::move(w));
            return *this;
        }

        // ---------------- CTEs (WITH / WITH RECURSIVE) y Conjuntos ----------------

        // Registra una expresión de tabla común (WITH [RECURSIVE] cteName AS ...).
        QuerySet& with_cte(const String& cteName,
                           const std::vector<PgTuple>& baseRows,
                           bool recursive = false,
                           LateralGeneratorFn stepFn = nullptr,
                           uint8_t maxDepth = 16) {
            CteDefinition cte;
            cte.cteName           = cteName;
            cte.baseRows          = baseRows;
            cte.isRecursive       = recursive;
            cte.recursiveStepFn   = stepFn;
            cte.maxRecursionDepth = maxDepth;
            plan.ctes.push_back(std::move(cte));
            return *this;
        }

        // Adjunta una operación UNION [ALL] con otro conjunto de resultados.
        QuerySet& union_with(const std::vector<PgTuple>& rightRows, bool all = false) {
            plan.setOperations.push_back({all ? SetOpType::UNION_ALL : SetOpType::UNION_DISTINCT, rightRows});
            return *this;
        }

        // Adjunta una operación INTERSECT [ALL] con otro conjunto de resultados.
        QuerySet& intersect_with(const std::vector<PgTuple>& rightRows, bool all = false) {
            plan.setOperations.push_back({all ? SetOpType::INTERSECT_ALL : SetOpType::INTERSECT_DISTINCT, rightRows});
            return *this;
        }

        // Adjunta una operación EXCEPT [ALL] con otro conjunto de resultados.
        QuerySet& except_with(const std::vector<PgTuple>& rightRows, bool all = false) {
            plan.setOperations.push_back({all ? SetOpType::EXCEPT_ALL : SetOpType::EXCEPT_DISTINCT, rightRows});
            return *this;
        }

        // ---------------- Ordenamiento, Paginación y RETURNING ----------------

        // Agrega un criterio ORDER BY soportando prefijo '-' estilo Django ("-fecha") y NULLS FIRST / LAST.
        QuerySet& order_by(const String& col,
                           SortDirection dir = SortDirection::ASC,
                           NullsOrder nulls = NullsOrder::LAST) {
            OrderBySpec ob;
            if (col.startsWith("-")) {
                ob.column    = col.substring(1);
                ob.direction = SortDirection::DESC;
            } else {
                ob.column    = col;
                ob.direction = dir;
            }
            ob.nullsOrder = nulls;
            plan.orderBy.push_back(std::move(ob));
            return *this;
        }

        // Define el límite máximo de filas a devolver (LIMIT n).
        QuerySet& limit(int32_t maxRows) {
            plan.limitCount = maxRows;
            return *this;
        }

        // Define el desplazamiento inicial de filas (OFFSET n).
        QuerySet& offset(int32_t skipRows) {
            plan.offsetCount = skipRows;
            return *this;
        }

        // Equivale al estándar SQL FETCH FIRST n ROWS ONLY.
        QuerySet& fetch_first(int32_t maxRows) {
            plan.limitCount = maxRows;
            return *this;
        }

        // Solicita que la operación DML posterior (update / remove) devuelva las columnas indicadas en RETURNING.
        QuerySet& returning(const std::vector<String>& cols = std::vector<String>{"*"}) {
            returningCols = cols;
            return *this;
        }

        // ---------------- Ejecutores Terminales DQL y DML ----------------

        // Ejecuta la consulta SELECT y devuelve todas las tuplas resultantes.
        std::vector<PgTuple> all() const {
            return ModuloDQL::getInstance().executeSelect(plan);
        }

        // Ejecuta la consulta SELECT y mapea cada fila a un struct/DTO de C++ sin overhead dinámico.
        template <typename TStruct>
        std::vector<TStruct> all_as(TStruct (*mapper)(const PgTuple&)) const {
            std::vector<PgTuple> rows = all();
            std::vector<TStruct> out;
            out.reserve(rows.size());
            for (const auto& r : rows) {
                out.push_back(mapper(r));
            }
            return out;
        }

        // Equivale a .get() en Django: exige exactamente 1 fila o emite error en DdlDiagnosticConsole.
        bool get(PgTuple& outSingleRow) const {
            std::vector<PgTuple> rows = all();
            if (rows.size() != 1) {
                DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().raiseError(
                    SqlState::DATA_EXCEPTION, "SELECT GET", plan.fromTable, "",
                    rows.empty() ? "QuerySet.get() returned no rows (DoesNotExist)"
                                 : "QuerySet.get() returned more than one row (MultipleObjectsReturned)"
                );
                return false;
            }
            outSingleRow = std::move(rows[0]);
            return true;
        }

        // Devuelve la primera fila del resultado o false si está vacío (.first() en Django).
        bool first(PgTuple& outRow) const {
            SelectQueryPlan fastPlan = plan;
            fastPlan.limitCount = 1;
            auto rows = ModuloDQL::getInstance().executeSelect(fastPlan);
            if (rows.empty()) return false;
            outRow = std::move(rows[0]);
            return true;
        }

        // Verifica eficientemente si existe al menos una fila que cumpla los filtros (.exists() en Django).
        bool exists() const {
            SelectQueryPlan fastPlan = plan;
            fastPlan.limitCount = 1;
            return !ModuloDQL::getInstance().executeSelect(fastPlan).empty();
        }

        // Cuenta el total de filas que satisfacen el QuerySet (.count() en Django).
        size_t count() const {
            return all().size();
        }

        // Ejecuta un UPDATE determinista sobre las filas filtradas por este QuerySet.
        DmlExecutionResult update(const std::vector<PgCell>& changes,
                                  bool allowUnconditional = false,
                                  const String& fromAuxTable = "") const {
            return ModuloDML::getInstance().updateRows(
                plan.fromTable, changes, plan.wherePredicates, returningCols, allowUnconditional, fromAuxTable
            );
        }

        // Ejecuta un DELETE determinista sobre las filas filtradas por este QuerySet.
        DmlExecutionResult remove(bool allowUnconditional = false,
                                  const String& usingAuxTable = "") const {
            return ModuloDML::getInstance().deleteRows(
                plan.fromTable, plan.wherePredicates, returningCols, allowUnconditional, usingAuxTable
            );
        }

        // Ejecuta la consulta y devuelve el reporte EXPLAIN ANALYZE con costos de Stack, CPU, Tiempo e Índice.
        String explain_analyze() const {
            ModuloDQL::getInstance().executeSelect(plan);
            return ModuloDQL::getInstance().getLastCost().toExplainAnalyzeString();
        }

        // Devuelve el plan lógico compilado para inspección en tests.
        const SelectQueryPlan& compiledPlan() const { return plan; }
    };

    using QuerySet = DjangoQuerySet;

} // namespace DbRuntime::Orm