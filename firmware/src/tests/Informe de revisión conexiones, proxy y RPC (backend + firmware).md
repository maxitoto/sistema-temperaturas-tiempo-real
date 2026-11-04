# Informe de revisión: conexiones, proxy y RPC (backend + firmware)

*4 de noviembre de 2026 · Alcance: `src/backend/connections/**` (Electron/TypeScript) y `src/tests/connections/**` (ESP32/C++). No se recibió el código de la base de datos ni de la UI; esas zonas quedan como contrato (comentarios `//zona base de datos` y `//cargar de base de datos`).*

## 1. Resumen ejecutivo

La arquitectura en capas (Phy → Reliability → Security → Transport → Proxy → RPC) es sólida y está bien espejada entre backend y firmware: mismo framing COBS+CRC16, mismo handshake ECDH+AES-GCM, mismos mensajes de control (PING/PONG/IDLE\_CLOSE) y mismo envelope RPC. El problema no es el diseño general sino cuatro cosas: la configuración no se persiste, las sesiones no están aisladas de verdad en la capa RPC, el liveness y el cierre por inactividad no hacen todavía lo que pide el objetivo, y hay varios defectos puntuales.

Lo que más pesa, en orden:

1. **El bug del log** son dos fallas encadenadas: abrir el puerto serie reinicia el ESP32 (hipótesis principal) y el firmware no persiste el WiFi, así que despierta sin red.
2. **RPC no aísla medios**: el stub acepta una respuesta venga del medio que venga, y las llamadas de usuario usan un «cliente preferido» global.
3. **El cierre por inactividad** está en 5 min (no 10), lo decide un lado y el otro obedece sin poder negarse, y en UART cierra el puerto físico, lo que dispara el reinicio del punto 1.
4. **La caché AtMostOnce** sobrevive entre sesiones y puede devolver respuestas viejas a pedidos nuevos.
5. **`network.configure_sta` bloquea 8 s dentro del handler**, con `schedule()` reentrante, y las retransmisiones del backend lo re-ejecutan.
6. **AES-GCM reutiliza nonce**: ambas direcciones (y TCP/UDP) usan la misma clave con contadores que arrancan en 0.

## 2. El bug del log, paso a paso

Reconstrucción con las marcas de tiempo del log (UTC).

| Hora | Llamada | Resultado | Lectura |
| --- | --- | --- | --- |
| — | `configurarWiFiCasa` (4 args) | Error: no hay conexión autenticada | Correcto, aún no había medio. Los args 3 y 4 (AP) se ignoran: la firma del backend recibe solo 2. |
| 03:02:33 | `conectarSerial` | true | UART autenticado. |
| 03:02:40 | `conectarWiFi(.14)` | false | Esperado: el ESP32 aún no tenía STA. Devuelve `false` sin motivo. |
| 03:03:06 | `verEstadoRed` | STA false, AP activo, MAC en ceros | Respondió por UART. `WiFi.macAddress()` es la del STA y en modo solo-AP no existe. |
| 03:03:23 | `configurarWiFiCasa` (2 args) | STA conectado, IP .14 | OK. Bloqueó el firmware hasta 8 s (H-05). |
| 03:03:30 | `conectarWiFi(.14)` | true | WiFi autenticado; el cliente preferido pasa a WiFi. |
| sin hora | `verEstadoRed` | Error: no hay conexión autenticada | Ningún medio estaba autenticado. Hay 7,5 min hasta la siguiente hora registrada: compatible con el cierre por inactividad de 5 min. |
| 03:10:57 | `conectarWiFi(.14)` | true | Reconexión desde cero; el ESP32 conservaba el STA (no se había reiniciado). |
| 03:11:04 | `verEstadoRed` | STA conectado | OK por WiFi. |
| 03:11:15 | `conectarSerial` | true | Al abrir el puerto el ESP32 se reinicia (hipótesis principal). |
| 03:11:21 | `verEstadoRed` | STA false, SSID vacío, IP 0.0.0.0 | `staSsid_` solo se vacía en `loadConfig()` durante `setup()`: el firmware arrancó de cero. |
| 03:11:33 | `conectarWiFi(.14)` | false | Esperado: esa IP ya no existe porque el STA se perdió. |

