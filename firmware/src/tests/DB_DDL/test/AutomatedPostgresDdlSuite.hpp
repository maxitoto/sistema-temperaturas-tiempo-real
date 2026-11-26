#pragma once
#include <Arduino.h>
#include "../engine/ModuloDDL.hpp"
#include "../models/DjangoModel.hpp"

namespace DbNotRuntime::Tests {

    using namespace DbNotRuntime::Engine;
    using namespace DbNotRuntime::Standard;
    using namespace DbNotRuntime::Models;

    // Almacena las métricas de ejecución de la suite automatizada de conformidad PostgreSQL.
    struct SuiteReport {
        uint16_t totalTests = 0;
        uint16_t passedTests = 0;
        uint16_t failedTests = 0;
    };

    class AutomatedPostgresDdlSuite {
    private:
        // Evalúa una aserción comparando el resultado booleano y el código SQLSTATE devuelto por la consola de diagnóstico.
        static void assertSqlState(SuiteReport& rep,
                                   const char* testName,
                                   bool actualSuccess,
                                   bool expectedSuccess,
                                   const char* expectedSqlState) {
            rep.totalTests++;
            const PgDiagnosticRecord& last = DdlDiagnosticConsole::getInstance().last();
            bool stateMatches = last.returnedSqlState.equalsIgnoreCase(expectedSqlState);

            if (actualSuccess == expectedSuccess && stateMatches) {
                rep.passedTests++;
                Serial.print(" [PASS] ");
                Serial.print(testName);
                Serial.print(" -> SQLSTATE [");
                Serial.print(last.returnedSqlState);
                Serial.println("]");
            } else {
                rep.failedTests++;
                Serial.print(" [FAIL] ");
                Serial.print(testName);
                Serial.print(" | Esperado: (ok=");
                Serial.print(expectedSuccess);
                Serial.print(", state=");
                Serial.print(expectedSqlState);
                Serial.print(") Obtenido: (ok=");
                Serial.print(actualSuccess);
                Serial.print(", state=");
                Serial.print(last.returnedSqlState);
                Serial.println(")");
                Serial.println(last.toPsqlFormat());
            }
        }

        // Verifica las reglas léxicas de identificadores y palabras reservadas de PostgreSQL (Sección 1 y 8 de informe.md).
        static void testLexicalAndReservedWords(SuiteReport& rep) {
            Serial.println("\n--- GRUPO 1: Reglas Lexicas, NAMEDATALEN (63B) y Palabras Reservadas ---");
            ModuloDDL& ddl = ModuloDDL::getInstance();

            // 1.1 Identificador vacío -> 42602 (INVALID_NAME)
            bool r1 = ddl.createTable("").column("id", PgDataType::INTEGER).execute();
            assertSqlState(rep, "1.1 Identificador de tabla vacio rechazado", r1, false, SqlState::INVALID_NAME);

            // 1.2 Identificador que empieza con número -> 42602 (INVALID_NAME)
            bool r2 = ddl.createTable("9TablaInvalida").column("id", PgDataType::INTEGER).execute();
            assertSqlState(rep, "1.2 Identificador iniciado con digito rechazado", r2, false, SqlState::INVALID_NAME);

            // 1.3 Identificador mayor a 63 bytes (NAMEDATALEN) -> 42622 (NAME_TOO_LONG)
            String longName = "tabla_con_nombre_extremadamente_largo_que_supera_los_sesenta_y_tres_bytes_permitidos";
            bool r3 = ddl.createTable(longName).column("id", PgDataType::INTEGER).execute();
            assertSqlState(rep, "1.3 Identificador > 63 bytes (NAMEDATALEN) rechazado", r3, false, SqlState::NAME_TOO_LONG);

            // 1.4 Palabra RESERVED de PostgreSQL usada como nombre de tabla sin comillas -> 42601 (SYNTAX_ERROR)
            bool r4 = ddl.createTable("TABLE").column("id", PgDataType::INTEGER).execute();
            assertSqlState(rep, "1.4 Palabra RESERVED 'TABLE' sin comillas rechazada", r4, false, SqlState::SYNTAX_ERROR);

            // 1.5 Palabra RESERVED usada como nombre de columna sin comillas -> 42601 (SYNTAX_ERROR)
            bool r5 = ddl.createTable("T_Valida").column("CONSTRAINT", PgDataType::INTEGER).execute();
            assertSqlState(rep, "1.5 Palabra RESERVED 'CONSTRAINT' en columna rechazada", r5, false, SqlState::SYNTAX_ERROR);

            // 1.6 Palabra RESERVED entre comillas dobles ("USER") permitida por PostgreSQL -> 00000
            bool r6 = ddl.createTable("\"USER\"").column("id", PgDataType::INTEGER).primaryKey().execute();
            assertSqlState(rep, "1.6 Palabra RESERVED entrecomillada '\"USER\"' aceptada", r6, true, SqlState::SUCCESSFUL_COMPLETION);
            ddl.dropTable("\"USER\"", true);
        }

