#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include <string>
#include "CapaEnlace.hpp"
#include "../physic/medio/CanalMock.hpp"
#include "../utils/trazador.hpp"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

using namespace OSI;
using namespace OSI::Debug;

CapaFisica troncoFisico;

CanalMock<1024> mockUsbCrudo("MOCK_USB", TipoMedio::USB_UART, ModoDuplex::HALF_DUPLEX, true);
CanalMock<1024> mockBtSeguro("MOCK_BT", TipoMedio::BLUETOOTH_SPP, ModoDuplex::FULL_DUPLEX, false);

CapaEnlace enlaceUsb;
CapaEnlace enlaceBt;

int idUsb = -1;
int idBt  = -1;

static BufferPaquete pktTx;
static BufferPaquete pktRx;
static uint8_t       bufferCable[512];
static uint8_t       datosPrueba[300];

// Ceba el handshake SYNC inicial antes de las pruebas en loopback de una sola instancia
void cebarHandshakeSyncLoopback() {
    enlaceUsb.reiniciarEmisor();
    enlaceUsb.habilitarArqEnlace(true);
    mockUsbCrudo.reiniciarMetricas();

    const uint8_t msgPrime[] = "PRIME";
    pktTx.escribirPayload(msgPrime, sizeof(msgPrime));
    enlaceUsb.bajarTrama(pktTx);

    for (int i = 0; i < 6; i++) {
        size_t n = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
        if (n > 0) mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, n);
        enlaceUsb.subirTrama(pktRx);
    }

    enlaceUsb.reiniciarMetricas();
    mockUsbCrudo.reiniciarMetricas();
}

