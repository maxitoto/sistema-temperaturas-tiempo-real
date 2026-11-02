#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "CapaRed.h"
#include "../physic/medio/CanalMock.hpp"
#include "../utils/trazador.hpp"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

using namespace OSI;
using namespace OSI::Debug;

CapaFisica troncoFisico;
CapaRed    red;

CanalMock<1024> mockUsb("MOCK_USB", TipoMedio::USB_UART, ModoDuplex::FULL_DUPLEX, true);
CanalMock<1024> mockWifi("MOCK_WIFI", TipoMedio::WIFI_UDP, ModoDuplex::FULL_DUPLEX, true);
CanalMock<1024> mockBt("MOCK_BT", TipoMedio::BLUETOOTH_SPP, ModoDuplex::FULL_DUPLEX, false);

int idUsb  = -1;
int idWifi = -1;
int idBt   = -1;

static BufferPaquete pktTx;
static BufferPaquete pktRx;
static uint8_t       bufferCable[256];

void ejecutarPruebasCapa3() {
    Serial.println("\n======================================================================");
    Serial.println(" VERIFICACION CAPA 3 (RED) - CAJA NEGRA Y CAJA BLANCA");
    Serial.println("======================================================================");

    UBaseType_t hwmInicio = uxTaskGetStackHighWaterMark(nullptr);

    // -------------------------------------------------------------------------
    // PRUEBA 1: Passthrough en Bluetooth (0 bytes de cabecera de Red agregados)
    // -------------------------------------------------------------------------
    const uint8_t payloadBt[] = { 0x11, 0x22, 0x33, 0x44 };
    pktTx.escribirPayload(payloadBt, sizeof(payloadBt), LimitesPila::OFFSET_TRANSPORTE);

    imprimirSalida(pktTx, Serial, "Red TX -> BT (payload; sin cabecera de red)");

    bool okTxBt = red.bajarPaqueteUnicast(idBt, pktTx);
    size_t lenCableBt = mockBt.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    imprimirSalida(bufferCable, lenCableBt, Serial, "Cable BT (passthrough, 0B de cabecera Red)");

    mockBt.inyectarRxDesdeExterior(bufferCable, lenCableBt);
    bool okRxBt = red.subirPaquete(pktRx);

    if (okRxBt) imprimirEntrada(pktRx, Serial, "Red RX <- BT (passthrough, sin cabecera)");

    bool test1Pass = okTxBt && okRxBt &&
                     (lenCableBt == sizeof(payloadBt)) &&
                     (pktRx.longitud == sizeof(payloadBt)) &&
                     (memcmp(pktRx.ptrActual(), payloadBt, sizeof(payloadBt)) == 0) &&
                     (red.obtenerUltimoMetaRx().fuePassthrough == true);

    Serial.printf("[TEST 1 - PASSTHROUGH BT] Pasa de largo sin cabecera de Red: %s\n",
                  test1Pass ? "PASS" : "FAIL");
    if (!test1Pass) return;

    // -------------------------------------------------------------------------
    // PRUEBA 2: Red Propia en USB (Cabecera 2B [Origen=0|Destino=0][TTL=8])
    // -------------------------------------------------------------------------
    const uint8_t payloadUsb[] = "PING_CAPA_3";
    pktTx.escribirPayload(payloadUsb, sizeof(payloadUsb), LimitesPila::OFFSET_TRANSPORTE);

    imprimirSalida(pktTx, Serial, "Red TX -> USB (payload de capa superior)");

    bool okTxUsb = red.bajarPaqueteUnicast(idUsb, pktTx);
    size_t lenCableUsb = mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    imprimirSalida(bufferCable, lenCableUsb, Serial, "Cable USB (HDLC enlace + cabecera Red 2B + payload)");

    mockUsb.inyectarRxDesdeExterior(bufferCable, lenCableUsb);
    bool okRxUsb = red.subirPaquete(pktRx);

    if (okRxUsb) imprimirEntrada(pktRx, Serial, "Red RX <- USB (payload tras consumir Red 2B)");

    const MetadatosRedRx& metaUsb = red.obtenerUltimoMetaRx();
    bool test2Pass = okTxUsb && okRxUsb &&
                     (metaUsb.fuePassthrough == false) &&
                     (metaUsb.origenLogico == CapaRed::NODO_SERVIDOR_LOCAL) &&
                     (metaUsb.ttlRecibido == CapaRed::DEFAULT_TTL) &&
                     (pktRx.longitud == sizeof(payloadUsb)) &&
                     (memcmp(pktRx.ptrActual(), payloadUsb, sizeof(payloadUsb)) == 0);

    Serial.printf("[TEST 2 - RED PROPIA USB] Cabecera 2B + Direccionamiento + TTL=%u: %s\n",
                  metaUsb.ttlRecibido, test2Pass ? "PASS" : "FAIL");
    if (!test2Pass) return;

    // -------------------------------------------------------------------------
    // PRUEBA 3: Encaminamiento entre subredes (USB -> Router ESP32 -> WiFi) y salto TTL
    // -------------------------------------------------------------------------
    red.configurarRuta(0x05, idWifi);

    const uint8_t msgTransito[] = "RUTEO_ENTRE_SUBREDES";
    pktTx.escribirPayload(msgTransito, sizeof(msgTransito), LimitesPila::OFFSET_TRANSPORTE);
    uint8_t* hdrTransito = pktTx.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
    hdrTransito[0] = (0x02 << 4) | 0x05;
    hdrTransito[1] = 4;

    {
        const size_t offHdr = LimitesPila::OFFSET_TRANSPORTE - LimitesPila::TAMANO_CABECERA_RED;
        RegionTrama regs[2] = {
            { "Cab Red (org=2 | dst=5) | TTL=4", offHdr, LimitesPila::TAMANO_CABECERA_RED },
            { "Payload: RUTEO_ENTRE_SUBREDES", offHdr + LimitesPila::TAMANO_CABECERA_RED, sizeof(msgTransito) },
        };
        imprimirSalida(pktTx, Serial, "Red TX (transito: org=0x2 -> dst=0x5, TTL=4)", regs, 2);
    }

    red.obtenerEnlace(idUsb)->bajarTrama(pktTx);
    size_t lenEntrada = mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, lenEntrada, Serial, "Cable USB (HDLC + Red hdr + payload)");
    mockUsb.inyectarRxDesdeExterior(bufferCable, lenEntrada);

    bool entregadoLocal = red.subirPaquete(pktRx);
    if (entregadoLocal) {
        imprimirEntrada(pktRx, Serial, "Red RX <- USB (entregado LOCAL)");
    } else {
        Serial.println(F("\n[Red] No entregado localmente -> REENVIO activado\n"));
    }

    size_t lenSalidaWifi = mockWifi.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, lenSalidaWifi, Serial, "Cable WiFi (reenvio de la capa Red, TTL 4 -> 3)");
    mockWifi.inyectarRxDesdeExterior(bufferCable, lenSalidaWifi);

    red.obtenerEnlace(idWifi)->subirTrama(pktRx);
    imprimirEntrada(pktRx, Serial, "Enlace WiFi RX (Red hdr + payload; aun sin consumir)");

    const uint8_t* hdrReenviado = pktRx.consumirCabecera(LimitesPila::TAMANO_CABECERA_RED);

    bool test3Pass = (!entregadoLocal) &&
                     (red.obtenerMetricas().paquetesReenviados == 1) &&
                     (hdrReenviado != nullptr) &&
                     (((hdrReenviado[0] >> 4) & 0x0F) == 0x02) &&
                     ((hdrReenviado[0] & 0x0F) == 0x05) &&
                     (hdrReenviado[1] == 3);

    Serial.printf("[TEST 3 - ENCAMINAMIENTO USB->WIFI] Reenvio automatico + Salto TTL (4->3): %s\n",
                  test3Pass ? "PASS" : "FAIL");
    if (!test3Pass) return;

    // -------------------------------------------------------------------------
    // PRUEBA 4: Descarte por TTL agotado (TTL = 0) y Control de Congestión
    // -------------------------------------------------------------------------
    pktTx.escribirPayload(payloadUsb, sizeof(payloadUsb), LimitesPila::OFFSET_TRANSPORTE);
    bool txTtlCero = red.bajarPaqueteUnicast(idUsb, pktTx, 0);

    red.configurarLimiteCongestion(idUsb, 2);
    bool txCong1 = red.bajarPaqueteUnicast(idUsb, pktTx, 8);
    bool txCong2 = red.bajarPaqueteUnicast(idUsb, pktTx, 8);
    bool txCong3 = red.bajarPaqueteUnicast(idUsb, pktTx, 8);

    const MetricasRed& m = red.obtenerMetricas();
    bool test4Pass = (!txTtlCero) &&
                     (m.erroresTtlAgotado == 1) &&
                     txCong1 && txCong2 && (!txCong3) &&
                     (m.paquetesDescartadosCongestion == 1);

    Serial.printf("[TEST 4 - TTL=0 Y CONTROL DE CONGESTION] Filtro de inundacion: %s\n",
                  test4Pass ? "PASS" : "FAIL");
    Serial.printf("   -> PassthroughRx:%lu | RedPropiaRx:%lu | Reenviados:%lu | TTL_Err:%lu | CongestionDrop:%lu\n",
                  (unsigned long)m.paquetesPassthroughRx,
                  (unsigned long)m.paquetesRedPropiaRx,
                  (unsigned long)m.paquetesReenviados,
                  (unsigned long)m.erroresTtlAgotado,
                  (unsigned long)m.paquetesDescartadosCongestion);

    UBaseType_t hwmFin = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("[STACK TELEMETRIA] HighWaterMark Inicio: %u B | Fin: %u B\n",
                  (unsigned)(hwmInicio * sizeof(StackType_t)),
                  (unsigned)(hwmFin * sizeof(StackType_t)));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    troncoFisico.limpiarRaices();
    idUsb  = troncoFisico.registrarRaiz(&mockUsb);
    idWifi = troncoFisico.registrarRaiz(&mockWifi);
    idBt   = troncoFisico.registrarRaiz(&mockBt);
    troncoFisico.inicializarTodas();

    red.vincularMedios(&troncoFisico, CapaRed::NODO_SERVIDOR_LOCAL);

    ejecutarPruebasCapa3();
}

void loop() {
    delay(10000);
}