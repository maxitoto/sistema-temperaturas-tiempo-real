#include <WiFi.h>
#include <WiFiClientSecure.h>

// --- Configuración de tu red WiFi ---
const char* ssid     = "Zhone_96F8";
const char* password = "Ushuaia30";

// --- Servidor HTTPS de destino ---
const char* host = "httpbin.org";
const int httpsPort = 443;

// --- Variable para simular el botón ---
int botonPresionado = 0;

// --- Función que se ejecuta al "presionar" el botón ---
void hacerPeticionHTTPS() {
  // Creamos el cliente seguro
  WiFiClientSecure client;
  
  // --- MODO INSEGURO (solo para pruebas) ---
  // No valida el certificado del servidor. Sirve para ejemplos rápidos.
  // En producción, usa client.setCACert(root_ca);
  client.setInsecure(); 

  Serial.printf("Conectando a %s:%d ...\n", host, httpsPort);

  if (!client.connect(host, httpsPort)) {
    Serial.println("❌ Falló la conexión");
    return;
  }
  Serial.println("✅ Conectado por HTTPS");

  // Construimos la petición GET
  String url = "/get?from=ESP32&mensaje=Hola";
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Connection: close\r\n\r\n");

  // Leemos la respuesta
  Serial.println("--- Respuesta del servidor ---");
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  Serial.println("--- Fin de la respuesta ---");
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando...");

  // 1. Conexión a WiFi (WPA2 seguro)
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado a WiFi");
}

void loop() {
  // --- SIMULACIÓN DEL BOTÓN ---
  // Cada 5 segundos, "presionamos" el botón automáticamente
  if (millis() - botonPresionado > 5000) {
    botonPresionado = millis();
    Serial.println("\n--- BOTÓN PRESIONADO (simulado) ---");
    hacerPeticionHTTPS();
  }
}