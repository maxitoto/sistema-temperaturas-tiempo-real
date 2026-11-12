/*
 * DEMO COMPLETA - Puertos Serie UART en ESP32
 * 
 * Demuestra TODAS las funciones del API Serial (HardwareSerial).
 * Usa loopback interno en UART1 para no requerir hardware externo.
 * 
 * Pines usados:
 *   Serial  (UART0) → GPIO1 (TX) / GPIO3 (RX)  → Monitor Serie del PC
 *   Serial1 (UART1) → Loopback interno (TX→RX)
 *   Serial2 (UART2) → GPIO16 (RX) / GPIO17 (TX) → Demo de reasignación
 *
 * Monitor Serie: 115200 baud, 8N1, Newline: \n
 */

#include <HardwareSerial.h>

// ============================================================
// CONFIGURACIÓN DE PINES
// ============================================================

// UART1 - pines por defecto (se usarán con loopback)
#define UART1_RX  4   // GPIO10
#define UART1_TX  5   // GPIO11

// UART2 - pines personalizados
#define UART2_RX  16   // GPIO16
#define UART2_TX  17   // GPIO17

// Pines de flow control (solo para demo de setPins)
#define UART1_RTS 32   // GPIO32
#define UART1_CTS 33   // GPIO33

// ============================================================
// SETUP
// ============================================================
void setup() {
  // --- UART0: comunicación con el PC (Monitor Serie) ---
  Serial.begin(115200, SERIAL_8N1);
  Serial.setTimeout(200);  // Timeout de 200ms para readString etc.

  delay(1000);
  Serial.println("\n============================================");
  Serial.println("  DEMO COMPLETA - Serial UART ESP32");
  Serial.println("============================================\n");

  // --- UART1: loopback interno para pruebas ---
  // begin(baud, config, rxPin, txPin, invert, timeout, rxfifo_full_thrhd)
  Serial1.begin(115200, SERIAL_8N1, UART1_RX, UART1_TX, false, 1000, 120);
  
  // Loopback interno: conecta TX1 a RX1 dentro del chip
  uart_internal_loopback(1, UART1_RX);  // (uartNum, rxPin)
  
  Serial.println("[OK] UART1 inicializado con loopback interno");
  Serial.println("     Pines: RX=GPIO10, TX=GPIO11");
  Serial.println("     Config: 115200 baud, 8N1\n");

  // --- UART2: demo de reasignación de pines ---
  Serial2.begin(9600, SERIAL_8N1, UART2_RX, UART2_TX);
  Serial.println("[OK] UART2 inicializado");
  Serial.println("     Pines: RX=GPIO16, TX=GPIO17");
  Serial.println("     Config: 9600 baud, 8N1\n");

  // --- Demo de setPins (sin loopback, solo configuración) ---
  // Serial1.setPins(rxPin, txPin, ctsPin, rtsPin);
  // Serial1.setPins(UART1_RX, UART1_TX, UART1_CTS, UART1_RTS);
  Serial.println("[INFO] setPins() disponible para reasignar pines");
  Serial.println("       Serial1.setPins(rx, tx, cts, rts)\n");

  // --- Demo de Flow Control Hardware ---
  // Serial1.setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS, 64);
  // Modos:
  //   UART_HW_FLOWCTRL_DISABLE (0)
  //   UART_HW_FLOWCTRL_RTS     (1) - RX flow control
  //   UART_HW_FLOWCTRL_CTS     (2) - TX flow control
  //   UART_HW_FLOWCTRL_CTS_RTS (3) - Full flow control
  Serial.println("[INFO] setHwFlowCtrlMode() disponible:");
  Serial.println("       UART_HW_FLOWCTRL_DISABLE / RTS / CTS / CTS_RTS\n");
}

