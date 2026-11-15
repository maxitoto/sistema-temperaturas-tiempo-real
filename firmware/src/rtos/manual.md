### 2. Guía Rápida de Uso de `src/rtos/` para el Desarrollador

Puedes guardar esta referencia como `src/rtos/GUIA_RTOS.md`:

* **1. Crear y controlar una Tarea periódica (`TareaSegura`):**
No necesitas escribir bucles `while(true)` ni recordar `vTaskDelay(pdMS_TO_TICKS(...))`:
```cpp
RTOS::TareaSegura miTarea;

void pasoMiTarea(void* ctx) {
    // Este código se ejecuta automáticamente en cada ciclo
}

// En setup(): nombre, función, período_ms, stack_bytes, prioridad, núcleo (0 o 1)
miTarea.iniciarEnNucleo("Sensores", &pasoMiTarea, 500, 4096, 2, 1);

// Consultar telemetría o detenerla de forma segura:
uint32_t stackLibre = miTarea.obtenerStackLibreMin();
miTarea.eliminarTarea();

```


* **2. Comunicar dos tareas sin variables globales peligrosas (`ColaSegura<T, N>`):**
Solo acepta `structs` planos (`POD`) para impedir fugas de memoria:
```cpp
struct EventoAudio { uint8_t tipoSonido; };
RTOS::ColaSegura<EventoAudio, 8> colaAudio;

// Tarea emisora (0 ms = no bloquea si está llena):
colaAudio.enviar({1}, 0);

// Tarea receptora (espera hasta 50 ms):
EventoAudio ev;
if (colaAudio.recibir(ev, 50)) { /* procesar */ }

```


* **3. Proteger un recurso compartido con detección de Deadlock (`SemaforoSeguro`):**
```cpp
RTOS::SemaforoSeguro mutexBus{"MutexBus"};
if (mutexBus.tomar(100)) { // espera máximo 100 ms
    // usar recurso
    mutexBus.soltar();
}

```


* **4. Validar tiempos antes de lanzar las tareas (`AnalizadorPlanificacionRTOS::inicializarFreeRTOS`):**
Mide en hardware cuánto tardan los procedimientos `A` y `B` de cada tarea y te informa si el tiempo asignado es menor al mínimo físico (`Y_min`), cuál es el recomendado (`H_rec`), el holgado (`Z_sobrado`) y si una tarea frena a otra (`produceStopRepentino`).

---

### 4. Manual Completo de `src/rtos/`: Qué es POD y cómo conectar tus Repositorios y RPCs

#### A. ¿Qué significa "Estructura Plana (POD)" y por qué `ColaSegura` lo exige?

En C++, **POD (*Plain Old Data* o Estructura Plana)** es un `struct` cuyos campos ocupan un bloque fijo y contiguo de bytes en sí mismos (como `int32_t`, `float`, `bool`, `char texto[24]`, `PgDate`, `PgDateTime`), **sin punteros ocultos al Heap** (como `Arduino::String` o `std::vector`).

* **¿Por qué es vital en FreeRTOS?**
Cuando envías un struct por una cola de FreeRTOS (`xQueueSend`), FreeRTOS hace un `memcpy(&tuStruct, ...)` byte por byte hacia la cola.
* **❌ Peligro con `String` (No-POD):** Un objeto `Arduino::String` adentro solo guarda un puntero `char* buffer` que apunta al Heap. Si la Tarea A mete un `struct` con `String` en la cola y sale de la función, el destructor `~String()` **libera esa memoria del Heap**. Cuando la Tarea B lee la cola 10 ms después, lee un puntero a memoria muerta (*Use-After-Free* $\rightarrow$ *Guru Meditation Error*).
* **✅ Seguridad con `char[N]` / `PgDateTime` (POD):** Todos los caracteres viajan copiados físicamente dentro del bloque de la cola. Aunque la Tarea A termine, la Tarea B recibe su propia copia íntegra. Por eso `ColaSegura` tiene un `static_assert(std::is_trivially_copyable<T>::value)` que te avisa en compilación si intentas meter un `String` por accidente.



---

#### B. Cómo usar `creado_el` y `actualizado_el` en tus Repositorios (`src/database/repositories/`)

Cuando tengas tus modelos definitivos con `DateTimeField("creado_el")` y `DateTimeField("actualizado_el")` (o `DateField`), en tus repositorios simplemente llamas a `RTOS::RelojSistema::fechaHoraActualPg()`:

```cpp
#include "../../rtos/RelojSistema.hpp"

// Ejemplo al hacer un INSERT (create) en cualquier Repositorio:
String ahora = RTOS::RelojSistema::fechaHoraActualPg(); // Devuelve "2026-11-13 14:35:20"

auto res = Sensor.create({
    {"hexadecimal",     String(dto.mac),        PgDataType::VARCHAR},
    {"offset",         String(dto.offset, 2),  PgDataType::REAL},
    {"creado_el",      ahora,                  PgDataType::TIMESTAMP},
    {"actualizado_el", ahora,                  PgDataType::TIMESTAMP}
});

// Ejemplo al hacer un UPDATE en cualquier Repositorio:
auto resUpd = Sensor
    .filter_eq("hexadecimal", String(dto.mac))
    .update({
        {"offset",         String(nuevoOffset, 2),              PgDataType::REAL},
        {"actualizado_el", RTOS::RelojSistema::fechaHoraActualPg(), PgDataType::TIMESTAMP}
    });

```

