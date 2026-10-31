
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
// CONFIGURACION
// ============================================================

static constexpr uint8_t PIN_DS18B20 = 4;
static constexpr uint32_t BAUDIOS = 115200;
static constexpr uint32_t INTERVALO_PRUEBAS_MS = 5000;
static constexpr uint8_t CANTIDAD_REINTENTOS = 3;

OneWire bus(PIN_DS18B20);
DallasTemperature sensores(&bus);

// ============================================================
// ESTADISTICAS
// ============================================================

struct Estadisticas {
    uint32_t ciclos = 0;
    uint32_t detecciones = 0;
    uint32_t desconexiones = 0;
    uint32_t lecturasValidas = 0;
    uint32_t lecturasInvalidas = 0;
    uint32_t erroresCRC = 0;
    float temperaturaMinima = 1000.0f;
    float temperaturaMaxima = -1000.0f;
    double sumaTemperaturas = 0.0;
};

Estadisticas estadisticas;

// ============================================================
// UTILIDADES
// ============================================================

void imprimirSeparador() {
    Serial.println(
        "------------------------------------------------------------"
    );
}

void imprimirDireccion(const DeviceAddress direccion) {
    for (uint8_t i = 0; i < 8; i++) {
        if (direccion[i] < 0x10) Serial.print('0');
        Serial.print(direccion[i], HEX);

        if (i < 7) Serial.print(':');
    }
}

const char* identificarFamilia(uint8_t codigo) {
    switch (codigo) {
        case 0x10: return "DS18S20 / DS1820";
        case 0x28: return "DS18B20";
        case 0x22: return "DS1822";
        case 0x3B: return "DS1825 / MAX31850";
        case 0x42: return "DS28EA00";
        default:   return "Familia desconocida";
    }
}

const char* nombreResolucion(uint8_t resolucion) {
    switch (resolucion) {
        case 9:  return "9 bits (0.5 C)";
        case 10: return "10 bits (0.25 C)";
        case 11: return "11 bits (0.125 C)";
        case 12: return "12 bits (0.0625 C)";
        default: return "Resolucion desconocida";
    }
}

void imprimirTemperatura(const char* etiqueta, float temperatura) {
    Serial.print(etiqueta);

    if (temperatura == DEVICE_DISCONNECTED_C) {
        Serial.println("DISPOSITIVO DESCONECTADO");
        return;
    }

    if (!isfinite(temperatura)) {
        Serial.println("VALOR NO FINITO");
        return;
    }

    Serial.print(temperatura, 4);
    Serial.println(" C");
}

// ============================================================
// PRUEBA 1: ESTADO ELECTRICO DEL BUS
// ============================================================

void probarEstadoBus() {
    Serial.println("\n[PRUEBA 1] Estado electrico del bus");

    pinMode(PIN_DS18B20, INPUT_PULLUP);

    delay(5);

    int nivel = digitalRead(PIN_DS18B20);

    Serial.printf("GPIO: %u\n", PIN_DS18B20);
    Serial.printf("Nivel de reposo: %s\n",
                  nivel ? "HIGH (correcto)" : "LOW (anormal)");

    if (nivel == LOW) {
        Serial.println(
            "ADVERTENCIA: la linea permanece en LOW."
        );
        Serial.println(
            "Revisar pull-up, cortocircuitos o dispositivo."
        );
    }
}

// ============================================================
// PRUEBA 2: PRESENCE PULSE
// ============================================================

void probarPresencia() {
    Serial.println("\n[PRUEBA 2] Deteccion de presencia OneWire");

    uint8_t respuestas = 0;

    for (uint8_t i = 0; i < 20; i++) {
        bool presencia = bus.reset() != 0;

        if (presencia) {
            respuestas++;
        }

        Serial.printf(
            "Reset %02u: %s\n",
            i + 1,
            presencia ? "PRESENCE DETECTED" : "NO PRESENCE"
        );

        delay(100);
    }

    Serial.printf(
        "Resultado: %u/20 respuestas de presencia (%.1f%%)\n",
        respuestas,
        respuestas * 5.0f
    );

    if (respuestas == 20) {
        Serial.println("BUS ESTABLE durante esta prueba.");
    } else if (respuestas > 0) {
        Serial.println("BUS INTERMITENTE.");
    } else {
        Serial.println("NO SE DETECTAN DISPOSITIVOS.");
    }
}

