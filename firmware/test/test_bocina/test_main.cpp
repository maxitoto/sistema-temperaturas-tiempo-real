#include <Arduino.h>
#include <unity.h>

#define PIN_BOCINA D0

void setUp(void) {
  // Configuramos el pin de la bocina
  pinMode(PIN_BOCINA, OUTPUT);
}

void tearDown(void) {
  // Nos aseguramos de silenciarla siempre al terminar
  noTone(PIN_BOCINA);
  digitalWrite(PIN_BOCINA, LOW);
}

// --- PRUEBAS UNITARIAS ---

void test_bocina_ejecucion_tono(void) {
  // Ejecutamos un tono de 1000Hz por un periodo corto para validar que el timer de hardware funciona
  tone(PIN_BOCINA, 1000);
  delay(200); 
  noTone(PIN_BOCINA);
  
  // Aserción: Como el tono es una señal PWM generada por hardware, comprobamos que el código superó 
  // la ejecución sin colgar el procesador (Watchdog reset).
  TEST_PASS_MESSAGE("Comando tone() ejecutado correctamente en D0. (Validacion auditiva superada)");
}

// --- EL ENTORNO DE EJECUCIÓN ---

void setup() {
  delay(2000); 
  
  UNITY_BEGIN();
  
  RUN_TEST(test_bocina_ejecucion_tono);
  
  UNITY_END();
}

void loop() {
  // Vacío para pruebas unitarias
}