bool probarTest1PassthroughBT() {
    const uint8_t datosBt[] = { 0x7E, 0x7D, 0xFF, 0x10, 0x20, 0x30 };
    pktTx.escribirPayload(datosBt, sizeof(datosBt));

    imprimirSalida(pktTx, Serial, "Enlace TX -> BT (payload de capa superior)");

    bool okTxBt = enlaceBt.bajarTrama(pktTx);
    size_t lenCableBt = mockBtSeguro.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    imprimirSalida(bufferCable, lenCableBt, Serial, "Cable BT (passthrough, sin HDLC)");

    mockBtSeguro.inyectarRxDesdeExterior(bufferCable, lenCableBt);
    bool okRxBt = enlaceBt.subirTrama(pktRx);

    if (okRxBt) imprimirEntrada(pktRx, Serial, "Enlace RX <- BT (passthrough)");

    bool pass = okTxBt && okRxBt &&
                (lenCableBt == sizeof(datosBt)) &&
                (pktRx.longitud == sizeof(datosBt)) &&
                (memcmp(pktRx.ptrActual(), datosBt, sizeof(datosBt)) == 0) &&
                (enlaceBt.obtenerMetricas().tramasPassthroughTx == 1) &&
                (enlaceBt.obtenerMetricas().tramasPassthroughRx == 1) &&
                (enlaceBt.obtenerMetricas().chunksTx == 0);

    Serial.printf("[TEST 1 - PASSTHROUGH BT] Pasa directo sin alterar bytes: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest2FragmentacionHdlc() {
    for (size_t i = 0; i < 150; i++) {
        if (i % 15 == 0)      datosPrueba[i] = 0x7E;
        else if (i % 15 == 1) datosPrueba[i] = 0x7D;
        else if (i % 15 == 2) datosPrueba[i] = 0xFF;
        else                  datosPrueba[i] = static_cast<uint8_t>(i & 0xFF);
    }

    pktTx.escribirPayload(datosPrueba, 150);
    imprimirSalida(pktTx, Serial, "Enlace TX -> USB (150B de payload)");

    bool okTxUsb = enlaceUsb.bajarTrama(pktTx);
    size_t lenCableUsb = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    imprimirSalida(bufferCable, lenCableUsb, Serial, "Cable USB (3 chunks HDLC + escaping 0x7E/0x7D/0xFF)");

    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenCableUsb);
    bool okRxUsb = enlaceUsb.subirTrama(pktRx);

    if (okRxUsb) imprimirEntrada(pktRx, Serial, "Enlace RX <- USB (150B reensamblados)");

    const MetricasEnlace& mUsb = enlaceUsb.obtenerMetricas();
    bool pass = okTxUsb && okRxUsb &&
                (mUsb.chunksTx == 3) &&
                (mUsb.chunksRxValidos == 3) &&
                (pktRx.longitud == 150) &&
                (memcmp(pktRx.ptrActual(), datosPrueba, 150) == 0);

    Serial.printf("[TEST 2 - FRAGMENTACION + HDLC + CRC16] 150B -> %lu Chunks (%uB en cable): %s\n",
                  (unsigned long)mUsb.chunksTx, (unsigned)lenCableUsb, pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest3CorrupcionCrcNackArq() {
    cebarHandshakeSyncLoopback();

    const uint8_t msgCritico[] = "UMBRAL_HELADERA=-18.5C";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgCritico), sizeof(msgCritico));
    enlaceUsb.bajarTrama(pktTx);

    size_t lenTrama = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, lenTrama, Serial, "Cable USB (frame original intacto)");

    bufferCable[6] ^= 0xFF;
    imprimirSalida(bufferCable, lenTrama, Serial, "Cable USB (frame corrompido: byte[6]^=0xFF)");

    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenTrama);
    bool subioCorrupto = enlaceUsb.subirTrama(pktRx);

    size_t lenNack = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, lenNack, Serial, "NACK numerado emitido por el receptor");

    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenNack);
    enlaceUsb.subirTrama(pktRx);

    size_t lenReenviada = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, lenReenviada, Serial, "Reenvio ARQ (frame original reinyectado)");

    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenReenviada);
    bool subioRecuperado = enlaceUsb.subirTrama(pktRx);

    if (subioRecuperado) imprimirEntrada(pktRx, Serial, "Enlace RX <- payload recuperado");

    mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    const MetricasEnlace& mArq = enlaceUsb.obtenerMetricas();
    bool pass = (!subioCorrupto) &&
                (mArq.erroresCrcChunk == 1) &&
                (mArq.nacksEnviados == 1) &&
                (mArq.nacksRecibidos == 1) &&
                (mArq.reintentosTx == 1) &&
                subioRecuperado &&
                (pktRx.longitud == sizeof(msgCritico)) &&
                (memcmp(pktRx.ptrActual(), msgCritico, sizeof(msgCritico)) == 0);

    Serial.printf("[TEST 3 - CORRUPCION CRC + NACK + REENVIO ARQ] Recuperacion automatica: %s\n", pass ? "PASS" : "FAIL");
    Serial.printf("   -> CRC1_Err:%lu | NACK_Tx:%lu | NACK_Rx:%lu | ReintentosTx:%lu | RxFinalOK:%lu\n",
                  (unsigned long)mArq.erroresCrcChunk,
                  (unsigned long)mArq.nacksEnviados,
                  (unsigned long)mArq.nacksRecibidos,
                  (unsigned long)mArq.reintentosTx,
                  (unsigned long)mArq.tramasProtegidasRx);
    return pass;
}

bool probarTest4CorrupcionChunk1De5SinTormentaNack() {
    cebarHandshakeSyncLoopback();

    for (size_t i = 0; i < sizeof(datosPrueba); i++) {
        datosPrueba[i] = static_cast<uint8_t>(i & 0xFF);
    }
    pktTx.escribirPayload(datosPrueba, sizeof(datosPrueba));
    enlaceUsb.bajarTrama(pktTx);

    size_t lenCable5 = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    // Buscar el 2do SOF (0x7E): [1]=Chunk 0, [2]=Chunk 1
    size_t sofCount = 0, posChunk1 = 0;
    for (size_t i = 0; i < lenCable5; i++) {
        if (bufferCable[i] == CapaEnlace::SOF_BYTE) {
            sofCount++;
            if (sofCount == 2) { posChunk1 = i; break; }
        }
    }
    bufferCable[posChunk1 + 5] ^= 0xFF;
    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenCable5);

    bool rxCorrupto5 = enlaceUsb.subirTrama(pktRx);
    const MetricasEnlace& m4 = enlaceUsb.obtenerMetricas();
    bool pass = (!rxCorrupto5) &&
                (m4.erroresCrcChunk == 1) &&
                (m4.erroresSecuencia == 0) &&
                (m4.nacksEnviados == 1);

    Serial.printf("[TEST 4 - CORRUPCION CHUNK 1 DE 5] Unico NACK sin tormenta: %s (NACKs=%lu, SeqErr=%lu)\n",
                  pass ? "PASS" : "FAIL",
                  (unsigned long)m4.nacksEnviados,
                  (unsigned long)m4.erroresSecuencia);
    return pass;
}