        // Verifica modificadores de tipos de datos, rangos, precisión/escala, JSON, BIT y ENUMs (Sección 2 de informe.md).
        static void testDataTypesAndLiterals(SuiteReport& rep) {
            Serial.println("\n--- GRUPO 2: Tipos de Datos, Precision/Escala, ENUMs y Literales DEFAULT ---");
            ModuloDDL& ddl = ModuloDDL::getInstance();

            // 2.1 INTEGER con modificador de longitud inválido -> 42601
            bool r1 = ddl.createTable("T_Type1").column("edad", PgDataType::INTEGER, 10).execute();
            assertSqlState(rep, "2.1 INTEGER con maxLength rechazado", r1, false, SqlState::SYNTAX_ERROR);

            // 2.2 NUMERIC con escala mayor que la precisión -> 22023 (INVALID_PARAMETER_VALUE)
            bool r2 = ddl.createTable("T_Type2").column("precio", PgDataType::NUMERIC, 0, 4, 6).execute();
            assertSqlState(rep, "2.2 NUMERIC(4,6) con escala > precision rechazado", r2, false, SqlState::INVALID_PARAMETER_VALUE);

            // 2.3 SMALLINT con DEFAULT fuera de rango (-32768..32767) -> 22003 (NUMERIC_VALUE_OUT_OF_RANGE)
            bool r3 = ddl.createTable("T_Type3").column("puerto", PgDataType::SMALLINT).defaultValue("70000").execute();
            assertSqlState(rep, "2.3 SMALLINT DEFAULT 70000 fuera de rango rechazado", r3, false, SqlState::NUMERIC_VALUE_OUT_OF_RANGE);

            // 2.4 VARCHAR(5) con DEFAULT de 10 caracteres -> 22001 (STRING_DATA_RIGHT_TRUNCATION)
            bool r4 = ddl.createTable("T_Type4").column("cod", PgDataType::VARCHAR, 5).defaultValue("ABCDEFGHIJ").execute();
            assertSqlState(rep, "2.4 VARCHAR(5) con DEFAULT excesivo rechazado", r4, false, SqlState::STRING_DATA_RIGHT_TRUNCATION);

            // 2.5 BOOLEAN con DEFAULT inválido -> 22P02 (INVALID_TEXT_REPRESENTATION)
            bool r5 = ddl.createTable("T_Type5").column("activo", PgDataType::BOOLEAN).defaultValue("QUIZAS").execute();
            assertSqlState(rep, "2.5 BOOLEAN con literal 'QUIZAS' rechazado", r5, false, SqlState::INVALID_TEXT_REPRESENTATION);

            // 2.6 JSON con DEFAULT mal formado -> 22P02
            bool r6 = ddl.createTable("T_Type6").column("cfg", PgDataType::JSON).defaultValue("no_es_json").execute();
            assertSqlState(rep, "2.6 JSON con literal mal formado rechazado", r6, false, SqlState::INVALID_TEXT_REPRESENTATION);

            // 2.7 BIT(4) con caracteres no binarios -> 22P02
            bool r7 = ddl.createTable("T_Type7").column("mascara", PgDataType::BIT, 4).defaultValue("1021").execute();
            assertSqlState(rep, "2.7 BIT(4) con digito '2' rechazado", r7, false, SqlState::INVALID_TEXT_REPRESENTATION);

            // 2.8 Uso de ENUM inexistente -> 42704 (UNDEFINED_OBJECT)
            bool r8 = ddl.createTable("T_Type8").column("modo", PgDataType::ENUM, 0, 0, 0, "enum_fantasma").execute();
            assertSqlState(rep, "2.8 Columna con ENUM no registrado rechazada", r8, false, SqlState::UNDEFINED_OBJECT);

            // 2.9 Registro de ENUM con etiquetas duplicadas -> 42P17 (INVALID_OBJECT_DEFINITION)
            bool r9 = ddl.createEnumType("enum_dup", {"UNO", "DOS", "UNO"});
            assertSqlState(rep, "2.9 CREATE TYPE AS ENUM con etiquetas duplicadas rechazado", r9, false, SqlState::INVALID_OBJECT_DEFINITION);

            // 2.10 ENUM válido pero con DEFAULT que no pertenece al conjunto de etiquetas -> 22P02
            ddl.createEnumType("modo_operacion_test", {"AUTO", "MANUAL"});
            bool r10 = ddl.createTable("T_Type10")
                          .column("modo", PgDataType::ENUM, 0, 0, 0, "modo_operacion_test")
                          .defaultValue("HIBRIDO")
                          .execute();
            assertSqlState(rep, "2.10 DEFAULT fuera de las etiquetas del ENUM rechazado", r10, false, SqlState::INVALID_TEXT_REPRESENTATION);
            PhysicalSchemaCatalog::getInstance().removeEnum("modo_operacion_test");
        }

