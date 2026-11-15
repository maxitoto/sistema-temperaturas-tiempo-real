src/database/
├── common/
│   ├── PostgresSqlStates.hpp        // Códigos SQLSTATE estándar
│   ├── PostgresDataTypes.hpp        // ÚNICA fuente de verdad de los 30 tipos, validación ISO y conversión
│   ├── PostgresConstraints.hpp      // Definiciones de columnas, FKs, secuencias e índices
│   ├── DiagnosticConsole.hpp        // Consola de errores PostgreSQL compartida
│   └── PhysicalSchemaCatalog.hpp    // Catálogo en RAM + guardado y carga de /db/pg_catalog.cat
├── DB_DDL/                          // <--- Puedes excluir esta carpeta entera en platformio.ini en producción
│   ├── standard/
│   │   └── PostgresReservedWords.hpp
│   ├── engine/
│   │   ├── DeterministicTableBuilder.hpp
│   │   └── ModuloDDL.hpp
│   └── models/
│       ├── DjangoFields.hpp         // Los 35 Fields de Django/PostgreSQL completos
│       ├── DjangoModel.hpp
│       └── PhysicalModelsDefinition.hpp
├── DB_DQL_DML/                      // <--- CERO dependencia de DB_DDL/
│   ├── interfaces/
│   │   └── IRuntimeDbEngine.hpp
│   ├── standard/
│   │   └── PostgresDmlDqlKeywords.hpp
│   ├── telemetry/
│   │   └── RuntimeCostProfiler.hpp
│   ├── storage/
│   │   ├── RuntimeTypedTuple.hpp
│   │   └── RuntimeIndexAccessPath.hpp
│   ├── engine/
│   │   ├── AccessPathPlanner.hpp
│   │   ├── ModuloDQL.hpp
│   │   └── ModuloDML.hpp
│   └── orm/
│       ├── DjangoQuerySet.hpp
│       └── DjangoManager.hpp
└── repositories/
    ├── RepositorioBase.hpp
    └── repoFalso.hpp