bool probarTest5PerdidaAckTimeout250msDuplicado() {
    cebarHandshakeSyncLoopback();

    const uint8_t msgTimeout[] = "PRUEBA_TIMEOUT_250MS";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgTimeout), sizeof(msgTimeout));
    enlaceUsb.bajarTrama(pktTx);

    size_t lenTmo = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenTmo);

    bool rxPrimeraVez = enlaceUsb.subirTrama(pktRx);

    mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    delay(260);
    enlaceUsb.mantenerEnlace();

    size_t lenReenvioTmo = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenReenvioTmo);

    bool rxSegundaVez = enlaceUsb.subirTrama(pktRx);

    size_t lenAckFinal = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, lenAckFinal);
    enlaceUsb.subirTrama(pktRx);

    const MetricasEnlace& m5 = enlaceUsb.obtenerMetricas();
    bool pass = rxPrimeraVez && (!rxSegundaVez) &&
                (lenReenvioTmo > 0) &&
                (m5.reintentosTx == 1) &&
                (m5.tramasDuplicadasRx == 1) &&
                (m5.acksRecibidos == 1) &&
                (!enlaceUsb.estaEsperandoAck());

    Serial.printf("[TEST 5 - PERDIDA ACK + TIMEOUT 250MS + DUPLICADO] %s (Reintentos=%lu, Duplicados=%lu)\n",
                  pass ? "PASS" : "FAIL",
                  (unsigned long)m5.reintentosTx,
                  (unsigned long)m5.tramasDuplicadasRx);
    return pass;
}

bool probarTest6ReinicioEmisorAntiFalsoDuplicado() {
    enlaceUsb.reiniciarEmisor();
    mockUsbCrudo.reiniciarMetricas();

    const uint8_t msgPostReboot[] = "MENSAJE_TRAS_REBOOT";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgPostReboot), sizeof(msgPostReboot));
    enlaceUsb.bajarTrama(pktTx);

    bool rxPostRebootOk = false;
    for (int i = 0; i < 6; i++) {
        size_t n = mockUsbCrudo.extraerTxCapturado(bufferCable, sizeof(bufferCable));
        if (n > 0) mockUsbCrudo.inyectarRxDesdeExterior(bufferCable, n);
        if (enlaceUsb.subirTrama(pktRx)) {
            rxPostRebootOk = true;
        }
    }

    bool pass = rxPostRebootOk &&
                (pktRx.longitud == sizeof(msgPostReboot)) &&
                (memcmp(pktRx.ptrActual(), msgPostReboot, sizeof(msgPostReboot)) == 0);

    Serial.printf("[TEST 6 - REINICIO DEL EMISOR (HANDSHAKE SYNC)] %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// =============================================================================
// PRUEBAS CON DOS NODOS ENFRENTADOS (test-enlace-dos-nodos)
// =============================================================================
struct NodoEnlaceTest {
    CapaFisica      f;
    CanalMock<1024> m{"M", TipoMedio::USB_UART, ModoDuplex::FULL_DUPLEX, true};
    CapaEnlace      e;
    int             id = -1;
    std::vector<std::string> rx;

    void iniciar() {
        f.limpiarRaices();
        m.reiniciarMetricas();
        id = f.registrarRaiz(&m);
        f.inicializarTodas();
        e.configurarEnlace(&f, id, true);
        rx.clear();
    }

    void reboot() {
        e = CapaEnlace();
        e.configurarEnlace(&f, id, true);
    }

    void poll() {
        for (;;) {
            if (!e.subirTrama(pktRx)) break;
            rx.push_back(std::string(reinterpret_cast<const char*>(pktRx.ptrActual())));
        }
    }
};

static NodoEnlaceTest nodoA;
static NodoEnlaceTest nodoB;
static uint8_t bufC1[512], bufC2[512], bufA1[64], bufA2[64];

static size_t takeNodo(NodoEnlaceTest& n, uint8_t* b, size_t cap = 512) {
    return n.m.extraerTxCapturado(b, cap);
}

static void putNodo(NodoEnlaceTest& n, const uint8_t* b, size_t l) {
    if (l > 0) n.m.inyectarRxDesdeExterior(b, l);
}

static bool sendNodo(NodoEnlaceTest& n, const char* s) {
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(s), strlen(s) + 1);
    return n.e.bajarTrama(pktTx);
}

