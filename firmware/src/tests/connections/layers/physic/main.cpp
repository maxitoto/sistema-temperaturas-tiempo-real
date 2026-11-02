#include <Arduino.h>
#include "CapaFisica.hpp"
#include "medium/CanalMock.hpp"
#include "medium/CanalUSB.hpp"
#include "medium/CanalBluetooth.hpp"
#include "medium/CanalWiFiUDP.hpp"

#include "../utils/trazador.hpp"

using namespace OSI;

CapaFisica troncoFisico;

// Raíces simuladas para pruebas deterministas de Caja Negra y Caja Blanca
CanalMock<256> raizMockUSB ("MOCK_USB_UART", TipoMedio::USB_UART,      ModoDuplex::HALF_DUPLEX, true);
CanalMock<256> raizMockWiFi("MOCK_WIFI_UDP", TipoMedio::WIFI_UDP,      ModoDuplex::FULL_DUPLEX, true);
CanalMock<256> raizMockBT  ("MOCK_BT_SPP",   TipoMedio::BLUETOOTH_SPP, ModoDuplex::FULL_DUPLEX, false);

int idUsb  = -1;
int idWifi = -1;
int idBt   = -1;

void ejecutarPruebasCapa1() {
    using namespace OSI::Debug;          // <-- para llamar a imprimirEntrada/Salida sin prefijos

    Serial.println("\n==================================================================");
    Serial.println(" VERIFICACION CAPA 1 (FISICA) - CAJA NEGRA Y CAJA BLANCA");
    Serial.println("==================================================================");

    raizMockUSB.reiniciarMetricas();
    raizMockWiFi.reiniciarMetricas();
    raizMockBT.reiniciarMetricas();

    // =========================================================================
    // PRUEBA 1 (CAJA NEGRA): Aislamiento de medio (Prohibido hacer Broadcast)
    // =========================================================================
    const uint8_t payloadBT[] = { 0xAA, 0xBB, 0xCC, 0xDD };

    // ---- TRAZA SALIDA (intención): lo que la capa fisica EMPUJA hacia el medio
    imprimirSalida(payloadBT, sizeof(payloadBT), Serial, "Fisica TX -> BT_SPP");

    size_t enviados = troncoFisico.enviarBytes(idBt, payloadBT, sizeof(payloadBT));

    uint8_t capturadoBT[16] = {0};
    size_t leidosBT   = raizMockBT.extraerTxCapturado(capturadoBT, sizeof(capturadoBT));
    size_t fugaEnUSB  = raizMockUSB.obtenerBytesTxEnEspera();
    size_t fugaEnWiFi = raizMockWiFi.obtenerBytesTxEnEspera();

    // ---- TRAZA SALIDA (verificación): lo que REALMENTE salió del medio
    imprimirSalida(capturadoBT, leidosBT, Serial, "Fisica TX <- BT_SPP (capturado)");

    bool okAislamiento = (enviados == 4) &&
                         (leidosBT == 4) &&
                         (memcmp(payloadBT, capturadoBT, 4) == 0) &&
                         (fugaEnUSB == 0) &&
                         (fugaEnWiFi == 0);

    Serial.printf("[CAJA NEGRA 1] Enrutamiento exclusivo a Bluetooth (Sin Broadcast): %s\n",
                  okAislamiento ? "PASS" : "FAIL");
    Serial.printf("   -> Bytes en BT: %u | Fuga en USB: %u | Fuga en WiFi: %u\n",
                  (unsigned)leidosBT, (unsigned)fugaEnUSB, (unsigned)fugaEnWiFi);

    // =========================================================================
    // PRUEBA 2 (CAJA NEGRA): Subida RX independiente por raíz
    // =========================================================================
    const uint8_t tramaEntranteWiFi[] = { 0x7E, 0x01, 0x02, 0x03, 0x04 };
    raizMockWiFi.inyectarRxDesdeExterior(tramaEntranteWiFi, sizeof(tramaEntranteWiFi));

    uint8_t bufferSubida[16] = {0};
    size_t rxDesdeUsb  = troncoFisico.recibirBytes(idUsb, bufferSubida, sizeof(bufferSubida));
    size_t rxDesdeWifi = troncoFisico.recibirBytes(idWifi, bufferSubida, sizeof(bufferSubida));

    // ---- TRAZA ENTRADA: lo que la capa fisica ENTREGA hacia arriba
    imprimirEntrada(bufferSubida, rxDesdeWifi, Serial, "Fisica RX <- WiFi_UDP");

    bool okSubida = (rxDesdeUsb == 0) &&
                    (rxDesdeWifi == 5) &&
                    (memcmp(bufferSubida, tramaEntranteWiFi, 5) == 0);

    Serial.printf("[CAJA NEGRA 2] Subida RX aislada desde raiz WiFi: %s\n",
                  okSubida ? "PASS" : "FAIL");

    // =========================================================================
    // PRUEBA 3 (CAJA BLANCA): Inspección de telemetría interna y propiedades
    // =========================================================================
    const MetricasCanal& mBt   = raizMockBT.obtenerMetricas();
    const MetricasCanal& mWifi = raizMockWiFi.obtenerMetricas();
    const MetricasCanal& mUsb  = raizMockUSB.obtenerMetricas();

    bool okCajaBlanca = (mBt.bytesTx == 4 && mBt.tramasTx == 1) &&
                        (mWifi.bytesRx == 5 && mWifi.tramasRx == 1) &&
                        (mUsb.bytesTx == 0 && mUsb.bytesRx == 0) &&
                        (troncoFisico.esMedioCrudo(idUsb) == true) &&
                        (troncoFisico.esMedioCrudo(idBt) == false);

    Serial.printf("[CAJA BLANCA] Auditoria de contadores y banderas de elevacion: %s\n",
                  okCajaBlanca ? "PASS" : "FAIL");
    Serial.printf("   -> [USB]  Tipo:%u | Crudo:%s | TX:%luB | RX:%luB\n",
                  (unsigned)troncoFisico.obtenerTipoMedio(idUsb),
                  troncoFisico.esMedioCrudo(idUsb) ? "SI" : "NO (Elevado)",
                  (unsigned long)mUsb.bytesTx, (unsigned long)mUsb.bytesRx);
    Serial.printf("   -> [WIFI] Tipo:%u | Crudo:%s | TX:%luB | RX:%luB\n",
                  (unsigned)troncoFisico.obtenerTipoMedio(idWifi),
                  troncoFisico.esMedioCrudo(idWifi) ? "SI" : "NO (Elevado)",
                  (unsigned long)mWifi.bytesTx, (unsigned long)mWifi.bytesRx);
    Serial.printf("   -> [BT]   Tipo:%u | Crudo:%s | TX:%luB | RX:%luB\n",
                  (unsigned)troncoFisico.obtenerTipoMedio(idBt),
                  troncoFisico.esMedioCrudo(idBt) ? "SI" : "NO (Elevado)",
                  (unsigned long)mBt.bytesTx, (unsigned long)mBt.bytesRx);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    troncoFisico.limpiarRaices();
    idUsb  = troncoFisico.registrarRaiz(&raizMockUSB);
    idWifi = troncoFisico.registrarRaiz(&raizMockWiFi);
    idBt   = troncoFisico.registrarRaiz(&raizMockBT);

    troncoFisico.inicializarTodas();
    ejecutarPruebasCapa1();
}

void loop() {
    delay(10000);
}