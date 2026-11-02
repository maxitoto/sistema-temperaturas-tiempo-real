======================================================================
 VERIFICACION CAPA 2 (ENLACE) - CAJA NEGRA, CAJA BLANCA Y DOS NODOS
======================================================================

==== TRAMA SALIDA / Enlace TX -> BT (payload de capa superior) ====  vista=[20..25]  total=26B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x0013  ( 20 bytes)
  [Vista actual (capa superior)]  0x0014..0x0019  (  6 bytes)

Hex dump:
  0000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
  0010  00 00 00 00 7E 7D FF 10  20 30                   |....~}.. 0|


==== TRAMA SALIDA / Cable BT (passthrough, sin HDLC) ====  total=6B
Hex dump:
  0000  7E 7D FF 10 20 30                                |~}.. 0|


==== TRAMA ENTRADA / Enlace RX <- BT (passthrough) ====  total=6B
Hex dump:
  0000  7E 7D FF 10 20 30                                |~}.. 0|

[TEST 1 - PASSTHROUGH BT] Pasa directo sin alterar bytes: PASS

==== TRAMA SALIDA / Enlace TX -> USB (150B de payload) ====  vista=[20..169]  total=170B
Regiones:
  [Encabezados (ya consumidos)]  0x0000..0x0013  ( 20 bytes)
  [Vista actual (capa superior)]  0x0014..0x00A9  (150 bytes)

Hex dump:
  0000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
  0010  00 00 00 00 7E 7D FF 03  04 05 06 07 08 09 0A 0B |....~}..........|
  0020  0C 0D 0E 7E 7D FF 12 13  14 15 16 17 18 19 1A 1B |...~}...........|
  0030  1C 1D 7E 7D FF 21 22 23  24 25 26 27 28 29 2A 2B |..~}.!"#$%&'()*+|
  0040  2C 7E 7D FF 30 31 32 33  34 35 36 37 38 39 3A 3B |,~}.0123456789:;|
  0050  7E 7D FF 3F 40 41 42 43  44 45 46 47 48 49 4A 7E |~}.?@ABCDEFGHIJ~|
  0060  7D FF 4E 4F 50 51 52 53  54 55 56 57 58 59 7E 7D |}.NOPQRSTUVWXY~}|
  0070  FF 5D 5E 5F 60 61 62 63  64 65 66 67 68 7E 7D FF |.]^_`abcdefgh~}.|
  0080  6C 6D 6E 6F 70 71 72 73  74 75 76 77 7E 7D FF 7B |lmnopqrstuvw~}.{|
  0090  7C 7D 7E 7F 80 81 82 83  84 85 86 7E 7D FF 8A 8B ||}~........~}...|
  00A0  8C 8D 8E 8F 90 91 92 93  94 95                   |..........|


==== TRAMA SALIDA / Cable USB (3 chunks HDLC + escaping 0x7E/0x7D/0xFF) ====  total=206B
Hex dump:
  0000  7E 00 13 40 7D 5E 7D 5D  7D DF 03 04 05 06 07 08 |~..@}^}]}.......|
  0010  09 0A 0B 0C 0D 0E 7D 5E  7D 5D 7D DF 12 13 14 15 |......}^}]}.....|
  0020  16 17 18 19 1A 1B 1C 1D  7D 5E 7D 5D 7D DF 21 22 |........}^}]}.!"|
  0030  23 24 25 26 27 28 29 2A  2B 2C 7D 5E 7D 5D 7D DF |#$%&'()*+,}^}]}.|
  0040  30 31 32 33 34 35 36 37  38 39 3A 3B 7D 5E 7D 5D |0123456789:;}^}]|
  0050  7D DF 3F 5B 4C 00 00 7E  01 13 40 40 41 42 43 44 |}.?[L..~..@@ABCD|
  0060  45 46 47 48 49 4A 7D 5E  7D 5D 7D DF 4E 4F 50 51 |EFGHIJ}^}]}.NOPQ|
  0070  52 53 54 55 56 57 58 59  7D 5E 7D 5D 7D DF 5D 5E |RSTUVWXY}^}]}.]^|
  0080  5F 60 61 62 63 64 65 66  67 68 7D 5E 7D 5D 7D DF |_`abcdefgh}^}]}.|
  0090  6C 6D 6E 6F 70 71 72 73  74 75 76 77 7D 5E 7D 5D |lmnopqrstuvw}^}]|
  00A0  7D DF 7B 7C 7D 5D 7D 5E  7F 63 D4 00 00 7E 02 13 |}.{|}]}^.c...~..|
  00B0  16 80 81 82 83 84 85 86  7D 5E 7D 5D 7D DF 8A 8B |........}^}]}...|
  00C0  8C 8D 8E 8F 90 91 92 93  94 95 A5 92 BA 82       |..............|


