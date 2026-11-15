#include <Arduino.h>
#include "hardware/driver/tarjetasd/TarjetaSD.hpp"

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("Inicializando SD...");
    if (!tarjetaSD.estaInicializada()) {
        Serial.println("Error: no se pudo inicializar la SD.");
        while (1);
    }

    Serial.println("Borrando todo el contenido...");
    if (tarjetaSD.borrarTodo()) {
        Serial.println("SD vaciada correctamente.");
    } else {
        Serial.println("Error: no se pudo borrar todo.");
    }
}

void loop() {}   