### Causas, de la más segura a la más probable

1. **El firmware no persiste el WiFi (seguro).** `WifiTransport::loadConfig()` fija `staSsid_ = ''` y `apPassword_ = '12345678'` a mano, y `WiFi.persistent(false)` evita que el core guarde credenciales. `staSsid_` solo se vacía en `loadConfig()`, que solo corre en `setup()`. Ver `staSsid: ''` a las 03:11:21 prueba que el equipo arrancó de cero. Cualquier reinicio (watchdog, brownout, reflash, reset por DTR) deja al equipo sin red hasta reconfigurar por UART.
2. **Abrir el puerto serie reinicia el ESP32 (hipótesis principal, falta confirmar).** `UartPhy.begin()` abre el puerto y recién después baja DTR/RTS (`set({dtr:false, rts:false})`). En placas con CP210x/CH340 esas líneas gobiernan EN e IO0 y el pulso al abrir basta para resetear el chip; `hupcl:false` solo afecta al cierre. Encaja con que el único evento entre 03:11:04 (todo bien) y 03:11:21 (todo vacío) fue `conectarSerial`.
3. **Nadie detecta el reinicio.** El handshake no lleva identificador de arranque, así que el backend no distingue «sesión nueva sobre firmware recién iniciado» de una sesión normal: no puede avisar a la UI ni reponer la configuración.
4. **El error sin hora es compatible con el cierre por inactividad.** `RPC_IDLE_TIMEOUT_MS` es 5 min en ambos lados: a las \~03:08 los dos medios se cierran de forma negociada y la siguiente llamada falla con un mensaje genérico. El log no lo prueba (falta la hora de la llamada fallida). Si la llamada fue inmediata hay que buscar otra causa, y por eso la observabilidad (T04) es prioridad 0.

Las causas 2 y 4 se potencian: al cerrar por inactividad el backend también cierra el puerto serie, y reabrirlo reinicia el equipo. Hoy, dejar la app 5 minutos y volver por UART equivale a borrar la red del dispositivo.

**Cómo confirmarlo en minutos:** (a) devolver `bootId` (contador en RAM/NVS) y `esp_reset_reason()` en `network.get_status`; (b) repetir `conectarSerial` con el STA configurado y mirar si `bootId` cambia; (c) registrar cada transición de estado con su motivo (T04). Un capacitor de \~10 µF entre EN y GND descarta el reset por DTR.

**Lo que no es:** ni el protocolo RPC, ni la criptografía, ni el `WiFiTransport` del backend. `conectarWiFi` funcionó siempre que el ESP32 tenía STA configurado.

## 3. Estado de conexiones, proxy y RPC

### 3.1 Lo que está bien

- Capas separadas con el mismo árbol de directorios en backend y firmware.
- Framing COBS + CRC16, ACK por frame, reintentos, fragmentación y descarte de duplicados por `seq`.
- ECDH P-256 + SHA-256 + AES-256-GCM con contador y AAD (anti-replay dentro de una sesión).
- Las tramas de control del proxy (PING/PONG/IDLE\_CLOSE) **no** reinician el reloj de inactividad RPC: separa bien «enlace vivo» de «hay uso».
- Las respuestas RPC vuelven por el `clientId` de origen, y los tickets permiten llamadas concurrentes.
- El backend aborta al instante los tickets de un medio caído (`onMediumDisconnected`).
- El firmware responde `DISC` a datos no autenticados: detecta rápido un reinicio del lado contrario.
- Todos los módulos ya tienen un gancho `loadConfig()`; falta llenarlo.

### 3.2 Hallazgos de severidad alta

