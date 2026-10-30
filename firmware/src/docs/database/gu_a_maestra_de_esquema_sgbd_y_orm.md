# Guía Maestra 1: Esquema Unificado, SGBD y ORM en ESP32

Este documento es el manual definitivo de referencia para comprender, utilizar, modificar, crear o eliminar tablas y registros dentro del Sistema Gestor de Base de Datos (SGBD) y el ORM del Firmware en el ESP32, así como la organización moderna de archivos C++ (`.hpp`, `.tpp`, `.ipp`, `.cpp`).

---

## 1. Uso Correcto de Extensiones en C++ (`.hpp`, `.cpp`, `.tpp`, `.ipp`)

Uno de los mayores dolores de cabeza al escalar el firmware proviene de tener todo el código mezclado en archivos `.h` gigantes. En C++ moderno para sistemas embebidos (ESP32), cada extensión cumple un rol estricto:

| Extensión | Rol Arquitectónico | ¿Qué debe contener? | Ejemplo en tu Proyecto |
| :--- | :--- | :--- | :--- |
| **`.hpp`** | **Declaración Pura (Interfaz / Contrato)** | `structs`, `enums`, firmas de clases y prototipos de métodos. Cero lógica pesada. | `EsquemaFisico.hpp`, `ContratosRPC.hpp`, `ClienteORM.hpp` |
| **`.cpp`** | **Implementación Compilada** | El cuerpo de las funciones y métodos no genéricos. Se compila una sola vez en su propia unidad de traducción. | `FirmwareService.cpp`, `Modem.cpp`, `MicroSD.cpp` |
| **`.tpp`** | **Implementación de Templates (`template <typename T>`)** | El cuerpo de los métodos de clases plantilla. Se incluye **al final** de su `.hpp` correspondiente (`#include "RepositorioTabla.tpp"`). | `RepositorioTabla.tpp`, `ModuloDML.tpp`, `ModuloDQL.tpp` |
| **`.ipp`** | **Implementación Inline / Registro de Rutas** | Funciones `inline` de alto rendimiento o tablas de enlace (como el registro de *Skeletons* RPC) separadas para no ensuciar el `.hpp`. | `ServerSkeleton.ipp`, `SerializadoresRPC.ipp` |

---

## 2. Esquema de Base de Datos Unificado (Firmware como Única Fuente de Verdad)

Al fusionar `db_electron.txt` y `db_firmware.txt`:
1. **En el ESP32 (MicroSD `/db/*.bin`):** Residen todas las tablas del dominio (`sensor`, `senso`, `evento_sensor`, `auditoria`, `correos`, `usuarios`, `configuracion_smtp`, `servidores_ntp`, `lts`, `configuracion_conectividad`).
2. **En Electron (`conexion_firmware.json`):** Prisma y SQLite desaparecen. El backend de Electron solo guarda un archivo JSON local con los parámetros para abrir el socket o el puerto serial antes de que exista comunicación con el ESP32.

### 2.1. Diccionario de Datos Unificado (DBML)

