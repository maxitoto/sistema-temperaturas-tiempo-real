#include <Arduino.h>
#include <unity.h>
#include <SPI.h>
#include <SD.h>

// Configuración definitiva validada por hardware
const int PIN_CS = D8;
const char* ARCHIVO_PRUEBA = "/test_3v3.txt";

void setUp(void) {
  // Se ejecuta antes de cada test. Aquí no necesitamos nada especial.
}

void tearDown(void) {
  // Se ejecuta después de cada test.
}

// 1. Prueba de comunicación base
void test_sd_initialization(void) {
  bool init_success = SD.begin(PIN_CS);
  TEST_ASSERT_TRUE_MESSAGE(init_success, "Fallo al iniciar SD en D8 a 3.3V. Revisa formato FAT32.");
}

// 2. Prueba de escritura suave (Sin estresar el regulador de voltaje)
void test_sd_gentle_write(void) {
  // Si quedó un archivo de una prueba anterior, lo borramos para empezar limpio
  if (SD.exists(ARCHIVO_PRUEBA)) {
    SD.remove(ARCHIVO_PRUEBA);
  }

  File archivo = SD.open(ARCHIVO_PRUEBA, FILE_WRITE);
  TEST_ASSERT_TRUE_MESSAGE(archivo, "No se pudo crear el archivo de prueba.");

  // Escribimos una sola línea de datos
  int bytesEscritos = archivo.println("Prueba de hardware a 3.3V superada. [OK]");
  archivo.close();

  // Verificamos que físicamente se haya transferido información
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, bytesEscritos, "El archivo se creo pero no se pudo escribir en el.");
}

// 3. Prueba de lectura (Confirmación de no corrupción)
void test_sd_read_verification(void) {
  // Validamos que el archivo realmente exista en la memoria
  TEST_ASSERT_TRUE_MESSAGE(SD.exists(ARCHIVO_PRUEBA), "El archivo de prueba desaparecio.");

  File archivo = SD.open(ARCHIVO_PRUEBA, FILE_READ);
  TEST_ASSERT_TRUE_MESSAGE(archivo, "No se pudo abrir el archivo para lectura.");

  // Leemos la primera línea
  String contenido = archivo.readStringUntil('\n');
  archivo.close();

  // Buscamos la etiqueta [OK] para confirmar que el dato está intacto
  TEST_ASSERT_TRUE_MESSAGE(contenido.indexOf("[OK]") > 0, "El contenido leido esta corrupto o no coincide.");
}

// --- EL ENTORNO DE EJECUCIÓN ---
void setup() {
  // Pausa vital para abrir el monitor serie sin perder el primer mensaje
  delay(2000); 
  
  UNITY_BEGIN();
  
  RUN_TEST(test_sd_initialization);
  RUN_TEST(test_sd_gentle_write);
  RUN_TEST(test_sd_read_verification);
  
  UNITY_END();
}

void loop() {
  // Bucle vacío para pruebas unitarias
}