#include <Arduino.h>
#include <unity.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_SENSORES D4

// Declaramos los objetos para el bus y los sensores
OneWire oneWire(PIN_SENSORES);
DallasTemperature sensors(&oneWire);

void setUp(void) {
  // Inicializamos el bus antes de cada prueba
  sensors.begin();
}

void tearDown(void) {
  // No se requiere limpieza de hardware para el bus OneWire
}

// --- PRUEBAS UNITARIAS ---

void test_deteccion_bus_onewire(void) {
  // Validamos que físicamente haya sensores respondiendo en el pin D4
  int cantidad = sensors.getDeviceCount();
  
  // Aserción: Si es 0, el puente con la resistencia de 4.7k falló o no hay sensores conectados.
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, cantidad, "Fallo: No se detectaron sensores en D4. Revisa resistencia de 4.7k.");
}

void test_lectura_temperatura_valida(void) {
  // Solicitamos a todos los sensores que preparen sus datos
  sensors.requestTemperatures();
  int cantidad = sensors.getDeviceCount();
  
  if (cantidad > 0) {
    // Verificamos que NINGÚN sensor conectado arroje error de lectura
    for (int i = 0; i < cantidad; i++) {
      float tempC = sensors.getTempCByIndex(i);
      
      // Aserción 1: El sensor responde con algo distinto al código de error estándar (-127.00)
      TEST_ASSERT_NOT_EQUAL_FLOAT_MESSAGE(DEVICE_DISCONNECTED_C, tempC, "Fallo: Un sensor se desconecto durante la lectura.");
      
      // Aserción 2: La temperatura leída tiene coherencia física (-50C a 80C)
      TEST_ASSERT_TRUE_MESSAGE(tempC > -50.0 && tempC < 80.0, "Fallo: Temperatura fuera de rango fisico logico.");
    }
  } else {
    // Fallo directo si no hay sensores para evitar falsos positivos
    TEST_FAIL_MESSAGE("Lectura omitida: No hay sensores detectados en el bus.");
  }
}

// --- EL ENTORNO DE EJECUCIÓN ---

void setup() {
  delay(2000); 
  
  UNITY_BEGIN();
  
  RUN_TEST(test_deteccion_bus_onewire);
  RUN_TEST(test_lectura_temperatura_valida);
  
  UNITY_END();
}

void loop() {
  // Vacío para pruebas unitarias
}