```dbml
// ============================================================================
// ENUMS DE DOMINIO (1 Byte en Flash: uint8_t)
// ============================================================================
enum EstadoSensor {
  ESTADO_OK               // 0
  ESTADO_DESCONEXION_WIRE // 1
  ESTADO_RUIDO            // 2
  ESTADO_SUPERO_INFERIOR  // 3
  ESTADO_SUPERO_SUPERIOR  // 4
}

enum CriticidadEvento {
  CRITICIDAD_BAJA         // 0
  CRITICIDAD_MEDIA        // 1
  CRITICIDAD_CRITICA      // 2
}

enum TipoRecepcionCorreo {
  RECIBIR_TODOS            // 0
  RECIBIR_EVENTOS          // 1
  RECIBIR_EVENTOS_CRITICOS // 2
}

enum EstadoEnvioCorreo {
  ENVIO_PENDIENTE // 0
  ENVIO_ENVIADO   // 1
  ENVIO_FALLIDO   // 2
}

enum RolUsuario {
  ROL_REGULAR // 0
  ROL_ADMIN   // 1
}

// ============================================================================
// TABLAS DEL SGBD EN MICROSD (ESP32)
// ============================================================================

// 1. Catálogo de Sensores (Fusión de sensor_fw + sensor_be + Física atmosférica)
Table sensor_fw {
  id_sensor              uint16    [pk, increment]
  hexadecimal            char(17)  [unique, note: 'Dirección ROM 1-Wire']
  nombre                 char(32)  [note: 'Absorbido de Electron']
  descripcion            char(64)  [note: 'Absorbido de Electron']
  ubicacion              char(32)  [note: 'Absorbido de Electron']
  ubicacion_descripcion  char(64)  [note: 'Absorbido de Electron']
  correcion_superior     float     [default: 0.0, note: 'Ajuste en punto de ebullición (100 °C)']
  correcion_inferior     float     [default: 0.0, note: 'Ajuste en punto de fusión (0 °C)']
  altura                 float     [default: 0.0, note: 'Metros sobre el nivel del mar (msnm)']
  presion_atmosferica    float     [default: 1013.25, note: 'Presión en hPa para corrección de 0 °C y 100 °C']
  umbral_superior        float     [default: 126.0]
  umbral_inferior        float     [default: -126.0]
  activa                 uint8     [default: 1]
  fecha_creacion         uint32
  fecha_modificacion     uint32
}

// 2. Serie de Tiempo Circular: Mediciones (1024 filas máx.)
Table senso_fw {
  id_senso              uint32       [pk, increment]
  id_sensor             uint16       [ref: > sensor_fw.id_sensor]
  temperatura_cruda     float
  temperatura_corregida float
  estado_sensor         EstadoSensor
  activa                uint8        [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

// 3. Serie de Tiempo Circular: Eventos Confirmados (256 filas máx.)
Table evento_sensor_fw {
  id_evento_sensor     uint32           [pk, increment]
  id_sensor            uint16           [ref: > sensor_fw.id_sensor]
  id_senso             uint32           [ref: > senso_fw.id_senso]
  estado_previo_sensor EstadoSensor
  evento               EstadoSensor
  criticidad           CriticidadEvento
  activa               uint8            [default: 1]
  fecha_creacion       uint32
  fecha_modificacion   uint32
}

// 4. Serie de Tiempo Circular: Auditoría Unificada (256 filas máx.)
Table auditoria_fw {
  id_auditoria       uint32   [pk, increment]
  id_usuario         uint16   [ref: > usuarios_fw.id_usuario]
  id_sensor          uint16   [default: 0, note: '0 si es acción global; >0 si afecta a un sensor']
  accion             char(24)
  detalle            char(40)
  activa             uint8    [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

// 5. Catálogo de Destinatarios de Correo
Table correos_fw {
  id_correo               uint16              [pk, increment]
  nombre                  char(24)
  apellido                char(24)
  correo_electronico      char(48)
  tipo_recepcion_mensajes TipoRecepcionCorreo [default: 'RECIBIR_TODOS']
  estado_envio            EstadoEnvioCorreo   [default: 'ENVIO_PENDIENTE']
  activa                  uint8               [default: 1]
  fecha_creacion          uint32
  fecha_modificacion      uint32
}

// 6. Catálogo de Usuarios del Sistema
Table usuarios_fw {
  id_usuario         uint16     [pk, increment]
  usuario            char(20)   [unique]
  contrasena         blob(32)
  rol                RolUsuario [default: 'ROL_REGULAR']
  activa             uint8      [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

// 7. Singleton (PK=1): Configuración SMTP
Table configuracion_smtp_fw {
  id_configuracion_smtp uint8   [pk]
  host_smtp             char(40)
  puerto                uint16
  usuario               char(40)
  password              char(48)
  email_remitente       char(48)
  activa                uint8   [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

// 8. Catálogo de Servidores NTP
Table servidores_ntp_fw {
  id_server          uint8    [pk, increment]
  server             char(40)
  activa             uint8    [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

// 9. Singleton (PK=1): Certificados y Clave Simétrica de Canal
Table lts_fw {
  id_lts                uint8    [pk]
  nombre_certificado    char(24)
  certificado           char(64)
  clave_privada         blob(32)
  cadena_certificados   char(64)
  fecha_inicio_vigencia uint32
  fecha_fin_vigencia    uint32
  activa                uint8    [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

// 10. Singleton (PK=1): Configuración de Conectividad del Firmware
Table configuracion_conectividad_fw {
  id_configuracion_conectividad uint8   [pk]
  SSID_STA                      char(33)
  STA_password                  char(65)
  SSID_AP                       char(33)
  AP_password                   char(65)
  puerto_tcp                    uint16  [default: 80]
  puerto_udp                    uint16  [default: 81]
  baudios_usb                   uint32  [default: 115200]
  activa                        uint8   [default: 1]
  fecha_creacion                uint32
  fecha_modificacion            uint32
}
```