**H-01 · Firmware · Configuración WiFi no persistente.** Ver sección 2. Además `network.configure_sta` solo cubre STA; no existe endpoint para el AP, y la clave del AP está fija en `12345678` (el log usa `ESP32@123456`). Corrección: `WifiConfig` con STA y AP, `loadConfig()` que lee de NVS (`//cargar de base de datos`) y `applyConfig()` que persiste antes de aplicar (`//zona base de datos`).

**H-02 · Backend UART · Abrir el puerto reinicia el equipo.** `UartSTransport.connect()` siempre llama `init()` → `phy.begin()` → reabre el puerto, incluso si ya estaba autenticado (`conectarWiFi` sí tiene ese atajo; `conectarSerial` no). Corrección: atajo si ya está autenticado; en cierre por inactividad terminar la sesión criptográfica pero **dejar el puerto abierto**; tras abrir, esperar el arranque (\~1,5 s) tolerando basura del bootloader; tratar el reinicio como evento normal (H-01 + `bootId`).

**H-03 · Backend RPC · El stub no valida el origen.** `RpcStub.onIncomingResponse(env)` no recibe `clientId`: una respuesta que entre por el medio B puede resolver un ticket emitido por el medio A, y los listeners de stream se indexan solo por `streamId`. Además `callTo(0, …)` usa el cliente «preferido» global, que cambia cada vez que alguien conecta algo. Es exactamente el síntoma «me llegan mensajes por un medio que no es el mío». Corrección: pasar `clientId` desde el proxy hasta el stub, descartar y registrar respuestas con origen distinto al del ticket, y prohibir `clientId = 0` en llamadas de usuario.

**H-04 · Ambos · La caché AtMostOnce cruza sesiones.** La clave es `(clientId, callId)`: no incluye `methodId`, no se vacía al desconectar y `clientId` es estable por medio (peerId siempre 1). Si Electron se reinicia sin reiniciar el ESP32 (por WiFi), los `callId` vuelven a 1 y el firmware responde con la respuesta guardada de **otro método** de la sesión anterior. Tampoco hay marca «en curso», así que un reintento llega y ejecuta el handler por segunda vez. Corrección: `sessionId` aleatorio por handshake dentro de la clave, comparar `methodId`, vaciar al cerrar la sesión y guardar un marcador «en curso».

**H-05 · Firmware · `connectToSta` bloquea dentro del handler.** Espera hasta 8 s llamando `proxy.schedule()` por `onWaitTick`, es decir, reentra al despachador mientras el handler sigue activo. El stub del backend reintenta a los 2,5 s (2 reintentos): el duplicado entra por la reentrada, la caché aún no tiene la respuesta, y `reconfigureSta()` se ejecuta de nuevo (`WiFi.disconnect()`/`begin()` reinician la conexión). Además el tiempo total del backend (3 × 2,5 s = 7,5 s) es menor que los 8 s del firmware. Corrección: conexión STA asíncrona (el endpoint responde «conectando» y el estado se consulta con `network.get_status`, o responde por evento), sin `delay()` ni `schedule()` dentro de handlers.

**H-06 · Ambos · Cierre por inactividad.** 5 min fijos en ambos lados (el objetivo es 10); lo decide quien vence primero y el otro cierra siempre, aunque tenga llamadas o streams en curso; cierra todos los medios por igual; el error posterior es el genérico «No hay conexión autenticada». Ver diseño en 4.3.

**H-07 · Ambos · Reutilización de nonce en AES-GCM.** La clave de sesión es la misma en ambas direcciones y el IV es solo el contador (que arranca en 0 en cada lado): el primer mensaje del backend y el primero del firmware usan el mismo par (clave, IV). Lo mismo ocurre entre TCP y UDP (`copySessionKeyTo`). Con GCM esto compromete confidencialidad e integridad. Corrección: derivar claves distintas por dirección y por canal (HKDF con etiquetas `c2s`, `s2c`, `tcp`, `udp`), o al menos fijar un bit de dirección/canal en el IV.

### 3.3 Hallazgos de severidad media y baja

