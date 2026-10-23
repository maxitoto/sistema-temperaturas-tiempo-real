#include <Arduino.h>
#include <unity.h>

#define PIN_LED D8

void setUp(void) {
  // Aseguramos que el pin esté configurado como salida antes de cada prueba
  pinMode(PIN_LED, OUTPUT);
}

void tearDown(void) {
  // Apagamos el LED al terminar cada prueba para que no quede encendido
  digitalWrite(PIN_LED, LOW);
}

// --- PRUEBAS UNITARIAS ---

void test_led_encendido(void) {
  // Ordenamos encender
  digitalWrite(PIN_LED, HIGH);
  
  // Aserción: Leemos el estado del pin. Si no es HIGH, el hardware no retuvo la orden.
  TEST_ASSERT_EQUAL_INT_MESSAGE(HIGH, digitalRead(PIN_LED), "Fallo: El pin D8 no retiene el estado HIGH.");
}

void test_led_apagado(void) {
  // Ordenamos apagar
  digitalWrite(PIN_LED, LOW);
  
  // Aserción: Validamos que realmente haya bajado el voltaje a LOW.
  TEST_ASSERT_EQUAL_INT_MESSAGE(LOW, digitalRead(PIN_LED), "Fallo: El pin D8 no retiene el estado LOW.");
}

// --- EL ENTORNO DE EJECUCIÓN ---

void setup() {
  delay(2000); 
  
  UNITY_BEGIN();
  
  RUN_TEST(test_led_encendido);
  RUN_TEST(test_led_apagado);
  
  UNITY_END();
}

void loop() {
  // Vacío para pruebas unitarias
}