// ============================================================
// DEMO 1: print() y printf()
// ============================================================
void demo1_print_printf() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 1: print() / println() / printf() ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // print() con distintos formatos
  Serial.print("  Entero DEC: "); Serial.println(1234, DEC);
  Serial.print("  Entero HEX: "); Serial.println(1234, HEX);
  Serial.print("  Entero OCT: "); Serial.println(1234, OCT);
  Serial.print("  Entero BIN: "); Serial.println(1234, BIN);
  Serial.print("  Float:      "); Serial.println(3.14159, 4);  // 4 decimales
  Serial.print("  String:     "); Serial.println("Hola mundo");
  Serial.print("  Char:       "); Serial.println('A');
  Serial.print("  Byte:       "); Serial.println((char)255);

  // printf() - formato C
  Serial.printf("\n  printf: %s | %d | %f | 0x%04X\n", 
                "texto", 42, 3.14, 0xABCD);

  // println() agrega \r\n automáticamente
  Serial.println("\n  [println agrega \\r\\n al final]");
}

// ============================================================
// DEMO 2: write() - bytes crudos
// ============================================================
void demo2_write_raw() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 2: write() - Datos binarios crudos ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // write() envía bytes crudos (no formatea)
  Serial.println("  Enviando bytes crudos por UART1 (loopback):");
  
  // Escribir un byte
  Serial1.write(0x41);  // 'A'
  
  // Escribir un array de bytes
  uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
  Serial1.write(data, sizeof(data));
  
  // Escribir con longitud específica
  uint8_t raw[] = {0x00, 0x01, 0x02, 0xFF, 0x00, 0xAB};
  Serial1.write(raw, 3);  // Solo los primeros 3 bytes

  delay(100);
  Serial.println("  Bytes recibidos por loopback:");
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    Serial.printf("    0x%02X", b);
  }
  Serial.println("\n  (0x41 0x48 0x65 0x6C 0x6C 0x6F 0x00 0x01 0x02)");
}

// ============================================================
// DEMO 3: read() y peek()
// ============================================================
void demo3_read_peek() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 3: read() y peek()                 ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // Enviar datos por loopback
  Serial1.print("ABC");
  delay(100);

  Serial.println("  Datos en buffer: \"ABC\"");
  Serial.printf("  available() = %d bytes\n", Serial1.available());

  // peek() - mira sin consumir
  int p = Serial1.peek();
  Serial.printf("  peek() = %c (el byte NO se consume)\n", p);
  Serial.printf("  available() aún = %d\n", Serial1.available());

  // read() - consume el byte
  int r = Serial1.read();
  Serial.printf("  read() = %c (el byte SÍ se consume)\n", r);
  Serial.printf("  available() ahora = %d\n", Serial1.available());

  // Leer todo
  while (Serial1.available()) {
    char c = Serial1.read();
    Serial.print("  read() → "); Serial.println(c);
  }
  Serial.println("  Buffer vacío: available() = 0");
}

// ============================================================
// DEMO 4: readString() y readStringUntil()
// ============================================================
void demo4_readString() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 4: readString() / readStringUntil()║");
  Serial.println("╚══════════════════════════════════════════╝");

  // readString() - lee hasta timeout
  Serial1.print("Hola mundo");
  delay(300);  // Esperar timeout
  String s1 = Serial1.readString();
  Serial.printf("  readString() → \"%s\" (len=%d)\n", s1.c_str(), s1.length());

  // readStringUntil() - lee hasta un carácter terminador
  Serial1.print("Línea1\nLínea2\nLínea3");
  delay(100);
  String s2 = Serial1.readStringUntil('\n');
  Serial.printf("  readStringUntil('\\n') → \"%s\"\n", s2.c_str());
  String s3 = Serial1.readStringUntil('\n');
  Serial.printf("  readStringUntil('\\n') → \"%s\"\n", s3.c_str());
  String s4 = Serial1.readString();  // Resto
  Serial.printf("  readString() (resto)  → \"%s\"\n", s4.c_str());
}

// ============================================================
// DEMO 5: readBytes() y readBytesUntil()
// ============================================================
void demo5_readBytes() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 5: readBytes() / readBytesUntil()  ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // readBytes() - lee exactamente N bytes
  Serial1.print("12345");
  delay(100);
  
  char buf[6];
  int n = Serial1.readBytes(buf, 3);  // Leer solo 3 bytes
  Serial.printf("  readBytes(3) → \"%.*s\" (n=%d)\n", n, buf, n);
  
  // Limpiar resto
  while (Serial1.available()) Serial1.read();

  // readBytesUntil() - lee hasta un carácter
  Serial1.print("HELLO_WORLD");
  delay(100);
  
  char buf2[20];
  int n2 = Serial1.readBytesUntil('_', buf2, 19);
  Serial.printf("  readBytesUntil('_') → \"%.*s\" (n=%d)\n", n2, buf2, n2);
}