// ============================================================
// PRUEBA 3: DESCUBRIMIENTO DE DISPOSITIVOS
// ============================================================

void probarDescubrimiento() {
    Serial.println("\n[PRUEBA 3] Descubrimiento DallasTemperature");

    sensores.begin();

    uint8_t cantidad = sensores.getDeviceCount();
    uint8_t cantidadDS18 = sensores.getDS18Count();

    Serial.printf(
        "Dispositivos detectados: %u\n",
        cantidad
    );

    Serial.printf(
        "Dispositivos de temperatura reconocidos: %u\n",
        cantidadDS18
    );

    if (cantidad == 0) {
        Serial.println("No se encontraron dispositivos.");
        return;
    }

    Serial.println("Dispositivos encontrados.");
}

// ============================================================
// PRUEBA 4: DIRECCION ROM Y CRC
// ============================================================

void probarDirecciones() {
    Serial.println("\n[PRUEBA 4] Identificacion ROM y CRC");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("No hay dispositivos para identificar.");
        return;
    }

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        Serial.printf("\nDispositivo %u\n", i);

        if (!sensores.getAddress(direccion, i)) {
            Serial.println("No se pudo obtener la direccion.");
            continue;
        }

        Serial.print("ROM: ");
        imprimirDireccion(direccion);
        Serial.println();

        Serial.printf(
            "Familia: 0x%02X (%s)\n",
            direccion[0],
            identificarFamilia(direccion[0])
        );

        uint8_t crcCalculado = OneWire::crc8(direccion, 7);

        Serial.printf(
            "CRC almacenado: 0x%02X\n",
            direccion[7]
        );

        Serial.printf(
            "CRC calculado:  0x%02X\n",
            crcCalculado
        );

        bool crcValido = crcCalculado == direccion[7];

        Serial.printf(
            "CRC ROM: %s\n",
            crcValido ? "VALIDO" : "INVALIDO"
        );

        Serial.printf(
            "Direccion valida: %s\n",
            sensores.validAddress(direccion)
                ? "SI"
                : "NO"
        );

        Serial.printf(
            "Familia valida: %s\n",
            sensores.validFamily(direccion)
                ? "SI"
                : "NO"
        );

        if (!crcValido) {
            estadisticas.erroresCRC++;
        }
    }
}

// ============================================================
// PRUEBA 5: CONECTIVIDAD INDIVIDUAL
// ============================================================

void probarConectividad() {
    Serial.println("\n[PRUEBA 5] Conectividad individual");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("Sin dispositivos.");
        return;
    }

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        bool conectado = sensores.isConnected(direccion);

        Serial.printf(
            "Dispositivo %u: %s\n",
            i,
            conectado ? "CONECTADO" : "DESCONECTADO"
        );

        uint8_t scratchpad[9];

        bool lecturaScratchpad =
            sensores.readScratchPad(direccion, scratchpad);

        Serial.printf(
            "Lectura Scratchpad: %s\n",
            lecturaScratchpad ? "EXITOSA" : "FALLIDA"
        );

        if (lecturaScratchpad) {
            Serial.print("Scratchpad: ");

            for (uint8_t j = 0; j < 9; j++) {
                if (scratchpad[j] < 0x10) Serial.print('0');
                Serial.print(scratchpad[j], HEX);
                Serial.print(' ');
            }

            Serial.println();

            uint8_t crc = OneWire::crc8(scratchpad, 8);

            Serial.printf(
                "CRC Scratchpad: %s\n",
                crc == scratchpad[8] ? "VALIDO" : "INVALIDO"
            );

            if (crc != scratchpad[8]) {
                estadisticas.erroresCRC++;
            }
        }
    }
}

// ============================================================
// PRUEBA 6: ALIMENTACION
// ============================================================

void probarAlimentacion() {
    Serial.println("\n[PRUEBA 6] Modo de alimentacion");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("Sin dispositivos.");
        return;
    }

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        bool alimentacionExterna =
            sensores.readPowerSupply(direccion);

        Serial.printf(
            "Dispositivo %u: %s\n",
            i,
            alimentacionExterna
                ? "ALIMENTACION EXTERNA"
                : "ALIMENTACION PARASITA"
        );
    }

    Serial.println(
        "Nota: este resultado indica el modo detectado, "
        "no mide el voltaje real."
    );
}