---

## 3. La Regla de Oro de Alineación en Memoria (Cómo no sufrir nunca más con `_pad`)

El procesador Xtensa LX6 de 32 bits del ESP32 exige que todo tipo de $4\text{ bytes}$ (`uint32_t`, `int32_t`, `float`) comience en una dirección de memoria múltiplo de $4$, y todo tipo de $2\text{ bytes}$ (`uint16_t`) en un múltiplo de $2$.

### ¿Por qué antes era difícil modificar un `struct`?
Porque se mezclaban cadenas `char[17]` al principio, seguidas de `uint16_t`, seguidas de `float`, obligándote a insertar rellenos manuales (`_pad0`, `_pad1`) entre los campos.

### La Solución Definitiva: Ordenamiento Descendente por Tamaño ($4\text{B} \rightarrow 2\text{B} \rightarrow 1\text{B}$)
Si dentro de cualquier `struct` de `EsquemaFisico.hpp` declaras los campos en este orden estricto:
1. **Grupo de $4\text{ bytes}$ (`float`, `uint32_t`):** Todos quedan automáticamente alineados en múltiplos de $4$ ($0, 4, 8, 12, \dots$). **Cero padding intermedio.**
2. **Grupo de $2\text{ bytes}$ (`uint16_t`):** Empieza en un múltiplo de $4$ (que también es múltiplo de $2$). **Cero padding intermedio.**
3. **Grupo de $1\text{ byte}$ (`uint8_t` y arreglos `char[N]`):** Pueden ir en cualquier byte sin restricción de alineación.
4. **Cierre del `struct`:** Solo si la suma total de bytes no es múltiplo de $4$, agregas un único `uint8_t _padFinal[R]` al final para cerrar al siguiente múltiplo de $4$.

Observa cómo `FilaSensor` pasa de tener rellenos confusos a tener **cero bytes de padding** con $244\text{ bytes}$ exactos ($244 / 4 = 61$):