| ID | Sev. | Dónde | Hallazgo y corrección |
| --- | --- | --- | --- |
| H-08 | M | Firmware WiFi | Un TCP nuevo expulsa a la sesión autenticada **antes** de completar su handshake; además no hay timeout de handshake (ni en WiFi ni en UART), así que un socket mudo ocupa el único cupo. Mantener la sesión viva hasta que la nueva autentique, o rechazar mientras haya una viva; timeout de handshake \~3 s. |
| H-09 | M | Ambos | Dos relojes de liveness: `UartReliability` corta el transporte tras 4 × 250 ms = 1 s sin ACK, y el proxy tolera 2,8 s. Debe haber una sola autoridad (el proxy); Reliability solo emite eventos. Invariante: `ackTimeout × maxRetries ≤ linkTimeout`. |
| H-10 | M | Firmware | No existe equivalente de `onMediumDisconnected`: llamadas y streams iniciados por el firmware esperan su timeout y el historial no se limpia al caer el medio. |
| H-11 | M | Backend | `estaAutenticado()` llama `uartS.disconnect()` (un getter con efectos), y `ultimaIpDhcpConocida` / `preferredClientId_` son estado global mutable compartido por todos los llamadores. |
| H-12 | M | Ambos | Sin autenticación mutua: el ECDH es efímero y anónimo, protege contra espionaje pasivo pero no contra un intermediario activo. Considerar clave de emparejamiento (PSK) o firmas. |
| H-13 | M | Backend WiFi | Tras completar el handshake TCP hay un `return`: si llegaron más bytes en el mismo segmento quedan sin procesar hasta el próximo evento `data`. Seguir al bucle de framing. |
| H-14 | B | Firmware | `getNetworkStatus`: MAC en ceros en modo solo-AP (usar `WiFi.softAPmacAddress()`), `staSsid` es el configurado y no el real, `apActive_` no se refresca. Añadir `bootId`, `uptimeMs`, `resetReason`, canal. |
| H-15 | B | Ambos | `UartReliability`: el reensamblado no tiene timeout (un chunk perdido deja basura que reaparece cuando `msgId` da la vuelta, 8 bits) y los mensajes >255 chunks se truncan. |
| H-16 | B | Ambos | MsgPack propio: arrays de más de 15 elementos corrompen el conteo (`0x90 \| len & 0x0F`), `push(...v)` con binarios grandes puede desbordar la pila, y no hay versión de protocolo. Añadir vectores de prueba compartidos. |
| H-17 | B | Firmware | `RpcStub::invoke` devuelve el `streamId` como «ticket» y no avisa si no hay ruta (el backend sí); `client::call` bloquea hasta 3 s. |
| H-18 | B | Ambos | Constantes divergentes sin documentar: `maxPacketsPerTick` 16 vs 12, reintentos RPC 2500 ms × 2 vs 400 ms × 3, `maxRetries_` 10 inicial vs 4 en `loadConfig`. |

## 4. Objetivos del desarrollo vs. código actual

### 4.1 Cada persona por su medio, sin ver los mensajes de la otra

Hoy se cumple a medias: el firmware y el skeleton del backend responden por el `clientId` del pedido (bien), pero el stub, el cliente preferido y las llamadas iniciadas por el firmware («el último que habló») rompen el aislamiento. Reglas objetivo:

- Una **sesión** por (medio, peer) con `clientId`, `sessionId` aleatorio, estado, reloj de recepción, reloj RPC y último motivo de cierre. Reemplaza los siete mapas paralelos por medio del proxy.
- El `clientId` viaja hasta el stub; toda respuesta cuyo origen no coincida con el del ticket se descarta y se registra.
- Prohibido `clientId = 0` en llamadas de usuario: al conectar, la UI recibe un `sessionHandle` y lo pasa en cada llamada. El «cliente preferido» queda solo para la consola de desarrollo.
- Respuestas, streams, PONG e IDLE\_CLOSE salen siempre por el medio de origen. Lo iniciado por el firmware necesita destino explícito o difusión explícita.
- Límite que conviene documentar: **un par por medio** (un cable UART, un socket TCP). Un segundo WiFi se rechaza mientras haya una sesión viva (H-08).

