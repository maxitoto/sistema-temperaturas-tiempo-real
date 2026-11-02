==================================================================
 VERIFICACION CAPA 1 (FISICA) - CAJA NEGRA Y CAJA BLANCA
==================================================================

==== TRAMA SALIDA / Fisica TX -> BT_SPP ====  total=4B
Hex dump:
  0000  AA BB CC DD                                      |....|


==== TRAMA SALIDA / Fisica TX <- BT_SPP (capturado) ====  total=4B
Hex dump:
  0000  AA BB CC DD                                      |....|

[CAJA NEGRA 1] Enrutamiento exclusivo a Bluetooth (Sin Broadcast): PASS
   -> Bytes en BT: 4 | Fuga en USB: 0 | Fuga en WiFi: 0

==== TRAMA ENTRADA / Fisica RX <- WiFi_UDP ====  total=5B
Hex dump:
  0000  7E 01 02 03 04                                   |~....|

[CAJA NEGRA 2] Subida RX aislada desde raiz WiFi: PASS
[CAJA BLANCA] Auditoria de contadores y banderas de elevacion: PASS
   -> [USB]  Tipo:1 | Crudo:SI | TX:0B | RX:0B
   -> [WIFI] Tipo:3 | Crudo:SI | TX:0B | RX:5B
   -> [BT]   Tipo:6 | Crudo:NO (Elevado) | TX:4B | RX:0B