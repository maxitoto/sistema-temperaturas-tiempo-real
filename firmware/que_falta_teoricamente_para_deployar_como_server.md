# Auditoría: qué le falta al firmware para ser "deployable"

> TL;DR: la **pila OSI está bien** (incluso es elegante). Lo que falta es lo que va **arriba de la pila** y lo que la **rodea**: el firmware no hace nada útil por sí solo, no es seguro, no sobrevive a fallos reales, no se puede diagnosticar en campo y no se puede actualizar. Es un *framework de comunicaciones* con un esqueleto de DB, no un servidor.

---

## 1. No hay lógica de negocio: el firmware no hace nada

Esto es lo más grave. Un servidor "auténtico" tiene que **producir y consumir datos**. Hoy el firmware:

- **Nunca lee el sensor de temperatura.** `PIN_SENSOR_TEMP` está definido en `const/Pines.h` y no se usa en ningún lado. No hay driver 1-Wire/DS18B20, no hay `analogRead`, no hay nada.
- **Nunca escribe en `senso` ni en `eventoSensor` por su cuenta.** Los únicos INSERT vienen de RPCs manuales disparados por el backend (`RPC_DB_GUARDAR_SENSOR`, etc.). La tabla circular `senso` está pensada para telemetría periódica y **nadie la escribe periódicamente**.
- **No hay máquina de estados de umbral.** No existe el código que dice *"si `temp_corregida > umbral_superior` durante N muestras, generar `FilaEventoSensor` con criticidad X"*. Eso es exactamente el corazón del producto.
- **`PIN_BOCINA` y `PIN_LED` no se usan.** No hay alarma, no hay actuador, no hay control.
- **`FilaCorreo` y `FilaConfiguracionSMTP` son decorativas.** No hay código que envíe un email. La tabla se llena vía RPC y muere ahí.
- **`FilaServidorNTP` no se usa.** No hay cliente SNTP. `gEstadoSistema.relojSincronizado` solo se pone en `true` si el backend responde `SYNC_RELOJ`. Si el backend está caído al bootear, **todos los `fecha_creacion` de la DB quedan en 0** (epoch).

Lo que falta en `main.ino`:

```cpp
// tareaMuestreoSensor()   -> cada 2 min lee DS18B20, corrige, inserta en senso
// tareaEvaluacionEventos() -> cada 30 s compara contra umbrales, genera evento, audita
// tareaActuadores()        -> bocina/LED según estado
// tareaEnvioCorreos()      -> si hay evento crítico, manda mail vía SMTP
// tareaSincronizacionNTP() -> configTime() al boot + re-sync periódico
```

---

## 2. Seguridad: es un demo, no un servidor

### 2.1 El "TLS" no existe

`ContenedorComunicacion` no llama a `presentacion.configurarTransformadores(...)`. La `CapaPresentacion` del firmware tiene el hook, pero **nadie lo conecta**. El backend sí lo configura (`serverExample.ts` → `transformarMensajeCanalTLS`). Resultado: **el canal va en MsgPack plano por el cable/aire**. El "TLS" que ves en los comentarios es solo del lado backend.

Y ese "TLS" del backend es un XOR con clave hardcodeada:

```ts
vista[i] ^= (CLAVE_CANAL_TLS[i & 0x0f] ^ ((i * 29) & 0xff)) & 0xff;
```

Eso **no es cifrado**, es ofuscación. Cualquiera con el binario lo revierte. La `FilaLTS` con `certificado[64]` y `clave_privada[32]` es un placeholder: **32 bytes no alcanzan para una clave privada RSA/ECC real**.

### 2.2 La verificación de certificado es un `strcmp`

En `serverExample.ts` (backend):

```ts
if (certEsp !== "CERT-TLS-UNTDF-2026-VALIDO") return { permitido: false };
```

No hay firma, no hay CA, no hay `x509`. **Cualquiera manda ese string y pasa.** Lo mismo del lado firmware: `calcularHashDesafio` usa `"SECRETO_HMAC_DEFAULT"` **hardcodeado en el binario**. Si flasheás el ESP, cualquiera que lea el firmware conoce el secreto.

### 2.3 La autenticación de usuarios no existe

`FilaUsuario` tiene `contrasena[32]` (hash SHA-256) y `rol`. **Nadie la consulta.** Los DTOs `UsuarioCredencialDTO { username, pinHash }` llegan por RPC, pero en el firmware **no se verifica contra la tabla `usuarios`**. El `ContextoSesionSGBD` que se pasa a todo el ORM es:

```cpp
ContextoSesionSGBD::sistemaInterno()  // = ROL_ADMIN, autenticado=true
```

Es decir: **cualquier sesión autenticada por handshake puede hacer cualquier cosa**, sin importar el rol. El `ModuloDCL` existe pero es letra muerta.