### 4.2 «Si no responde a los mensajes de servicio por mi medio, se desconectó»

El PING/PONG por medio ya existe; falta ordenarlo:

- **Una sola autoridad**: el proxy. Reliability y los transportes solo reportan eventos (`ACK_FAIL`, `PORT_CLOSED`).
- Una sesión está viva mientras llegue **cualquier** trama del par (ACK de enlace, PONG, datos) dentro de `linkTimeout`. El PING se envía solo si no hubo tráfico reciente.
- Al caer: registrar el motivo, notificar a RPC en **ambos** lados, vaciar colas de ese medio, limpiar la caché AtMostOnce de esa sesión y dejar el medio listo para reconectar (firmware escuchando, backend con destino recordado). Los demás medios no se tocan.
- `probeInterval`, `linkTimeout` y reintentos salen de la base de datos con límites validados (`linkTimeout ≥ 2 × probeInterval`) y se intercambian en el handshake para que ambos lados usen los mismos valores.

### 4.3 Cierre negociado tras 10 minutos sin uso

La mensajería IDLE\_CLOSE / IDLE\_CLOSE\_ACK es una buena base. Protocolo propuesto:

1. El handshake intercambia `idleTimeoutMs` (por defecto 600000) y ambos adoptan el mínimo.
2. El reloj de inactividad por sesión cuenta solo tramas RPC reales (como hoy).
3. Al vencer, el primero envía `IDLE_CLOSE(sessionId)`.
4. El par responde `IDLE_CLOSE_ACK` si también está ocioso, o `IDLE_CLOSE_NACK(busy)` si tuvo actividad o tiene llamadas/streams pendientes; el iniciador reinicia su reloj.
5. Con ACK ambos pasan a `CLOSED_IDLE`, un estado distinto de caída: no es error y la UI muestra «en reposo». Si se cruzan dos IDLE\_CLOSE se resuelve como ACK mutuo (hoy ya funciona).
6. Sin ACK dentro de la ventana de gracia (400 ms hoy, quedan cortos para WiFi) se cierra igual, marcado «sin acuerdo».
7. **Ahorro real de recursos**: en UART se cierra la sesión criptográfica pero no el puerto; en WiFi se cierra el TCP, se activa `WiFi.setSleep(true)` y se liberan buffers.
8. El backend reconecta bajo demanda (`ensureConnected(medio)`) con el destino recordado; el mensaje de error pasa a ser específico («cerrada por inactividad», «sin respuesta», «nunca conectada»).
9. Para probarlo sin esperar, configurar `idleTimeoutMs = 20000` desde el endpoint de configuración.

### 4.4 Cada directorio configurable desde fuera

Patrón único para todos los módulos:

- `XConfig`: estructura con los parámetros y sus límites (`validate()`).
- Constructor / `loadConfig()`: lee del repositorio y, si no hay datos, usa defaults. Así un equipo que despierta sin saber nada se repone solo (la IP DHCP puede cambiar, no importa).
- `applyConfig(cfg)`: valida → persiste → aplica en RAM (o destruye y recrea el objeto) → devuelve el resultado.
- El **endpoint** que llama a `applyConfig` vive fuera del módulo (capa RPC o raíz de composición). El módulo nunca conoce RPC; la dependencia va de afuera hacia adentro.
- Si aplicar corta el enlace por el que llegó la orden (por ejemplo, cambiar el AP mientras se configura por el AP), la respuesta sale primero y la aplicación se difiere al siguiente ciclo de `poll()`.

```cpp
// interfaces/WiFi/WiFiTransport.hpp (firmware)
void loadConfig() override {
    // cargar de base de datos
    cfg_ = repo_.loadWifi().value_or(WifiConfig::defaults());
}

Result applyConfig(const WifiConfig& in) {
    if (!in.validate()) return Result::Invalid;     // clave AP >= 8 caracteres, SSID 1..32
    // zona base de datos
    if (!repo_.saveWifi(in)) return Result::PersistFailed;
    cfg_ = in;                 // RAM
    reinitPending_ = true;     // poll() reconstruye AP/STA DESPUÉS de enviar la respuesta
    return Result::Ok;
}

// main.cpp o messages/RPC/Endpoints (fuera del directorio WiFi)
srv.bind("network.configure", [](WifiConfigDto d) { return wifi.applyConfig(d.toConfig()); });
```

