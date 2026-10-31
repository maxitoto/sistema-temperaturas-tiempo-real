#include <Arduino.h>
#include "TarjetaSD.hpp"

void setup() {
    Serial.begin(115200);

    TarjetaSD& sd = TarjetaSD::getInstance();

    if (!sd.estaInicializada()) {
        Serial.println("Error al inicializar microSD");
        return;
    }

    sd.escribirArchivo("/temperaturas.txt", "25.6\n");

    sd.anexarArchivo("/temperaturas.txt", "26.1\n");

    String contenido;

    if (sd.leerArchivo("/temperaturas.txt", contenido)) {
        Serial.println(contenido);
    }

    Serial.printf(
        "Espacio total: %llu bytes\n",
        sd.espacioTotal()
    );

    Serial.printf(
        "Espacio usado: %llu bytes\n",
        sd.espacioUsado()
    );
}

void loop() {}