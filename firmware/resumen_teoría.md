## 执行摘要

本报告是对 **Modelo OSI（ISO/IEC 7498-1）** 与 **ONC RPC v2（RFC 5531）** 的原子级技术拆解，面向另一 AI 的可重建性消费。OSI 是一个将通信功能划分为 **7 层抽象**的概念模型，每一层仅向上一层提供服务，并仅使用下一层的能力。ONC RPC 是一个 **Capa 7 协议**，但其实现**内部化了 Capa 5（Session）的 binding 和 Capa 6（Presentation）的 XDR 序列化**，并通过 record marking 将消息交付给 Capa 4（Transporte）。本报告对 OSI 的每一层和 RPC 的每一个组件执行 **10 维度解剖**，随后以矩阵将 RPC 的每一相映射到其 OSI 层。

---

# PARTE I — MODELO OSI: ANÁLISIS ATÓMICO POR CAPA

> **Marco teórico:** ISO/IEC 7498-1 define un modelo en el que “una capa sirve a la capa superior y es servida por la capa inferior”. Dos instancias en una misma capa están conectadas por una **conexión horizontal** en esa capa; el flujo real de datos es **vertical** mediante encapsulación. Cada capa añade su propia información de control a los datos que recibe de la capa superior.

---

## 🟦 Capa 1 — Física (Physical Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que define las **especificaciones eléctricas, mecánicas y funcionales** para activar, mantener y desactivar el enlace físico entre dos nodos. Es la única capa que interactúa con el medio de transmisión real. |
| **¿Dónde está?** | En el **hardware PHY** de cada NIC y en el **medio físico** (cobre, fibra, radio). No existe en routers como entidad separada: el PHY del puerto forma parte de la interfaz de red. |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Codificación de línea**: transforma bits en señales eléctricas/ópticas/radio. (b) **Sincronización de bits**: mantiene el reloj del receptor alineado con el emisor. (c) **Gestión del medio**: participa en la detección de portadora y, en medios compartidos, en la compartición física. |
| **¿Por qué lo hace?** | Porque los bits lógicos no pueden viajar por un medio sin una **representación física** que el receptor pueda muestrear y decodificar de forma fiable. |
| **¿Cuántas cosas hace?** | Tres funciones nucleares: codificación, sincronización y gestión del medio físico. |
| **¿Cuándo lo hace?** | De forma **continua** durante toda la transmisión; la activación/desactivación del enlace ocurre en los extremos del establecimiento y cierre. |
| **¿Dónde lo hace?** | En los **extremos del enlace** (PHY del emisor y del receptor) y en **repetidores/hubs** que regeneran la señal. |
| **¿La salida va dirigida a?** | La señal física se dirige al **medio de transmisión**; en recepción, el PHY entrega **bits** a la **Capa 2 (Enlace de Datos)** . |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** teoría de señales, códigos de línea (NRZ, Manchester, 4B/5B), modulación (QAM, PAM). **Empírico:** medición de BER (Bit Error Rate), niveles de voltaje, presupuesto de potencia óptica. |
| **¿Cómo se implementa?** | **Chip transceptor PHY** en la NIC + **driver del SO** que configura el PHY (velocidad, dúplex, codificación). |

---

