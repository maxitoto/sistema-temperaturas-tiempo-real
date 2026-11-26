```Printext
╔══════════════════════════════════════════════════════╗
║  BENCHMARK — Serialización en ESP32                 ║
╚══════════════════════════════════════════════════════╝
  CPU: 240 MHz
  Heap libre inicial: 330184 bytes

  Datos de prueba:
    temp=23.50  hum=65.20  co2=412  id=1
    ts=1699999999  ok=1  sensor="Temperature"

  Iteraciones por benchmark: 100

  Valores absolutos:
╔═══════════╦════════════╦═══════════╦════════════╦══════════════╦═══════════╗
║ Formato   ║ Tamaño (B) ║ Ser (µs)  ║ Deser (µs) ║ RAM pico (B) ║ UART (ms) ║
╠═══════════╬════════════╬═══════════╬════════════╬══════════════╬═══════════╣
║ JSON      ║         90 ║       128 ║        196 ║         1192 ║         7 ║
║ MsgPack   ║         62 ║        85 ║        128 ║         1192 ║         5 ║
║ CBOR      ║         62 ║        23 ║         67 ║            0 ║         5 ║
║ TLV       ║         35 ║         2 ║          2 ║            0 ║         3 ║
╚═══════════╩════════════╩═══════════╩════════════╩══════════════╩═══════════╝

  Comparación relativa a JSON (JSON = 100%):
╔═══════════╦════════╦════════╦══════════╦════════════╦════════╗
║ Formato   ║ Tamaño ║ Ser    ║ Deser    ║ RAM pico   ║ UART   ║
╠═══════════╬════════╬════════╬══════════╬════════════╬════════╣
║ JSON      ║   100% ║   100% ║     100% ║       100% ║   100% ║
║ MsgPack   ║    69% ║    66% ║      65% ║       100% ║    71% ║
║ CBOR      ║    69% ║    18% ║      34% ║         0% ║    71% ║
║ TLV       ║    39% ║     2% ║       1% ║         0% ║    43% ║
╚═══════════╩════════╩════════╩══════════╩════════════╩════════╝

  Resumen (mejor por métrica):
    Más compacto:       TLV       (35 B)
    Ser más rápida:     TLV       (2 µs)
    Deser más rápida:   TLV       (2 µs)
    Menor RAM pico:     CBOR      (0 B)

  Verificación (deserializar y comparar):
    JSON     : temp=23.50 hum=65.20 co2=412 id=1 ts=1699999999 ok=1 sensor=Temperature
    MsgPack  : temp=23.50 hum=65.20 co2=412 id=1 ts=1699999999 ok=1 sensor=Temperature
    CBOR     : temp=23.50 hum=65.20 co2=412 id=1 ts=1699999999 ok=1 sensor=Temperature
    TLV      : temp=23.50 hum=65.20 co2=412 id=1 ts=1699999999 ok=1 sensor=Temperature

  Dump hex de cada formato:
  JSON      (90 B): 7B 22 74 65 6D 70 22 3A 32 33 2E 35 2C 22 68 75 6D 22 3A 36 35 2E 32 2C 22 63 6F 32 22 3A 34 31 32 2C 22 69 64 22 3A 31 2C 22 74 73 22 3A 31 36 39 39 39 39 39 39 39 39 2C 22 6F 6B 22 3A 74 72 ...
  MsgPack   (62 B): 87 A4 74 65 6D 70 CA 41 BC 00 00 A3 68 75 6D CA 42 82 66 66 A3 63 6F 32 CD 01 9C A2 69 64 01 A2 74 73 CE 65 53 F0 FF A2 6F 6B C3 A6 73 65 6E 73 6F 72 AB 54 65 6D 70 65 72 61 74 75 72 65 
  CBOR      (62 B): A7 64 74 65 6D 70 FA 41 BC 00 00 63 68 75 6D FA 42 82 66 66 63 63 6F 32 19 01 9C 62 69 64 01 62 74 73 1A 65 53 F0 FF 62 6F 6B F5 66 73 65 6E 73 6F 72 6B 54 65 6D 70 65 72 61 74 75 72 65 
  TLV       (35 B): 01 00 00 BC 41 02 66 66 82 42 03 01 9C 04 01 05 FF F0 53 65 06 01 07 0B 54 65 6D 70 65 72 61 74 75 72 65 

  [FIN]
```