Tampoco hay RPC para:
- `crearUsuario`, `cambiarPassword`, `desactivarUsuario`
- Bootstrap del admin inicial (¿quién crea el primer usuario?)
- Rate limiting de intentos fallidos

### 2.4 El `EvaluadorPoliticaRPC` del firmware no se setea

En `setup()` no veo `pila.appRpc.establecerEvaluadorPolitica(...)`. Sin eso, **todo RPC entrante es aceptado** una vez que el handshake pasa.

### 2.5 CRC16 no es integridad

`calcularCRC16` detecta errores, no ataques. Un atacante con acceso al cable/aire modifica el payload y recalcula el CRC. Falta **HMAC** sobre payload (o AEAD) en Capa 4/5/6.

### 2.6 Replay attacks

`reqId` es un contador de 16 bits. No hay ventana deslizante ni cache de `reqId` recientes. **Un `RPC_REQ` capturado se puede re-emitir** y el handler corre de nuevo. Para RPCs no idempotentes (crear correo, insertar senso) esto duplica datos.

### 2.7 Inyección de origen

`CapaRed::bajarPaqueteUnicast`:

```cpp
uint8_t origenLogico = (uint8_t)(idDestino & 0x0F);
uint8_t destinoLogico = (uint8_t)(idDestino & 0x0F);
```

**Origen == destino siempre.** El nibble de origen debería ser el ID local. Esto rompe cualquier filtrado por origen y permite spoofing trivial.

---

## 3. Robustez: no sobrevive a fallos reales

### 3.1 `setup()` no verifica fallos críticos

```cpp
ServerExampleStub::registrarStubsServidor(...); // <- retorno ignorado
```

Si la SD no monta, **el firmware sigue arrancando**. Todos los `dbOrm.*` devuelven `false` silenciosamente y el sistema queda vivo pero sin DB. Lo mismo si `dbOrm.inicializar()` falla dentro de `registrarStubsServidor`. Debería haber un modo "degradado" explícito (LED rojo, RPC de health con `status: "no_sd"`) y reintentos periódicos de montaje.

### 3.2 No hay watchdog

`esp_task_wdt` no está configurado. Un `while` colgado en `CapaEnlace::subirTrama` (por ejemplo si `bytesDisponibles()` miente) no se recupera. Tampoco hay watchdog de tarea por tarea.

### 3.3 Corte de energía = corrupción de SD

`MicroSD::cerrarSeguro` hace `flush` + `close`, pero FAT **no es journaling**. Si se corta la luz en medio de un append a `senso.bin`, el cluster queda huérfano. El patrón `.tmp` + `rename` de `guardarSingleton` mitiga solo singletons. Las tablas circulares (`senso`, `evento_sensor`, `auditoria`) escriben in-place sin transacción.

Falta:
- Botón de "apagado seguro" que llame `tarjetaSd.sincronizarTodo()` + `desmontar()`.
- Detección de brown-out (`BROWNOUT` del ESP) y `sincronizarTodo()` en ISR.
- Un `RPC_SHUTDOWN` o `RPC_FACTORY_RESET`.

### 3.4 El transporte TCP no tiene retransmisión si no llegan paquetes

`mantenerTransporte()` se llama **dentro** de `CapaTransporte::subirSegmento`, que a su vez solo se ejecuta cuando hay un paquete entrante (o cuando `procesar()` pasa por acá). Si un PSH se pierde y **no llega nada más**, el ARQ **no retransmite** porque nadie vuelve a llamar a `mantenerTransporte`. En un servidor real con clientes silenciosos, esto deja sesiones colgadas.

Solución: llamar `mantenerTransporte()` también desde el loop principal (o separar en una tarea del scheduler).

### 3.5 `CapaTransporte` TCP tiene cola de 1 slot

`ControlFlujoNodo::hayPaqueteEnCola` es booleano. Si el servidor necesita mandar CALL + REPLY al mismo cliente, la segunda se **descarta silenciosamente** (`return false`). Un servidor real necesita cola real.

### 3.6 Sin SYN flood protection

Cada SYN aloca estado (`nodos[idNodo]`). No hay rate limit por `idNodo`, no hay `MAX_CONEXIONES_SIMULTANEAS` efectivo más allá de `MAX_NODOS_RED = 4`. Un atacante en la misma red UDP puede saturar los 3 slots.

### 3.7 Sin reconexión automática

Si `auditarSesiones` cierra la sesión por timeout (15 s sin heartbeat), el firmware **no reintenta** ni notifica al backend. El backend tiene que re-handshakear manualmente. Un servidor real debería exponer un estado `RECONECTANDO` y reintentar con backoff.

### 3.8 `idNodo` cambia si el cliente UDP se reconecta