```cpp
struct FilaSensor {
    // 1. GRUPO DE 4 BYTES (8 campos x 4B = 32 bytes -> Offsets 0..31)
    float    correcion_superior;      // Offset 0  (4B)
    float    correcion_inferior;      // Offset 4  (4B)
    float    altura;                  // Offset 8  (4B)
    float    presion_atmosferica;     // Offset 12 (4B)
    float    umbral_superior;         // Offset 16 (4B)
    float    umbral_inferior;         // Offset 20 (4B)
    uint32_t fecha_creacion;          // Offset 24 (4B)
    uint32_t fecha_modificacion;      // Offset 28 (4B)

    // 2. GRUPO DE 2 BYTES (1 campo x 2B = 2 bytes -> Offset 32..33)
    uint16_t id_sensor;               // Offset 32 (2B)

    // 3. GRUPO DE 1 BYTE Y CHAR[] (1 + 17 + 32 + 64 + 32 + 64 = 210 bytes -> Offsets 34..243)
    uint8_t  activa;                  // Offset 34 (1B)
    char     hexadecimal[17];         // Offset 35 (17B) -> 32 + 2 + 1 + 17 = 52 (Múltiplo de 4)
    char     nombre[32];              // Offset 52 (32B)
    char     descripcion[64];         // Offset 84 (64B)
    char     ubicacion[32];           // Offset 148 (32B)
    char     ubicacion_descripcion[64]; // Offset 180 (64B) -> Total = 244 bytes
};
static_assert(sizeof(FilaSensor) == 244, "FilaSensor mide exactamente 244 bytes sin padding");
```

---

## 4. Cómo Usar el ORM (Operaciones CRUD Paso a Paso)

El `ClienteORM` expone tres familias de plantillas (`ModeloCatalogo`, `ModeloCircular`, `ModeloSingleton`). Todas reciben opcionalmente un `ContextoSesionSGBD`:
*   Si operas desde una tarea interna del ESP32 (ej. leer un sensor DS18B20 en el `loop`), **no pases sesión**: por defecto usa `ContextoSesionSGBD::sistemaInterno()` (permiso total).
*   Si operas desde un endpoint RPC invocado por un usuario, pasa la `sesion` construida con las credenciales del usuario para que el módulo **DCL** valide su rol (`ROL_REGULAR` vs `ROL_ADMIN`).

### 4.1. Uso de `ModeloCatalogo` (`sensor`, `correos`, `usuarios`, `servidoresNtp`)

```cpp
// A. CREAR (INSERT): Asigna id autoincremental, activa=1 y fechas automáticamente
EsquemaFisicoDB::FilaSensor nuevo = {};
strncpy(nuevo.hexadecimal, "28FF641E82160450", sizeof(nuevo.hexadecimal) - 1);
strncpy(nuevo.nombre, "Heladera Vacunas 1", sizeof(nuevo.nombre) - 1);
nuevo.altura              = 15.0f;
nuevo.presion_atmosferica = 1008.5f;
nuevo.umbral_inferior     = 2.0f;
nuevo.umbral_superior     = 8.0f;

bool ok = dbOrm.sensor.crear(nuevo, gEstadoSistema.ultimoUnixTimestamp);
// Ahora `nuevo.id_sensor` contiene el ID asignado (ej. 1).

// B. LEER POR ID EN O(1) (SELECT * WHERE id = X)
EsquemaFisicoDB::FilaSensor leido = {};
if (dbOrm.sensor.buscarPorId(1, leido)) {
    // Encontrado y activo
}

// C. BUSCAR POR CONDICIÓN (SELECT * WHERE predicado LIMIT 1)
EsquemaFisicoDB::FilaSensor porUbicacion = {};
bool hallado = dbOrm.sensor.buscarPrimero(porUbicacion, [](const EsquemaFisicoDB::FilaSensor& f) {
    return strcmp(f.ubicacion, "Laboratorio Central") == 0;
});

// D. LISTAR TODOS LOS ACTIVOS (SELECT * WHERE activa = 1)
dbOrm.sensor.recorrerActivos([](const EsquemaFisicoDB::FilaSensor& f) -> bool {
    Serial.printf("Sensor #%u: %s\n", f.id_sensor, f.nombre);
    return true; // Devolver `true` para seguir iterando, `false` para detener (LIMIT)
});

// E. ACTUALIZAR EN O(1) (UPDATE WHERE id = X): Conserva fecha_creacion original
leido.umbral_superior = 7.5f;
dbOrm.sensor.actualizarPorId(leido.id_sensor, leido, gEstadoSistema.ultimoUnixTimestamp);

// F. ELIMINAR / BAJA LÓGICA (UPDATE SET activa = 0 WHERE id = X)
dbOrm.sensor.desactivarPorId(1, gEstadoSistema.ultimoUnixTimestamp);
```

