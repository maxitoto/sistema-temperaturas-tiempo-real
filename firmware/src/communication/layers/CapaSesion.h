// Archivo: ./src/communication/layers/CapaSesion.h
#pragma once
#include <Arduino.h>
#include <mbedtls/md.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaTransporte.h"

struct MensajeSesion {
    int idNodo = -1;
    uint8_t tipo = 0;
    uint16_t sessionId = 0;
    uint16_t reqId = 0;
    uint16_t payloadLen = 0;
    void* callbackPendiente = nullptr;
    bool fiable = true; // true = llegó por PROTO_TCP (ARQ), false = llegó por PROTO_UDP (Tiempo Real)

    MensajeSesion() = default;
    MensajeSesion(int nodo, uint8_t t, uint16_t sid, uint16_t rid, uint16_t len,
                  void* cb = nullptr, bool esFiable = true)
        : idNodo(nodo), tipo(t), sessionId(sid), reqId(rid),
          payloadLen(len), callbackPendiente(cb), fiable(esFiable) {}
};

class CapaSesion {
  public:
    static const uint8_t VERSION_PROTOCOLO       = 0x01;
    static const size_t  MAX_PETICIONES_EN_VUELO = 4;

    static const uint8_t TIPO_HEARTBEAT       = 0x01;
    static const uint8_t TIPO_HANDSHAKE_INIT  = 0x02;
    static const uint8_t TIPO_HANDSHAKE_CHAL  = 0x03;
    static const uint8_t TIPO_HANDSHAKE_AUTH  = 0x04;
    static const uint8_t TIPO_HANDSHAKE_OK    = 0x05;
    static const uint8_t TIPO_RPC_REQ         = 0x10; // CALL
    static const uint8_t TIPO_RPC_RES         = 0x11; // REPLY
    static const uint8_t TIPO_EVENTO          = 0x20; // Notificación asíncrona (reqId = 0)
    static const uint8_t TIPO_TIMEOUT_LOCAL   = 0xFE; // Señal interna de timeout en Capa 5
    static const uint8_t TIPO_ERROR_SESION    = 0xFF;

    static const uint8_t ERR_NO_AUTORIZADO    = 0x01;
    static const uint8_t ERR_AUTH_FALLIDA     = 0x02;
    static const uint8_t ERR_SOBRE_INVALIDO   = 0x03;

    enum EstadoAuth {
        NO_AUTENTICADO,
        ESPERANDO_DESAFIO,
        AUTENTICADO
    };

  private:
    CapaTransporte* transporte;
    char secretoCompartido[64];
    unsigned long timeoutMs;
    uint16_t contadorTickets = 1;

    struct EstadoNodoSesion {
        EstadoAuth estado = NO_AUTENTICADO;
        uint16_t sessionId = 0;
        uint32_t nonceActual = 0;
        unsigned long ultimoLatido = 0;
    };

    struct PeticionPendiente {
        bool activa = false;
        int idNodo = -1;
        uint16_t reqId = 0;
        unsigned long enviadaMs = 0;
        unsigned long timeoutMs = 4000;
        void* callback = nullptr;

        PeticionPendiente() = default;
        PeticionPendiente(bool a, int nodo, uint16_t rid, unsigned long env,
                          unsigned long tmo, void* cb)
            : activa(a), idNodo(nodo), reqId(rid), enviadaMs(env),
              timeoutMs(tmo), callback(cb) {}
    };

    EstadoNodoSesion nodos[LimitesPila::MAX_NODOS_RED];
    PeticionPendiente pendientes[MAX_PETICIONES_EN_VUELO];

    uint32_t aleatorioHardware() const {
        #if defined(ESP32)
            return esp_random();
        #elif defined(ESP8266)
            return *(volatile uint32_t*)0x3FF20E44;
        #else
            return (uint32_t)random(100000, 0x7FFFFFFF);
        #endif
    }

