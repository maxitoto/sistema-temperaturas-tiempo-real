Antes!
                            [ Capas 5, 6 y 7: Sesión (SHA-256) + Presentación (TLS/MsgPack) + App RPC ]
                                                                │
                            [ Capa 4 Propia: CapaTransporte (SYN/ACK + Retransmisión ARQ o PROTO_UDP) ]
                                                                │
                            [ Capas 2 y 3 Propias: CapaRed (TTL) + CapaEnlace (Framing 0x7E + CRC-16) ]
                                                                ▼
                            ╔═════════════════════════════════════════════════════════════════════════╗
                            ║ CAPA 1 FICTICIA: WiFiClient (ESP) <───> net.Socket (Node.js)            ║
                            ║ ❌ El OS volvía a hacer Handshake TCP, ACKs, Nagle y bloqueaba a UDP    ║
                            ╚═════════════════════════════════════════════════════════════════════════╝





Actual!
                            [ Capas 5, 6 y 7: Sesión (SHA-256) + Presentación (TLS/MsgPack) + App RPC ]
                                                                │
                                    ┌───────────────────────────┴───────────────────────────┐
                            (fiable = true)                                        (fiable = false)
                                    ▼                                                       ▼
                            [ Capa 4: PROTO_TCP (0x06) ]                           [ Capa 4: PROTO_UDP (0x11) ]
                            SEQ + ACK + Stop&Wait ARQ                              Directo sin ACK ni cola
                                    └───────────────────────────┬───────────────────────────┘
                                                                ▼
                            [ Capas 2 y 3 Propias: CapaRed (TTL) + CapaEnlace (Framing 0x7E + CRC-16) ]
                                                                ▼
                            ╔═════════════════════════════════════════════════════════════════════════╗
                            ║ CAPA 1 CRUDA: WiFiUDP (ESP :81) / dgram (Node)  ó  PuertoUSB (Serial)   ║
                            ║ ✅ Envío "fire-and-forget" puro: toda la inteligencia la aporta tu pila ║
                            ╚═════════════════════════════════════════════════════════════════════════╝


En el modelo antiguo, aunque la CapaTransporte marcara un paquete como PROTO_UDP, abajo viajaba atrapado en un socket TCP del sistema operativo (WiFiClient), duplicando ACKs, retransmisiones y sufriendo bloqueos de línea.
En el modelo actual, la base Wi-Fi usa WiFiUDP ("fire and forget" sin estado, igual de crudo que el cable PuertoUSB), por lo que las Capas 2 a 4 son las únicas que garantizan integridad y deciden en vivo si un mensaje viaja como TCP fiable o UDP puro. 😎🫵🏻👍🏻