```ts
// ContenedorComunicacion.ts (backend)
configurarRedFirmware: async (cfg: WifiConfigDto, s: SessionHandle) => {
  // zona base de datos: guardar lo enviado, para reponerlo si el firmware despierta vacío
  await repo.guardarWifi(cfg);
  return c.callTo(s.clientId, "network.configure", cfg).as<NetworkStatusDto>();
},
```

| Endpoint (nuevo) | Módulo destino | Parámetros |
| --- | --- | --- |
| `network.configure` | WiFiTransport | STA (ssid, clave), AP (ssid, clave, canal), puertos |
| `link.configure` | TransportProxy | probeInterval, linkTimeout, idleTimeout, gracefulWait, maxPacketsPerTick |
| `reliability.configure` | UartReliability | ackTimeout, maxRetries, maxChunkSize |
| `rpc.configure` | Skeleton / Stub | historial, TTL, reintentos, timeouts por método |
| `config.get` | todos | lectura con `revision` y sin secretos |

## 5. Qué debe controlar cada directorio

El árbol es el mismo en backend y firmware.

### Raíz: `ContenedorComunicacion.ts` / `main.cpp`

- **Controla:** orden de construcción (repositorio → `loadConfig` → `registerMedium` → endpoints), cableado, API hacia la UI o registro de endpoints, apagado ordenado.
- **No debe:** contener protocolo, timeouts ni estado global mutable (`ultimaIpDhcpConocida`, cliente preferido).
- **Configuración:** es el único lugar que conoce el repositorio y lo inyecta; aloja los endpoints de configuración.

### `proxy/ITransport`

- **Controla:** el contrato de un medio: estados, `connect/disconnect/poll`, `sendReliable/sendStream`.
- **Ampliar con:** `lastDisconnectReason()`, `bootId` del par, `applyConfig()`, eventos (`onStateChange`).
- **No debe:** exponer detalles físicos al proxy.

### `proxy/TransportProxy`

- **Controla:** sesiones por medio, liveness (PING/PONG), cierre por inactividad negociado, ruteo `clientId` ↔ medio, colas rx/tx, notificación de caídas a RPC.
- **No debe:** conocer el envelope RPC, el cifrado ni el tramado.
- **Configuración:** probeInterval, linkTimeout, idleTimeout, gracefulWait, maxPacketsPerTick, todo desde BD con validación y negociado en el handshake.

### `interfaces/UART-S/UART` (Phy)

- **Controla:** bytes ↔ frames COBS, apertura y cierre del puerto, límite de buffer (512 B), descubrimiento de puertos (backend).
- **No debe:** tener lógica de sesión. Política nueva: `keepOpen` mientras la app viva, para no pulsar DTR/RTS.
- **Configuración:** baudios y pines fijos (invariantes físicos); en backend, último puerto usado (BD).

### `interfaces/UART-S/Reliability`

- **Controla:** ACK, reintentos, fragmentación, reensamblado, descarte de duplicados.
- **No debe:** desconectar el transporte por sí mismo; reporta «enlace degradado» al proxy.
- **Configuración:** ackTimeout, maxRetries, maxChunkSize (BD, con `ackTimeout × maxRetries ≤ linkTimeout`). Falta timeout de reensamblado.

### `interfaces/UART-S/Security`

- **Controla:** ECDH, derivación de claves, nonces, anti-replay.
- **Debe añadir:** claves por dirección y canal, y opcionalmente una clave de emparejamiento guardada cifrada (NVS/BD).
- **No debe:** conocer tramas ni medios.

### `interfaces/UART-S/UartSTransport`

