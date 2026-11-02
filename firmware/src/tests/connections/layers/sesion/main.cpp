#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "CapaSesion.hpp"
#include "../physic/medio/CanalMock.hpp"
#include "../utils/trazador.hpp"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

using namespace OSI;
using namespace OSI::Debug;

// Dos pilas OSI enfrentadas (Cliente App Electron <-> Servidor ESP32 Heladera)
struct PilaSesionNodo {
    CapaFisica      fisica;
    CanalMock<1024> mockUsb{"MOCK_USB", TipoMedio::USB_UART, ModoDuplex::FULL_DUPLEX, true};
    CanalMock<1024> mockBt{"MOCK_BT", TipoMedio::BLUETOOTH_SPP, ModoDuplex::FULL_DUPLEX, false};
    CapaRed         red;
    CapaTransporte  transporte{&red};
    CapaSesion      sesion{&transporte, "CLAVE_MAESTRA_LAB_2026", 500};
    int             idUsb = -1;
    int             idBt  = -1;

    void iniciar(uint8_t dirLocal) {
        fisica.limpiarRaices();
        mockUsb.reiniciarMetricas();
        mockBt.reiniciarMetricas();
        idUsb = fisica.registrarRaiz(&mockUsb);
        idBt  = fisica.registrarRaiz(&mockBt);
        fisica.inicializarTodas();

        red.vincularMedios(&fisica, dirLocal);
        red.configurarRuta(0x00, idUsb);
        red.configurarRuta(0x01, idUsb);

        transporte.vincularRed(&red);
        transporte.fijarIsnParaTests(200);
        sesion.vincularTransporte(&transporte);
        sesion.fijarSemillasParaTests(0xCAFEBABE, 0x1234);
    }
};

static PilaSesionNodo cliente;
static PilaSesionNodo servidor;
static BufferPaquete  pktTx;
static BufferPaquete  pktRx;
static MensajeSesion  metaRx;
static uint8_t        bufferCable[512];

void bombearCableUsb(PilaSesionNodo& origen, PilaSesionNodo& destino, const char* etiqueta = nullptr) {
    size_t n = origen.mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    if (n > 0) {
        if (etiqueta) imprimirSalida(bufferCable, n, Serial, etiqueta);
        destino.mockUsb.inyectarRxDesdeExterior(bufferCable, n);
    }
}

void sincronizarDialogoUsb(int rondas = 6) {
    for (int i = 0; i < rondas; i++) {
        bombearCableUsb(cliente, servidor);
        servidor.sesion.subirSesion(pktRx, metaRx);
        bombearCableUsb(servidor, cliente);
        cliente.sesion.subirSesion(pktRx, metaRx);
    }
}