// ============================================================
// PRUEBA 7: RESOLUCION
// ============================================================

void probarResolucion() {
    Serial.println("\n[PRUEBA 7] Resolucion configurada");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("Sin dispositivos.");
        return;
    }

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        uint8_t resolucion =
            sensores.getResolution(direccion);

        Serial.printf(
            "Dispositivo %u: %s\n",
            i,
            nombreResolucion(resolucion)
        );

        Serial.printf(
            "Tiempo maximo de conversion: %u ms\n",
            sensores.millisToWaitForConversion(resolucion)
        );
    }
}

// ============================================================
// PRUEBA 8: CONVERSION Y LECTURA
// ============================================================

void probarConversion() {
    Serial.println("\n[PRUEBA 8] Conversion de temperatura");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("Sin dispositivos.");
        return;
    }

    sensores.setWaitForConversion(true);
    sensores.setCheckForConversion(true);

    Serial.println("Solicitando conversion...");

    DallasTemperature::request_t solicitud =
        sensores.requestTemperatures();

    Serial.printf(
        "Solicitud: %s\n",
        solicitud ? "ACEPTADA" : "FALLIDA"
    );

    Serial.printf(
        "Conversion completa: %s\n",
        sensores.isConversionComplete() ? "SI" : "NO"
    );

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        Serial.printf("\nDispositivo %u\n", i);

        float celsius = sensores.getTempC(direccion);
        float fahrenheit = sensores.getTempF(direccion);

        imprimirTemperatura("Temperatura: ", celsius);
        imprimirTemperatura("Fahrenheit: ", fahrenheit);

        if (celsius == DEVICE_DISCONNECTED_C ||
            !isfinite(celsius)) {
            estadisticas.lecturasInvalidas++;
            continue;
        }

        estadisticas.lecturasValidas++;

        if (celsius < estadisticas.temperaturaMinima) {
            estadisticas.temperaturaMinima = celsius;
        }

        if (celsius > estadisticas.temperaturaMaxima) {
            estadisticas.temperaturaMaxima = celsius;
        }

        estadisticas.sumaTemperaturas += celsius;
    }
}

// ============================================================
// PRUEBA 9: CONFIGURACION Y DATOS DEL SCRATCHPAD
// ============================================================

void probarScratchpad() {
    Serial.println("\n[PRUEBA 9] Analisis del Scratchpad");

    uint8_t cantidad = sensores.getDeviceCount();

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;
        uint8_t datos[9];

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        if (!sensores.readScratchPad(direccion, datos)) {
            Serial.println("No se pudo leer Scratchpad.");
            continue;
        }

        Serial.printf("\nDispositivo %u\n", i);

        int16_t raw =
            static_cast<int16_t>(
                (static_cast<uint16_t>(datos[1]) << 8) |
                datos[0]
            );

        Serial.printf("Temperatura RAW: %d\n", raw);

        Serial.printf("TH (alarma alta): %d\n",
                      static_cast<int8_t>(datos[2]));

        Serial.printf("TL (alarma baja): %d\n",
                      static_cast<int8_t>(datos[3]));

        Serial.printf("Configuracion: 0x%02X\n", datos[4]);

        Serial.printf("Reservado 5: 0x%02X\n", datos[5]);
        Serial.printf("Reservado 6: 0x%02X\n", datos[6]);
        Serial.printf("Reservado 7: 0x%02X\n", datos[7]);

        Serial.printf("CRC: 0x%02X\n", datos[8]);

        Serial.printf(
            "Temperatura calculada desde RAW: %.4f C\n",
            DallasTemperature::rawToCelsius(raw)
        );

        Serial.printf(
            "Temperatura Fahrenheit desde RAW: %.4f F\n",
            DallasTemperature::rawToFahrenheit(raw)
        );
    }
}

// ============================================================
// PRUEBA 10: ALARMAS
// ============================================================

