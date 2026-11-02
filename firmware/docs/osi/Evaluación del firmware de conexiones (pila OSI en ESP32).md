Sigo con la validación: confirmé tres escenarios de error reales en la capa 2. Ahora compruebo en la documentación una última duda sobre `const_cast`, y luego redacto el veredicto.

Ya tengo los resultados. Dejo el parche y el test de dos nodos como archivos para que puedas aplicarlos y ejecutarlos.

## Veredicto

Casi todo está bien, pero **la capa 2 todavía tiene dos defectos que pueden perder o duplicar mensajes sin avisar**. Tus tests no los ven, porque en ellos una misma instancia hace de emisor y receptor. Capas 1, 3 y 4 no tienen problemas pendientes que yo haya podido demostrar.

Esta evaluación cubre la última versión (`_318099`). Incluye lo que dejé pendiente de la anterior (`_345019`).

## Qué comprobé

- **Tus logs de placa:** 17 PASS (capa 2: 6, capa 3: 4, capa 4: 4, capa 1: 3). No hay FAIL ni panic.
- **Compilación en host:** las cuatro suites también dan 17/17.
- **Telemetría de stack en placa:** el mínimo libre fue 14316 B en enlace y 14188 B en transporte, sobre 16384 B. Eso es un pico de unos 2,1 y 2,2 KB, suponiendo que el valor viene en bytes como lo interpreta tu código. Antes el pico pasaba de 8 KB.
- **Escenarios con dos pilas enfrentadas:** los escribí yo (`test-enlace-dos-nodos.cpp`). En ellos fallan tres.

## Lo que está bien

1. **Stack arreglado.** El test usa buffers `static` y `SET_LOOP_TASK_STACK_SIZE(16*1024)`. Esa macro existe en Arduino-ESP32 y la documentación indica que por defecto son 8 KB. Debe ir una sola vez, en un único `.cpp`, porque define una función.
2. **Push y restore sin copias.** La cabecera se empuja, se envía y se restaura. Funciona y ahorra stack. Sigue habiendo una observación sobre `const_cast`, más abajo.
3. **Byte stuffing.** El log muestra 0x7E→`7D 5E`, 0x7D→`7D 5D` y 0xFF→`7D DF`. Es el esquema asíncrono tipo HDLC/PPP (RFC 1662): se escapa y se invierte el bit 5. Tus apuntes describen *bit* stuffing, que es la variante síncrona, así que conviene citarlo así. Escapar 0xFF no hace falta, pero no daña.
4. **NACK único.** El test 4 confirma 1 NACK, sin tormenta.
5. **Reintento por timeout y descarte de duplicado.** El test 5 lo cubre. La teoría exige reconfirmar el duplicado y descartarlo, y lo haces.
6. **ISN aleatorio.** Es aceptable. Según la documentación de Espressif, `esp_random()` solo da números realmente aleatorios con Wi-Fi o Bluetooth activos. Solo con USB es pseudoaleatorio. Aquí el ISN sirve para distinguir duplicados retrasados, no es una medida de seguridad. Para esto basta, pero no lo presentes como criptográfico.

## Lo que está mal

### Defecto 1 (alto): los ACK y NACK no llevan número

**Reproducido.** Si el ACK tarda más de 250 ms (por ejemplo, el receptor está ocupado escribiendo en flash), el emisor retransmite el mensaje 1. El receptor lo ACKea, descarta la copia y la ACKea otra vez. Con eso llegan dos ACK.
- El primero cierra el mensaje 1.
- El emisor envía el mensaje 2.
- El segundo ACK, que en realidad era del mensaje 1, se toma como confirmación del mensaje 2.
- Si el mensaje 2 se perdió, nadie lo retransmite.

Resultado en tu versión: `A sigue esperando ACK = 0` (debería ser 1) y `MSG2 entregado = 0`.

**Razón.** La teoría de stop-and-wait pide numerar también los ACK. Los apuntes de Columbia lo cuentan como el error histórico de HDLC con ACK sin número. Tus tramas de datos ya llevan número de mensaje (4 bits, valores 1..15), pero los ACK y NACK se envían siempre con `{tipo, 1, 0, …}`.

**Cómo arreglarlo.** El parche adjunto hace esto:
1. `enviarSenalControl(tipo, seq)` pone `crudo[1] = (seq << 4) | 1`.
2. El receptor responde `ACK` con `seqMsgEnEnsamblaje`.
3. El NACK lleva el número de la cabecera que falló.
4. La detección de trama de control pasa de `cabeceraRx[1] == 1` a `(cabeceraRx[1] & 0x0F) == 1`.
5. Al recibir un ACK o NACK, se ignora si `!esperandoAckArq` o si `seqMsg != seqUltimoTxEnviado`. El valor 0 queda libre como comodín, porque tus números van de 1 a 15.

