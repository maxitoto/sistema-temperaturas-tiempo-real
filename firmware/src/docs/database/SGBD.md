                  ┌──────────────────────────────────────┐
                  │         MotorSGBD (Orquestador)      │
                  │  Enruta DDL / DML / DQL / DCL        │
                  └──────┬───────┬────────┬───────┬──────┘
                         │       │        │       │
        ┌────────────────┘       │        │       └────────────────┐
        ▼                        ▼        ▼                        ▼
┌───────────────┐        ┌───────────────┐┌───────────────┐┌───────────────┐
│  ModuloDDL.h  │        │  ModuloDCL.h  ││  ModuloDML.h  ││  ModuloDQL.h  │
│ CREATE / DROP │        │ GRANT / REVOKE││INSERT / UPDATE││ SELECT / SCAN │
│   TRUNCATE    │        │ Security Mgr  ││ DELETE LÓGICO ││  (Solo lee)   │
└───────┬───────┘        └───────┬───────┘└───────┬───────┘└───────┬───────┘
        │                        │                │                │
        ▼                        ▼                ▼                ▼
┌────────────────────────────────────────┐        │                │
│           CatalogoSistema.h            │◄───────┴────────────────┘
│   (Data Dictionary: Esquemas y Roles)  │   (Solo lectura de metadatos)
└───────────────────┬────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                  MicroSD.h (Motor de Almacenamiento Físico)              │
└──────────────────────────────────────────────────────────────────────────┘