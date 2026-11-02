======================================================================
 VERIFICACION CAPA 3 (RED) - CAJA NEGRA Y CAJA BLANCA
======================================================================

==== TRAMA SALIDA / Red TX -> BT (payload; sin cabecera de red) ====  vista=[2..5]  total=6B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x0001  (  2 bytes)
  [Vista actual (capa superior)]  0x0002..0x0005  (  4 bytes)

Hex dump:
  0000  00 00 11 22 33 44                                |..."3D|


==== TRAMA SALIDA / Cable BT (passthrough, 0B de cabecera Red) ====  total=4B
Hex dump:
  0000  11 22 33 44                                      |."3D|


==== TRAMA ENTRADA / Red RX <- BT (passthrough, sin cabecera) ====  total=4B
Hex dump:
  0000  11 22 33 44                                      |."3D|

[TEST 1 - PASSTHROUGH BT] Pasa de largo sin cabecera de Red: PASS

==== TRAMA SALIDA / Red TX -> USB (payload de capa superior) ====  vista=[2..13]  total=14B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x0001  (  2 bytes)
  [Vista actual (capa superior)]  0x0002..0x000D  ( 12 bytes)

Hex dump:
  0000  00 00 50 49 4E 47 5F 43  41 50 41 5F 33 00       |..PING_CAPA_3.|


==== TRAMA SALIDA / Cable USB (HDLC enlace + cabecera Red 2B + payload) ====  total=22B
Hex dump:
  0000  7E 00 11 0E 00 08 50 49  4E 47 5F 43 41 50 41 5F |~.....PING_CAPA_|
  0010  33 00 D0 73 C4 F5                                |3..s..|


==== TRAMA ENTRADA / Red RX <- USB (payload tras consumir Red 2B) ====  vista=[2..13]  total=14B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x0001  (  2 bytes)
  [Vista actual (capa superior)]  0x0002..0x000D  ( 12 bytes)

Hex dump:
  0000  00 08 50 49 4E 47 5F 43  41 50 41 5F 33 00       |..PING_CAPA_3.|

[TEST 2 - RED PROPIA USB] Cabecera 2B + Direccionamiento + TTL=8: PASS

==== TRAMA SALIDA / Red TX (transito: org=0x2 -> dst=0x5, TTL=4) ====  total=23B
Regiones:
  [Cab Red (org=2 | dst=5) | TTL=4]  0x0000..0x0001  (  2 bytes)
  [Payload: RUTEO_ENTRE_SUBREDES]  0x0002..0x0016  ( 21 bytes)

Hex dump:
  0000  25 04 52 55 54 45 4F 5F  45 4E 54 52 45 5F 53 55 |%.RUTEO_ENTRE_SU|
  0010  42 52 45 44 45 53 00                             |BREDES.|


==== TRAMA SALIDA / Cable USB (HDLC + Red hdr + payload) ====  total=31B
Hex dump:
  0000  7E 00 21 17 25 04 52 55  54 45 4F 5F 45 4E 54 52 |~.!.%.RUTEO_ENTR|
  0010  45 5F 53 55 42 52 45 44  45 53 00 70 A9 31 D7    |E_SUBREDES.p.1.|


[Red] No entregado localmente -> REENVIO activado


==== TRAMA SALIDA / Cable WiFi (reenvio de la capa Red, TTL 4 -> 3) ====  total=31B
Hex dump:
  0000  7E 00 11 17 25 03 52 55  54 45 4F 5F 45 4E 54 52 |~...%.RUTEO_ENTR|
  0010  45 5F 53 55 42 52 45 44  45 53 00 4D CC 20 FE    |E_SUBREDES.M. .|


==== TRAMA ENTRADA / Enlace WiFi RX (Red hdr + payload; aun sin consumir) ====  total=23B
Hex dump:
  0000  25 03 52 55 54 45 4F 5F  45 4E 54 52 45 5F 53 55 |%.RUTEO_ENTRE_SU|
  0010  42 52 45 44 45 53 00                             |BREDES.|

[TEST 3 - ENCAMINAMIENTO USB->WIFI] Reenvio automatico + Salto TTL (4->3): PASS
[TEST 4 - TTL=0 Y CONTROL DE CONGESTION] Filtro de inundacion: PASS
   -> PassthroughRx:1 | RedPropiaRx:1 | Reenviados:1 | TTL_Err:1 | CongestionDrop:1
[STACK TELEMETRIA] HighWaterMark Inicio: 15224 B | Fin: 14284 B