### 4.2. Uso de `ModeloCircular` (`senso`, `eventoSensor`, `auditoria`)
Son anillos FIFO de tamaño fijo en la MicroSD. Nunca llenan la tarjeta: cuando superan su capacidad máxima (`MAX_FILAS_SENSO = 1024`), sobrescriben el registro más antiguo en $O(1)$.

```cpp
// A. REGISTRAR NUEVA MEDICIÓN
EsquemaFisicoDB::FilaSenso medicion = {};
medicion.id_sensor             = 1;
medicion.temperatura_cruda     = 4.25f;
medicion.temperatura_corregida = 4.10f;
medicion.estado_sensor         = EsquemaFisicoDB::ESTADO_OK;
dbOrm.senso.registrar(medicion, gEstadoSistema.ultimoUnixTimestamp);

// B. CONSULTAR LAS ÚLTIMAS N MEDICIONES (vía Repositorio en orden DESC)
EsquemaFisicoDB::FilaSenso ultimas[10];
size_t cantidad = repositorios().sensos.listarUltimos(
    ultimas, 10, /*filtroIdSensor=*/0, SGBD::ContextoSesionSGBD::sistemaInterno()
);
```

### 4.3. Uso de `ModeloSingleton` (`configSmtp`, `lts`, `conectividad`)
Tablas de una única fila (`PK = 1`). Al guardar, escriben primero en un archivo `.tmp` y luego renombran atómicamente (Write-Ahead seguro ante cortes de luz).

```cpp
// A. LEER SINGLETON
EsquemaFisicoDB::FilaConfiguracionConectividad red = {};
if (dbOrm.conectividad.obtener(red)) {
    // Configuración cargada en `red`
}

// B. GUARDAR / REEMPLAZAR SINGLETON
strncpy(red.SSID_STA, "Red_Hospital", sizeof(red.SSID_STA) - 1);
red.puerto_tcp  = 80;
red.puerto_udp  = 81;
red.baudios_usb = 115200;
dbOrm.conectividad.guardar(red, gEstadoSistema.ultimoUnixTimestamp);
```

---

## 5. Recetario Sin Ambigüedad: Modificar, Crear o Eliminar Tablas

### Receta A: Cómo MODIFICAR una tabla existente (Agregar, quitar o renombrar un campo)

Cuando modificas los campos de un `struct` en `EsquemaFisico.hpp`, su tamaño en bytes (`sizeof`) cambia. Si la tarjeta MicroSD ya tenía un archivo `.bin` grabado con el tamaño viejo, el tamaño del archivo ya no será múltiplo del nuevo `sizeof(Fila)`.

Sigue exactamente estos **4 pasos**:

1. **Paso 1 (`src/database/EsquemaFisico.hpp`):**
   Agrega o modifica el campo respetando el orden ($4\text{B} \rightarrow 2\text{B} \rightarrow 1\text{B}$) y actualiza el número en `static_assert(sizeof(FilaX) == NUEVO_TAMANO)`.
2. **Paso 2 (`src/database/sgbd/ModuloDDL.h` - Auto-Migración de Cáscara):**
   En `ModuloDDL::crearTabla()`, asegúrate de tener esta validación de integridad de esquema para que, si detecta un archivo `.bin` con el tamaño de estructura viejo, lo recree automáticamente en vez de fallar en silencio:
   ```cpp
   if (sd->existe(meta->rutaArchivo)) {
       uint64_t bytes = sd->tamanoArchivo(meta->rutaArchivo);
       // Si el archivo existente no es múltiplo exacto del nuevo sizeof(Fila),
       // significa que el struct cambió en compilación: truncamos para evitar lectura corrupta.
       if (meta->tamanoFila > 0 && (bytes % meta->tamanoFila) != 0) {
           sd->eliminarArchivo(meta->rutaArchivo);
       } else {
           return true;
       }
   }
   ```
