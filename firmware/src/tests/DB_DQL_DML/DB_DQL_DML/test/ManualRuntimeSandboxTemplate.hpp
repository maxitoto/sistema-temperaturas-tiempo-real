#pragma once
#include <Arduino.h>
#include "../../DB_DDL/models/PhysicalModelsDefinition.hpp"
#include "../orm/DjangoManager.hpp"

namespace DbRuntime::Tests {

    using namespace DbRuntime::Orm;
    using namespace DbRuntime::Storage;
    using namespace DbRuntime::Standard;
    using DbNotRuntime::Standard::PgDataType;

    // Struct C++ del usuario para demostrar el mapeo tipado directo sin std::map.
    struct ResumenLecturaDTO {
        int32_t idSenso;
        String nombreSonda;
        float tempCorregida;
    };

    inline void bannerManualRuntime(const char* titulo) {
        Serial.println("\n======================================================================");
        Serial.print(" [SANDBOX MANUAL RUNTIME] ");
        Serial.println(titulo);
        Serial.println("======================================================================");
    }

    // Ejecuta el laboratorio manual interactivo de DML, DQL y EXPLAIN ANALYZE.
    inline void ejecutarSandboxManualRuntime() {
        DbNotRuntime::Engine::DdlDiagnosticConsole::getInstance().setSerialOutput(true);
        ModelManager Usuarios = runtimeDb.model("Usuarios");
        ModelManager Sensor   = runtimeDb.model("Sensor");
        ModelManager Senso    = runtimeDb.model("Senso");

        // =====================================================================
        // PASO 1: [INTENTA INSERTAR O HACER UPSERT EN UNA TABLA AQUI]
        // =====================================================================
        bannerManualRuntime("1. CREATE CON VALIDACION DE ENUM Y RETURNING");

        Usuarios.objects().filter_eq("usuario", "tomas_untdf").remove();
        auto resUser = Usuarios.create({
            PgCell("usuario", "tomas_untdf"),
            PgCell("salt", "SALT_USHUAIA_2026"),
            PgCell("contrasena_hash", "9f86d081884c7d659a2feaa0c55ad015"),
            PgCell("rol", "ADMIN", PgDataType::ENUM)
        }, {"id_usuario", "usuario", "rol"});

        if (resUser && !resUser.returningRows.empty()) {
            Serial.print("  -> RETURNING id_usuario=");
            Serial.print(resUser.returningRows[0].getInt("id_usuario"));
            Serial.print(" | usuario=");
            Serial.print(resUser.returningRows[0].getText("usuario"));
            Serial.print(" | rol=");
            Serial.println(resUser.returningRows[0].getText("rol"));
        }
        Serial.println(resUser.cost.toExplainAnalyzeString());

        // =====================================================================
        // PASO 2: [COMPARA INDEX SCAN VS SEQ SCAN CON EXPLAIN ANALYZE AQUI]
        // =====================================================================
        bannerManualRuntime("2. COMPARACION DE COSTO HARDWARE: INDEX SCAN VS SEQ SCAN");

        Serial.println("--> A) Filtrando por columna UNIQUE indexada (hexadecimal = '28FFAA01'):");
        Serial.println(Sensor.filter_eq("hexadecimal", "28FFAA01").explain_analyze());

        Serial.println("--> B) Filtrando por columna NO indexada (ubicacion = 'Sector Par'):");
        Serial.println(Sensor.filter_eq("ubicacion", "Sector Par").explain_analyze());

        // =====================================================================
        // PASO 3: [INTENTA MAPEAR UN QUERYSET A TU STRUCT C++ AQUI]
        // =====================================================================
        bannerManualRuntime("3. JOIN + MAPEO DIRECTO A STRUCT C++ (all_as<ResumenLecturaDTO>)");

        auto listaTipada = Senso.objects()
            .select_related("Sensor", "id_sensor", "id_sensor", JoinType::INNER)
            .filter("temperatura_corregida", FilterOp::GT, "5.0")
            .order_by("-temperatura_corregida")
            .all_as<ResumenLecturaDTO>(+[](const PgTuple& r) -> ResumenLecturaDTO {
                ResumenLecturaDTO dto;
                dto.idSenso       = static_cast<int32_t>(r.getInt("id_senso"));
                dto.nombreSonda   = r.getText("Sensor.nombre", "<AUSENTE/NULL>");
                dto.tempCorregida = r.getFloat("temperatura_corregida");
                return dto;
            });

        for (const auto& item : listaTipada) {
            Serial.print("  -> Struct DTO: idSenso=");
            Serial.print(item.idSenso);
            Serial.print(" | Sonda='");
            Serial.print(item.nombreSonda);
            Serial.print("' | Temp=");
            Serial.print(item.tempCorregida, 2);
            Serial.println(" C");
        }
        Serial.println(ModuloDQL::getInstance().getLastCost().toExplainAnalyzeString());
    }

} // namespace DbRuntime::Tests