        // Verifica el determinismo del builder ante combinaciones prohibidas en PostgreSQL (Sección 3 de informe.md).
        static void testDeterministicColumnCombinations(SuiteReport& rep) {
            Serial.println("\n--- GRUPO 3: Determinismo de Combinaciones y Restricciones de Columna ---");
            ModuloDDL& ddl = ModuloDDL::getInstance();

            // 3.1 Tabla sin columnas -> 42P16 (INVALID_TABLE_DEFINITION)
            bool r1 = ddl.createTable("T_Vacia").execute();
            assertSqlState(rep, "3.1 Tabla sin columnas rechazada", r1, false, SqlState::INVALID_TABLE_DEFINITION);

            // 3.2 Columnas duplicadas en la misma tabla -> 42701 (DUPLICATE_COLUMN)
            bool r2 = ddl.createTable("T_DupCol")
                         .column("id", PgDataType::INTEGER)
                         .column("ID", PgDataType::TEXT)
                         .execute();
            assertSqlState(rep, "3.2 Columna duplicada (case-insensitive) rechazada", r2, false, SqlState::DUPLICATE_COLUMN);

            // 3.3 Múltiples PRIMARY KEY en una misma tabla -> 42P16 (TUPLE_ALREADY_HAS_PRIMARY_KEY)
            bool r3 = ddl.createTable("T_TwoPk")
                         .column("id1", PgDataType::INTEGER).primaryKey()
                         .column("id2", PgDataType::INTEGER).primaryKey()
                         .execute();
            assertSqlState(rep, "3.3 Doble PRIMARY KEY en una tabla rechazada", r3, false, SqlState::TUPLE_ALREADY_HAS_PRIMARY_KEY);

            // 3.4 GENERATED AS IDENTITY sobre columna no entera (TEXT) -> 42P17 (INVALID_OBJECT_DEFINITION)
            bool r4 = ddl.createTable("T_BadId")
                         .column("nombre", PgDataType::TEXT).generatedAsIdentity()
                         .execute();
            assertSqlState(rep, "3.4 GENERATED AS IDENTITY sobre tipo TEXT rechazado", r4, false, SqlState::INVALID_OBJECT_DEFINITION);

            // 3.5 Combinar GENERATED AS IDENTITY y DEFAULT en la misma columna -> 42601 (SYNTAX_ERROR)
            bool r5 = ddl.createTable("T_IdDef")
                         .column("id", PgDataType::INTEGER).generatedAsIdentity().defaultValue("10")
                         .execute();
            assertSqlState(rep, "3.5 Combinacion IDENTITY + DEFAULT rechazada", r5, false, SqlState::SYNTAX_ERROR);

            // 3.6 Columna NOT NULL con DEFAULT "NULL" -> 23502 (NOT_NULL_VIOLATION)
            bool r6 = ddl.createTable("T_NullViol")
                         .column("nombre", PgDataType::TEXT).notNull().defaultValue("NULL")
                         .execute();
            assertSqlState(rep, "3.6 Columna NOT NULL con DEFAULT NULL rechazada", r6, false, SqlState::NOT_NULL_VIOLATION);

            // 3.7 Valor DEFAULT que viola la restricción CHECK de la propia columna -> 23514 (CHECK_VIOLATION)
            bool r7 = ddl.createTable("T_CheckViol")
                         .column("temperatura", PgDataType::INTEGER)
                         .check("temperatura >= -55", [](const String& v) { return v.toInt() >= -55; })
                         .defaultValue("-100")
                         .execute();
            assertSqlState(rep, "3.7 DEFAULT que infringe el predicado CHECK rechazado", r7, false, SqlState::CHECK_VIOLATION);
        }

