En ESP32 con **FreeRTOS**, hay varias herramientas para coordinar tareas, compartir información y controlar cuándo se ejecuta cada tarea. Vos ya conocés los **semáforos, los eventos y el tiempo**, que son una buena base.

Vamos a organizar los mecanismos principales para que tengas un mapa completo de lo que podés utilizar al programar un ESP32.

# 1. Formas de sincronización y comunicación entre tareas

Estas son las herramientas más importantes de FreeRTOS.

| N.º | Mecanismo | ¿Para qué sirve? | Ejemplo en ESP32 |
|---:|---|---|---|
| 1 | **Semáforo binario** | Sincronizar tareas o señalar que ocurrió algo. | Una interrupción indica que llegó un dato. |
| 2 | **Semáforo contador** | Contabilizar eventos o recursos disponibles. | Contar cuántos elementos están pendientes de procesar. |
| 3 | **Mutex** | Proteger un recurso compartido para evitar accesos simultáneos incompatibles. | Evitar que dos tareas usen simultáneamente un periférico compartido. |
| 4 | **Colas (Queues)** | Enviar datos de una tarea a otra. | Una tarea envía las mediciones de temperatura a otra. |
| 5 | **Grupos de eventos (Event Groups)** | Esperar uno o varios eventos o condiciones. | Iniciar una tarea cuando se hayan conectado el Wi-Fi y el sensor. |
| 6 | **Notificaciones de tarea (Task Notifications)** | Notificar eventos directamente a una tarea, con poca sobrecarga. | Despertar una tarea cuando termina una adquisición. |
| 7 | **Colas de longitud 1 sobrescribibles** | Mantener el dato más reciente disponible. | Compartir la última lectura de un sensor. |
| 8 | **Colas de conjuntos (Queue Sets)** | Esperar actividad en varias colas o semáforos compatibles mediante un único mecanismo de espera. | Atender mensajes de distintas fuentes. |

**Una distinción importante:** los eventos no son exactamente lo mismo que los semáforos. En FreeRTOS, los grupos de eventos permiten representar varias condiciones mediante bits, mientras que los semáforos representan disponibilidad de recursos o señales de sincronización.

Además, los *Task Notifications* permiten que una tarea reciba una notificación directamente, sin necesitar necesariamente una cola o un semáforo.

# 2. Control del tiempo y ejecución periódica

Como mencionaste el tiempo, hay varias formas de controlar cuándo se ejecuta una tarea.

| N.º | Mecanismo | ¿Para qué sirve? |
|---:|---|---|
| 9 | `vTaskDelay()` | Suspender una tarea durante una cantidad de ticks. |
| 10 | `vTaskDelayUntil()` | Ejecutar una tarea periódicamente manteniendo una referencia temporal. |
| 11 | Temporizadores de software (*Software Timers*) | Ejecutar una función de callback cuando vence un tiempo o periódicamente. |
| 12 | `esp_timer` | Utilizar temporizadores de alta resolución de ESP-IDF. |
| 13 | `xTaskAbortDelay()` | Intentar sacar una tarea de una espera bloqueante por tiempo. |
| 14 | Espera con timeout | Esperar un evento, dato o recurso durante un tiempo máximo. |
| 15 | Temporizadores de hardware | Utilizar periféricos de temporización para generar eventos temporales precisos. |

**Importante:** los temporizadores de software de FreeRTOS y `esp_timer` no crean automáticamente una tarea nueva para cada vencimiento. Sus callbacks se ejecutan en contextos específicos de temporización.

Por ejemplo, imaginá que tenés tres tareas:

- **Tarea A:** lee un sensor cada segundo.
- **Tarea B:** actualiza una pantalla cada 200 ms.
- **Tarea C:** espera hasta que llegue una orden.

Podrías usar `vTaskDelayUntil()` en A y B, y una cola o notificación en C.

---

# 3. Herramientas para administrar las tareas

Estos mecanismos permiten controlar el ciclo de vida y la ejecución de las tareas.

| N.º | Mecanismo | Función |
|---:|---|---|
| 16 | `xTaskCreate()` | Crear una tarea. |
| 17 | `xTaskCreatePinnedToCore()` | Crear una tarea vinculada a un núcleo, cuando la configuración y el chip lo permiten. |
| 18 | `vTaskDelete()` | Eliminar una tarea. |
| 19 | `vTaskSuspend()` | Suspender una tarea. |
| 20 | `vTaskResume()` | Reanudar una tarea suspendida. |
| 21 | `vTaskPrioritySet()` | Modificar la prioridad de una tarea. |
| 22 | `uxTaskPriorityGet()` | Consultar la prioridad de una tarea. |
| 23 | `taskYIELD()` | Ceder voluntariamente el procesador al planificador. |
| 24 | `xTaskGetTickCount()` | Consultar el contador de ticks de FreeRTOS. |
| 25 | `eTaskGetState()` | Consultar el estado de una tarea. |
| 26 | `xTaskCreateStatic()` | Crear una tarea usando memoria asignada estáticamente. |

