#pragma once

#if defined(ESP32)
  // --- PINES PARA WOKWI (ESP32) ---
  const int PIN_SENSOR_TEMP = 4;
  const int PIN_BOCINA      = 26;
  const int PIN_LED         = 25;
  const int PIN_CS_SD       = 5;
  const int PIN_I2C_SDA     = 21;
  const int PIN_I2C_SCL     = 22;

#else
  // --- PINES PARA HARDWARE REAL (ESP8266) ---
  const int PIN_SENSOR_TEMP = D4;
  const int PIN_BOCINA      = D0;
  const int PIN_LED         = D8;
  const int PIN_CS_SD       = D1;
  const int PIN_I2C_SDA     = D3;
  const int PIN_I2C_SCL     = D2;
#endif