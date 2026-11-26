#include <Arduino.h>
#include "GestorSensores.hpp"

void imprimirReporteCiclo() {
    EstadoBus estBus = gestorSensores.verificarEstadoElectricoBus();
    if (estBus == EstadoBus::LINEA_EN_CORTO_GND) {
        Serial.printf("[GESTOR ERR] EstadoBus: %s | Evento: %s\n",
                      GestorSensores::nombreEstadoBus(estBus),
                      GestorSensores::nombreEventoGestor(gestorSensores.obtenerUltimoEvento()));
        return;
    }

    if (gestorSensores.verificarCambiosEnCaliente()) {
        Serial.printf("[GESTOR EVT] %s -> Reasignando sensores...\n",
                      GestorSensores::nombreEventoGestor(gestorSensores.obtenerUltimoEvento()));
        gestorSensores.escanearYAsignarSensores(12, false);
    }

    gestorSensores.actualizarTodosLosSlots();
    uint8_t enAlarmaHw = gestorSensores.procesarBusquedaAlarmasHardware();

    Serial.println("\n======================================================================");
    Serial.printf(" GESTOR | Bus:%s | Evento:%s | EnBus:%u | Activos:%u/6 | Alarmas:%u\n",
                  GestorSensores::nombreEstadoBus(gestorSensores.obtenerEstadoBus()),
                  GestorSensores::nombreEventoGestor(gestorSensores.obtenerUltimoEvento()),
                  gestorSensores.obtenerTotalEnBus(),
                  gestorSensores.obtenerTotalAsignados(),
                  enAlarmaHw);
    Serial.println("======================================================================");

    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        const Sensor& s = gestorSensores.obtenerSensor(i);
        if (!s.estaActivo()) {
            Serial.printf("Slot [%u] -> [VACIO]\n", i);
            continue;
        }

        Serial.printf("Slot [%u] | MAC:%s (MAC_2:%s) | Fam:%s | Estado:%s\n",
                      s.obtenerSlot(),
                      s.obtenerMAC(),
                      s.obtenerMAC2(),
                      GestorSensores::nombreFamilia(s.obtenerFamilia()),
                      GestorSensores::nombreEstadoSensor(s.obtenerEstado()));

        Serial.printf("         | Temp: %.2f C (RAW:%ld) | Offset: %.2f C | Umbral:[%d C .. %d C] | Alarma:%s\n",
                      s.obtenerTempCelsius(),
                      static_cast<long>(s.obtenerTempRaw()),
                      s.obtenerOffset(),
                      s.obtenerUmbralInferior(),
                      s.obtenerUmbralSuperior(),
                      s.estaEnAlarma() ? "SI" : "NO");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    uint8_t asignados = gestorSensores.escanearYAsignarSensores(12, false);

    Serial.printf("[INIT] Sensores detectados e inicializados (-126C a 126C, offset 0): %u/6\n",
                  asignados);
}

void loop() {
    imprimirReporteCiclo();
    delay(3000);
}