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

Uso

```
// En main.ino o en tu contenedor de base de datos:
MicroSD sd(PIN_CS_SD);
SGBD::MotorSGBD sgbd(&sd);
ORM::ClienteORM db(&sgbd);

// 1. En setup():
db.inicializar(); // Monta la SD, crea las 10 tablas si no existen y sincroniza IDs

// 2. Insertar un senso nuevo (asigna id_senso, activa=1 y fechas automáticamente):
EsquemaFisicoDB::FilaSenso lectura = {};
lectura.id_sensor             = 1;
lectura.temperatura_cruda     = -17.5f;
lectura.temperatura_corregida = -17.2f;
lectura.estado_sensor         = EsquemaFisicoDB::ESTADO_OK;
db.senso.registrar(lectura, gEstadoSistema.ultimoUnixTimestamp);

// 3. Actualizar la configuración SMTP (Singleton PK=1, preserva fecha_creacion):
EsquemaFisicoDB::FilaConfiguracionSMTP smtp = {};
strncpy(smtp.host_smtp, "smtp.gmail.com", sizeof(smtp.host_smtp) - 1);
smtp.puerto = 587;
db.configSmtp.guardar(smtp, gEstadoSistema.ultimoUnixTimestamp);

// 4. Buscar un sensor por su dirección 1-Wire hexadecimal:
EsquemaFisicoDB::FilaSensor s;
if (db.buscarSensorPorHex("28FF641E82160450", s)) {
    // s.id_sensor, s.umbral_inferior, s.umbral_superior ya están cargados
}
```