- **Controla:** máquina de estados UART (DISCONNECTED → PHY\_LINKED → HANDSHAKING → AUTHENTICATED), SYN/SYN\_ACK/DISC, timeout de handshake, intercambio de `bootId` y parámetros de enlace.
- **Configuración:** puerto (backend) y timeouts de handshake.

### `interfaces/WiFi/WiFiTransport`

- **Controla:** radio STA/AP, servidor TCP 8443 y UDP 8444, una sesión viva, reconexión del STA, estado de red real.
- **No debe:** bloquear el bucle ni expulsar una sesión autenticada por un cliente aún sin handshake.
- **Configuración:** STA, AP, canal y puertos en BD/NVS, con `applyConfig` diferido y `setSleep(true)` en reposo.

### `messages/RPC/DTOs`

- **Controla:** el contrato de cable: envelope, tipos, MsgPack, hash FNV-1a, límites y `protocolVersion`.
- **No debe:** tener lógica. Sin configuración; cambia solo con versión y vectores de prueba compartidos entre TS y C++.

### `messages/RPC/Skeletons`

- **Controla:** registro de endpoints, ejecución, deduplicación AtMostOnce, errores, control de acceso por endpoint y medio.
- **Debe corregir:** clave de dedup con `sessionId` + `methodId`, marcador «en curso», limpieza al cerrar sesión, handlers no bloqueantes.
- **Configuración:** tamaño y TTL del historial, políticas de acceso.

### `messages/RPC/Stubs`

- **Controla:** tickets, reintentos, timeouts, correlación y errores, siempre **por sesión**.
- **Debe corregir:** recibir `clientId` en cada respuesta y validarlo; timeouts por método (STA ≥ 12 s o asíncrono).
- **Configuración:** retryTimeout, maxRetries, timeouts por método.

### `messages/RPC/RpcApp`

- **Controla:** fachada `server` / `client` para el resto de la aplicación.
- **No debe:** elegir un medio por defecto global: `call` exige sesión. Los mensajes de error indican medio y causa.

## 6. TODO priorizado