`ServidorUDP` libera el slot a los 15 s sin actividad. Si el backend se reconecta, obtiene **otro `idNodo`** (otro índice). `nodos[oldIdNodo]` queda con sesión vieja, `Transporte` con estado viejo. La auditoría cierra la sesión, pero el estado del transporte no se limpia.

---

## 4. Observabilidad: no podés diagnosticar nada en campo

- **`DebugOLED` es lo único.** Y muestra 4 líneas que se pisan. No hay log persistente, no hay forma de mandar logs al backend, no hay `RPC_GET_LOGS`.
- **`gEstadoSistema.totalLlamadasRecibidas` nunca se incrementa** (o al menos no en `CapaAplicacion::procesar`). El OLED muestra 0 siempre.
- **No hay métricas**: uptime, heap libre, heap mínimo histórico, stack high-water mark, contadores por capa (CRC fallidos, timeouts ARQ, drops por cola llena, handshakes fallidos).
- **No hay endpoint de health.** El backend no puede preguntar "¿estás vivo?" ni "¿cuánto espacio queda en SD?" sin inventar un RPC nuevo.
- **No hay `RPC_GET_VERSION`.** El backend no sabe qué firmware corre. No hay `firmware_version` en la DB ni en el handshake.
- **Los errores se cuentan en una variable estática** (`contErr`) que se pierde al rebootear. Sin persistencia de errores, no hay diagnóstico post-mortem.
- **`DebugOLED` se compila con `#define DEBUG_OLED_ACTIVO 1`**. No hay forma de apagarlo en release sin recompilar. Debería ser `-DDEBUG_OLED=0` en platformio o similar.

---

## 5. Operación: no se puede desplegar ni actualizar

- **No hay OTA.** Cero `ArduinoOTA`, cero `HTTPUpdate`, cero `Update.begin()`. Para actualizar un equipo en el campo tenés que ir físicamente con un cable.
- **No hay provisioning inicial.** Si booteás con SD en blanco:
  - `dbOrm.conectividad.obtener()` falla → `modem.inicializar()` → WiFi OFF, solo USB.
  - El backend tiene que conectarse por USB para mandar `RPC_DB_GUARDAR_RED` y recién ahí reiniciar.
  - No hay **captive portal**, no hay **AP de provisioning**, no hay **botón físico** de reset. En un despliegue real, el técnico no va a abrir un serial para configurar WiFi.
- **`RPC_DB_GUARDAR_RED` no reinicia.** Devuelve `requiereReinicio: true` y el backend tiene que llamar `RPC_REINICIAR_FIRMWARE` por separado. Propenso a error: si el backend se cae entre las dos llamadas, el equipo queda con config nueva pero sin aplicar.
- **No hay factory reset.** Ni por RPC, ni por botón, ni por combinación de pines al boot.
- **No hay migración de esquema.** `EsquemaFisico.h` está hardcodeado. Si cambiás un `struct`, los `.bin` viejos quedan ilegibles y no hay `schema_version` en el archivo ni en el catálogo para detectarlo. El `sincronizarSecuenciaInicial()` va a leer basura.
- **No hay política de retención.** `MAX_FILAS_SENSO = 1024` es fijo. En un equipo con muestreo cada 2 min, son 34 horas de historia. ¿Y después? Se pisa. No hay config para "guardar 7 días" ni compresión ni export.
- **No hay backup.** No hay forma de dump-ear la DB por RPC.

---

## 6. Conectividad real

- **El firmware escucha en UDP :81 sin NAT traversal.** En Wokwi/LAN funciona. En una casa/edificio con NAT, el backend **no puede iniciar la conexión**. No hay UPnP, no hay STUN, no hay relay. Para deployment real, el ESP **tiene que iniciar** la conexión al backend (o usar un broker MQTT, o WebSocket cliente).
- **No hay discovery.** El backend tiene que saber la IP del ESP de antemano. Sin mDNS, sin broadcast de anuncio, sin "hello" al backend al bootear.
- **`RedHost::conectar` bloquea 5 s en `setup()`.** En un edificio con DHCP lento, el watchdog no corre (no está configurado) pero el OLED no refresca. Aceptable pero feo. Mejor no bloqueante con máquina de estados.
- **No hay reconexión WiFi.** `Modem::mantenerRed` no chequea `WiFi.status()`. Si el AP se cae, el ESP queda desconectado para siempre (o hasta reboot). Falta `WiFi.reconnect()` con backoff.
- **`ServidorUDP` no maneja fragmentación IP.** Un frame de 512 B con escape puede llegar a ~1100 B. En WiFi, MTU es 1500, pero si hay VLAN/tunnel, puede fragmentar. UDP fragmentado puede perderse en trozos y el `parsePacket` devuelve el paquete completo solo si llegó entero. Falta defensa: **limitar el tamaño del datagrama** (ya está, `MAX_TRAMA_ENLACE=512` pero el escapado lo infla) o **usar un puerto con MTU conocida**.