### Defecto 2 (alto): `ultimoSeqMensajeRx = -1` dentro de `abortarEnsamblajeConNack()`

**Reproducido.** Un ACK perdido, más una retransmisión corrupta, hace que el mensaje se entregue **dos veces**:
- Tu versión: `ORDEN_UNICA entregada 2 veces`, duplicadas descartadas = 0.
- Con el parche: entregada 1 vez, duplicadas descartadas = 1.

Si esa línea fuera una orden como `CMD_SAVE_EEPROM`, se ejecutaría dos veces.

**Razón.** Esa línea borra el último número visto en cada error. Sirve para que el reinicio del emisor funcione, pero desactiva la detección de duplicados justo cuando hay errores, que es cuando más hace falta.

### Defecto 3 (medio): la sincronización tras reinicio no es fiable

**Reproducido.** Si la primera transmisión (la que lleva `FLAG_SYNC_BOOT`) se pierde o se corrompe, la retransmisión sale sin el flag. El receptor aún recuerda el número 1 y descarta el mensaje como duplicado. En tu versión: `DOS = 0`, es decir, perdido sin aviso.

Probé también una variante que mantiene el flag hasta recibir el ACK. Falla en un bucle de reinicios (crash loop): el test 6 de tu propio suite dio FAIL. Con dos reinicios seguidos, el número 1 se repite y el receptor lo descarta. Por eso no basta con el flag.

**Solución que sí pasó todas las pruebas:** un handshake de enlace.
1. Tras un reinicio, el emisor envía una trama de control `SYNC` (0x83) y retiene el primer mensaje, sin transmitirlo todavía.
2. Reintenta el `SYNC` cada 250 ms, hasta 3 veces.
3. El receptor resetea `ultimoSeqMensajeRx = -1` y responde con ACK de número 0.
4. Solo entonces el emisor transmite el mensaje retenido.
5. Si agota los reintentos, transmite igualmente, como alternativa de último recurso.

Es el mismo mecanismo de "iniciación" que describen tus apuntes de capa 2. En una línea serie las tramas no se reordenan, así que un `SYNC` retrasado no puede llegar después de los datos.

## Resultados con el parche

| Escenario | Tu versión | Con el parche |
|---|---|---|
| ACK retrasado (>250 ms) | MSG2 perdido | MSG2 entregado |
| Dos reinicios seguidos | 1 1 1 | 1 1 1 |
| `SYNC` perdido en el cable | `DOS` perdido | `DOS` entregado |
| ACK perdido + retransmisión corrupta | entrega duplicada | 1 entrega |

## Qué hacer, en orden

1. **Aplica** `CapaEnlace-fix-ack-numerado-y-sync.patch` sobre `CapaEnlace.hpp`.
2. **Adapta tu suite de capa 2.** Con el handshake, la primera trama en el cable pasa a ser el `SYNC`, no el dato. Tus tests 3 a 6 asumen lo contrario (el comentario del test 4 dice "[0]=CTRL_RESET_SEQ"). Por eso el test 3 sale FAIL con el parche. Es una falla del test, no del parche. Antes de cada prueba, haz que el emisor y el receptor completen el `SYNC` (basta con enviar un mensaje de cebado y bombear en ambos sentidos hasta que se calme).
3. **Añade el test de dos nodos** `test-enlace-dos-nodos.cpp`, que contiene los cuatro escenarios de arriba. Debe vivir en tu suite: sin él no vas a ver ninguno de estos defectos.
4. **Borra el código muerto:** `FLAG_SYNC_BOOT` y `syncBootEnEnsamblaje`, que el handshake deja sin uso.

## Observaciones menores

- **`const_cast` en `bajarPaqueteUnicast` y `bajarSegmentoUnicast`.** Modificar un objeto realmente `const` a través de un acceso no `const` es comportamiento indefinido, según cppreference. Hoy es seguro porque nadie pasa un `BufferPaquete const`, pero la firma promete algo que no cumple. Cámbiala a `BufferPaquete&` para que el compilador impida el error.
- **Capa 3 sin telemetría.** El test de capa 3 no mide el stack. Su función de prueba ocupa unos 4,3 KB en mi build x86, y en placa pasó, pero añade la misma medición que ya tienes en las capas 2 y 4.
- **Dos limitaciones que no toqué:** WiFi UDP sigue sin autenticación, y el modo passthrough de BT no tiene direcciones ni puertos.

## Qué no pude verificar

- Corrí el parche solo en host, con un `crc16_be` propio en lugar del de la ROM. No lo probé en la placa.
- No ejecuté los drivers reales de USB, BT y WiFi.
- El handshake no pasa tu suite actual hasta que la adaptes. Lo verifiqué con mis cuatro escenarios, no con tus seis tests.

Archivos:
- CapaEnlace-fix-ack-numerado-y-sync.patch
- test-enlace-dos-nodos.cpp