        // Verifica todas las reglas de FOREIGN KEY, compatibilidad de tipos y dependencias CASCADE/RESTRICT.
        static void testForeignKeysAndDependencies(SuiteReport& rep) {
            Serial.println("\n--- GRUPO 4: Integridad Referencial (FOREIGN KEY) y Dependencias ---");
            ModuloDDL& ddl = ModuloDDL::getInstance();

            // Prepara tabla padre de prueba
            ddl.createTable("PadreTest")
               .column("id_padre", PgDataType::INTEGER).primaryKey()
               .column("codigo_unico", PgDataType::VARCHAR, 16).unique()
               .column("nombre_no_unico", PgDataType::VARCHAR, 32)
               .execute();

            // 4.1 FK apuntando a tabla inexistente -> 42P01 (UNDEFINED_TABLE)
            bool r1 = ddl.createTable("Hija1")
                         .column("fk", PgDataType::INTEGER).references("TablaFantasma", "id")
                         .execute();
            assertSqlState(rep, "4.1 FK hacia tabla inexistente rechazada", r1, false, SqlState::UNDEFINED_TABLE);

            // 4.2 FK apuntando a columna inexistente en tabla padre -> 42703 (UNDEFINED_COLUMN)
            bool r2 = ddl.createTable("Hija2")
                         .column("fk", PgDataType::INTEGER).references("PadreTest", "col_inexistente")
                         .execute();
            assertSqlState(rep, "4.2 FK hacia columna inexistente rechazada", r2, false, SqlState::UNDEFINED_COLUMN);

            // 4.3 FK apuntando a columna padre que NO es PRIMARY KEY ni UNIQUE -> 42830 (INVALID_FOREIGN_KEY)
            bool r3 = ddl.createTable("Hija3")
                         .column("fk", PgDataType::VARCHAR, 32).references("PadreTest", "nombre_no_unico")
                         .execute();
            assertSqlState(rep, "4.3 FK hacia columna padre sin UNIQUE/PK rechazada", r3, false, SqlState::INVALID_FOREIGN_KEY);

            // 4.4 FK con tipo de dato incompatible con la PK padre (BOOLEAN -> INTEGER) -> 42804 (DATATYPE_MISMATCH)
            bool r4 = ddl.createTable("Hija4")
                         .column("fk", PgDataType::BOOLEAN).references("PadreTest", "id_padre")
                         .execute();
            assertSqlState(rep, "4.4 FK con tipo incompatible (BOOLEAN vs INTEGER) rechazada", r4, false, SqlState::DATATYPE_MISMATCH);

            // 4.5 Combinación incoherente: Columna NOT NULL con ON DELETE SET NULL -> 42830 (INVALID_FOREIGN_KEY)
            bool r5 = ddl.createTable("Hija5")
                         .column("fk", PgDataType::INTEGER).notNull()
                         .references("PadreTest", "id_padre", ReferentialAction::SET_NULL)
                         .execute();
            assertSqlState(rep, "4.5 FK NOT NULL con ON DELETE SET NULL rechazada", r5, false, SqlState::INVALID_FOREIGN_KEY);

            // 4.6 Combinación incoherente: ON DELETE SET DEFAULT sin valor DEFAULT declarado -> 42830
            bool r6 = ddl.createTable("Hija6")
                         .column("fk", PgDataType::INTEGER)
                         .references("PadreTest", "id_padre", ReferentialAction::SET_DEFAULT)
                         .execute();
            assertSqlState(rep, "4.6 FK con ON DELETE SET DEFAULT sin DEFAULT rechazada", r6, false, SqlState::INVALID_FOREIGN_KEY);

            // 4.7 Creación exitosa de tabla hija válida -> 00000
            bool r7 = ddl.createTable("HijaValida")
                         .column("id_hija", PgDataType::INTEGER).primaryKey()
                         .column("id_padre", PgDataType::INTEGER)
                             .references("PadreTest", "id_padre", ReferentialAction::CASCADE)
                         .execute();
            assertSqlState(rep, "4.7 Creacion de tabla hija con FK valida exitosa", r7, true, SqlState::SUCCESSFUL_COMPLETION);

            // 4.8 DROP TABLE sobre PadreTest en modo RESTRICT existiendo HijaValida -> 2BP01 (DEPENDENT_OBJECTS_STILL_EXIST)
            bool r8 = ddl.dropTable("PadreTest", false, ReferentialAction::RESTRICT);
            assertSqlState(rep, "4.8 DROP TABLE Padre en modo RESTRICT bloqueado por dependencia", r8, false, SqlState::DEPENDENT_OBJECTS_STILL_EXIST);

            // 4.9 ALTER TABLE DROP COLUMN sobre la PK referenciada en modo RESTRICT -> 2BP01
            bool r9 = ddl.alterTableDropColumn("PadreTest", "id_padre", false, ReferentialAction::RESTRICT);
            assertSqlState(rep, "4.9 ALTER TABLE DROP COLUMN de PK referenciada bloqueado", r9, false, SqlState::DEPENDENT_OBJECTS_STILL_EXIST);

            // 4.10 DROP TABLE sobre PadreTest en modo CASCADE -> 00000
            bool r10 = ddl.dropTable("PadreTest", false, ReferentialAction::CASCADE);
            assertSqlState(rep, "4.10 DROP TABLE Padre con CASCADE exitoso", r10, true, SqlState::SUCCESSFUL_COMPLETION);
            ddl.dropTable("HijaValida", true);
        }

