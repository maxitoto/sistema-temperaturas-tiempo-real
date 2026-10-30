                 ┌──────────────────────────────────────────┐
                 │           Aplicación / Firmware           │
                 │       Usa entidades del dominio           │
                 └────────────────────┬─────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │         FirmwareRepositories.h            │
                 │   Repositorios específicos del dominio    │
                 │   Sensor / Evento / Usuario / Auditoría   │
                 └────────────────────┬─────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │              ClienteORM.h                │
                 │              Fachada del ORM             │
                 │  Expone modelos y operaciones tipadas    │
                 └────────────────────┬─────────────────────┘
                                      │
                   ┌──────────────────┼──────────────────┐
                   ▼                  ▼                  ▼
        ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
        │  ModeloCatalogo  │ │  ModeloCircular  │ │  ModeloSingleton │
        │                  │ │                  │ │                  │
        │ Registros        │ │ Registros        │ │ Configuración    │
        │ persistentes     │ │ de tamaño fijo   │ │ única por tabla  │
        │                  │ │                  │ │                  │
        └────────┬─────────┘ └────────┬─────────┘ └────────┬─────────┘
                 │                    │                    │
                 └────────────────────┼────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │             RepositorioTabla.h           │
                 │     Abstracción común de las tablas      │
                 │   Acceso a registros y metadatos         │
                 └────────────────────┬─────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │               MotorSGBD.h                │
                 │ Ejecuta operaciones sobre la base de     │
                 │ datos mediante los módulos del SGBD      │
                 └────────────────────┬─────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │           EsquemaFisico.h                │
                 │ Tipos, enumeraciones y estructuras       │
                 │ binarias que representan los registros  │
                 └────────────────────┬─────────────────────┘
                                      │
                                      ▼
                 ┌──────────────────────────────────────────┐
                 │              MicroSD / .bin              │
                 │       Persistencia física de datos       │
                 └──────────────────────────────────────────┘