---

#### C. Regla de Oro: ¿Cuándo llamo a un Repositorio directamente y cuándo uso la Cola de RTOS?

Cuando en unos días conectes tus repositorios (`RepoSensor`, `RepoSenso`, `RepoAuditoria`, `RepoUsuarios`), solo tienes que seguir esta regla:

| Situación en tu código | ¿Cómo interactúo con la Base de Datos / RTOS? | ¿Por qué? |
| --- | --- | --- |
| **1. Dentro de un procedimiento RPC (`RPAuth`, `RPSensor`, `RPSistema`) que debe devolver datos al cliente** | **Llamas directo a tu `RepoX::metodo()**` (sin tocar nada de `rtos/`). | Porque el motor `DB_DQL_DML` ya tiene `lockEngine()` automático adentro y el cliente Electron está esperando la respuesta del `SELECT` o `UPDATE`.

 |
| **2. Quieres dejar registrada una Auditoría o un Evento desde un RPC o Middleware sin frenar la respuesta** | Llamas en 1 línea a **`RTOS::encolarAuditoria(...)`** y retornas enseguida. | La llamada tarda **3 microsegundos** (solo copia el struct POD a la cola) y `TaskAlertasYPersistencia` hace el `INSERT` en la MicroSD en segundo plano. |
| **3. Guardado automático cada 2 minutos o fallos eléctricos de sensores** | **Ya está conectado automáticamente:** `TaskSensores` encola el evento y `TaskAlertasYPersistencia` lo recibe en su `switch (reg.tipo)`. | Tú solo tienes que poner la llamada `RepoSenso::crearEnLote(...)` o `RepoEventoSensor::crear(...)` donde dejamos el comentario `// consulta DB` en `TaskAlertasYPersistencia.hpp`. |

---

#### D. Ejemplo concreto de los 2 lugares donde pondrás tus Repositorios

**Lugar 1: En un procedimiento remoto (ej. `src/connections/remote/private/RPDatabase/RPSensor.hpp`)**
Aquí no tienes que tocar tareas ni colas; llamas directamente a tu repositorio:

```cpp
static void obtenerCensosPorMac(const FiltroPorMacDTO& filtro, HistorialSensoresDTO& salida) {
    memset(&salida, 0, sizeof(HistorialSensoresDTO));
    // Llamas directamente a tu repositorio (el SGBD ya protege la concurrencia por dentro):
    // auto lista = RepoSenso::buscarPorMac(macHex, limiteSeguro);
}

```

**Lugar 2: En `src/rtos/tasks/TaskAlertasYPersistencia.hpp` (donde están los 4 `// consulta DB`)**
Cuando tengas listos `RepoSenso`, `RepoEventoSensor` y `RepoAuditoria`, solo reemplazas cada `// consulta DB` dentro de `procesarColaPersistenciaDB()`:

```cpp
case TipoRegistroDB::LOTE_SENSOS_2_MINUTOS: {
    SnapshotSensor lista[GestorSensores::MAX_SENSORES];
    uint8_t cant = gestorSensores.obtenerSensoresConectados(lista);
    if (cant > 0) {
        DbRuntime::Repositories::TransaccionBatch tx; // Abre batch (1 sola escritura física a SD)
        PgDateTime ahora = RelojSistema::fechaHoraActualPg();
        for (uint8_t i = 0; i < cant; ++i) {
            // RepoSenso::crear({ lista[i].idSensorDB, lista[i].tempCruda, lista[i].tempCorregida, ahora });
        }
        tx.confirmar(); // Cierra batch y vuelca a la MicroSD
    }
    break;
}

```

---

```Printxt
src/rtos/
├── PrimitivasRTOS.hpp               // Envoltorios seguros (TareaSegura, ColaSegura, SemaforoSeguro)
├── AnalizadorRTOS.hpp               // Predictor matemático inicializarFreeRTOS()
├── RelojSistema.hpp                 // Sincronización NTP automática al conectar WiFi STA + formato OLED
├── EventosYColas.hpp                // Colas globales sin bloqueo para Sonidos y Persistencia DB/Auditoría
├── GestorRTOS.hpp                   // Orquestador que arranca y supervisa las 4 tareas en setup()
└── tasks/
    ├── TaskSensores.hpp             // Refresco 1-Wire adaptativo (1s en stream / 3s normal) + lote DB cada 2 min
    ├── TaskComunicaciones.hpp       // Procesa RPC (UART-S y WiFi), emite streaming cada 500ms y detecta clientes
    ├── TaskOled.hpp                 // Dueña única del I2C: Logo UNTDF, iconos WiFi/Serie/User, hora NTP y grilla/rotación
    └── TaskAlertasYPersistencia.hpp // Controla Buzzer/LED (alarmas + sonidos cortos) y drena la cola hacia la DB
```