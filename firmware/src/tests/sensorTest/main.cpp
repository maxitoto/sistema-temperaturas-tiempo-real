#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin de datos del bus OneWire (cambialo según tu conexión)
#define ONE_WIRE_BUS 4  

// Instancias de OneWire y DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  Serial.println("Test de sensores DS18B20 - Dallas");

  sensors.begin();

  // Muestra cuántos dispositivos se detectaron
  Serial.print("Dispositivos encontrados en el bus: ");
  Serial.println(sensors.getDeviceCount());
}

void loop() {
  // Solicita la conversión de temperatura a todos los sensores
  sensors.requestTemperatures();

  int deviceCount = sensors.getDeviceCount();

  if (deviceCount == 0) {
    Serial.println("No hay sensores conectados o falla el bus.");
    delay(2000);
    return;
  }

  // Itera e imprime la temperatura de cada sensor
  for (int i = 0; i < deviceCount; i++) {
    float tempC = sensors.getTempCByIndex(i);

    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(": ");

    if (tempC != DEVICE_DISCONNECTED_C) {
      Serial.print(tempC, 2); // 2 decimales
      Serial.println(" °C");
    } else {
      Serial.println("Error de lectura (dispositivo desconectado)");
    }
  }

  Serial.println("---");
  delay(2000); // Espera 2 segundos antes de la siguiente lectura
}