// ============================================================
// DEMO 6: find() / findUntil() / parseFloat() / parseInt()
// ============================================================
void demo6_find_parse() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 6: find() / parseFloat() / parseInt()║");
  Serial.println("╚══════════════════════════════════════════╝");

  // find() - busca una cadena
  Serial1.print("xxTEMP=23.5yy");
  delay(100);
  while (Serial1.available()) Serial1.read();  // Limpiar

  Serial1.print("TEMP=23.5");
  delay(100);
  bool found = Serial1.find("TEMP");
  Serial.printf("  find(\"TEMP\") → %s\n", found ? "TRUE" : "FALSE");

  // parseFloat() - busca y parsea un float
  Serial1.print("valor=3.14159");
  delay(100);
  float f = Serial1.parseFloat();
  Serial.printf("  parseFloat() → %.5f\n", f);

  // parseInt() - busca y parsea un entero
  Serial1.print("id=42");
  delay(100);
  int i = Serial1.parseInt();
  Serial.printf("  parseInt() → %d\n", i);

  // findUntil()
  Serial1.print("AAA_BBB_CCC");
  delay(100);
  while (Serial1.available()) Serial1.read();
  Serial1.print("XXX_target_YYY");
  delay(100);
  bool f2 = Serial1.findUntil("target", "_");
  Serial.printf("  findUntil(\"target\", '_') → %s\n", f2 ? "TRUE" : "FALSE");
}

// ============================================================
// DEMO 7: availableForWrite()
// ============================================================
void demo7_availableForWrite() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 7: availableForWrite()             ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial.printf("  availableForWrite() = %d bytes libres en TX buffer\n", 
                Serial1.availableForWrite());
  Serial.println("  (Útil para saber cuántos bytes puedes enviar sin bloquear)");

  // Verificar con un envío masivo
  Serial1.print("Dato de prueba para verificar el buffer de transmisión");
  delay(10);
  Serial.printf("  Tras escribir: availableForWrite() = %d\n", 
                Serial1.availableForWrite());
  while (Serial1.available()) Serial1.read();
}

// ============================================================
// DEMO 8: flush()
// ============================================================
void demo8_flush() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 8: flush()                         ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial.println("  flush() espera a que todo el TX buffer se transmita.");
  Serial.println("  flush(true)  → solo TX");
  Serial.println("  flush(false) → TX + limpia RX buffer");

  // Enviar datos y medir tiempo de flush
  Serial1.print("Datos para flush: 01234567890123456789");
  unsigned long t0 = micros();
  Serial1.flush();  // Espera a que todo salga
  unsigned long t1 = micros();
  Serial.printf("  flush() tomó %lu µs\n", t1 - t0);
  
  while (Serial1.available()) Serial1.read();
}

// ============================================================
// DEMO 9: setTimeout() / getTimeout()
// ============================================================
void demo9_timeout() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 9: setTimeout() / getTimeout()     ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial.printf("  Timeout actual: %lu ms\n", Serial1.getTimeout());
  
  // Cambiar timeout
  Serial1.setTimeout(500);
  Serial.printf("  Timeout nuevo:  %lu ms\n", Serial1.getTimeout());
  
  Serial.println("  (Afecta a readString(), readBytes(), etc.)");
  
  // Restaurar
  Serial1.setTimeout(1000);
}