## 🟦 Capa 2 — Enlace de Datos (Data Link Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que proporciona **transferencia de tramas** entre nodos **directamente conectados** en el mismo enlace, con **direccionamiento físico (MAC)** y **detección de errores** en el ámbito local. |
| **¿Dónde está?** | En el **hardware MAC** de la NIC y en **switches de Capa 2** (conmutadores que reenvían por dirección MAC, sin routing IP). |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Delimitación de tramas**: marca el inicio y fin de cada trama. (b) **Direccionamiento MAC**: inserta MAC origen/destino. (c) **Detección de errores**: calcula y verifica el **FCS (CRC-32)** . (d) **Control de acceso al medio**: CSMA/CD en Ethernet half-duplex histórico, CSMA/CA en Wi-Fi. |
| **¿Por qué lo hace?** | Porque el medio físico puede introducir errores y porque múltiples nodos pueden compartir el mismo medio; la Capa 2 **contiene** esos problemas en el enlace local sin involucrar a la Capa 3. |
| **¿Cuántas cosas hace?** | Cuatro: delimitación, direccionamiento MAC, detección de errores y control de acceso al medio. |
| **¿Cuándo lo hace?** | Por **cada paquete** que la Capa 3 entrega para transmisión en el enlace. |
| **¿Dónde lo hace?** | En el **enlace local** entre nodos adyacentes (host↔switch, switch↔router, etc.). |
| **¿La salida va dirigida a?** | La **trama** se entrega al **PHY (Capa 1)** para su serialización en señales; en recepción, el payload útil se entrega a la **Capa 3 (Red)** . |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** polinomio generador CRC-32, temporización CSMA, ventanas de contención. **Empírico:** captura de tramas, contadores de colisiones (`ethtool -S`), verificación de FCS en Wireshark. |
| **¿Cómo se implementa?** | **MAC hardware** en la NIC; **driver** del SO; **ASICs** en switches para reenvío a velocidad de línea. |

---

## 🟦 Capa 3 — Red (Network Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que **enruta paquetes** desde un host origen hasta un host destino a través de **múltiples redes interconectadas**, mediante **direccionamiento lógico global**. |
| **¿Dónde está?** | En los **routers** (plano de control y reenvío) y en la **pila IP del kernel** de todos los hosts finales. |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Direccionamiento IP**: asigna y usa direcciones origen/destino. (b) **Enrutamiento**: selecciona el siguiente salto según la tabla de rutas. (c) **Fragmentación/reensamblado**: adapta paquetes al MTU del enlace. (d) **TTL/Hop Limit**: evita bucles. |
| **¿Por qué lo hace?** | Porque dos hosts finales pueden estar separados por redes heterogéneas; se necesita un **direccionamiento independiente del enlace** y un mecanismo de **salto a salto** para alcanzar el destino. |
| **¿Cuántas cosas hace?** | Cuatro funciones nucleares: direccionamiento, enrutamiento, fragmentación y control de bucles (TTL). |
| **¿Cuándo lo hace?** | Por **cada paquete** en **cada salto** del camino, y en los hosts extremos al originar/recibir. |
| **¿Dónde lo hace?** | En **cada router intermedio** y en los **hosts extremos**. |
| **¿La salida va dirigida a?** | El **paquete IP** se entrega a la **Capa 2** para su encapsulación en trama en el siguiente enlace. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** algoritmos de estado de enlace (Dijkstra/OSPF) y vector de distancias/path-vector (BGP). **Empírico:** tablas de rutas (`ip route`), `traceroute`, contadores de paquetes descartados. |
| **¿Cómo se implementa?** | **Pila IP del kernel**, **daemons de enrutamiento** (FRRouting, BIRD) y **hardware de router** (FIB en TCAM). |

---

## 🟦 Capa 4 — Transporte (Transport Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que proporciona **transferencia extremo a extremo** entre **procesos** (no hosts), con el nivel de fiabilidad y control que elija el protocolo: **TCP** (fiable, orientado a conexión) o **UDP** (no fiable, datagramas). |
| **¿Dónde está?** | **Exclusivamente en los hosts finales**. Los routers no implementan Capa 4 (salvo NAT/firewalls que inspeccionan puertos, pero no terminan la conexión de transporte extremo a extremo). |
| **¿Qué hace cada parte dentro de esta capa?** | **TCP:** (a) establecimiento/cierre por **three-way handshake** (SYN → SYN/ACK → ACK), (b) transmisión fiable con **números de secuencia, ACKs acumulativos y retransmisión** (RTO y triple-duplicate ACK), (c) **control de flujo** por ventana deslizante, (d) **control de congestión** (slow start, congestion avoidance, fast retransmit/recovery). **UDP:** multiplexación por puertos y checksum opcional. |
| **¿Por qué lo hace?** | Porque IP **no garantiza entrega, orden ni ausencia de duplicados**; la Capa 4 proporciona esas garantías cuando la aplicación las necesita, y multiplexa múltiples aplicaciones sobre una única pila IP. |
| **¿Cuántas cosas hace?** | TCP: cuatro grandes bloques (conexión, fiabilidad, flujo, congestión). UDP: una (multiplexación). |
| **¿Cuándo lo hace?** | TCP: durante **toda la vida de la conexión**, por cada segmento. UDP: por cada datagrama, sin estado. |
| **¿Dónde lo hace?** | En los **hosts extremos** (procesos de transporte del kernel). |
| **¿La salida va dirigida a?** | El **segmento TCP** o **datagrama UDP** se entrega a la **Capa 3** para su encapsulación en paquete IP. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** máquina de estados TCP (RFC 9293), ventana deslizante, algoritmos de congestión (Reno, CUBIC, BBR). **Empírico:** `ss`, `netstat`, `tcpdump` (retransmisiones, ventana, RTT), `tcpprof`. |
| **¿Cómo se implementa?** | **Pila TCP/IP del kernel** (Linux `net/ipv4/tcp_*.c`, FreeBSD `tcp_subr.c`). |

