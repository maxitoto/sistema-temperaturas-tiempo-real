┌────────────────────────────────────────────────────────────────────────────┐
│ CAPA 7 — Aplicacion                                                        │
│                                                                            │
│   · Camino 1 (Request/Reply): request() / onRequest()  [Transaccional]     │
│   · Camino 2 (Streaming):     stream()  / onStream()   [Tiempo Real]       │
│                                                                            │
│   CAMINO 1: TRANSACCIONAL (REQUEST)      CAMINO 2: TIEMPO REAL (STREAM)    │
│   · onRequest(cmd, handler)              · onStream(canal, handler)        │
│   · request(cmd, args, cb)               · queryStream(canal, args, cb)    │
│   · emitirEventoGlobal(...)              · emitirStream(canal, args)       │
├──────────────────────┬───────────────────────────────────┬─────────────────┤
│ CAPA 6 — Presentación│                                   │                 │
│    Marshalling/Unmarshalling MessagePack + AEAD AES-256-GCM (mbedTLS HW)   │
│                      │                                   │                 │
│                      ▼                                   ▼                 │
│   · bajarRequest / bajarReply            · bajarStream / bajarStreamUnicast│
├──────────────────────┼───────────────────────────────────┼─────────────────┤
│ CAPA 5 — Sesion  ▼                                   ▼                     │
│   · TIPO_RPC_REQ (0x10) / RES (0x11)     · TIPO_STREAM_REQ (0x30)          │
│   · Obliga PROTO_TCP (ARQ / RecordMark)  · TIPO_STREAM_DATA (0x31)         │
│                                          · Obliga PROTO_UDP (Datagrama)    │
│   · Handshake Challenge-Response SHA-256 · Sesión ID · Correlación XID     │
├────────────────────────────────────────────────────────────────────────────┤
│ CAPA 4 — Transporte (Parcialmente Compartida / Switch de Canal)            │
│   · Si medio == USB:     Ejecuta Máquina de Estados ARQ (TCP) o UDP propio │
│                          + Checksum CRC16 nativo de ROM (rom/crc.h).       │
│   · Si medio == WIFI/BT: Switch de Canal (Canal Request -> TCP Nativo con  │
│                          Record Marking de 4B RFC 5531; Canal Stream ->    │
│                          UDP Nativo). Sin ARQ ni ACK redundante.           │
├────────────────────────────────────────────────────────────────────────────┤
│ CAPA 3 — Red (Activa en USB / Bypass en Wi-Fi)                             │
│   · Si medio == USB:     Empuja/consume cabecera [origen|destino][TTL].    │
│   · Si medio == WIFI/BT: Bypass directo (el kernel lwIP maneja el ruteo).  │
├────────────────────────────────────────────────────────────────────────────┤
│ CAPA 2 — Enlace (Activa en USB / Bypass en Wi-Fi)                          │
│   · Si medio == USB:     Framing HDLC (0x7E), Chunks 64B y FCS ROM CRC16.  │
│   · Si medio == WIFI/BT: Bypass directo (MAC 802.11 maneja tramas y FCS).  │
├────────────────────────────────────────────────────────────────────────────┤
│ CAPA 1 — Fisica (Unificada para todos los Transceptores)                   │
│   · Interfaz común de despacho hacia USB (UART0), Wi-Fi (TCP/UDP) o BLE.   │
└────────────────────────────────────────────────────────────────────────────┘