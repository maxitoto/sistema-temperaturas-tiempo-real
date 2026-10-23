#include <Arduino.h>
#include <unity.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ANCHO_PANTALLA 128
#define ALTO_PANTALLA 64
#define DIRECCION_OLED 0x3C

// Declaramos el objeto display
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

void setUp(void) {
  // Aseguramos que los pines I2C estén configurados antes de cada prueba
  Wire.begin(D3, D2);
}

void tearDown(void) {
  // Limpiamos la pantalla al terminar las pruebas
  display.clearDisplay();
  display.display();
}

// --- PRUEBAS UNITARIAS ---

void test_oled_initialization(void) {
  // Intentamos inicializar la pantalla y reservar la memoria RAM
  bool success = display.begin(SSD1306_SWITCHCAPVCC, DIRECCION_OLED);
  
  // Aserción: Si success es falso, falla la comunicación I2C o no hay RAM.
  TEST_ASSERT_TRUE_MESSAGE(success, "Fallo al iniciar OLED. Revisa direccion 0x3C y cables D1/D2.");
}

void test_oled_dimensions(void) {
  // Validamos que el objeto display se haya construido con las proporciones correctas
  // Esto asegura que la librería no esté configurada por error para una pantalla de 128x32
  TEST_ASSERT_EQUAL_INT16_MESSAGE(128, display.width(), "El ancho de la pantalla no es 128px");
  TEST_ASSERT_EQUAL_INT16_MESSAGE(64, display.height(), "El alto de la pantalla no es 64px");
}

// --- EL ENTORNO DE EJECUCIÓN ---

void setup() {
  delay(2000); // Pausa para abrir el monitor serie
  
  UNITY_BEGIN();
  
  // Ejecutamos las pruebas en orden
  RUN_TEST(test_oled_initialization);
  RUN_TEST(test_oled_dimensions);
  
  UNITY_END();
}

void loop() {
  // Vacío para pruebas unitarias
}