┌─────────────────────────────────────────────────────────────────────────────┐
│ CAPA 7 — CapaAplicacion                                                     │
│   RPC CALL / REPLY / EVENTO · EvaluadorPoliticaRPC (DCL inyectable)         │
│   MetaCall { idNodo, reqId, protocolo }                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 6 — CapaPresentacion                                                   │
│   MessagePack Zero-Copy (LectorMsgPack / EscritorMsgPack)                   │
│   TransformadorPayloadCapa6 (cifrado de canal)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 5 — CapaSesion                                                         │
│   Handshake SHA-256 challenge-response · sessionId 16b · reqId (XID)        │
│   ProtocoloTransporte protocolo = UDP   ← (default del firmware)            │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 4 — CapaTransporte                                                     │
│   protocolo == UDP:  sin ARQ, sin ACK, sin RTO, sin retransmisión           │
│   protocolo == TCP:  Stop-and-Wait ARQ + RTO 1000 ms (modo legado)          │
│   ─────────────────────────────────────────────────────────────────────     │
│   CAMINO UDP (el que usás):                                                 │
│     escribirCabeceraYCRC(pkt, PROTO_UDP, ...) → CapaRed::bajarPaquete       │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 3 — CapaRed                                                            │
│   Cabecera 2B: [origen:4 | destino:4] [TTL:8]                               │
│   Ruteo unicast / broadcast · TTL decremento                                │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 2 — CapaEnlace                                                         │
│   HDLC byte-stuffing: SOF 0x7E · ESC 0x7D · IAC 0xFF                        │
│   Fragmentación 64B · CRC16 por chunk + CRC16 del mensaje completo          │
├─────────────────────────────────────────────────────────────────────────────┤
│ CAPA 1 — CapaFisica  (interfaz abstracta, implementación por medio)         │
│  obtenerTotalMedios() · estaActivo(idMedio) · recibirBytes() / enviarBytes()│
└───────────────┬─────────────────────────────────────┬───────────────────────┘
                │                                     │
      ┌─────────┴─────────┐                 ┌─────────┴─────────┐
      │   MEDIO 0         │                 │   MEDIO 1..N      │
      │   USB Serial      │                 │   WiFi            │
      │   (idNodo = 0)    │                 │   (idNodo = 1..3) │
      └─────────┬─────────┘                 └─────────┬─────────┘
                │                                     │
                ▼                                     ▼
     ┌─────────────────────────┐         ┌─────────────────────────┐
     │ PuertoUSB               │         │ ServidorUDP (⚠ UDP)     │
     │ Serial (115200 bps)     │         │ WiFiUDP                 │
     │                         │         │                         │
     │ Transporta BYTES        │         │ parsePacket()           │
     │ crudos del stream       │         │ beginPacket()           │
     │                         │         │ write() / read()        │
     │ No hay noción de        │         │                         │
     │ datagrama en el HW      │         │ Datagramas nativos      │
     │                         │         │ del módem WiFi          │
     └─────────────────────────┘         └─────────────────────────┘
              │                                     │
              │  (bytes del stream)                 │  (datagramas UDP)
              ▼                                     ▼
     ┌─────────────────────────┐         ┌─────────────────────────┐
     │ /dev/tty.usbserial      │         │ wokwigw                 │
     │ (o socat udp:4000       │         │ --forward udp:9081:     │
     │  en Wokwi)              │         │   10.13.37.3:81         │
     └─────────────────────────┘         └─────────────────────────┘
              │                                     │
              ▼                                     ▼
     ┌─────────────────────────┐         ┌─────────────────────────┐
     │ Backend                 │         │ Backend                 │
     │ PuertoUSBNode           │         │ ClienteUDPNode          │
     │ SerialPort (Node)       │         │ dgram.Socket (Node)     │
     └─────────────────────────┘         └─────────────────────────┘



Flujo!

     Backend                                Firmware
   │                                      │
   │  leerDatosEnVivo({ fiable: false })  │
   │  ─────────────────────────────────►  │
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaAplicacion│
   │                              │  dispatch CALL│
   │                              └───────┬───────┘
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaPresentac.│
   │                              │ decode MsgPack│
   │                              └───────┬───────┘
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaSesion    │
   │                              │  sessionId ✓  │
   │                              │  reqId ✓      │
   │                              │  protocolo=UDP│
   │                              └───────┬───────┘
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaTransporte│
   │                              │  PROTO_UDP    │
   │                              │  sin ARQ      │
   │                              └───────┬───────┘
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaRed       │
   │                              │  TTL, origen  │
   │                              └───────┬───────┘
   │                                      │
   │                              ┌───────┴───────┐
   │                              │ CapaEnlace    │
   │                              │  HDLC + CRC16 │
   │                              └───────┬───────┘
   │                                      │
   │                        ┌─────────────┴─────────────┐
   │                        │                           │
   │                   [ USB ]                    [ WiFi ]
   │                        │                           │
   │                  Serial.write()            udp.beginPacket()
   │                        │                           │
   │                        └─────────────┬─────────────┘
   │                                      │
   │                        (bytes por el aire o por el cable)
   │                                      │
   ▼                                      ▼