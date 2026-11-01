// Archivo: src/tests/hardware/gestorsensor/main.cpp
#include <Arduino.h>
#include "GestorSensores.hpp"

void imprimirReporteCiclo() {
    EstadoBus estadoBus = gestorSensores.verificarEstadoElectricoBus();
    if (estadoBus == EstadoBus::LINEA_EN_CORTO_GND) {
        Serial.println("[BUS ERR] Linea 1-Wire en LOW permanente (Cortocircuito a GND o falta Pull-Up)");
        return;
    }

    if (gestorSensores.verificarCambiosEnCaliente()) {
        Serial.println("[HOT-PLUG] Se detecto cambio en la cantidad de sensores. Re-escaneando...");
        gestorSensores.escanearYAsignarSensores(12, false);
    }

    gestorSensores.actualizarTodosLosSlots();
    uint8_t enAlarmaHw = gestorSensores.procesarBusquedaAlarmasHardware();

    Serial.println("\n======================================================================");
    Serial.printf(" GESTOR DE SENSORES (En bus: %u | Asignados: %u/6 | Alarmas: %u)\n",
                  gestorSensores.obtenerTotalEnBus(),
                  gestorSensores.obtenerTotalAsignados(),
                  enAlarmaHw);
    Serial.printf(" Modo parasito global: %s | Ultima conversion (ms): %lu\n",
                  gestorSensores.hayModoParasitoGlobal() ? "SI" : "NO",
                  (unsigned long)gestorSensores.obtenerTimestampConversion());
    Serial.println("======================================================================");

    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        const DiagnosticoSlot& d = gestorSensores.obtenerDiagnostico(i);
        if (!d.activo) {
            Serial.printf("Slot [%u] -> [VACIO]\n", i);
            continue;
        }

        Serial.printf("Slot [%u] | ROM:%s (%s) | %u bits | Pwr:%s | Estado:%s | Temp: %.2f C (RAW:%ld)\n",
                      i,
                      d.romHex,
                      GestorSensores::nombreFamilia(d.rom[0]),
                      d.resolucionBits,
                      d.alimentacionParasita ? "PARASITA" : "EXTERNA",
                      GestorSensores::nombreEstado(d.estado),
                      d.tempCelsius,
                      static_cast<long>(d.tempRaw));

        Serial.printf("       TH:%d C | TL:%d C | EnAlarma:%s | ID usuario:%d | CRC pad:0x%02X (%s)\n",
                      d.alarmaAltaC,
                      d.alarmaBajaC,
                      d.enAlarma ? "SI" : "NO",
                      d.datoUsuario,
                      d.ultimoScratchpad[8],
                      (OneWire::crc8(d.ultimoScratchpad, 8) == d.ultimoScratchpad[8])
                          ? "valido" : "INVALIDO");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    uint8_t asignados = gestorSensores.escanearYAsignarSensores(12, false);

    const uint8_t rom28EE943A52250BF5[8] = {
        0x28, 0xEE, 0x94, 0x3A, 0x52, 0x25, 0x0B, 0xF5
    };

    gestorSensores.configurarOffsetPorROM(rom28EE943A52250BF5, 100.5f);

    Serial.printf("[INIT] Sensores asignados en el gestor: %u de %u\n",
                  asignados, GestorSensores::MAX_SENSORES);

    // Configurar umbrales de alarma de prueba (-20 °C a +30 °C) en los sensores detectados
    for (uint8_t i = 0; i < asignados; i++) {
        gestorSensores.configurarAlarmasSlot(i, 30, -20, false);
    }
}

void loop() {
    imprimirReporteCiclo();
    delay(3000);
}