void probarAlarmas() {
    Serial.println("\n[PRUEBA 10] Configuracion de alarmas");

    uint8_t cantidad = sensores.getDeviceCount();

    if (cantidad == 0) {
        Serial.println("Sin dispositivos.");
        return;
    }

    for (uint8_t i = 0; i < cantidad; i++) {
        DeviceAddress direccion;

        if (!sensores.getAddress(direccion, i)) {
            continue;
        }

        Serial.printf(
            "Dispositivo %u: alarma alta = %d C\n",
            i,
            sensores.getHighAlarmTemp(direccion)
        );

        Serial.printf(
            "Dispositivo %u: alarma baja = %d C\n",
            i,
            sensores.getLowAlarmTemp(direccion)
        );

        Serial.printf(
            "Dispositivo %u: tiene alarma = %s\n",
            i,
            sensores.hasAlarm(direccion) ? "SI" : "NO"
        );
    }

    sensores.resetAlarmSearch();

    DeviceAddress direccionAlarma;
    uint8_t encontradas = 0;

    while (sensores.alarmSearch(direccionAlarma)) {
        encontradas++;

        Serial.print("Dispositivo en alarma: ");
        imprimirDireccion(direccionAlarma);
        Serial.println();
    }

    Serial.printf(
        "Dispositivos con alarma: %u\n",
        encontradas
    );
}

// ============================================================
// RESUMEN ESTADISTICO
// ============================================================

void imprimirEstadisticas() {
    Serial.println("\n[ESTADISTICAS ACUMULADAS]");

    Serial.printf("Ciclos ejecutados: %lu\n",
                  static_cast<unsigned long>(estadisticas.ciclos));

    Serial.printf("Lecturas validas: %lu\n",
                  static_cast<unsigned long>(estadisticas.lecturasValidas));

    Serial.printf("Lecturas invalidas: %lu\n",
                  static_cast<unsigned long>(estadisticas.lecturasInvalidas));

    Serial.printf("Errores CRC: %lu\n",
                  static_cast<unsigned long>(estadisticas.erroresCRC));

    if (estadisticas.lecturasValidas > 0) {
        double promedio =
            estadisticas.sumaTemperaturas /
            estadisticas.lecturasValidas;

        Serial.printf("Temperatura minima: %.4f C\n",
                      estadisticas.temperaturaMinima);

        Serial.printf("Temperatura maxima: %.4f C\n",
                      estadisticas.temperaturaMaxima);

        Serial.printf("Temperatura promedio: %.4f C\n",
                      promedio);
    } else {
        Serial.println("Todavia no hay temperaturas validas.");
    }
}

// ============================================================
// EJECUCION DE LA INVESTIGACION
// ============================================================

void ejecutarInvestigacion() {
    estadisticas.ciclos++;

    imprimirSeparador();

    Serial.printf(
        "\nINVESTIGACION DS18B20 | CICLO %lu\n",
        static_cast<unsigned long>(estadisticas.ciclos)
    );

    imprimirSeparador();

    probarEstadoBus();
    probarPresencia();
    probarDescubrimiento();

    if (sensores.getDeviceCount() > 0) {
        estadisticas.detecciones++;

        probarDirecciones();
        probarConectividad();
        probarAlimentacion();
        probarResolucion();
        probarConversion();
        probarScratchpad();
        probarAlarmas();
    } else {
        estadisticas.desconexiones++;

        Serial.println(
            "\nNo se ejecutan las pruebas dependientes "
            "de un dispositivo detectado."
        );
    }

    imprimirEstadisticas();

    imprimirSeparador();

    Serial.println("FIN DEL CICLO DE INVESTIGACION");
}

// ============================================================
// SETUP Y LOOP
// ============================================================

void setup() {
    Serial.begin(BAUDIOS);

    delay(1500);

    Serial.println();
    Serial.println("============================================");
    Serial.println(" INVESTIGACION EMPIRICA DS18B20");
    Serial.println(" ESP32 NODEMCU-32S");
    Serial.println("============================================");

    Serial.printf("GPIO OneWire: %u\n", PIN_DS18B20);
    Serial.printf("Baudios: %u\n", BAUDIOS);

    sensores.begin();

    sensores.setWaitForConversion(true);
    sensores.setCheckForConversion(true);

    ejecutarInvestigacion();
}

void loop() {
    delay(INTERVALO_PRUEBAS_MS);
    ejecutarInvestigacion();
}