static void pumpNodos(NodoEnlaceTest& A, NodoEnlaceTest& B, int ciclos = 6) {
    for (int i = 0; i < ciclos; i++) {
        size_t l = takeNodo(A, bufferCable);
        putNodo(B, bufferCable, l);
        B.poll();
        l = takeNodo(B, bufferCable);
        putNodo(A, bufferCable, l);
        A.poll();
    }
}

static int countRx(const NodoEnlaceTest& n, const char* s) {
    int c = 0;
    for (const auto& x : n.rx) {
        if (x == s) c++;
    }
    return c;
}

bool probarTest7DosNodosAckRetrasado() {
    nodoA.iniciar();
    nodoB.iniciar();

    sendNodo(nodoA, "PRIME");
    pumpNodos(nodoA, nodoB);

    sendNodo(nodoA, "MSG1");
    size_t l1 = takeNodo(nodoA, bufC1);
    delay(260);
    nodoA.e.mantenerEnlace();
    size_t l2 = takeNodo(nodoA, bufC2);

    putNodo(nodoB, bufC1, l1);
    nodoB.poll();
    size_t la1 = takeNodo(nodoB, bufA1, sizeof(bufA1));

    putNodo(nodoB, bufC2, l2);
    nodoB.poll();
    size_t la2 = takeNodo(nodoB, bufA2, sizeof(bufA2));

    putNodo(nodoA, bufA1, la1);
    nodoA.poll();

    sendNodo(nodoA, "MSG2");
    takeNodo(nodoA, bufferCable); // MSG2 se pierde en el cable

    putNodo(nodoA, bufA2, la2);   // Llega el ACK viejo (del duplicado de MSG1)
    nodoA.poll();

    bool sigueEsperandoMsg2 = nodoA.e.estaEsperandoAck();

    delay(260);
    nodoA.e.mantenerEnlace();
    size_t lr = takeNodo(nodoA, bufferCable);
    putNodo(nodoB, bufferCable, lr);
    nodoB.poll();

    bool pass = sigueEsperandoMsg2 &&
                (countRx(nodoB, "MSG2") == 1) &&
                (countRx(nodoB, "MSG1") == 1);

    Serial.printf("[TEST 7 - 2 NODOS S1: ACK RETRASADO (>250MS)] %s (EsperandoAck=%d, MSG1=%d, MSG2=%d)\n",
                  pass ? "PASS" : "FAIL",
                  sigueEsperandoMsg2 ? 1 : 0,
                  countRx(nodoB, "MSG1"),
                  countRx(nodoB, "MSG2"));
    return pass;
}

bool probarTest8DosNodosBootLoop() {
    nodoA.iniciar();
    nodoB.iniciar();

    sendNodo(nodoA, "UNO");
    pumpNodos(nodoA, nodoB);

    nodoA.reboot();
    sendNodo(nodoA, "DOS");
    pumpNodos(nodoA, nodoB);

    nodoA.reboot();
    sendNodo(nodoA, "TRES");
    pumpNodos(nodoA, nodoB);

    bool pass = (countRx(nodoB, "UNO") == 1) &&
                (countRx(nodoB, "DOS") == 1) &&
                (countRx(nodoB, "TRES") == 1);

    Serial.printf("[TEST 8 - 2 NODOS S2: BOOT-LOOP 2 REINICIOS SEGUIDOS] %s (UNO=%d, DOS=%d, TRES=%d)\n",
                  pass ? "PASS" : "FAIL",
                  countRx(nodoB, "UNO"),
                  countRx(nodoB, "DOS"),
                  countRx(nodoB, "TRES"));
    return pass;
}

bool probarTest9DosNodosSyncPerdido() {
    nodoA.iniciar();
    nodoB.iniciar();

    sendNodo(nodoA, "UNO");
    pumpNodos(nodoA, nodoB);

    nodoA.reboot();
    sendNodo(nodoA, "DOS");
    takeNodo(nodoA, bufferCable); // Se pierde el primer SYNC en el cable

    delay(260);
    nodoA.e.mantenerEnlace();
    pumpNodos(nodoA, nodoB);
    pumpNodos(nodoA, nodoB);

    bool pass = (countRx(nodoB, "DOS") == 1);
    Serial.printf("[TEST 9 - 2 NODOS S2b: SYNC PERDIDO EN EL CABLE] %s (DOS=%d)\n",
                  pass ? "PASS" : "FAIL",
                  countRx(nodoB, "DOS"));
    return pass;
}