    void calcularHashDesafio(uint32_t nonce, uint8_t* hashSalida32B) const {
        uint8_t bytesNonce[4] = {
            (uint8_t)(nonce >> 24),
            (uint8_t)(nonce >> 16),
            (uint8_t)(nonce >> 8),
            (uint8_t)(nonce & 0xFF)
        };
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, bytesNonce, 4);
        mbedtls_md_update(&ctx, (const unsigned char*)secretoCompartido, strlen(secretoCompartido));
        mbedtls_md_finish(&ctx, hashSalida32B);
        mbedtls_md_free(&ctx);
    }

    bool empujarSobreYEnviar(int idNodo, uint8_t tipo, uint16_t sesId, uint16_t reqId,
                             BufferPaquete& pkt, uint8_t protocolo = CapaTransporte::PROTO_TCP,
                             uint8_t puerto = CapaTransporte::PUERTO_RPC) {
        if (!transporte || pkt.longitud > LimitesPila::MAX_PAYLOAD_APP) return false;
        uint16_t lenPayload = (uint16_t)pkt.longitud;

        uint8_t* sobre = pkt.empujarCabecera(LimitesPila::TAMANO_SOBRE_SESION);
        if (!sobre) return false;

        sobre[0] = VERSION_PROTOCOLO;
        sobre[1] = tipo;
        sobre[2] = (uint8_t)(sesId >> 8);
        sobre[3] = (uint8_t)(sesId & 0xFF);
        sobre[4] = (uint8_t)(reqId >> 8);
        sobre[5] = (uint8_t)(reqId & 0xFF);
        sobre[6] = (uint8_t)(lenPayload >> 8);
        sobre[7] = (uint8_t)(lenPayload & 0xFF);

        return transporte->bajarSegmentoUnicast(idNodo, protocolo, puerto, puerto, pkt);
    }

    void enviarControlSesion(int idNodo, uint8_t tipo, uint16_t sesId, uint16_t reqId,
                             const uint8_t* datos, uint16_t len) {
        BufferPaquete pktCtrl;
        pktCtrl.prepararParaTx(idNodo);
        if (len > 0 && datos != nullptr) {
            memcpy(pktCtrl.ptrPayloadApp(), datos, len);
            pktCtrl.longitud = len;
        }
        empujarSobreYEnviar(idNodo, tipo, sesId, reqId, pktCtrl, CapaTransporte::PROTO_TCP);
    }

    void cancelarPendientesNodo(int idNodo) {
        for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
            if (pendientes[i].activa && pendientes[i].idNodo == idNodo) {
                pendientes[i].timeoutMs = 0;
            }
        }
    }

  public:
    CapaSesion(CapaTransporte* capaTransp, const char* secreto, unsigned long timeoutConexionMs = 15000)
        : transporte(capaTransp), timeoutMs(timeoutConexionMs) {
        establecerSecreto(secreto);
    }

    void establecerSecreto(const char* nuevoSecreto) {
        if (nuevoSecreto && nuevoSecreto[0] != '\0') {
            strncpy(secretoCompartido, nuevoSecreto, sizeof(secretoCompartido) - 1);
            secretoCompartido[sizeof(secretoCompartido) - 1] = '\0';
        }
    }

    uint16_t siguienteTicket() {
        contadorTickets = (contadorTickets % 65534) + 1;
        return contadorTickets;
    }

    bool estaAutenticado(int idNodo) const {
        if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
        return nodos[idNodo].estado == AUTENTICADO;
    }

    int resolverNodoPreferido() const {
        if (nodos[0].estado == AUTENTICADO) return 0;
        for (int i = 1; i < LimitesPila::MAX_NODOS_RED; i++) {
            if (nodos[i].estado == AUTENTICADO) return i;
        }
        return -1;
    }

    bool haySlotPendienteLibre() const {
        for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
            if (!pendientes[i].activa) return true;
        }
        return false;
    }

    bool subirSesion(BufferPaquete& pktSalida, MensajeSesion& metaSalida) {
        unsigned long ahora = millis();
        for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
            if (pendientes[i].activa && (ahora - pendientes[i].enviadaMs >= pendientes[i].timeoutMs)) {
                PeticionPendiente exp = pendientes[i];
                pendientes[i].activa = false;
                pktSalida.prepararParaRx(exp.idNodo);
                uint16_t sid = (exp.idNodo >= 0 && exp.idNodo < LimitesPila::MAX_NODOS_RED)
                                 ? nodos[exp.idNodo].sessionId
                                 : 0;
                metaSalida = MensajeSesion(exp.idNodo, TIPO_TIMEOUT_LOCAL, sid, exp.reqId, 0, exp.callback, true);
                return true;
            }
        }

        SegmentoTransporte seg;
        if (!transporte || !transporte->subirSegmento(pktSalida, seg)) return false;

        int id = pktSalida.idNodo;
        if (id < 0 || id >= LimitesPila::MAX_NODOS_RED) return false;

        const uint8_t* sobre = pktSalida.consumirCabecera(LimitesPila::TAMANO_SOBRE_SESION);
        if (!sobre) return false;

        uint8_t ver         = sobre[0];
        uint8_t tipo        = sobre[1];
        uint16_t sessionId  = ((uint16_t)sobre[2] << 8) | sobre[3];
        uint16_t reqId      = ((uint16_t)sobre[4] << 8) | sobre[5];
        uint16_t payloadLen = ((uint16_t)sobre[6] << 8) | sobre[7];
        bool esFiable       = (seg.protocolo == CapaTransporte::PROTO_TCP);

        if (ver != VERSION_PROTOCOLO || payloadLen > pktSalida.longitud) {
            uint8_t codErr = ERR_SOBRE_INVALIDO;
            enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
            return false;
        }

        pktSalida.longitud = payloadLen;
        const uint8_t* ptrPayload = pktSalida.ptrActual();
        EstadoNodoSesion& cli = nodos[id];

        if (tipo == TIPO_HANDSHAKE_INIT) {
            cli.nonceActual = aleatorioHardware();
            if (cli.nonceActual == 0) cli.nonceActual = 0xA5A5F00D;
            cli.estado = ESPERANDO_DESAFIO;
            cli.sessionId = 0;

            uint8_t payloadNonce[4] = {
                (uint8_t)(cli.nonceActual >> 24),
                (uint8_t)(cli.nonceActual >> 16),
                (uint8_t)(cli.nonceActual >> 8),
                (uint8_t)(cli.nonceActual & 0xFF)
            };
            enviarControlSesion(id, TIPO_HANDSHAKE_CHAL, 0, reqId, payloadNonce, 4);
            return false;
        }

        if (tipo == TIPO_HANDSHAKE_AUTH) {
            if (cli.estado != ESPERANDO_DESAFIO || payloadLen != 32) {
                uint8_t codErr = ERR_AUTH_FALLIDA;
                enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
                return false;
            }

            uint8_t hashEsperado[32];
            calcularHashDesafio(cli.nonceActual, hashEsperado);

            uint8_t diff = 0;
            for (size_t i = 0; i < 32; i++) {
                diff |= (ptrPayload[i] ^ hashEsperado[i]);
            }

            if (diff == 0) {
                cli.estado = AUTENTICADO;
                cli.sessionId = (uint16_t)((aleatorioHardware() % 65533) + 1);
                cli.ultimoLatido = millis();
                enviarControlSesion(id, TIPO_HANDSHAKE_OK, cli.sessionId, reqId, nullptr, 0);
            } else {
                cli.estado = NO_AUTENTICADO;
                uint8_t codErr = ERR_AUTH_FALLIDA;
                enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
            }
            return false;
        }

        if (cli.estado != AUTENTICADO || sessionId == 0 || sessionId != cli.sessionId) {
            uint8_t codErr = ERR_NO_AUTORIZADO;
            enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
            return false;
        }

        cli.ultimoLatido = millis();

        if (tipo == TIPO_HEARTBEAT) {
            enviarControlSesion(id, TIPO_HEARTBEAT, cli.sessionId, reqId, nullptr, 0);
            return false;
        }

        if (tipo == TIPO_RPC_RES || tipo == TIPO_ERROR_SESION) {
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                if (pendientes[i].activa &&
                    pendientes[i].idNodo == id &&
                    pendientes[i].reqId == reqId) {
                    void* cb = pendientes[i].callback;
                    pendientes[i].activa = false;
                    metaSalida = MensajeSesion(id, tipo, sessionId, reqId, payloadLen, cb, esFiable);
                    return true;
                }
            }
            return false;
        }

        metaSalida = MensajeSesion(id, tipo, sessionId, reqId, payloadLen, nullptr, esFiable);
        return true;
    }

    uint16_t enviarPeticionRPC(int idNodo, BufferPaquete& pkt, void* callback,
                               unsigned long timeoutPeticionMs = 4000, bool fiable = true) {
        if (!estaAutenticado(idNodo)) return 0;

        int slot = -1;
        for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
            if (!pendientes[i].activa) {
                slot = (int)i;
                break;
            }
        }
        if (slot < 0) return 0;

        uint8_t proto = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
        uint16_t reqId = siguienteTicket();
        if (!empujarSobreYEnviar(idNodo, TIPO_RPC_REQ, nodos[idNodo].sessionId, reqId, pkt, proto)) {
            return 0;
        }

        pendientes[slot] = PeticionPendiente(true, idNodo, reqId, millis(), timeoutPeticionMs, callback);
        return reqId;
    }

    bool bajarRespuesta(int idNodo, uint16_t reqId, BufferPaquete& pkt, bool fiable = true) {
        if (!estaAutenticado(idNodo)) return false;
        uint8_t proto = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
        return empujarSobreYEnviar(idNodo, TIPO_RPC_RES, nodos[idNodo].sessionId, reqId, pkt, proto);
    }

    void bajarEventoGlobal(BufferPaquete& pktBase, bool fiable = true) {
        uint8_t proto = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
        uint8_t puerto = fiable ? CapaTransporte::PUERTO_RPC : CapaTransporte::PUERTO_EVENTOS;

        if (nodos[0].estado == AUTENTICADO) {
            empujarSobreYEnviar(0, TIPO_EVENTO, nodos[0].sessionId, 0, pktBase, proto, puerto);
            return;
        }

        for (int i = 1; i < LimitesPila::MAX_NODOS_RED; i++) {
            if (nodos[i].estado == AUTENTICADO) {
                BufferPaquete copia = pktBase;
                empujarSobreYEnviar(i, TIPO_EVENTO, nodos[i].sessionId, 0, copia, proto, puerto);
            }
        }
    }

    void auditarSesiones() {
        unsigned long ahora = millis();
        for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
            if (nodos[i].estado == AUTENTICADO) {
                bool transporteCaido = transporte &&
                                       (transporte->obtenerEstadoTCP(i) != CapaTransporte::TCP_ESTABLECIDO);
                if (transporteCaido || (ahora - nodos[i].ultimoLatido > timeoutMs)) {
                    nodos[i].estado = NO_AUTENTICADO;
                    nodos[i].sessionId = 0;
                    cancelarPendientesNodo(i);
                }
            }
        }
    }

    bool hayClientesAutenticados() const {
        for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
            if (nodos[i].estado == AUTENTICADO) return true;
        }
        return false;
    }
};