bool probarTest1BypassBluetooth() {
    cliente.iniciar(0x00);
    servidor.iniciar(0x00);
    cliente.sesion.configurarBypassSesion(cliente.idBt, true);
    servidor.sesion.configurarBypassSesion(servidor.idBt, true);

    const uint8_t msgBt[] = "CMD_BT_SIN_SOBRE_SESION";
    pktTx.escribirPayload(msgBt, sizeof(msgBt));
    imprimirSalida(pktTx.ptrActual(), pktTx.longitud, Serial, "P1 - Sesion TX -> BT (payload puro)");

    uint16_t reqId = cliente.sesion.enviarPeticionRPC(cliente.idBt, pktTx);
    size_t n = cliente.mockBt.extraerTxCapturado(bufferCable, sizeof(bufferCable));
    imprimirSalida(bufferCable, n, Serial, "Cable BT (0B de sobre Sesion)");

    servidor.mockBt.inyectarRxDesdeExterior(bufferCable, n);
    bool okRx = servidor.sesion.subirSesion(pktRx, metaRx);

    if (okRx) imprimirEntrada(pktRx, Serial, "P1 - Sesion RX <- BT");

    bool pass = (reqId != 0) && okRx &&
                (n == sizeof(msgBt)) &&
                (metaRx.fuePassthrough == true) &&
                (pktRx.longitud == sizeof(msgBt)) &&
                (memcmp(pktRx.ptrActual(), msgBt, sizeof(msgBt)) == 0);

    Serial.printf("[TEST 1 - PASSTHROUGH BT] Pasa de largo sin sobre de 8B: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest2HandshakeSha256YRechazoIntruso() {
    cliente.iniciar(0x00);
    servidor.iniciar(0x00);

    // 1. Intento de llamada RPC sin autenticar -> Servidor debe rechazar con ERR_NO_AUTORIZADO
    servidor.transporte.iniciarHandshakeTCP(servidor.idUsb);
    sincronizarDialogoUsb(4);

    const uint8_t msgIntruso[] = "INTENTO_SABOTAJE_UMBRAL";
    pktTx.escribirPayload(msgIntruso, sizeof(msgIntruso));
    // Forzamos un paquete con sessionId falso (0x9999)
    uint8_t* sobreFalso = pktTx.empujarCabecera(LimitesPila::TAMANO_SOBRE_SESION);
    sobreFalso[0] = CapaSesion::VERSION_PROTOCOLO;
    sobreFalso[1] = CapaSesion::TIPO_RPC_REQ;
    sobreFalso[2] = 0x99; sobreFalso[3] = 0x99;
    sobreFalso[4] = 0x00; sobreFalso[5] = 0x01;
    sobreFalso[6] = 0x00; sobreFalso[7] = sizeof(msgIntruso);
    cliente.transporte.bajarSegmentoUnicast(cliente.idUsb, CapaTransporte::PROTO_TCP,
                                            CapaTransporte::PUERTO_RPC, CapaTransporte::PUERTO_RPC, pktTx);
    bombearCableUsb(cliente, servidor, "Cable USB (Paquete no autenticado con sessionId=0x9999)");

    bool subioIntruso = servidor.sesion.subirSesion(pktRx, metaRx);
    sincronizarDialogoUsb(2);

    // 2. Handshake Challenge-Response con clave incorrecta -> Debe fallar
    cliente.sesion.establecerSecreto("CLAVE_FALSA_INTRUSO");
    cliente.sesion.iniciarSesionCliente(cliente.idUsb);
    sincronizarDialogoUsb(6);
    bool authConClaveFalsa = servidor.sesion.estaAutenticado(servidor.idUsb);

    // 3. Handshake Challenge-Response con clave legítima -> Debe establecer sesión (sessionId = 0x1234)
    cliente.sesion.establecerSecreto("CLAVE_MAESTRA_LAB_2026");
    cliente.sesion.iniciarSesionCliente(cliente.idUsb);
    sincronizarDialogoUsb(6);

    bool authExitoso = cliente.sesion.estaAutenticado(cliente.idUsb) &&
                       servidor.sesion.estaAutenticado(servidor.idUsb) &&
                       (cliente.sesion.obtenerSessionId(cliente.idUsb) == 0x1234);

    const MetricasSesion& mSrv = servidor.sesion.obtenerMetricas();
    bool pass = (!subioIntruso) &&
                (!authConClaveFalsa) &&
                authExitoso &&
                (mSrv.rechazosNoAutorizado == 1) &&
                (mSrv.fallosAutenticacion == 1) &&
                (mSrv.sesionesEstablecidas == 1);

    Serial.printf("[TEST 2 - HANDSHAKE SHA-256 + BLOQUEO INTRUSO] %s (SessionID=0x%04X, Rechazos=%lu, FallosAuth=%lu)\n",
                  pass ? "PASS" : "FAIL",
                  cliente.sesion.obtenerSessionId(cliente.idUsb),
                  (unsigned long)mSrv.rechazosNoAutorizado,
                  (unsigned long)mSrv.fallosAutenticacion);
    return pass;
}

bool probarTest3ControlDialogoHalfDuplexToken() {
    // Configuramos diálogo Half-Duplex: Cliente arranca SIN turno (false), Servidor CON turno (true)
    cliente.sesion.configurarModoDialogo(cliente.idUsb, ModoDialogo::HALF_DUPLEX, false);
    servidor.sesion.configurarModoDialogo(servidor.idUsb, ModoDialogo::HALF_DUPLEX, true);

    const uint8_t msgRpc[] = "GET_TEMP_SONDA_1";
    pktTx.escribirPayload(msgRpc, sizeof(msgRpc));

    // Cliente intenta transmitir sin tener el token de turno -> Bloqueado localmente
    uint16_t reqBloqueado = cliente.sesion.enviarPeticionRPC(cliente.idUsb, pktTx);

    // Servidor cede el turno explícitamente (TIPO_TOKEN_GIVE) al Cliente
    bool cedidoOk = servidor.sesion.cederTurnoDialogo(servidor.idUsb);
    sincronizarDialogoUsb(4);

    bool clienteTieneTurno = cliente.sesion.tieneTurnoDialogo(cliente.idUsb);

    // Ahora el Cliente envía la petición RPC y el Servidor la recibe y responde
    uint16_t reqValido = cliente.sesion.enviarPeticionRPC(cliente.idUsb, pktTx);
    bombearCableUsb(cliente, servidor, "Cable USB (RPC_REQ con Turno Half-Duplex)");

    bool srvRecibioReq = servidor.sesion.subirSesion(pktRx, metaRx);
    uint16_t reqIdRecibido = metaRx.reqId;

    const uint8_t msgRes[] = "-18.4C_OK";
    pktTx.escribirPayload(msgRes, sizeof(msgRes));
    servidor.sesion.bajarRespuesta(servidor.idUsb, reqIdRecibido, pktTx);
    bombearCableUsb(servidor, cliente, "Cable USB (RPC_RES del Servidor)");

    bool cliRecibioRes = false;
    for (int i = 0; i < 4; i++) {
        if (cliente.sesion.subirSesion(pktRx, metaRx) && metaRx.tipo == CapaSesion::TIPO_RPC_RES) {
            cliRecibioRes = true;
            break;
        }
        bombearCableUsb(cliente, servidor);
        servidor.sesion.subirSesion(pktRx, metaRx);
        bombearCableUsb(servidor, cliente);
    }

    // Restaurar modo Full-Duplex para la siguiente prueba
    cliente.sesion.configurarModoDialogo(cliente.idUsb, ModoDialogo::FULL_DUPLEX, true);
    servidor.sesion.configurarModoDialogo(servidor.idUsb, ModoDialogo::FULL_DUPLEX, true);

    bool pass = (reqBloqueado == 0) &&
                (cliente.sesion.obtenerMetricas().violacionesTurnoDialogo == 1) &&
                cedidoOk &&
                clienteTieneTurno &&
                (reqValido != 0) &&
                srvRecibioReq &&
                cliRecibioRes &&
                (memcmp(pktRx.ptrActual(), msgRes, sizeof(msgRes)) == 0);

    Serial.printf("[TEST 3 - CONTROL DE DIALOGO (HALF-DUPLEX TOKEN + RPC)] %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool probarTest4CheckpointsRollbackYTimeout() {
    // 1. Insertar Punto de Comprobación (Checkpoint #1) y confirmarlo
    uint16_t chk1 = cliente.sesion.insertarCheckpoint(cliente.idUsb);
    sincronizarDialogoUsb(4);
    bool chkConfirmado = (cliente.sesion.obtenerUltimoCheckpointConfirmado(cliente.idUsb) == chk1);

    // 2. Enviar nueva orden crítica después del Checkpoint #1 y simular fallo para pedir Rollback
    const uint8_t msgPostChk[] = "SET_ALARMA_ALTA=-10C";
    pktTx.escribirPayload(msgPostChk, sizeof(msgPostChk));
    cliente.sesion.enviarPeticionRPC(cliente.idUsb, pktTx, nullptr, 120);

    // Descartamos el paquete en el cable para simular caída entre puntos de comprobación
    cliente.mockUsb.extraerTxCapturado(bufferCable, sizeof(bufferCable));

    // El servidor solicita Rollback desde el último punto de comprobación confirmado (chk1)
    servidor.sesion.solicitarRollbackCheckpoint(servidor.idUsb, chk1);
    bombearCableUsb(servidor, cliente, "Cable USB (TIPO_CHECKPOINT_ROLLBACK)");
    cliente.sesion.subirSesion(pktRx, metaRx);

    // Al procesar el Rollback, el cliente retransmite automáticamente la orden posterior a chk1
    bombearCableUsb(cliente, servidor, "Cable USB (Reenvio desde ultimo Checkpoint)");
    bool recuperadoEnServidor = servidor.sesion.subirSesion(pktRx, metaRx) &&
                                (metaRx.tipo == CapaSesion::TIPO_RPC_REQ) &&
                                (memcmp(pktRx.ptrActual(), msgPostChk, sizeof(msgPostChk)) == 0);

    // 3. No enviamos respuesta para verificar que a los >120ms salta TIPO_TIMEOUT_LOCAL en el cliente
    delay(140);
    bool saltoTimeoutRpc = cliente.sesion.subirSesion(pktRx, metaRx) &&
                           (metaRx.tipo == CapaSesion::TIPO_TIMEOUT_LOCAL);

    // 4. Cierre de sesión libera automáticamente la conexión de transporte subyacente
    cliente.sesion.cerrarSesion(cliente.idUsb, true);
    sincronizarDialogoUsb(4);
    bool transporteLiberado = (cliente.transporte.obtenerEstadoTCP(cliente.idUsb) == CapaTransporte::TCP_CERRADO);

    const MetricasSesion& mCli = cliente.sesion.obtenerMetricas();
    bool pass = chkConfirmado &&
                recuperadoEnServidor &&
                saltoTimeoutRpc &&
                transporteLiberado &&
                (mCli.recuperacionesCheckpoint == 1) &&
                (mCli.timeoutsPeticionesRpc == 1);

    Serial.printf("[TEST 4 - CHECKPOINTS + ROLLBACK + TIMEOUT RPC + CIERRE] %s\n", pass ? "PASS" : "FAIL");
    Serial.printf("   -> ChkAck:%u | Rollbacks:%lu | TimeoutsRPC:%lu | TranspCerrado:%s\n",
                  (unsigned)cliente.sesion.obtenerUltimoCheckpointConfirmado(cliente.idUsb),
                  (unsigned long)mCli.recuperacionesCheckpoint,
                  (unsigned long)mCli.timeoutsPeticionesRpc,
                  transporteLiberado ? "SI" : "NO");
    return pass;
}

void ejecutarPruebasCapa5() {
    Serial.println("\n======================================================================");
    Serial.println(" VERIFICACION CAPA 5 (SESION) - CAJA NEGRA, CAJA BLANCA Y DOS NODOS");
    Serial.println("======================================================================");

    UBaseType_t hwmInicio = uxTaskGetStackHighWaterMark(nullptr);

    if (!probarTest1BypassBluetooth())                { Serial.println("[ABORT] Fallo en TEST 1"); return; }
    if (!probarTest2HandshakeSha256YRechazoIntruso()) { Serial.println("[ABORT] Fallo en TEST 2"); return; }
    if (!probarTest3ControlDialogoHalfDuplexToken())  { Serial.println("[ABORT] Fallo en TEST 3"); return; }
    if (!probarTest4CheckpointsRollbackYTimeout())    { Serial.println("[ABORT] Fallo en TEST 4"); return; }

    UBaseType_t hwmFin = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("[STACK TELEMETRIA] HighWaterMark Inicio: %u B | Fin: %u B\n",
                  (unsigned)(hwmInicio * sizeof(StackType_t)),
                  (unsigned)(hwmFin * sizeof(StackType_t)));
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    ejecutarPruebasCapa5();
}

void loop() {
    delay(10000);
}