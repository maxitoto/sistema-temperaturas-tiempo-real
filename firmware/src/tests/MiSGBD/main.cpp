#include <Arduino.h>
#include "../hardware/tarjetasd/TarjetaSD.hpp"
#include "../hardware/tarjetasd/TarjetaSD.cpp"
#include "CriptoUtils.hpp"
#include "ORM.hpp"

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!TarjetaSD::getInstance().estaInicializada()) {
        Serial.println("Error al montar MicroSD");
        return;
    }

    ORM& db = ORM::getInstance();
    db.setSession("silvana_admin", ROL_ADMIN);

    db.truncateTable("Auto");
    db.truncateTable("Usuario");

    Serial.println("\n--- 1. PRUEBA DE ERROR EN TIEMPO DE DESARROLLO (ATRIBUTO DUPLICADO) ---");
    db.createTable("TablaFallida")
      .addAtribute("id").typeInt().pk()
      .addAtribute("nombre").notNull().typeSrt().valueDefault("")
      .addAtribute("nombre").typeSrt().valueDefault("Esto produce un error!");

    Serial.println("\n--- 2. CREACION DEL ESQUEMA FISICO (AUTO Y USUARIO CON FK CASCADE + CHECK) ---");
    db.createTable("Auto")
      .addAtribute("id").typeInt().pk()
      .addAtribute("modelo").typeSrt().notNull();

    db.createTable("Usuario")
      .addAtribute("id").typeInt().pk()
      .addAtribute("nombre").notNull().typeSrt().valueDefault("")
      .addAtribute("dni").typeSrt().unique().notNull().varChar(8)
          .check([](const String& dni) { return dni.length() >= 4; }, "dni.size() >= 4")
      .addAtribute("color_ojos").typeSrt().valueDefault("Marron")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0")
      .addAtribute("Auto").typeInt().FK("Auto", "id", CASCADE);

    db.createIndex("UsuarioIdx")
      .attachTable("Usuario")
      .onAtribute("color_ojos")
      .orderBy(ASC);

    Serial.println("\n--- 3. INSERCION Y PRUEBA DE RESTRICCIONES (CHECK / FK) ---");
    db.from("Auto").insert({ {"id", "10"}, {"modelo", "Toyota Hilux"} });
    db.from("Auto").insert({ {"id", "20"}, {"modelo", "Ford Ranger"} });

    db.from("Usuario").insert({
        {"nombre", "Tomas"}, {"dni", "42123456"}, {"color_ojos", "Cafe"},
        {"fecha_modificacion", "173000900"}, {"Auto", "10"}
    });
    db.from("Usuario").insert({
        {"nombre", "Silvana"}, {"dni", "25987654"}, {"color_ojos", "Verde"},
        {"fecha_modificacion", "173000100"}, {"Auto", "10"}
    });
    db.from("Usuario").insert({
        {"nombre", "Juan"}, {"dni", "38111222"}, {"color_ojos", "Cafe"},
        {"fecha_modificacion", "173000500"}, {"Auto", "20"}
    });

    // Debe fallar por CHECK (dni < 4 caracteres)
    db.from("Usuario").insert({
        {"nombre", "Intruso"}, {"dni", "12"}, {"Auto", "10"}
    });

    Serial.println("\n--- 4. PRUEBA DE ERROR SQL: COUNT(x) SIN GROUP BY ---");
    db.from("Usuario")
      .select("color_ojos, count(id) as cantidad")
      .execute(); // Dispara SQL ERROR [42803] GROUPING_ERROR

    Serial.println("\n--- 5. CONSULTA CORRECTA CON GROUP BY Y ORDER BY (ASC/DESC) ---");
    auto agrupados = db.from("Usuario")
                       .select("color_ojos, count(id) as cantidad")
                       .groupBy("color_ojos")
                       .orderBy("cantidad", DESC)
                       .execute();

    for (auto& f : agrupados) {
        Serial.printf(" -> Color: %s | Cantidad: %s\n", f["color_ojos"].c_str(), f["cantidad"].c_str());
    }

    Serial.println("\n--- 6. PRUEBA DEL MODULO DCL (PERMISOS Y ROLES) ---");
    db.revoke(PERM_DELETE).onTable("Auto").fromRole(ROL_OPERADOR);
    db.setSession("juan_operador", ROL_OPERADOR);

    // Debe fallar con SQL ERROR [42501] INSUFFICIENT_PRIVILEGE
    db.from("Auto").eq("id", "10").remove();

    Serial.println("\n--- 7. DELETE EN CASCADA (AUTO ID=10 BORRA SUS USUARIOS) ---");
    db.setSession("silvana_admin", ROL_ADMIN);
    db.from("Auto").eq("id", "10").remove();

    auto restantes = db.from("Usuario").select("*").orderBy("fecha_modificacion", ASC).execute();
    Serial.printf("Usuarios restantes tras borrar Auto #10 en cascada: %u\n", restantes.size());
    for (auto& u : restantes) {
        Serial.printf(" -> ID:%s | Nombre:%s | Auto:%s\n", u["id"].c_str(), u["nombre"].c_str(), u["Auto"].c_str());
    }
}

void loop() {}