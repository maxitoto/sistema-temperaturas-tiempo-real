src/database_runtime/
├── interfaces/
│   └── IRuntimeDbEngine.hpp          # Qué hace: Contrato abstracto para desacoplar el motor runtime de capas superiores (RPC/Repositorios) a futuro.
├── telemetry/
│   └── RuntimeCostProfiler.hpp       # Qué hace: Medidor hardware de Ciclos de CPU (CCOUNT), Tiempo (us), Stack (HighWaterMark), Heap e Index Hit.
├── standard/
│   └── PostgresDmlDqlKeywords.hpp    # Qué hace: Diccionario estático de cláusulas, operadores, joins, ventanas y conjuntos según informe_2.md.
├── storage/
│   ├── RuntimeTypedTuple.hpp         # Qué hace: Representación compacta de celdas y tuplas tipadas con mapeo directo a structs C++ sin std::map.
│   └── RuntimeIndexAccessPath.hpp    # Qué hace: Motor de índices reales (B-Tree / Hash en RAM con byte-offsets a SD) sincronizado con el DDL.
├── engine/
│   ├── AccessPathPlanner.hpp         # Qué hace: Optimizador que inspecciona el catálogo DDL y elige entre Index Scan, Unique Scan o Seq Scan.
│   ├── ModuloDQL.hpp                 # Qué hace: Ejecutor SELECT completo (Joins, LATERAL, GROUP BY/ROLLUP/CUBE, HAVING, FILTER, WITHIN GROUP, Ventanas OVER, CTE WITH RECURSIVE, UNION/INTERSECT/EXCEPT, DISTINCT ON, NULLS FIRST/LAST, FETCH).
│   └── ModuloDML.hpp                 # Qué hace: Ejecutor determinista INSERT (ON CONFLICT DO NOTHING/UPDATE), UPDATE (FROM), DELETE (USING), MERGE (WHEN MATCHED/NOT MATCHED), TRUNCATE y RETURNING.
├── orm/
│   ├── DjangoQuerySet.hpp            # Qué hace: API fluida estilo Django QuerySet (.filter, .exclude, .annotate, .window, .select_related, .union_with, .order_by, .values).
│   └── DjangoManager.hpp             # Qué hace: Manager estilo Model::objects (.all, .get, .create, .update_or_create, .merge, .truncate) y fachada db.
├── tests/
│   ├── AutomatedRuntimeCostAndSqlSuite.hpp # Qué hace: Suite automática que verifica todo el estándar PostgreSQL DML/DQL, el uso de índices y mide Stack/CPU/Tiempo.
│   └── ManualRuntimeSandboxTemplate.hpp    # Qué hace: Plantilla interactiva para que el desarrollador pruebe consultas Django, planes EXPLAIN y costos.
└── main.cpp                          # Qué hace: Punto de entrada para migrar las 12 tablas con el DDL y ejecutar la suite y el sandbox de database_runtime.