        // Verifica operaciones ALTER TABLE e INDEX sobre el catálogo físico.
        static void testAlterTableAndIndexes(SuiteReport& rep) {
            Serial.println("\n--- GRUPO 5: Operaciones ALTER TABLE y CREATE/DROP INDEX ---");
            ModuloDDL& ddl = ModuloDDL::getInstance();

            ddl.createTable("TablaAlter")
               .column("id", PgDataType::INTEGER).primaryKey()
               .column("nombre", PgDataType::VARCHAR, 32).notNull()
               .execute();

            // 5.1 Intentar quitar NOT NULL a una columna que es PRIMARY KEY -> 42P16
            bool r1 = ddl.alterTableAlterColumnNullability("TablaAlter", "id", false);
            assertSqlState(rep, "5.1 ALTER COLUMN DROP NOT NULL sobre PRIMARY KEY rechazado", r1, false, SqlState::INVALID_TABLE_DEFINITION);

            // 5.2 Crear índice sobre columna inexistente -> 42703 (UNDEFINED_COLUMN)
            IndexDefinition badIdx;
            badIdx.indexName  = "idx_col_fantasma";
            badIdx.tableName  = "TablaAlter";
            badIdx.keyColumns = {"columna_que_no_existe"};
            bool r2 = ddl.createIndex(badIdx);
            assertSqlState(rep, "5.2 CREATE INDEX sobre columna inexistente rechazado", r2, false, SqlState::UNDEFINED_COLUMN);

            // 5.3 Crear índice válido y luego intentar duplicarlo sin IF NOT EXISTS -> 42710 (DUPLICATE_OBJECT)
            IndexDefinition goodIdx;
            goodIdx.indexName  = "idx_tabla_alter_nombre";
            goodIdx.tableName  = "TablaAlter";
            goodIdx.keyColumns = {"nombre"};
            ddl.createIndex(goodIdx);
            bool r3 = ddl.createIndex(goodIdx, false);
            assertSqlState(rep, "5.3 CREATE INDEX duplicado rechazado", r3, false, SqlState::DUPLICATE_OBJECT);

            ddl.dropTable("TablaAlter", true);
        }

    public:
        // Ejecuta todos los grupos de pruebas automatizadas y devuelve el resumen final.
        static SuiteReport runAll() {
            DdlDiagnosticConsole& console = DdlDiagnosticConsole::getInstance();
            bool prevSerialState = false;
            console.setSerialOutput(false); // Silencia salidas individuales esperadas; solo muestra si un test falla

            Serial.println("\n######################################################################");
            Serial.println(" INICIANDO SUITE AUTOMATIZADA DE CONFORMIDAD POSTGRESQL DDL");
            Serial.println("######################################################################");

            SuiteReport report;
            testLexicalAndReservedWords(report);
            testDataTypesAndLiterals(report);
            testDeterministicColumnCombinations(report);
            testForeignKeysAndDependencies(report);
            testAlterTableAndIndexes(report);

            Serial.println("\n----------------------------------------------------------------------");
            Serial.print(" RESULTADO FINAL DE LA SUITE: ");
            Serial.print(report.passedTests);
            Serial.print("/");
            Serial.print(report.totalTests);
            Serial.print(" TESTS SUPERADOS (Fallos: ");
            Serial.print(report.failedTests);
            Serial.println(")");
            Serial.println("----------------------------------------------------------------------");

            console.setSerialOutput(prevSerialState);
            return report;
        }
    };

} // namespace DbNotRuntime::Tests