Estos mecanismos no son todos formas de comunicación. Algunos administran directamente las tareas.

## ¿Qué estados puede tener una tarea?

Una tarea puede encontrarse en diferentes estados:

- **Running:** se está ejecutando.
- **Ready:** está lista para ejecutarse, pero espera que el planificador le asigne el procesador.
- **Blocked:** está esperando un evento, un recurso o un tiempo.
- **Suspended:** está suspendida explícitamente.

Por ejemplo, cuando una tarea ejecuta `xQueueReceive()` y no hay mensajes disponibles, puede quedar bloqueada hasta que llegue uno o venza el timeout.

Esto es muy importante: **una tarea bloqueada no necesita consumir CPU continuamente para esperar**.

---

# 4. Comunicación entre tareas mediante datos compartidos

Además de las colas, podés comunicar tareas utilizando memoria compartida, aunque tenés que controlar cuidadosamente el acceso concurrente.

| N.º | Mecanismo | ¿Para qué sirve? |
|---:|---|---|
| 27 | Variables globales compartidas | Compartir datos entre tareas. |
| 28 | Variables `volatile` | Evitar determinadas optimizaciones del compilador sobre accesos a variables. |
| 29 | Secciones críticas | Proteger operaciones que no deben intercalarse con determinadas ejecuciones concurrentes. |
| 30 | `portMUX_TYPE` | Proporcionar protección mediante spinlock en las implementaciones de ESP-IDF que lo utilizan. |
| 31 | Operaciones atómicas | Leer o modificar valores compartidos mediante operaciones atómicas compatibles. |
| 32 | Búfer circular (*Ring Buffer*) | Compartir un flujo de datos entre productor y consumidor. |
| 33 | Búfer FIFO | Procesar datos en orden de llegada. |
| 34 | Memoria compartida protegida por mutex | Permitir que varias tareas accedan a una estructura de datos de forma controlada. |

### ¡Cuidado con `volatile`!

Supongamos que una tarea actualiza una variable:

```cpp
volatile int temperatura = 0;
```

Y otra tarea la lee.

`volatile` no garantiza que ambas tareas puedan acceder a la variable de forma segura ante cualquier concurrencia. Tampoco convierte una operación compuesta en atómica.

Para compartir datos, conviene evaluar si necesitás una operación atómica, un mutex, una sección crítica o una cola.

**Regla práctica:** si necesitás enviar una medición de una tarea a otra, normalmente una cola es una opción más clara que compartir una variable global.

---

# 5. Interrupciones y tareas

En ESP32, las interrupciones también pueden interactuar con FreeRTOS.

| N.º | Mecanismo | ¿Para qué sirve? |
|---:|---|---|
| 35 | ISR (*Interrupt Service Routine*) | Responder a un evento de hardware. |
| 36 | `xSemaphoreGiveFromISR()` | Liberar un semáforo desde una ISR, cuando está permitido. |
| 37 | `xQueueSendFromISR()` | Enviar un elemento a una cola desde una ISR. |
| 38 | `vTaskNotifyGiveFromISR()` | Notificar directamente a una tarea desde una ISR. |
| 39 | `xTaskNotifyFromISR()` | Enviar una notificación desde una ISR. |
| 40 | `portYIELD_FROM_ISR()` | Solicitar el cambio de contexto apropiado al terminar la ISR, según el port de FreeRTOS. |

Un ejemplo típico es un botón conectado a un GPIO.

Cuando se pulsa el botón:

1. Se produce una interrupción.
2. La ISR notifica a una tarea.
3. La tarea despierta y procesa el evento.
4. La tarea realiza el trabajo que no conviene ejecutar dentro de la interrupción.

Esto permite que la ISR sea breve y que el procesamiento más complejo se realice en una tarea.

**Importante:** no todas las funciones de FreeRTOS pueden utilizarse dentro de una ISR. Hay que usar las variantes específicas y respetar las restricciones del ESP32 y de ESP-IDF.

---

# 6. Otras herramientas útiles para sistemas con varias tareas

Hay mecanismos adicionales que ayudan a construir sistemas concurrentes.

| N.º | Mecanismo | ¿Para qué sirve? |
|---:|---|---|
| 41 | Prioridades de tareas | Favorecer la ejecución de tareas más prioritarias. |
| 42 | Planificador (*Scheduler*) | Determinar qué tarea se ejecuta. |
| 43 | Afinidad de núcleo | Controlar en qué núcleo puede ejecutarse una tarea, si la plataforma lo admite. |
| 44 | Herencia de prioridad | Reducir ciertos problemas de inversión de prioridad al usar mutexes compatibles. |
| 45 | `xEventGroupWaitBits()` | Bloquear una tarea hasta que se cumplan determinadas condiciones de eventos. |
| 46 | `xSemaphoreTake()` | Esperar a que un semáforo o mutex esté disponible. |
| 47 | `xQueueReceive()` | Esperar y recibir datos de una cola. |
| 48 | `ulTaskNotifyTake()` | Esperar una notificación de tarea. |
| 49 | `xTaskNotifyWait()` | Esperar una notificación y gestionar los valores notificados. |
| 50 | `xQueuePeek()` | Consultar el siguiente elemento de una cola sin retirarlo. |