==== TRAMA ENTRADA / Enlace RX <- USB (150B reensamblados) ====  total=150B
Hex dump:
  0000  7E 7D FF 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 7E |~}.............~|
  0010  7D FF 12 13 14 15 16 17  18 19 1A 1B 1C 1D 7E 7D |}.............~}|
  0020  FF 21 22 23 24 25 26 27  28 29 2A 2B 2C 7E 7D FF |.!"#$%&'()*+,~}.|
  0030  30 31 32 33 34 35 36 37  38 39 3A 3B 7E 7D FF 3F |0123456789:;~}.?|
  0040  40 41 42 43 44 45 46 47  48 49 4A 7E 7D FF 4E 4F |@ABCDEFGHIJ~}.NO|
  0050  50 51 52 53 54 55 56 57  58 59 7E 7D FF 5D 5E 5F |PQRSTUVWXY~}.]^_|
  0060  60 61 62 63 64 65 66 67  68 7E 7D FF 6C 6D 6E 6F |`abcdefgh~}.lmno|
  0070  70 71 72 73 74 75 76 77  7E 7D FF 7B 7C 7D 7E 7F |pqrstuvw~}.{|}~.|
  0080  80 81 82 83 84 85 86 7E  7D FF 8A 8B 8C 8D 8E 8F |.......~}.......|
  0090  90 91 92 93 94 95                                |......|

[TEST 2 - FRAGMENTACION + HDLC + CRC16] 150B -> 3 Chunks (206B en cable): PASS

==== TRAMA SALIDA / Cable USB (frame original intacto) ====  total=31B
Hex dump:
  0000  7E 00 21 17 55 4D 42 52  41 4C 5F 48 45 4C 41 44 |~.!.UMBRAL_HELAD|
  0010  45 52 41 3D 2D 31 38 2E  35 43 00 9F 6A DE 14    |ERA=-18.5C..j..|


==== TRAMA SALIDA / Cable USB (frame corrompido: byte[6]^=0xFF) ====  total=31B
Hex dump:
  0000  7E 00 21 17 55 4D BD 52  41 4C 5F 48 45 4C 41 44 |~.!.UM.RAL_HELAD|
  0010  45 52 41 3D 2D 31 38 2E  35 43 00 9F 6A DE 14    |ERA=-18.5C..j..|


==== TRAMA SALIDA / NACK numerado emitido por el receptor ====  total=8B
Hex dump:
  0000  7E 82 21 00 AC 71 00 00                          |~.!..q..|


==== TRAMA SALIDA / Reenvio ARQ (frame original reinyectado) ====  total=31B
Hex dump:
  0000  7E 00 21 17 55 4D 42 52  41 4C 5F 48 45 4C 41 44 |~.!.UMBRAL_HELAD|
  0010  45 52 41 3D 2D 31 38 2E  35 43 00 9F 6A DE 14    |ERA=-18.5C..j..|


==== TRAMA ENTRADA / Enlace RX <- payload recuperado ====  total=23B
Hex dump:
  0000  55 4D 42 52 41 4C 5F 48  45 4C 41 44 45 52 41 3D |UMBRAL_HELADERA=|
  0010  2D 31 38 2E 35 43 00                             |-18.5C.|

[TEST 3 - CORRUPCION CRC + NACK + REENVIO ARQ] Recuperacion automatica: PASS
   -> CRC1_Err:1 | NACK_Tx:1 | NACK_Rx:1 | ReintentosTx:1 | RxFinalOK:1
[TEST 4 - CORRUPCION CHUNK 1 DE 5] Unico NACK sin tormenta: PASS (NACKs=1, SeqErr=0)
[TEST 5 - PERDIDA ACK + TIMEOUT 250MS + DUPLICADO] PASS (Reintentos=1, Duplicados=1)
[TEST 6 - REINICIO DEL EMISOR (HANDSHAKE SYNC)] PASS
[TEST 7 - 2 NODOS S1: ACK RETRASADO (>250MS)] PASS (EsperandoAck=1, MSG1=1, MSG2=1)
[TEST 8 - 2 NODOS S2: BOOT-LOOP 2 REINICIOS SEGUIDOS] PASS (UNO=1, DOS=1, TRES=1)
[TEST 9 - 2 NODOS S2b: SYNC PERDIDO EN EL CABLE] PASS (DOS=1)
[TEST 10 - 2 NODOS S3: ACK PERDIDO + RETRANSMISION CORRUPTA] PASS (Entregas=1, DupDescartadas=1)
[STACK TELEMETRIA] HighWaterMark Inicio: 15312 B | Fin: 14132 B