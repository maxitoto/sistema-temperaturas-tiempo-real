======================================================================
 VERIFICACION CAPA 4 (TRANSPORTE) - CAJA NEGRA Y CAJA BLANCA
======================================================================

==== TRAMA SALIDA / P1 - Transp TX -> BT (payload, antes de passthrough) ====  total=19B
Hex dump:
  0000  52 50 43 5F 44 49 52 45  43 54 4F 5F 50 4F 52 5F |RPC_DIRECTO_POR_|
  0010  42 54 00                                         |BT.|

[TRAZA] Bytes en cable BT = 19 (payload = 19)

==== TRAMA SALIDA / Cable (loopback) ====  total=19B
Hex dump:
  0000  52 50 43 5F 44 49 52 45  43 54 4F 5F 50 4F 52 5F |RPC_DIRECTO_POR_|
  0010  42 54 00                                         |BT.|


==== TRAMA ENTRADA / P1 - Transp RX <- BT (payload entregado) ====  total=19B
Hex dump:
  0000  52 50 43 5F 44 49 52 45  43 54 4F 5F 50 4F 52 5F |RPC_DIRECTO_POR_|
  0010  42 54 00                                         |BT.|

[TEST 1 - PASSTHROUGH BT] 0B overhead y entrega directa: PASS

==== TRAMA SALIDA / P2 - UDP TX pto=81 (payload) ====  total=27B
Hex dump:
  0000  54 45 4C 45 4D 45 54 52  49 41 5F 53 45 4E 53 4F |TELEMETRIA_SENSO|
  0010  52 5F 31 3D 2D 31 38 2E  34 43 00                |R_1=-18.4C.|


==== TRAMA SALIDA / Cable (loopback) ====  total=47B
Hex dump:
  0000  7E 00 11 27 01 08 11 51  51 00 00 00 00 00 67 5A |~..'...QQ.....gZ|
  0010  54 45 4C 45 4D 45 54 52  49 41 5F 53 45 4E 53 4F |TELEMETRIA_SENSO|
  0020  52 5F 31 3D 2D 31 38 2E  34 43 00 3E 57 4F CD    |R_1=-18.4C.>WO.|


==== TRAMA ENTRADA / P2 - UDP RX pto=81 OK (payload tras 10B consumidos) ====  vista=[12..38]  total=39B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x000B  ( 12 bytes)
  [Vista actual (capa superior)]  0x000C..0x0026  ( 27 bytes)

Hex dump:
  0000  01 08 11 51 51 00 00 00  00 00 67 5A 54 45 4C 45 |...QQ.....gZTELE|
  0010  4D 45 54 52 49 41 5F 53  45 4E 53 4F 52 5F 31 3D |METRIA_SENSOR_1=|
  0020  2D 31 38 2E 34 43 00                             |-18.4C.|


==== TRAMA SALIDA / P2 - UDP TX pto=99 (payload; puerto destino CERRADO) ====  total=27B
Hex dump:
  0000  54 45 4C 45 4D 45 54 52  49 41 5F 53 45 4E 53 4F |TELEMETRIA_SENSO|
  0010  52 5F 31 3D 2D 31 38 2E  34 43 00                |R_1=-18.4C.|


==== TRAMA SALIDA / Cable (loopback) ====  total=47B
Hex dump:
  0000  7E 00 21 27 01 08 11 51  63 00 00 00 00 00 89 9B |~.!'...Qc.......|
  0010  54 45 4C 45 4D 45 54 52  49 41 5F 53 45 4E 53 4F |TELEMETRIA_SENSO|
  0020  52 5F 31 3D 2D 31 38 2E  34 43 00 35 CA 05 ED    |R_1=-18.4C.5...|

[TRAZA] subirSegmento pto=99 devolvio: false (rechazo correcto)
[TEST 2 - UDP + MULTIPLEXACION TSAP] Puerto 81 OK / Puerto 99 Rechazado: PASS

==== TRAMA SALIDA / P3 - SYN cliente [proto=06 pO=80 pD=80 flags=01 seq=0x01F4 ack=0] ====  total=10B
Hex dump:
  0000  06 50 50 01 01 F4 00 00  C8 81                   |.PP.......|


==== TRAMA SALIDA / Cable (loopback) ====  total=20B
Hex dump:
  0000  7E 00 11 0C 00 08 06 50  50 01 01 F4 00 00 C8 81 |~......PP.......|
  0010  BE 15 C7 4E                                      |...N|

[TRAZA] tras SYN -> estado TCP: SYN_RECIBIDO

==== TRAMA SALIDA / P3 - ACK cliente [flags=02 seq=0x01F5 ack=0x0065] ====  total=10B
Hex dump:
  0000  06 50 50 02 01 F5 00 65  2D 60                   |.PP....e-`|


==== TRAMA SALIDA / Cable (loopback) ====  total=20B
Hex dump:
  0000  7E 00 31 0C 00 08 06 50  50 02 01 F5 00 65 2D 60 |~.1....PP....e-`|
  0010  23 DA C7 4E                                      |#..N|

[TRAZA] tras ACK -> estado TCP: ESTABLECIDO

==== TRAMA SALIDA / P3 - pkt1 TX (sale directo) ====  total=19B
Hex dump:
  0000  43 4D 44 5F 53 45 54 5F  55 4D 42 52 41 4C 3D 2D |CMD_SET_UMBRAL=-|
  0010  32 30 00                                         |20.|


==== TRAMA SALIDA / P3 - pkt2 TX (quedara en COLA porque pkt1 espera ACK) ====  total=18B
Hex dump:
  0000  43 4D 44 5F 53 41 56 45  5F 45 45 50 52 4F 4D 3D |CMD_SAVE_EEPROM=|
  0010  31 00                                            |1.|

[TRAZA] RTO: retransmisiones=1, bytesEnEspera=39
[TEST 3 - TCP HANDSHAKE + COLA FLUJO + RTO] Recuperacion por timeout: PASS

==== TRAMA SALIDA / P4 - ACK=102 (confirma pkt1 -> dispara pkt2 de la cola) ====  total=10B
Hex dump:
  0000  06 50 50 02 01 F5 00 66  1D 03                   |.PP....f..|


==== TRAMA SALIDA / Cable (loopback) ====  total=20B
Hex dump:
  0000  7E 00 61 0C 00 08 06 50  50 02 01 F5 00 66 1D 03 |~.a....PP....f..|
  0010  CE 92 C7 4E                                      |...N|


==== TRAMA SALIDA / P4 - ACK=103 (confirma pkt2) ====  total=10B
Hex dump:
  0000  06 50 50 02 01 F5 00 67  0D 22                   |.PP....g."|


==== TRAMA SALIDA / Cable (loopback) ====  total=20B
Hex dump:
  0000  7E 00 81 0C 00 08 06 50  50 02 01 F5 00 67 0D 22 |~......PP....g."|
  0010  2F 9C C7 4E                                      |/..N|

[TEST 4 - CONEXION SEMIABIERTA] Liberacion automatica por inactividad: PASS
   -> Handshakes:1 | TCP_Tx:2 | RTO_Reenvios:1 | SemiabiertasCerradas:1
[STACK TELEMETRIA] HighWaterMark Inicio: 15320 B | Fin: 14268 B