bool probarTest10DosNodosAckPerdidoYRetransmisionCorrupta() {
    nodoA.iniciar();
    nodoB.iniciar();

    sendNodo(nodoA, "PRIME");
    pumpNodos(nodoA, nodoB);

    sendNodo(nodoA, "ORDEN_UNICA");
    size_t l = takeNodo(nodoA, bufferCable);
    putNodo(nodoB, bufferCable, l);
    nodoB.poll();
    takeNodo(nodoB, bufferCable); // El ACK se pierde

    delay(260);
    nodoA.e.mantenerEnlace();
    l = takeNodo(nodoA, bufferCable);
    bufferCable[6] ^= 0xFF;       // La retransmisión se corrompe en el cable
    putNodo(nodoB, bufferCable, l);
    nodoB.poll();

    size_t ln = takeNodo(nodoB, bufferCable); // NACK de B -> A
    putNodo(nodoA, bufferCable, ln);
    nodoA.poll();

    l = takeNodo(nodoA, bufferCable);         // Reenvío limpio de A -> B
    putNodo(nodoB, bufferCable, l);
    nodoB.poll();
    takeNodo(nodoB, bufferCable);

    bool pass = (countRx(nodoB, "ORDEN_UNICA") == 1) &&
                (nodoB.e.obtenerMetricas().tramasDuplicadasRx == 1);

    Serial.printf("[TEST 10 - 2 NODOS S3: ACK PERDIDO + RETRANSMISION CORRUPTA] %s (Entregas=%d, DupDescartadas=%lu)\n",
                  pass ? "PASS" : "FAIL",
                  countRx(nodoB, "ORDEN_UNICA"),
                  (unsigned long)nodoB.e.obtenerMetricas().tramasDuplicadasRx);
    return pass;
}

void ejecutarPruebasCapa2() {
    Serial.println("\n======================================================================");
    Serial.println(" VERIFICACION CAPA 2 (ENLACE) - CAJA NEGRA, CAJA BLANCA Y DOS NODOS");
    Serial.println("======================================================================");

    UBaseType_t hwmInicio = uxTaskGetStackHighWaterMark(nullptr);

    if (!probarTest1PassthroughBT())                             { Serial.println("[ABORT] Fallo en TEST 1"); return; }
    if (!probarTest2FragmentacionHdlc())                         { Serial.println("[ABORT] Fallo en TEST 2"); return; }
    if (!probarTest3CorrupcionCrcNackArq())                      { Serial.println("[ABORT] Fallo en TEST 3"); return; }
    if (!probarTest4CorrupcionChunk1De5SinTormentaNack())        { Serial.println("[ABORT] Fallo en TEST 4"); return; }
    if (!probarTest5PerdidaAckTimeout250msDuplicado())           { Serial.println("[ABORT] Fallo en TEST 5"); return; }
    if (!probarTest6ReinicioEmisorAntiFalsoDuplicado())          { Serial.println("[ABORT] Fallo en TEST 6"); return; }
    if (!probarTest7DosNodosAckRetrasado())                      { Serial.println("[ABORT] Fallo en TEST 7"); return; }
    if (!probarTest8DosNodosBootLoop())                          { Serial.println("[ABORT] Fallo en TEST 8"); return; }
    if (!probarTest9DosNodosSyncPerdido())                       { Serial.println("[ABORT] Fallo en TEST 9"); return; }
    if (!probarTest10DosNodosAckPerdidoYRetransmisionCorrupta()) { Serial.println("[ABORT] Fallo en TEST 10"); return; }

    UBaseType_t hwmFin = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("[STACK TELEMETRIA] HighWaterMark Inicio: %u B | Fin: %u B\n",
                  (unsigned)(hwmInicio * sizeof(StackType_t)),
                  (unsigned)(hwmFin * sizeof(StackType_t)));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    troncoFisico.limpiarRaices();
    idUsb = troncoFisico.registrarRaiz(&mockUsbCrudo);
    idBt  = troncoFisico.registrarRaiz(&mockBtSeguro);
    troncoFisico.inicializarTodas();

    enlaceUsb.configurarEnlace(&troncoFisico, idUsb, false);
    enlaceBt.configurarEnlace(&troncoFisico, idBt, false);

    ejecutarPruebasCapa2();
}

void loop() {
    delay(10000);
}