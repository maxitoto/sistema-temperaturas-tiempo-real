// Archivo: src/tests/SQLite/main.cpp
#include <Arduino.h>
#include "InicializadorSGBD.hpp"
#include "EsquemaFisico.hpp"

void setup() {
    Serial.begin(115200);
    delay(1000);

    iniciarSGBDyORM(); // 1. Inicia la base de datos (SD + SGBD) y el ORM
    iniciarEsquema();  // 2. Ejecuta el modelo físico (diccionario de datos)

    // Limpiamos datos previos de prueba
    db.truncateTable("Tarjeta");
    db.truncateTable("Usuario");

    // Insertamos 2 usuarios
    db.from("Usuario").insert({ {"id", "1"}, {"nombre", "Tomas"},   {"dni", "42123456"} });
    db.from("Usuario").insert({ {"id", "2"}, {"nombre", "Silvana"}, {"dni", "25987654"} });

    // Insertamos tarjetas asociadas a cada usuario mediante id_usuario_fk
    db.from("Tarjeta").insert({
        {"numero", "4509-1111-2222"}, {"fecha_vencimiento", "12/28"}, {"cvv", "842"}, {"id_usuario_fk", "1"}
    });
    db.from("Tarjeta").insert({
        {"numero", "5412-9999-8888"}, {"fecha_vencimiento", "06/29"}, {"cvv", "105"}, {"id_usuario_fk", "2"}
    });

    Serial.println("\n--- CONSULTA JOIN: OBTENER LA TARJETA DE UN USUARIO ---");

    // Forma 1: JOIN automático (detecta id_usuario_fk -> Usuario.id desde el esquema)
    auto tarjetasDeTomas = db.from("Tarjeta")
                             .select("fecha_vencimiento, id_usuario_fk, cvv, Usuario.nombre, Usuario.dni")
                             .join("Usuario")
                             .eq("Usuario.nombre", "Tomas")
                             .execute();

    for (auto& fila : tarjetasDeTomas) {
        Serial.printf("Titular: %s (DNI %s) | Vence: %s | CVV: %s | FK_ID: %s\n",
                      fila["Usuario.nombre"].c_str(),
                      fila["Usuario.dni"].c_str(),
                      fila["fecha_vencimiento"].c_str(),
                      fila["cvv"].c_str(),
                      fila["id_usuario_fk"].c_str());
    }
}

void loop() {}