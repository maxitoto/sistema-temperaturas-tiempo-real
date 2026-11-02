#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "CapaTransporte.hpp"
#include "../physic/medio/CanalMock.hpp"
#include "../utils/trazador.hpp"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

using namespace OSI;
using namespace OSI::Debug;

CapaFisica     troncoFisico;
CapaRed        red;
CapaTransporte transporte(&red);

CanalMock<1024> mockUsb("MOCK_USB", TipoMedio::USB_UART, ModoDuplex::FULL_DUPLEX, true);
CanalMock<1024> mockWifi("MOCK_WIFI", TipoMedio::WIFI_UDP, ModoDuplex::FULL_DUPLEX, true);
CanalMock<1024> mockBt("MOCK_BT", TipoMedio::BLUETOOTH_SPP, ModoDuplex::FULL_DUPLEX, false);

int idUsb  = -1;
int idWifi = -1;
int idBt   = -1;

// Buffers de trabajo estáticos fuera del stack de loopTask
static BufferPaquete pktTx;
static BufferPaquete pktTx2;
static BufferPaquete pktRx;
static SegmentoTransporte metaRx;
static uint8_t bufferCable[512];

void loopbackCanal(CanalMock<1024>& mock) {
    size_t n = mock.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    if (n > 0) {
        imprimirSalida(bufferCable, n, Serial, "Cable (loopback)");
        mock.inyectarRxDesdeExterior(bufferCable, n);
    }
}