---

## 🟦 Capa 5 — Sesión (Session Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que **establece, gestiona, sincroniza y termina** el **diálogo** (sesión) entre dos aplicaciones, incluyendo el control del **turno de habla** y los **checkpoints** de recuperación. |
| **¿Dónde está?** | En los **hosts finales**, generalmente como **bibliotecas de software** o **funciones integradas en protocolos de aplicación**. La práctica moderna tiende a **fusionar Capas 5–7** en la aplicación, en lugar de implementarlas como servicios separados. |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Establecimiento/liberación de sesión**: negocia parámetros y cierra ordenadamente. (b) **Sincronización con checkpoints**: inserta puntos de recuperación para reanudar tras fallo. (c) **Gestión del diálogo**: controla quién transmite (half-duplex/full-duplex). |
| **¿Por qué lo hace?** | Porque una conversación entre aplicaciones puede requerir un **contexto persistente** que sobreviva a mensajes individuales, y porque la recuperación tras fallo necesita puntos de referencia. |
| **¿Cuántas cosas hace?** | Tres funciones nucleares: establecimiento/liberación, sincronización (checkpoints) y gestión del diálogo. |
| **¿Cuándo lo hace?** | Desde el **inicio** del diálogo hasta su **cierre**, y en los **checkpoints** intermedios. |
| **¿Dónde lo hace?** | En los **dos extremos** de la sesión (hosts finales). |
| **¿La salida va dirigida a?** | Las **PDU de sesión** (o los datos de diálogo) se entregan a la **Capa 6** para su transformación de representación, o directamente a la **Capa 4** si no hay Capa 6 separada. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** protocolos de sesión (ISO 8327), máquinas de estado de diálogo. **Empírico:** trazas de establecimiento/cierre, análisis de checkpoints en protocolos con reanudación. |
| **¿Cómo se implementa?** | **Bibliotecas de sesión** o **lógica embebida en el protocolo de aplicación**; **en RPC, el binding y la correlación XID cumplen la función de sesión** (véase Parte III). |

---

## 🟦 Capa 6 — Presentación (Presentation Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa que asegura que los datos intercambiados sean **interpretables independientemente de la arquitectura interna** de cada host (endianness, tamaño de palabra, representación de tipos). |
| **¿Dónde está?** | En los **hosts finales**, como **bibliotecas de serialización** (XDR, ASN.1) o **funciones de cifrado/compresión** (TLS, gzip). |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Codificación (marshalling)** : transforma tipos nativos a un **formato de transferencia neutral**. (b) **Decodificación (unmarshalling)** : reconstruye tipos nativos en el receptor. (c) **Transformaciones adicionales**: cifrado, compresión. |
| **¿Por qué lo hace?** | Porque un `int` de 32 bits puede ser little-endian en x86 y big-endian en un mainframe; sin una representación común, el receptor **no puede interpretar los bytes** de forma inequívoca. |
| **¿Cuántas cosas hace?** | Tres funciones nucleares: codificación, decodificación y transformaciones (cifrado/compresión). |
| **¿Cuándo lo hace?** | **Antes** de entregar los datos a Capa 5/4 (emisión) y **después** de recibirlos de Capa 5/4 (recepción). |
| **¿Dónde lo hace?** | En los **hosts finales** (bibliotecas de presentación). |
| **¿La salida va dirigida a?** | El **flujo de bytes codificado** se entrega a la **Capa 5** (o Capa 4) para su transmisión. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** **XDR (RFC 4506)** : big-endian, unidad base de 4 bytes, alineación a 4 bytes, padding con ceros para datos de longitud variable. **Empírico:** verificación byte a byte del stream XDR, pruebas de interoperabilidad entre arquitecturas. |
| **¿Cómo se implementa?** | **Bibliotecas XDR** (p. ej. `libtirpc`); **en RPC, los stubs cliente y servidor realizan marshalling/unmarshalling XDR** (véase Parte II). |