| ID | Prio | Dónde | Tarea | Listo cuando |
| --- | --- | --- | --- | --- |
| T01 | P0 | Firmware | `WifiConfig` (STA+AP) persistida en NVS; `loadConfig()` real con `//cargar de base de datos`; `applyConfig()` con `//zona base de datos` | Reiniciar el ESP32 y recuperar STA/AP sin intervención |
| T02 | P0 | Ambos | `bootId` + `resetReason` en el handshake y en `network.get_status`; el backend detecta reinicios y lo informa a la UI | Un reinicio genera un evento visible |
| T03 | P0 | Backend UART | Atajo si ya está autenticado; no cerrar el puerto al cerrar por inactividad; esperar arranque tras abrir | `conectarSerial` repetido no reinicia el ESP32 |
| T04 | P0 | Ambos | Log de cada transición de estado con motivo (`IDLE_CLOSE`, `LINK_TIMEOUT`, `ACK_FAIL`, `DISC_RX`, `PORT_CLOSED`) y errores específicos por causa | El log del 4/11 se podría explicar sin hipótesis |
| T05 | P0 | Backend | `configurarWiFiCasa` acepta AP (ssid, clave), valida y devuelve motivo; `conectarWiFi` devuelve `{ok, motivo}` | Los 4 argumentos del log tienen efecto |
| T06 | P1 | Backend RPC | `clientId` hasta el stub, validar origen, eliminar `call(0)`, `SessionHandle` | Dos medios simultáneos no se cruzan respuestas |
| T07 | P1 | Ambos | Sesión como objeto (`ClientSession`) en el proxy, reemplazando mapas paralelos | Un medio se puede caer sin afectar al otro |
| T08 | P1 | Ambos | Idle de 10 min configurable, negociado en handshake, con NACK `busy`, estado `CLOSED_IDLE`, reconexión perezosa | Prueba con idle de 20 s pasa en ambos lados |
| T09 | P1 | Ambos | Una sola autoridad de liveness; invariante `ackTimeout × maxRetries ≤ linkTimeout`; parámetros desde BD | Cortar un medio lo marca caído en ≤ linkTimeout |
| T10 | P1 | Ambos | Dedup AtMostOnce con `sessionId` + `methodId` + «en curso»; vaciar al cerrar sesión | Reiniciar Electron no devuelve respuestas viejas |
| T11 | P1 | Firmware | Conexión STA asíncrona; sin `delay`/`schedule` dentro de handlers | Reintentos del backend no re-ejecutan el endpoint |
| T12 | P1 | Firmware WiFi | No expulsar sesión viva antes del handshake; timeout de handshake en WiFi y UART | Un socket mudo no bloquea el único cupo |
| T13 | P1 | Ambos | Claves AES-GCM por dirección y canal (HKDF) | Ningún par (clave, IV) se repite entre direcciones |
| T14 | P1 | Ambos | Patrón `XConfig` / `loadConfig` / `applyConfig` + endpoints `network.configure`, `link.configure`, `reliability.configure`, `rpc.configure`, `config.get` | Cada módulo se reconfigura por RPC sin tocar su directorio |
| T15 | P1 | Firmware | Aplicación diferida de config que corta el enlace de origen | La respuesta llega antes de cortar el AP |
| T16 | P2 | Firmware | Notificar caída de medio a la capa RPC y limpiar tickets/streams | Sin llamadas colgadas tras una caída |
| T17 | P2 | Backend | Quitar efectos de `estaAutenticado()`; eliminar estado global (`ultimaIpDhcpConocida`, preferido) | Getters sin efectos |
| T18 | P2 | Backend WiFi | Procesar bytes sobrantes tras el handshake TCP | Sin latencia extra en el primer mensaje |
| T19 | P2 | Firmware | Corregir estado de red: MAC del AP, SSID real, canal, uptime | Sin MAC en ceros |
| T20 | P2 | Ambos | Timeout de reensamblado y manejo de `msgId` | Sin basura tras pérdida de chunks |
| T21 | P2 | Ambos | MsgPack: límites, versión de protocolo, vectores TS↔C++ | Pruebas cruzadas en CI |
| T22 | P2 | Firmware | `RpcStub::invoke` devuelve `callId` y error inmediato sin ruta | Igual comportamiento que el backend |
| T23 | P2 | Ambos | Alinear constantes divergentes y documentarlas | Una tabla única de valores por defecto |
| T24 | P2 | Firmware | `WiFi.setSleep(true)` en reposo, `false` al reanudar | Menor consumo en `CLOSED_IDLE` |
| T25 | P3 | Ambos | Autenticación mutua (PSK o firmas) | Un intermediario activo falla el handshake |

## 7. Plan de pruebas mínimo

- **Reinicio del ESP32** con STA configurado: vuelve solo a la red y el backend informa el cambio de `bootId`.
- **Abrir/cerrar UART repetidamente:** el equipo no se reinicia.
- **Dos medios a la vez** (UART y WiFi) con llamadas y streams simultáneos: cada respuesta vuelve a su origen; desenchufar uno no afecta al otro.
- **Corte de un medio** (cable, apagar AP): detección en ≤ `linkTimeout` y error específico.
- **Idle negociado** con 20 s: cierre limpio, estado `CLOSED_IDLE`, reconexión perezosa; y caso `busy` (llamada justo antes de vencer).
- **Reinicio de Electron sin reiniciar el ESP32** (por WiFi): ninguna respuesta vieja.
- **Configurar STA desde el AP:** la respuesta llega antes de que el AP se reinicie.
- **Retransmisión durante `network.configure`:** el endpoint se ejecuta una sola vez.

## 8. Límites de esta revisión

No se vio el repositorio de BD, la UI ni el entorno físico. La causa 2 del bug (reset por DTR/RTS) y la 4 (cierre por inactividad) son hipótesis coherentes con el log, no pruebas: T02 y T04 existen para confirmarlas. Los hallazgos H-03, H-04, H-05, H-07 y H-08 salen directamente de leer el código y no dependen de esas hipótesis.