Observá que algunos elementos de esta tabla son funciones de los mecanismos anteriores, no mecanismos independientes.

---

# 7. ¿Cómo elegir el mecanismo correcto?

Esta es la parte más útil para diseñar tus programas.

| Situación | Mecanismo recomendado |
|---|---|
| Una tarea debe ejecutarse cada segundo. | `vTaskDelayUntil()` |
| Una tarea debe despertarse cuando ocurre algo. | Notificación o semáforo binario. |
| Una tarea debe recibir datos de otra. | Cola. |
| Una tarea debe esperar varias condiciones. | Grupo de eventos. |
| Dos tareas deben utilizar un recurso compartido. | Mutex, si corresponde. |
| Necesitás contar eventos pendientes. | Semáforo contador o notificación con contador. |
| Querés conservar solamente la última medición. | Cola sobrescribible de longitud 1. |
| Un botón debe despertar una tarea. | Interrupción GPIO y notificación desde ISR. |
| Una tarea debe esperar un evento durante un máximo de 500 ms. | Espera con timeout. |
| Necesitás medir intervalos de tiempo cortos. | `esp_timer` o un periférico de hardware, según la precisión necesaria. |
| Una tarea debe esperar a que Wi-Fi y un sensor estén listos. | Grupo de eventos, si esas condiciones se representan mediante bits. |
| Necesitás proteger una estructura de datos compartida. | Mutex o mecanismo de sincronización apropiado. |

---

# 8. Ejemplo: un sistema de temperatura con ESP32

Imaginemos un ESP32 que lee un sensor, actualiza una pantalla y genera alarmas.

Podríamos organizarlo así:

```text
                  ESP32 + FreeRTOS
                         |
          +--------------+--------------+
          |              |              |
      Tarea Sensor   Tarea Pantalla   Tarea Alarma
          |              ^              ^
          |              |              |
          +---- Cola ----+              |
                         |              |
                         +-- Notificación
```

Una arquitectura más concreta sería:

**Tarea 1: Sensor**
- Lee la temperatura cada segundo.
- Envía la medición mediante una cola.

**Tarea 2: Pantalla**
- Recibe las mediciones.
- Actualiza la pantalla OLED.

**Tarea 3: Alarma**
- Recibe información sobre la temperatura.
- Comprueba si se superó un umbral.
- Activa una alarma cuando corresponde.

**Tarea 4: Comunicaciones**
- Espera órdenes recibidas por Wi-Fi.
- Procesa las solicitudes cuando llegan.

Para implementar esto podrías combinar:

- `xTaskCreate()` para crear las tareas.
- `vTaskDelayUntil()` para las lecturas periódicas.
- `xQueueSend()` y `xQueueReceive()` para transportar las mediciones.
- Notificaciones para despertar tareas cuando sea necesario.
- Mutex para proteger un recurso compartido, si realmente existe esa necesidad.
- Grupos de eventos para coordinar condiciones de inicialización.

No necesitás utilizar todos los mecanismos en todos los proyectos. Lo importante es elegir los adecuados.

---

# 9. Mi recomendación: aprendé estos 8 primero

Si querés dominar la programación multitarea en ESP32, te recomiendo este orden:

1. **Tareas y prioridades:** `xTaskCreate()`, `vTaskDelay()` y `vTaskDelayUntil()`.
2. **Semáforos binarios:** sincronización entre eventos y tareas.
3. **Semáforos contadores:** contabilización de eventos.
4. **Mutex:** protección de recursos compartidos.
5. **Colas:** comunicación y transferencia de datos entre tareas.
6. **Grupos de eventos:** coordinación mediante varias condiciones.
7. **Notificaciones de tarea:** comunicación directa y eficiente con una tarea.
8. **Interrupciones + FreeRTOS:** conectar eventos de hardware con el procesamiento de las tareas.

Cuando domines esos ocho temas, vas a tener una base sólida para diseñar sistemas multitarea.

### Una última distinción importante

Cuando decís «formas de tareas», podemos referirnos a dos cosas diferentes:

- **Mecanismos de sincronización y comunicación:** semáforos, eventos, colas, mutexes y notificaciones.
- **Mecanismos de ejecución y planificación:** tareas periódicas, prioridades, temporizadores, suspensión, reanudación y afinidad de núcleo.

Ambas categorías son importantes, pero cumplen funciones diferentes.

**Para seguir aprendiendo, te propongo una pregunta:** cuando programás tu ESP32, ¿querés aprender principalmente cómo hacer que las tareas se comuniquen entre sí, o cómo controlar cuándo se ejecuta cada tarea y con qué prioridad? Podemos profundizar en cualquiera de los dos caminos.