bool probarTest1PassthroughBT() {
    const uint8_t msgBt[] = "RPC_DIRECTO_POR_BT";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgBt), sizeof(msgBt), LimitesPila::OFFSET_SESION);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P1 - Transp TX -> BT (payload, antes de passthrough)");

    bool okTxBt = transporte.bajarSegmentoUnicast(idBt, CapaTransporte::PROTO_TCP,
                                                  CapaTransporte::PUERTO_RPC, CapaTransporte::PUERTO_RPC, pktTx);
    size_t bytesEnAireBt = mockBt.obtenerBytesTxEnEspera();
    Serial.printf("[TRAZA] Bytes en cable BT = %u (payload = %u)\n", (unsigned)bytesEnAireBt, (unsigned)sizeof(msgBt));

    loopbackCanal(mockBt);

    bool okRxBt = transporte.subirSegmento(pktRx, metaRx);
    if (okRxBt) {
        imprimirEntrada(pktRx, Serial, "P1 - Transp RX <- BT (payload entregado)");
    }

    bool pass = okTxBt && okRxBt &&
                (bytesEnAireBt == sizeof(msgBt)) &&
                (metaRx.fuePassthrough == true) &&
                (pktRx.longitud == sizeof(msgBt)) &&
                (memcmp(pktRx.ptrActual(), msgBt, sizeof(msgBt)) == 0);

    Serial.printf("[TEST 1 - PASSTHROUGH BT] 0B overhead y entrega directa: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest2UdpMultiplexacionTsap() {
    const uint8_t msgUdp[] = "TELEMETRIA_SENSOR_1=-18.4C";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgUdp), sizeof(msgUdp), LimitesPila::OFFSET_SESION);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P2 - UDP TX pto=81 (payload)");

    // En loopback sobre idWifi (destino lógico 1), el nodo receptor tiene dirección lógica 1
    red.establecerDireccionLocal(CapaRed::NODO_SERVIDOR_LOCAL);
    transporte.bajarSegmentoUnicast(idWifi, CapaTransporte::PROTO_UDP,
                                    CapaTransporte::PUERTO_EVENTOS, CapaTransporte::PUERTO_EVENTOS, pktTx);
    loopbackCanal(mockWifi);

    red.establecerDireccionLocal(static_cast<uint8_t>(idWifi));
    bool okUdpAbierto = transporte.subirSegmento(pktRx, metaRx);
    if (okUdpAbierto) {
        imprimirEntrada(pktRx, Serial, "P2 - UDP RX pto=81 OK (payload tras 10B consumidos)");
    }

    // Enviamos a un puerto TSAP cerrado (puerto 99) -> Debe rechazarse
    red.establecerDireccionLocal(CapaRed::NODO_SERVIDOR_LOCAL);
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(msgUdp), sizeof(msgUdp), LimitesPila::OFFSET_SESION);
    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P2 - UDP TX pto=99 (payload; puerto destino CERRADO)");

    transporte.bajarSegmentoUnicast(idWifi, CapaTransporte::PROTO_UDP,
                                    CapaTransporte::PUERTO_EVENTOS, 99, pktTx);
    loopbackCanal(mockWifi);

    red.establecerDireccionLocal(static_cast<uint8_t>(idWifi));
    bool okUdpCerrado = transporte.subirSegmento(pktRx, metaRx);
    red.establecerDireccionLocal(CapaRed::NODO_SERVIDOR_LOCAL);

    Serial.printf("[TRAZA] subirSegmento pto=99 devolvio: %s\n", okUdpCerrado ? "true (INESPERADO)" : "false (rechazo correcto)");

    bool pass = okUdpAbierto && (!okUdpCerrado) &&
                (transporte.obtenerMetricas().datagramasUdpRx == 1) &&
                (transporte.obtenerMetricas().rechazosPuertoCerrado == 1);

    Serial.printf("[TEST 2 - UDP + MULTIPLEXACION TSAP] Puerto 81 OK / Puerto 99 Rechazado: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest3TcpHandshakeColaRto() {
    transporte.fijarIsnParaTests(100);
    transporte.configurarTemporizadores(40, 200);

    // 1. SYN del cliente (seq = 500)
    pktTx.prepararParaTx(idUsb);
    pktTx.inicio = LimitesPila::OFFSET_SESION;
    uint8_t* hdrSyn = pktTx.empujarCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
    hdrSyn[0] = CapaTransporte::PROTO_TCP;
    hdrSyn[1] = CapaTransporte::PUERTO_RPC;
    hdrSyn[2] = CapaTransporte::PUERTO_RPC;
    hdrSyn[3] = CapaTransporte::FLAG_SYN;
    hdrSyn[4] = 0x01; hdrSyn[5] = 0xF4;
    hdrSyn[6] = 0x00; hdrSyn[7] = 0x00;
    uint16_t crcSyn = calcularCRC16(hdrSyn, 8);
    hdrSyn[8] = static_cast<uint8_t>(crcSyn >> 8);
    hdrSyn[9] = static_cast<uint8_t>(crcSyn & 0xFF);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P3 - SYN cliente [proto=06 pO=80 pD=80 flags=01 seq=0x01F4 ack=0]");

    red.bajarPaqueteUnicast(idUsb, pktTx);
    loopbackCanal(mockUsb);

    transporte.subirSegmento(pktRx, metaRx);
    bool enSynRecibido = (transporte.obtenerEstadoTCP(idUsb) == CapaTransporte::TCP_SYN_RECIBIDO);
    Serial.printf("[TRAZA] tras SYN -> estado TCP: %s\n", enSynRecibido ? "SYN_RECIBIDO" : "OTRO");

    mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    // 2. ACK final del cliente (seq = 501, ack = 101)
    pktTx.prepararParaTx(idUsb);
    pktTx.inicio = LimitesPila::OFFSET_SESION;
    uint8_t* hdrAck = pktTx.empujarCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
    hdrAck[0] = CapaTransporte::PROTO_TCP;
    hdrAck[1] = CapaTransporte::PUERTO_RPC;
    hdrAck[2] = CapaTransporte::PUERTO_RPC;
    hdrAck[3] = CapaTransporte::FLAG_ACK;
    hdrAck[4] = 0x01; hdrAck[5] = 0xF5;
    hdrAck[6] = 0x00; hdrAck[7] = 0x65;
    uint16_t crcAck = calcularCRC16(hdrAck, 8);
    hdrAck[8] = static_cast<uint8_t>(crcAck >> 8);
    hdrAck[9] = static_cast<uint8_t>(crcAck & 0xFF);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P3 - ACK cliente [flags=02 seq=0x01F5 ack=0x0065]");

    red.bajarPaqueteUnicast(idUsb, pktTx);
    loopbackCanal(mockUsb);
    transporte.subirSegmento(pktRx, metaRx);

    bool establecidoOk = (transporte.obtenerEstadoTCP(idUsb) == CapaTransporte::TCP_ESTABLECIDO);
    Serial.printf("[TRAZA] tras ACK -> estado TCP: %s\n", establecidoOk ? "ESTABLECIDO" : "OTRO");

    // 3. Envío de 2 segmentos: pkt1 sale en vuelo y pkt2 queda en cola Stop-and-Wait
    const uint8_t payload1[] = "CMD_SET_UMBRAL=-20";
    const uint8_t payload2[] = "CMD_SAVE_EEPROM=1";
    pktTx.escribirPayload(reinterpret_cast<const uint8_t*>(payload1), sizeof(payload1), LimitesPila::OFFSET_SESION);
    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P3 - pkt1 TX (sale directo)");

    pktTx2.escribirPayload(reinterpret_cast<const uint8_t*>(payload2), sizeof(payload2), LimitesPila::OFFSET_SESION);
    imprimirSalida(pktTx2.ptrActual(), pktTx2.longitud, Serial, "P3 - pkt2 TX (quedara en COLA porque pkt1 espera ACK)");

    bool send1 = transporte.bajarSegmentoUnicast(idUsb, CapaTransporte::PROTO_TCP,
                                                 CapaTransporte::PUERTO_RPC, CapaTransporte::PUERTO_RPC, pktTx);
    bool send2 = transporte.bajarSegmentoUnicast(idUsb, CapaTransporte::PROTO_TCP,
                                                 CapaTransporte::PUERTO_RPC, CapaTransporte::PUERTO_RPC, pktTx2);

    mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    delay(50);
    transporte.mantenerTransporte();

    bool huboRto = (transporte.obtenerMetricas().retransmisionesRto == 1) &&
                   (mockUsb.obtenerBytesTxEnEspera() > 0);

    Serial.printf("[TRAZA] RTO: retransmisiones=%lu, bytesEnEspera=%u\n",
                  (unsigned long)transporte.obtenerMetricas().retransmisionesRto,
                  (unsigned)mockUsb.obtenerBytesTxEnEspera());

    bool pass = enSynRecibido && establecidoOk && send1 && send2 && huboRto;
    Serial.printf("[TEST 3 - TCP HANDSHAKE + COLA FLUJO + RTO] Recuperacion por timeout: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest4ConexionSemiabierta() {
    mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    // Confirmar pkt1 (ack = 102) -> desencola y transmite pkt2
    pktTx.prepararParaTx(idUsb);
    pktTx.inicio = LimitesPila::OFFSET_SESION;
    uint8_t* hdrAck = pktTx.empujarCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
    hdrAck[0] = CapaTransporte::PROTO_TCP;
    hdrAck[1] = CapaTransporte::PUERTO_RPC;
    hdrAck[2] = CapaTransporte::PUERTO_RPC;
    hdrAck[3] = CapaTransporte::FLAG_ACK;
    hdrAck[4] = 0x01; hdrAck[5] = 0xF5;
    hdrAck[6] = 0x00; hdrAck[7] = 0x66;
    uint16_t crcAck = calcularCRC16(hdrAck, 8);
    hdrAck[8] = static_cast<uint8_t>(crcAck >> 8);
    hdrAck[9] = static_cast<uint8_t>(crcAck & 0xFF);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P4 - ACK=102 (confirma pkt1 -> dispara pkt2 de la cola)");

    red.bajarPaqueteUnicast(idUsb, pktTx);
    loopbackCanal(mockUsb);
    transporte.subirSegmento(pktRx, metaRx);

    mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    // Confirmar pkt2 (ack = 103)
    hdrAck[6] = 0x00; hdrAck[7] = 0x67;
    crcAck = calcularCRC16(hdrAck, 8);
    hdrAck[8] = static_cast<uint8_t>(crcAck >> 8);
    hdrAck[9] = static_cast<uint8_t>(crcAck & 0xFF);

    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P4 - ACK=103 (confirma pkt2)");

    red.bajarPaqueteUnicast(idUsb, pktTx);
    loopbackCanal(mockUsb);
    transporte.subirSegmento(pktRx, metaRx);

    delay(220);
    transporte.mantenerTransporte();

    const MetricasTransporte& m = transporte.obtenerMetricas();
    bool pass = (transporte.obtenerEstadoTCP(idUsb) == CapaTransporte::TCP_CERRADO) &&
                (m.cierresSemiabiertosTimeout == 1);

    Serial.printf("[TEST 4 - CONEXION SEMIABIERTA] Liberacion automatica por inactividad: %s\n", pass ? "PASS" : "FAIL");
    Serial.printf("   -> Handshakes:%lu | TCP_Tx:%lu | RTO_Reenvios:%lu | SemiabiertasCerradas:%lu\n",
                  (unsigned long)m.handshakesEstablecidos,
                  (unsigned long)m.segmentosTcpDatosTx,
                  (unsigned long)m.retransmisionesRto,
                  (unsigned long)m.cierresSemiabiertosTimeout);
    return pass;
}

void ejecutarPruebasCapa4() {
    Serial.println("\n======================================================================");
    Serial.println(" VERIFICACION CAPA 4 (TRANSPORTE) - CAJA NEGRA Y CAJA BLANCA");
    Serial.println("======================================================================");

    UBaseType_t hwmInicio = uxTaskGetStackHighWaterMark(nullptr);

    if (!probarTest1PassthroughBT())         { Serial.println("[ABORT] Fallo en TEST 1"); return; }
    if (!probarTest2UdpMultiplexacionTsap()) { Serial.println("[ABORT] Fallo en TEST 2"); return; }
    if (!probarTest3TcpHandshakeColaRto())   { Serial.println("[ABORT] Fallo en TEST 3"); return; }
    if (!probarTest4ConexionSemiabierta())   { Serial.println("[ABORT] Fallo en TEST 4"); return; }

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
    transporte.vincularRed(&red);

    ejecutarPruebasCapa4();
}

void loop() {
    delay(10000);
}