---

## 7. Bugs concretos que te van a morder en producción

| # | Bug | Impacto |
|---|-----|---------|
| 1 | `gEstadoSistema.ultimoUnixTimestamp = 0` hasta sync → `fecha_creacion = 0` en DB | Datos con epoch 1970, ordenamientos rotos, auditoría inútil |
| 2 | `CapaRed::bajarPaqueteUnicast` pone `origen = destino` | Spoofing de origen, filtros por nodo inválidos |
| 3 | `mantenerTransporte()` solo se llama desde `subirSegmento` | ARQ no retransmite si el cliente calla |
| 4 | Cola TCP de 1 slot → CALL + REPLY simultáneos descartan uno | Pérdida silenciosa de respuestas |
| 5 | `setup()` ignora retorno de `registrarStubsServidor` | Arranca sin DB y no se entera |
| 6 | `evaluadorPolitica` del firmware nunca se setea | RPCs sin autorización |
| 7 | `transformadorTx/Rx` del firmware nunca se setea | Canal en MsgPack plano (no hay "TLS") |
| 8 | `FilaLTS::clave_privada[32]` | No cabe una clave real; el "TLS" es teatro |
| 9 | `FilaUsuario` nunca se consulta | AuthN de usuario inexistente |
| 10 | `totalLlamadasRecibidas` nunca se incrementa | Debug OLED miente |
| 11 | Sin watchdog | Cuelgues no se recuperan |
| 12 | Sin NTP local | Reloj inválido si backend cae |
| 13 | Sin OTA | Actualización = visita física |
| 14 | Sin provisioning | Primer boot en campo requiere serial |
| 15 | Sin `schema_version` | Cambios de `struct` rompen la DB |
| 16 | Sin replay protection | RPCs no idempotentes se pueden duplicar |
| 17 | `ServidorUDP` libera slots a los 15 s → `idNodo` cambia | Estado de transporte huérfano |
| 18 | Sin `WiFi.reconnect()` | AP caído = equipo muerto |
| 19 | Sin rate limit | SYN flood / UDP flood tumba el server |
| 20 | Sin factory reset | RMA imposible |

---

## 8. Lo que **sí** está bien (para no ser injusto)

- La **pila OSI** está muy bien pensada: zero-copy real, `BufferPaquete` con `empujarCabecera`/`consumirCabecera`, CRC incremental, fragmentación con CRC por chunk + CRC de mensaje completo.
- El **handshake SHA-256 challenge-response** está bien implementado (comparación constant-time, nonce de CSPRNG).
- El **`ModuloDCL`** (aunque no se use) y el **`CatalogoSistema`** son un buen intento de separar DDL/DML/DQL/DCL al estilo SGBD real.
- La **separación `ModeloCatalogo` / `ModeloCircular` / `ModeloSingleton`** en el ORM es limpia.
- El **`MicroSD` handle-based** con `cerrarSeguro` y `.tmp` + `rename` para singletons es correcto.
- El **patrón `Contratos::IFirmware` / `IBackend`** con stubs que se pueden reemplazar es la forma correcta de testear.

---

## 9. Hoja de ruta mínima para "deployable"

Si tuviera que ordenar por impacto:

1. **Lógica de negocio** (sensor + umbrales + eventos + actuadores). Sin esto no es un producto.
2. **NTP local + RTC** o al menos fallback a `millis()` para no escribir `fecha_creacion = 0`.
3. **AuthN de usuario real** contra `FilaUsuario` + `ContextoSesionSGBD` derivado del `UsuarioCredencialDTO` (no `sistemaInterno()`).
4. **Setear `evaluadorPolitica`** y **`transformadores`** en `setup()` del firmware.
5. **Watchdog** (`esp_task_wdt`) + reconexión WiFi + reconexión de sesión con backoff.
6. **Health check RPC** (`RPC_HEALTH` con uptime, heap, errores, SD libre, versión de firmware).
7. **OTA** (`ArduinoOTA` o HTTP pull).
8. **Provisioning** (captive portal o AP mode con form + botón de reset).
9. **Persistencia de errores** (ring buffer de logs en SD o en RTC memory).
10. **Factory reset** + **`RPC_SHUTDOWN` seguro**.
11. **Migración de esquema** (campo `version` en `CatalogoSistema` y migraciones).
12. **Replay protection** (ventana de `reqId` por sesión).
13. **Arreglar bug #2** (origen = ID local, no destino).
14. **Cola TCP real** (>1 slot) y `mantenerTransporte()` en el loop.
15. **Rate limiting** por nodo y por RPC.

Con eso, el firmware pasa de "framework de comunicaciones con ORM de juguete" a "servidor de dispositivo desplegable".