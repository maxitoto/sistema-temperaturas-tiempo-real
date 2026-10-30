#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "HolaMundo.hpp"
#include "funcion.hpp"

#include <memory>

// 1. Objeto global (pila, vive todo el programa)
HolaMundo holaGlobal;

void setup() {
  Serial.begin(115200);

  // ─── 1. Objeto en pila (stack) ───────────────────────────
  HolaMundo holaStack;
  holaStack.holaMundo();

  // ─── 2. Referencia al objeto ─────────────────────────────
  HolaMundo& ref = holaStack;
  ref.holaMundo();

  // ─── 3. Puntero a objeto en pila ─────────────────────────
  HolaMundo* ptr = &holaStack;
  ptr->holaMundo();

  // ─── 4. Puntero de función miembro ───────────────────────
  void (HolaMundo::*pMetodo)() = &HolaMundo::holaMundo;
  (holaStack.*pMetodo)();

  // ─── 5. new / delete (heap) ──────────────────────────────
  HolaMundo* holaHeap = new HolaMundo();
  holaHeap->holaMundo();
  delete holaHeap;

  // ─── 6. Objeto global (declarado arriba) ─────────────────
  holaGlobal.holaMundo();

  // ─── 7. auto con new ─────────────────────────────────────
  auto holaAuto = new HolaMundo();
  holaAuto->holaMundo();
  delete holaAuto;

  // ─── 8. unique_ptr (smart pointer, C++11) ────────────────
  std::unique_ptr<HolaMundo> holaSmart = std::make_unique<HolaMundo>();
  holaSmart->holaMundo();
  holaSmart->adiosMundo();

  // ─── 9. Método estático (requiere static en la clase) ────
  // HolaMundo::holaMundo();  // ← solo si lo declaras static

  // ─── 10. Función libre que envuelve la llamada ───────────
  fnHolaMundo();

  delay(250);
  vTaskDelete(nullptr);
}

void loop() {
  // Inaccesible después de eliminar esta tarea.
}   