---

## 🟦 Capa 7 — Aplicación (Application Layer)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es esta capa?** | La capa más alta, que **interactúa directamente con el software de usuario** y proporciona **servicios de red** a las aplicaciones (HTTP, SMTP, DNS, **RPC**). |
| **¿Dónde está?** | En los **procesos de usuario** (navegadores, clientes de correo, stubs RPC). |
| **¿Qué hace cada parte dentro de esta capa?** | (a) **Interfaz de servicio**: expone operaciones de red a la aplicación. (b) **Protocolo de aplicación**: define la sintaxis y semántica de los mensajes (p. ej. CALL/REPLY en RPC). (c) **Gestión de la sesión de aplicación** (en pilas fusionadas, absorbe Capa 5). |
| **¿Por qué lo hace?** | Porque las aplicaciones necesitan **servicios de comunicación** sin gestionar directamente los detalles de las capas inferiores; la Capa 7 es el punto de **encapsulación inicial** y **desencapsulación final**. |
| **¿Cuántas cosas hace?** | Depende del protocolo; para RPC: definir el contrato (programa/versión/procedimiento), construir el mensaje CALL, interpretar el REPLY. |
| **¿Cuándo lo hace?** | Durante la **ejecución de la aplicación**, por cada operación de red solicitada. |
| **¿Dónde lo hace?** | En el **espacio de usuario** del host final (proceso de aplicación). |
| **¿La salida va dirigida a?** | Los **datos de aplicación** se entregan a la **Capa 6** para su serialización (o a Capa 5/4 en pilas sin Capa 6 separada). |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** especificación del protocolo (RFC), máquina de estados de aplicación. **Empírico:** trazas de aplicación, `strace`/`ltrace`, captura del payload de aplicación. |
| **¿Cómo se implementa?** | **Bibliotecas y frameworks** (stubs RPC generados, clientes HTTP, resolvers DNS). |

---

# PARTE II — ONC RPC v2 (RFC 5531): ANÁLISIS ATÓMICO POR COMPONENTE

> **Marco del protocolo:** ONC RPC v2 se especifica en **RFC 5531** (Mayo 2009), que obsoleta RFC 1831 sin cambios en el protocolo sobre el cable. El protocolo de mensajes se define usando el lenguaje **XDR (RFC 4506)**. El modelo RPC replica la llamada local: el caller envía un **call message** con parámetros y **bloquea** hasta recibir un **reply message** con los resultados.

---

## 🧩 Componente 1 — Cliente (Client)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es?** | El **proceso de aplicación** que decide invocar un procedimiento remoto y llama a la función expuesta por el **stub cliente**. |
| **¿Dónde está?** | Espacio de usuario del **host cliente**. |
| **¿Qué hace?** | Ejecuta la **invocación** de la función del stub, pasando los argumentos nativos de la aplicación. |
| **¿Por qué lo hace?** | Porque necesita que se ejecute lógica en un servidor remoto **sin escribir código de red** explícito (sockets, serialización, timeouts). |
| **¿Cuántas cosas hace?** | **Una**: realizar la llamada a la interfaz del stub. La preparación de argumentos y la lógica de negocio previa son responsabilidad de la aplicación, no del componente RPC. |
| **¿Cuándo lo hace?** | En el **momento** en que la aplicación decide invocar el servicio remoto. |
| **¿Dónde lo hace?** | En el **host cliente**, en el contexto del proceso de aplicación. |
| **¿La salida va dirigida a?** | La **llamada con argumentos nativos** se dirige al **Stub Cliente**. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** llamada a función local según la interfaz definida (IDL). **Empírico:** ejecución del código de aplicación que invoca el stub; trazas de `strace` para observar la llamada. |
| **¿Cómo se implementa?** | Código de aplicación enlazado con la **biblioteca RPC** y el **stub generado** desde el archivo IDL (`.x` en ONC RPC). |