3. **Paso 3 (`src/database/repositories/FirmwareRepositories.hpp`):**
   **Regla crítica:** Nunca pases columnas sueltas como parámetros en los métodos del repositorio (`guardarSensor(hex, a, b, c, d)`). Pasa siempre la estructura o DTO completo (`guardarSensor(const FilaSensor& datos, ...)`). Así, cuando agregues un campo a la tabla, ¡no tendrás que cambiar la firma de ningún método del repositorio!
4. **Paso 4 (Si se expone por RPC):**
   Actualiza el serializador de esa fila en `ContratosRPC.hpp` y `contratos.ts` (ver *Guía Maestra 2*).

---

### Receta B: Cómo CREAR una tabla nueva desde cero

Supongamos que deseas crear una tabla independiente `calibracion_fw` (`/db/calib.bin`) como Singleton:

1. **Paso 1 (`src/database/EsquemaFisico.hpp`):** Define el `struct` físico alineado a $4\text{ bytes}$:
   ```cpp
   struct FilaCalibracion {
       float    altura_msnm;         // Offset 0  (4B)
       float    presion_hpa;         // Offset 4  (4B)
       uint32_t fecha_creacion;      // Offset 8  (4B)
       uint32_t fecha_modificacion;  // Offset 12 (4B)
       uint8_t  id_calibracion;      // Offset 16 (1B)
       uint8_t  activa;              // Offset 17 (1B)
       uint8_t  _padFinal[2];        // Offset 18 (2B) -> Total = 20 bytes
   };
   static_assert(sizeof(FilaCalibracion) == 20, "FilaCalibracion debe medir 20B");
   ```
2. **Paso 2 (`src/database/sgbd/CatalogoSistema.h`):**
   * Agrega `TABLA_CALIBRACION` al `enum IdTabla` justo antes de `TOTAL_TABLAS_SISTEMA`.
   * En el constructor `CatalogoSistema()`, registra su metadato:
     ```cpp
     tablas[TABLA_CALIBRACION] = {
         TABLA_CALIBRACION, "calibracion", "/db/calib.bin", "/db/calib.tmp",
         sizeof(FilaCalibracion), /*esSingleton=*/true, ROL_ADMIN
     };
     ```
3. **Paso 3 (`src/database/orm/ClienteORM.h`):**
   * Declara la propiedad en `ClienteORM`:
     ```cpp
     ModeloSingleton<FilaCalibracion, &FilaCalibracion::id_calibracion> calibracion;
     ```
   * Inicialízala en la lista del constructor de `ClienteORM`:
     ```cpp
     , calibracion(motorSgbd, TABLA_CALIBRACION)
     ```
   *(Si fuera `ModeloCircular`, agrega también `calibracion.sincronizarSecuenciaInicial();` dentro de `ClienteORM::inicializar()`).*

¡Listo! Al arrancar el ESP32, `ModuloDDL` creará `/db/calib.bin` automáticamente y ya podrás usar `dbOrm.calibracion.obtener(...)` y `dbOrm.calibracion.guardar(...)`.

---

### Receta C: Cómo ELIMINAR una tabla existente

1. Elimina la propiedad de `ClienteORM.h` y su repositorio en `FirmwareRepositories.h`.
2. Quita su entrada de `CatalogoSistema.h` y del `enum IdTabla`.
3. Elimina el `struct` de `EsquemaFisico.hpp`.
4. Si quieres borrar el archivo físico remanente en la tarjeta MicroSD durante el booteo, ejecuta una única vez `tarjetaSd.eliminarArchivo("/db/tabla_vieja.bin");`.