// ============================================================
// DEMO 10: Múltiples formatos de frame
// ============================================================
void demo10_multiple_formats() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 10: Formatos de frame (paridad)    ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial.println("  Formatos disponibles:");
  Serial.println("    SERIAL_8N1  → 8 datos, No parity, 1 stop (estándar)");
  Serial.println("    SERIAL_8E1  → 8 datos, Even parity, 1 stop");
  Serial.println("    SERIAL_8O1  → 8 datos, Odd parity, 1 stop");
  Serial.println("    SERIAL_7N1  → 7 datos, No parity, 1 stop");
  Serial.println("    SERIAL_7E1  → 7 datos, Even parity, 1 stop");
  Serial.println("    SERIAL_7O1  → 7 datos, Odd parity, 1 stop");
  Serial.println("    SERIAL_6N1  → 6 datos, No parity, 1 stop");
  Serial.println("    SERIAL_6E1  → 6 datos, Even parity, 1 stop");
  Serial.println("    SERIAL_6O1  → 6 datos, Odd parity, 1 stop");
  Serial.println("    SERIAL_5N1  → 5 datos, No parity, 1 stop");
  Serial.println("    SERIAL_5E1  → 5 datos, Even parity, 1 stop");
  Serial.println("    SERIAL_5O1  → 5 datos, Odd parity, 1 stop");
  Serial.println("    SERIAL_5N2  → 5 datos, No parity, 2 stops");
  Serial.println("    SERIAL_6N2  → 6 datos, No parity, 2 stops");
  Serial.println("    SERIAL_7N2  → 7 datos, No parity, 2 stops");
  Serial.println("    SERIAL_8N2  → 8 datos, No parity, 2 stops");
  Serial.println("    SERIAL_8E2  → 8 datos, Even parity, 2 stops");
  Serial.println("    SERIAL_8O2  → 8 datos, Odd parity, 2 stops");
  Serial.println();
  Serial.println("  Ejemplo: Serial1.begin(9600, SERIAL_7E1, 10, 11);");
}

// ============================================================
// DEMO 11: Comunicación binaria / Protocolos
// ============================================================
void demo11_binary() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 11: Protocolo binario (frame)      ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // Ejemplo de frame: [START][ID][DATA][CRC][END]
  uint8_t frame[] = {0xAA, 0x01, 0x42, 0x3C, 0x55};
  Serial.println("  Frame enviado: AA 01 42 3C 55");
  Serial1.write(frame, sizeof(frame));
  delay(100);

  Serial.println("  Frame recibido (loopback):");
  uint8_t rx[10];
  int n = 0;
  while (Serial1.available() && n < 10) {
    rx[n++] = Serial1.read();
  }
  for (int i = 0; i < n; i++) {
    Serial.printf("    %02X", rx[i]);
  }
  Serial.println();
  Serial.printf("  Bytes recibidos: %d\n", n);
}

// ============================================================
// DEMO 12: end() y re-inicialización
// ============================================================
void demo12_end_restart() {
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║  DEMO 12: end() y re-inicialización      ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial.println("  Cerrando UART1...");
  Serial1.end();
  Serial.println("  [OK] UART1 cerrado (end())");
  Serial.println("       available() = 0, no se puede leer/escribir");

  delay(500);

  Serial.println("  Reabriendo UART1 a 57600 baud...");
  Serial1.begin(57600, SERIAL_8N1, UART1_RX, UART1_TX);
  uart_internal_loopback(1, UART1_RX);
  
  Serial1.print("OK");
  delay(100);
  String test = Serial1.readString();
  Serial.printf("  [OK] UART1 reabierto. Test: \"%s\"\n", test.c_str());
  
  // Restaurar a 115200
  Serial1.end();
  Serial1.begin(115200, SERIAL_8N1, UART1_RX, UART1_TX);
  uart_internal_loopback(1, UART1_RX);
  Serial.println("  [OK] UART1 restaurado a 115200 baud\n");
}   


// ============================================================
// LOOP - DEMOSTRACIONES SECUENCIALES
// ============================================================
unsigned long lastDemo = 0;
int demoStep = 0;

void loop() {
  if (millis() - lastDemo < 3000) return;  // Cada 3 segundos
  lastDemo = millis();
  demoStep++;

  switch (demoStep % 12) {
    case 0: demo1_print_printf();    break;
    case 1: demo2_write_raw();       break;
    case 2: demo3_read_peek();       break;
    case 3: demo4_readString();      break;
    case 4: demo5_readBytes();       break;
    case 5: demo6_find_parse();      break;
    case 6: demo7_availableForWrite(); break;
    case 7: demo8_flush();           break;
    case 8: demo9_timeout();         break;
    case 9: demo10_multiple_formats(); break;
    case 10: demo11_binary();        break;
    case 11: demo12_end_restart();   break;
  }
}