---

## 🧩 Componente 2 — Stub Cliente (Client Stub)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es?** | Un **proxy generado automáticamente** desde el IDL que **serializa** los argumentos, construye la **cabecera del mensaje RPC** y asigna un **XID** de transacción. |
| **¿Dónde está?** | Espacio de usuario del **host cliente**, enlazado en la aplicación. |
| **¿Qué hace?** | (a) **Marshalling XDR** de los argumentos: codificación big-endian, alineación a 4 bytes, padding. (b) **Construcción de la cabecera CALL**: `xid` (4 bytes), `msg_type = CALL (0)`, `rpcvers = 2`, `prog`, `vers`, `proc`, `cred` y `verf` (autenticación). (c) **Asignación del XID**: valor único que correlaciona CALL con REPLY. |
| **¿Por qué lo hace?** | Para convertir una llamada con **tipos nativos** en un **mensaje de red neutral al hardware**, y para permitir que el runtime **correlacione** la respuesta correcta con la llamada correcta. |
| **¿Cuántas cosas hace?** | Tres: marshalling XDR, construcción de cabecera RPC y asignación de XID. |
| **¿Cuándo lo hace?** | **Inmediatamente después** de que el Cliente invoca el stub, antes de entregar el mensaje al runtime. |
| **¿Dónde lo hace?** | En el **host cliente**. |
| **¿La salida va dirigida a?** | El **mensaje CALL codificado en XDR** (cabecera + argumentos) se entrega al **RPC Runtime del cliente**. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** XDR (RFC 4506): big-endian, unidad base de 4 bytes, padding con ceros para datos de longitud variable. **Empírico:** generación con `rpcgen`; verificación byte a byte del mensaje en captura de red. |
| **¿Cómo se implementa?** | **Stub generado** por `rpcgen` (o herramienta equivalente) y enlazado con `libtirpc` o la biblioteca RPC correspondiente. |

---

## 🧩 Componente 3 — RPC Runtime (Cliente y Servidor)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es?** | La **biblioteca de tiempo de ejecución** que gestiona el **binding** (resolución de dirección), el **transporte**, el **record marking** en TCP y la **correlación de XID**. |
| **¿Dónde está?** | En **ambos extremos**: runtime del cliente y runtime del servidor. |
| **¿Qué hace?** | (a) **Binding**: consulta a `rpcbind` en el **puerto 111** con `RPCBPROC_GETADDR(prog, vers, netid)` para obtener la **dirección universal** del servicio. (b) **Transporte**: entrega el mensaje a **TCP o UDP** según la implementación; RPC **no implementa fiabilidad propia**. (c) **Record marking** (TCP): envuelve cada mensaje RPC en fragmentos con cabecera de **4 bytes** (1 bit de último fragmento + 31 bits de longitud) para delimitarlo en el stream. (d) **Correlación XID**: verifica que el `xid` del REPLY coincide con el del CALL. |
| **¿Por qué lo hace?** | Para **ocultar la heterogeneidad de la red** (direcciones dinámicas, transportes distintos) y garantizar que la **respuesta correcta** llegue a la **llamada correcta**, incluso con múltiples llamadas concurrentes. |
| **¿Cuántas cosas hace?** | Cuatro: binding, gestión de transporte, record marking y correlación de XID. |
| **¿Cuándo lo hace?** | **Binding**: al inicio de la asociación RPC (o cuando expira la dirección). **Transporte/record marking/correlación**: durante **toda la transacción**, por cada mensaje. |
| **¿Dónde lo hace?** | En **cliente y servidor**. |
| **¿La salida va dirigida a?** | Los **segmentos TCP** (con record marking) o **datagramas UDP** se entregan a la **Capa 4 (Transporte) del SO**. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** `RPCBPROC_GETADDR` (RFC 1833) devuelve la dirección universal `IP:puerto`; record marking: cabecera de 4 bytes con bit de último fragmento y longitud de 31 bits. **Empírico:** `tcpdump -i any port 111` para el binding; verificación del XID y del record marker en Wireshark. |
| **¿Cómo se implementa?** | **`libtirpc`** (o equivalente), **`rpcbind`** como daemon en el servidor, y la **pila TCP/UDP del kernel**. En **gRPC**, el runtime se sustituye por **HTTP/2** (streams con ID único, framing de longitud). |

