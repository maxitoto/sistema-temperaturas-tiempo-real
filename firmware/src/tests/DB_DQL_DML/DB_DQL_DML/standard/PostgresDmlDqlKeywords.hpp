#pragma once
#include <stdint.h>

namespace DbRuntime::Standard {

    // Operadores de comparación y predicados de filtrado según Sección DQL de informe_2.md.
    enum class FilterOp : uint8_t {
        EQ = 0,          // =
        NEQ,             // <> o !=
        GT,              // >
        GTE,             // >=
        LT,              // <
        LTE,             // <=
        LIKE,            // LIKE (sensible a mayúsculas)
        ILIKE,           // ILIKE (insensible a mayúsculas, exclusivo PostgreSQL)
        SIMILAR_TO,      // SIMILAR TO
        IN_LIST,         // IN (...)
        NOT_IN_LIST,     // NOT IN (...)
        BETWEEN,         // BETWEEN a AND b
        IS_NULL,         // IS NULL
        IS_NOT_NULL,     // IS NOT NULL
        EXISTS_SUBQUERY, // EXISTS (SELECT ...)
        ANY_SUBQUERY,    // = ANY (SELECT ...)
        ALL_SUBQUERY     // > ALL (SELECT ...)
    };

    // Conectores lógicos para árboles o listas de predicados WHERE / HAVING.
    enum class LogicConnector : uint8_t {
        AND_OP = 0,
        OR_OP  = 1,
        NOT_OP = 2
    };

    // Tipos de unión relacional en PostgreSQL según informe_2.md.
    enum class JoinType : uint8_t {
        INNER = 0,
        LEFT_OUTER,
        RIGHT_OUTER,
        FULL_OUTER,
        CROSS,
        NATURAL_INNER,
        LATERAL_JOIN
    };

    // Funciones de agregación y ordenación de grupos en PostgreSQL.
    enum class AggregateFunc : uint8_t {
        NONE = 0,
        COUNT_ALL,       // COUNT(*)
        COUNT_COL,       // COUNT(col)
        COUNT_DISTINCT,  // COUNT(DISTINCT col)
        SUM,
        AVG,
        MIN,
        MAX,
        PERCENTILE_CONT  // Uso con WITHIN GROUP (ORDER BY ...)
    };

    // Modos de agrupación avanzada en GROUP BY.
    enum class GroupingMode : uint8_t {
        SIMPLE = 0,
        ROLLUP,          // GROUP BY ROLLUP(...)
        CUBE,            // GROUP BY CUBE(...)
        GROUPING_SETS    // GROUP BY GROUPING SETS(...)
    };

    // Funciones de ventana analítica (OVER (PARTITION BY ... ORDER BY ...)).
    enum class WindowFunc : uint8_t {
        ROW_NUMBER = 0,
        RANK,
        DENSE_RANK,
        LAG,
        LEAD,
        FIRST_VALUE,
        LAST_VALUE,
        SUM_OVER,
        AVG_OVER
    };

    // Unidades y límites de marco de ventana (ROWS / RANGE BETWEEN ...).
    enum class WindowFrameUnit : uint8_t { ROWS = 0, RANGE = 1 };

    enum class WindowBoundType : uint8_t {
        UNBOUNDED_PRECEDING = 0,
        N_PRECEDING,
        CURRENT_ROW,
        N_FOLLOWING,
        UNBOUNDED_FOLLOWING
    };

    // Operaciones de teoría de conjuntos entre consultas DQL.
    enum class SetOpType : uint8_t {
        UNION_DISTINCT = 0,
        UNION_ALL,
        INTERSECT_DISTINCT,
        INTERSECT_ALL,
        EXCEPT_DISTINCT,
        EXCEPT_ALL
    };

    // Políticas de resolución de conflictos en INSERT ... ON CONFLICT (Upsert PostgreSQL).
    enum class ConflictAction : uint8_t {
        NONE = 0,
        DO_NOTHING,      // ON CONFLICT DO NOTHING
        DO_UPDATE        // ON CONFLICT (target) DO UPDATE SET ...
    };

    // Acciones para la sentencia estándar MERGE INTO ... WHEN MATCHED / NOT MATCHED.
    enum class MergeActionType : uint8_t {
        UPDATE_SET = 0,
        DELETE_ROW,
        INSERT_VALUES,
        DO_NOTHING
    };

} // namespace DbRuntime::Standard