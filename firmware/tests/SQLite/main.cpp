#include <Arduino.h>
#include "TarjetaSD.hpp"
#include "SupabaseDB.hpp"

void setup() {
    Serial.begin(115200);
    delay(1000);

    // 1. Montar hardware MicroSD usando tu clase existente
    TarjetaSD& sd = TarjetaSD::getInstance();
    if (!sd.estaInicializada()) {
        Serial.println("[ERR] Fallo al montar MicroSD");
        return;
    }

    // 2. Abrir base de datos SQLite en la SD
    SupabaseDB& db = SupabaseDB::getInstance();
    if (!db.abrir("/sd/heladera.db")) {
        Serial.println("[ERR] No se pudo abrir SQLite");
        return;
    }

    // 3. Crear tabla (sin preocuparse por paddings ni tamaños fijos)
    db.ejecutarRaw("CREATE TABLE IF NOT EXISTS sensor ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "nombre TEXT, temp REAL, pass_wifi TEXT, fecha_modificacion INTEGER);");

    // 4. Sembrar usuarios (Admin y Operador)
    db.registrarUsuario("silvana_admin", "ClaveSecreta2026", Rol::ADMIN);
    db.registrarUsuario("juan_operador", "Operador123", Rol::REGULAR);

    // 5. Autenticación
    if (db.signIn("silvana_admin", "ClaveSecreta2026")) {
        Serial.println("[AUTH] Login exitoso como ADMIN");
    }

    // 6. Insertar datos encriptando un campo sensible con AES-256
    String passCifrada = db.encriptarCampo("MiWiFiSecreto123");
    db.from("sensor").insert({
        {"nombre", "Sonda Glicol 1"},
        {"temp", "-18.4"},
        {"pass_wifi", passCifrada},
        {"fecha_modificacion", "1730000100"}
    });

    db.from("sensor").insert({
        {"nombre", "Sonda Ambiente"},
        {"temp", "22.1"},
        {"pass_wifi", passCifrada},
        {"fecha_modificacion", "1730000500"}
    });

    // 7. Consulta idéntica a Supabase
    auto res = db.from("sensor")
                 .select("*")
                 .orderBy("fecha_modificacion", false) // DESC
                 .limit(5)
                 .execute();

    if (res.ok) {
        Serial.printf("\n[SUPABASE QUERY] Filas obtenidas: %u\n", res.data.size());
        for (auto& fila : res.data) {
            Serial.printf(" -> ID: %s | Nombre: %s | Temp: %s C | Modificado: %s\n",
                          fila["id"].c_str(),
                          fila["nombre"].c_str(),
                          fila["temp"].c_str(),
                          fila["fecha_modificacion"].c_str());
            Serial.printf("    WiFi Cifrado en SD: %s\n", fila["pass_wifi"].c_str());
            Serial.printf("    WiFi Desencriptado: %s\n", db.desencriptarCampo(fila["pass_wifi"]).c_str());
        }
    }

    // 8. Prueba de Autorización: Cambiar a usuario regular e intentar un UPDATE de Admin
    db.signIn("juan_operador", "Operador123");
    auto intento = db.from("sensor").eq("id", "1").update({{"temp", "99.9"}});
    if (!intento.ok) {
        Serial.printf("\n[SEGURIDAD RBAC] Bloqueado correctamente: %s\n", intento.error.c_str());
    }
}

void loop() {}