---

## 🧩 Componente 4 — Stub Servidor (Server Stub)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es?** | El **proxy generado** en el servidor que **deserializa** el mensaje CALL y **invoca el procedimiento real** del servidor con argumentos nativos. |
| **¿Dónde está?** | Espacio de usuario del **host servidor**. |
| **¿Qué hace?** | (a) **Unmarshalling XDR** de los argumentos: reconstruye los tipos nativos del servidor. (b) **Invocación del procedimiento** del servidor con esos argumentos. |
| **¿Por qué lo hace?** | Para que el procedimiento del servidor reciba **argumentos en el formato nativo** que espera, **sin que el programador del servidor tenga que parsear XDR manualmente**. |
| **¿Cuántas cosas hace?** | Dos: unmarshalling XDR e invocación del procedimiento. |
| **¿Cuándo lo hace?** | **Al recibir** el mensaje CALL del runtime, antes de la ejecución del procedimiento. |
| **¿Dónde lo hace?** | En el **host servidor**. |
| **¿La salida va dirigida a?** | La **llamada con argumentos nativos** se dirige al **Servidor** (procedimiento real). |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** decodificación XDR inversa al marshalling (misma alineación, mismo byte order). **Empírico:** stub generado por `rpcgen`; pruebas de ida y vuelta (round-trip) de serialización. |
| **¿Cómo se implementa?** | **Stub generado** desde el IDL y enlazado con la biblioteca RPC del servidor. |

---

## 🧩 Componente 5 — Servidor (Server)

| Dimensión | Contenido técnico |
|---|---|
| **¿Qué es?** | El **proceso que ejecuta** la lógica de negocio del procedimiento remoto y **produce el resultado** que se devolverá al cliente. |
| **¿Dónde está?** | Espacio de usuario del **host servidor**. |
| **¿Qué hace?** | Ejecuta el **procedimiento** con los argumentos recibidos y retorna el **resultado nativo** al stub servidor para que lo serialice en el REPLY. |
| **¿Por qué lo hace?** | Para **proveer el servicio remoto** que el cliente ha solicitado; es el **punto final** de la semántica de la llamada. |
| **¿Cuántas cosas hace?** | **Una**: ejecutar el procedimiento y retornar el valor (o lanzar el error). |
| **¿Cuándo lo hace?** | **Tras** el unmarshalling realizado por el stub servidor, y **antes** de que el resultado se serialice en el REPLY. |
| **¿Dónde lo hace?** | En el **host servidor**. |
| **¿La salida va dirigida a?** | El **resultado nativo** se entrega al **Stub Servidor**, que lo serializa en XDR para construir el mensaje REPLY. |
| **¿Cómo lo hace teórica y empíricamente?** | **Teórico:** ejecución de código local con los argumentos deserializados; semántica definida por el contrato del programa/versión/procedimiento. **Empírico:** pruebas unitarias del procedimiento, logs de ejecución, métricas de latencia del lado servidor. |
| **¿Cómo se implementa?** | **Código de aplicación del servidor**, registrado ante `rpcbind` mediante `RPCBPROC_SET(prog, vers, netid, uaddr)`. |

---

# PARTE III — MATRIZ DE INTEGRACIÓN: RPC DENTRO DE CADA CAPA OSI

> **Ley de la matriz:** Cada fila de la tabla siguiente representa una **fase atómica** de una transacción RPC. La columna “Capa OSI” indica **dónde opera funcionalmente** esa fase según el modelo de referencia. La columna “Componente RPC” identifica **qué componente** del protocolo la ejecuta. Las capas 1–3 **no contienen componentes RPC**: RPC delega en la pila subyacente y **no implementa** funciones de enlace, red ni física.

| **Fase atómica de la transacción RPC** | **Componente RPC** | **Capa OSI** | **Función OSI equivalente** | **Artefacto / Protocolo concreto** |
|---|---|---|---|---|
| Invocación del procedimiento | Cliente → Stub Cliente | **7 — Aplicación** | Interfaz de servicio al usuario | Llamada a función local (API del stub) |
| **Marshalling de argumentos** | Stub Cliente | **6 — Presentación** | Serialización neutral al hardware | **XDR**: big-endian, alineación a 4 bytes, padding |
| **Construcción de cabecera CALL** | Stub Cliente | **7 — Aplicación** | Sintaxis del protocolo de aplicación | `xid`(4B) + `msg_type=CALL(0)` + `rpcvers=2` + `prog` + `vers` + `proc` + `cred` + `verf` |
| **Asignación de XID** | Stub Cliente / Runtime | **5 — Sesión** | Identificación de la transacción de diálogo | Campo `xid` de 4 bytes; el REPLY debe portar el **mismo XID** |
| **Binding (resolución de dirección)** | RPC Runtime (cliente) | **5 — Sesión** | Establecimiento de la asociación de sesión | `RPCBPROC_GETADDR(prog, vers, netid)` a `rpcbind` en **puerto 111**; devuelve dirección universal `IP:puerto` |
| **Record marking (TCP)** | RPC Runtime (cliente) | **4 — Transporte** | Delimitación de mensajes en stream | Cabecera de **4 bytes**: 1 bit de último fragmento + 31 bits de longitud |
| **Transporte extremo a extremo** | — (delegado) | **4 — Transporte** | Entrega fiable u no fiable | **TCP** (three-way handshake, retransmisión, control de flujo/congestión) o **UDP** |
| **Enrutamiento** | — (delegado) | **3 — Red** | Direccionamiento y routing | **IP** (IPv4/IPv6), tabla de rutas, TTL |
| **Entrega en enlace local** | — (delegado) | **2 — Enlace** | Encapsulación en tramas, MAC, FCS | **Ethernet** / **Wi-Fi** |
| **Señalización física** | — (delegado) | **1 — Física** | Transmisión de bits | PHY (cobre, fibra, radio) |
| Recepción y delimitación | RPC Runtime (servidor) | **4 → 5** | Reensamblado y correlación | Verificación de record marker y de XID del CALL |
| **Unmarshalling** | Stub Servidor | **6 — Presentación** | Deserialización | **XDR decode** |
| **Ejecución del procedimiento** | Servidor | **7 — Aplicación** | Lógica de servicio | Código de negocio del procedimiento |
| **Serialización del resultado** | Stub Servidor | **6 — Presentación** | Serialización del REPLY | **XDR encode** del resultado |
| **Envío del REPLY** | RPC Runtime (servidor) | **5 → 4** | Respuesta correlacionada | `xid` idéntico al CALL; record marking |
| **Recepción del REPLY** | RPC Runtime (cliente) | **4 → 5** | Correlación de respuesta | Verificación de que `xid` del REPLY == `xid` del CALL |
| **Unmarshalling del resultado** | Stub Cliente | **6 — Presentación** | Deserialización | **XDR decode** |
| **Retorno al Cliente** | Cliente | **7 — Aplicación** | Entrega del resultado | Valor de retorno de la función del stub |

**Nota sobre gRPC (paradigma moderno):** En gRPC, la fusión de capas se materializa en **HTTP/2**. La Capa 7 (Aplicación) expone las cabeceras `:method POST`, `:path /Servicio/Método`, `content-type application/grpc`; la Capa 5 (Sesión) se cubre con **streams HTTP/2 de ID único** y multiplexación; la Capa 6 (Presentación) se realiza mediante **`Length-Prefixed-Message`** (framing de longitud + payload Protobuf). Todo ello sobre **TCP** (Capa 4), que a su vez utiliza **control de flujo por stream y por conexión** (ventana inicial de **65,535 bytes** en HTTP/2, ajustable mediante `WINDOW_UPDATE`).