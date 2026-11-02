#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/md.h>
#include "../common/BufferRed.hpp"
#include "../transport/CapaTransporte.hpp"

namespace OSI {

    enum class ModoDialogo : uint8_t {
        FULL_DUPLEX = 0,
        HALF_DUPLEX = 1
    };

    struct MensajeSesion {
        int      idNodo            = -1;
        uint8_t  tipo              = 0;
        uint16_t sessionId         = 0;
        uint16_t reqId             = 0;
        uint16_t payloadLen        = 0;
        void*    callbackPendiente = nullptr;
        bool     fiable            = true;
        bool     fuePassthrough    = false;

        MensajeSesion() = default;
        MensajeSesion(int nodo, uint8_t t, uint16_t sid, uint16_t rid, uint16_t len,
                      void* cb = nullptr, bool esFiable = true, bool passthrough = false)
            : idNodo(nodo), tipo(t), sessionId(sid), reqId(rid),
              payloadLen(len), callbackPendiente(cb), fiable(esFiable), fuePassthrough(passthrough) {}
    };

    struct MetricasSesion {
        uint32_t sesionesEstablecidas      = 0;
        uint32_t sesionesCerradas          = 0;
        uint32_t sesionesExpiradas         = 0;
        uint32_t fallosAutenticacion       = 0;
        uint32_t rechazosNoAutorizado      = 0;
        uint32_t erroresSobreInvalido      = 0;
        uint32_t violacionesTurnoDialogo   = 0;
        uint32_t turnosCedidos             = 0;
        uint32_t checkpointsCreados        = 0;
        uint32_t checkpointsConfirmados    = 0;
        uint32_t recuperacionesCheckpoint  = 0;
        uint32_t peticionesRpcEnviadas     = 0;
        uint32_t respuestasRpcRecibidas    = 0;
        uint32_t timeoutsPeticionesRpc     = 0;
        uint32_t latidosProcesados         = 0;
        uint32_t mensajesPassthroughTx     = 0;
        uint32_t mensajesPassthroughRx     = 0;

        void reiniciar() { *this = MetricasSesion{}; }
    };

    class CapaSesion {
    public:
        static constexpr uint8_t  VERSION_PROTOCOLO        = 0x01;
        static constexpr size_t   MAX_PETICIONES_EN_VUELO  = 4;
        static constexpr size_t   MAX_VENTANA_CHECKPOINT   = 2;

        static constexpr uint8_t  TIPO_HEARTBEAT           = 0x01;
        static constexpr uint8_t  TIPO_HANDSHAKE_INIT      = 0x02;
        static constexpr uint8_t  TIPO_HANDSHAKE_CHAL      = 0x03;
        static constexpr uint8_t  TIPO_HANDSHAKE_AUTH      = 0x04;
        static constexpr uint8_t  TIPO_HANDSHAKE_OK        = 0x05;
        static constexpr uint8_t  TIPO_SESION_CLOSE        = 0x06;

        static constexpr uint8_t  TIPO_RPC_REQ             = 0x10;
        static constexpr uint8_t  TIPO_RPC_RES             = 0x11;
        static constexpr uint8_t  TIPO_EVENTO              = 0x20;

        static constexpr uint8_t  TIPO_TOKEN_GIVE          = 0x30;
        static constexpr uint8_t  TIPO_TOKEN_PLEASE        = 0x31;

        static constexpr uint8_t  TIPO_CHECKPOINT_SYNC     = 0x40;
        static constexpr uint8_t  TIPO_CHECKPOINT_ACK      = 0x41;
        static constexpr uint8_t  TIPO_CHECKPOINT_ROLLBACK = 0x42;

        static constexpr uint8_t  TIPO_TIMEOUT_LOCAL       = 0xFE;
        static constexpr uint8_t  TIPO_ERROR_SESION        = 0xFF;

        static constexpr uint8_t  ERR_NO_AUTORIZADO        = 0x01;
        static constexpr uint8_t  ERR_AUTH_FALLIDA         = 0x02;
        static constexpr uint8_t  ERR_SOBRE_INVALIDO       = 0x03;
        static constexpr uint8_t  ERR_SIN_TURNO_DIALOGO    = 0x04;
        static constexpr uint8_t  ERR_CHECKPOINT_INVALIDO  = 0x05;

        enum EstadoAuth : uint8_t {
            NO_AUTENTICADO = 0,
            ESPERANDO_DESAFIO,
            ESPERANDO_CONFIRMACION,
            AUTENTICADO
        };

    private:
        struct EntradaCheckpoint {
            bool          ocupado       = false;
            uint16_t      idCheckpoint  = 0;
            uint8_t       tipo          = TIPO_RPC_REQ;
            uint16_t      reqId         = 0;
            BufferPaquete paquete;
        };

        struct EstadoNodoSesion {
            EstadoAuth        estado                 = NO_AUTENTICADO;
            bool              autoAutenticar         = false;
            ModoDialogo       modoDialogo            = ModoDialogo::FULL_DUPLEX;
            bool              poseeTurnoTx           = true;
            uint16_t          sessionId              = 0;
            uint32_t          nonceActual            = 0;
            unsigned long     ultimoLatido           = 0;
            uint16_t          ultimoCheckpointTx     = 0;
            uint16_t          ultimoCheckpointAck    = 0;
            uint16_t          ultimoCheckpointRx     = 0;
            EntradaCheckpoint historial[MAX_VENTANA_CHECKPOINT];

            void limpiarHistorial() {
                for (size_t i = 0; i < MAX_VENTANA_CHECKPOINT; i++) {
                    historial[i].ocupado      = false;
                    historial[i].idCheckpoint = 0;
                }
            }

            void reiniciar() {
                estado              = NO_AUTENTICADO;
                sessionId           = 0;
                nonceActual         = 0;
                poseeTurnoTx        = true;
                ultimoCheckpointTx  = 0;
                ultimoCheckpointAck = 0;
                ultimoCheckpointRx  = 0;
                limpiarHistorial();
            }
        };

        struct PeticionPendiente {
            bool          activa    = false;
            int           idNodo    = -1;
            uint16_t      reqId     = 0;
            unsigned long enviadaMs = 0;
            unsigned long timeoutMs = 4000;
            void*         callback  = nullptr;
        };

        CapaTransporte*    transporte               = nullptr;
        char               secretoCompartido[64]    = {0};
        unsigned long      timeoutMs                = 15000;
        uint16_t           contadorTickets          = 1;
        uint32_t           nonceFijoParaTests       = 0;
        uint16_t           sessionIdFijoParaTests   = 0;

        EstadoNodoSesion   nodos[LimitesPila::MAX_NODOS_RED];
        PeticionPendiente  pendientes[MAX_PETICIONES_EN_VUELO];
        BufferPaquete      bufferCtrlTx;
        MetricasSesion     metricas;

        uint32_t generarNonce() const {
            if (nonceFijoParaTests != 0) return nonceFijoParaTests;
            uint32_t r = esp_random() ^ micros();
            return (r == 0) ? 0xA5A5F00DU : r;
        }

        uint16_t generarSessionId() const {
            if (sessionIdFijoParaTests != 0) return sessionIdFijoParaTests;
            return static_cast<uint16_t>(((esp_random() ^ micros()) % 65533U) + 1U);
        }

        bool asegurarTransporteConectado(int idNodo, uint8_t puerto = CapaTransporte::PUERTO_RPC) {
            if (!transporte) return false;
            if (transporte->obtenerEstadoTCP(idNodo) == CapaTransporte::TCP_ESTABLECIDO) {
                return true;
            }
            return transporte->iniciarHandshakeTCP(idNodo, puerto);
        }

        void cancelarPendientesNodo(int idNodo) {
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                if (pendientes[i].activa && pendientes[i].idNodo == idNodo) {
                    pendientes[i].timeoutMs = 0;
                }
            }
        }

        void guardarEnCheckpoint(int idNodo, uint8_t tipo, uint16_t reqId, const BufferPaquete& pktOriginal) {
            EstadoNodoSesion& cli = nodos[idNodo];
            uint16_t nuevoChk     = ++cli.ultimoCheckpointTx;
            size_t slot           = (nuevoChk - 1) % MAX_VENTANA_CHECKPOINT;

            cli.historial[slot].ocupado      = true;
            cli.historial[slot].idCheckpoint = nuevoChk;
            cli.historial[slot].tipo         = tipo;
            cli.historial[slot].reqId        = reqId;
            cli.historial[slot].paquete      = pktOriginal;
            metricas.checkpointsCreados++;
        }

        bool empujarSobreYEnviar(int idNodo, uint8_t tipo, uint16_t sesId, uint16_t reqId,
                                 BufferPaquete& pkt, uint8_t protocolo = CapaTransporte::PROTO_TCP,
                                 uint8_t puerto = CapaTransporte::PUERTO_RPC) {
            if (!transporte || idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            if (pkt.longitud > LimitesPila::MAX_PAYLOAD_APP) return false;

            if (nodos[idNodo].autoAutenticar) {
                if (transporte->bajarSegmentoUnicast(idNodo, protocolo, puerto, puerto, pkt)) {
                    metricas.mensajesPassthroughTx++;
                    return true;
                }
                return false;
            }

            if (pkt.inicio < LimitesPila::TAMANO_SOBRE_SESION) return false;

            uint16_t lenPayload = static_cast<uint16_t>(pkt.longitud);
            size_t iniPrevio    = pkt.inicio;
            size_t lenPrevia    = pkt.longitud;
            uint8_t respaldo[LimitesPila::TAMANO_SOBRE_SESION];
            memcpy(respaldo, &pkt.raw[iniPrevio - LimitesPila::TAMANO_SOBRE_SESION], LimitesPila::TAMANO_SOBRE_SESION);

            uint8_t* sobre = pkt.empujarCabecera(LimitesPila::TAMANO_SOBRE_SESION);
            sobre[0] = VERSION_PROTOCOLO;
            sobre[1] = tipo;
            sobre[2] = static_cast<uint8_t>(sesId >> 8);
            sobre[3] = static_cast<uint8_t>(sesId & 0xFF);
            sobre[4] = static_cast<uint8_t>(reqId >> 8);
            sobre[5] = static_cast<uint8_t>(reqId & 0xFF);
            sobre[6] = static_cast<uint8_t>(lenPayload >> 8);
            sobre[7] = static_cast<uint8_t>(lenPayload & 0xFF);

            bool ok = transporte->bajarSegmentoUnicast(idNodo, protocolo, puerto, puerto, pkt);

            pkt.inicio   = iniPrevio;
            pkt.longitud = lenPrevia;
            memcpy(&pkt.raw[iniPrevio - LimitesPila::TAMANO_SOBRE_SESION], respaldo, LimitesPila::TAMANO_SOBRE_SESION);
            return ok;
        }

        bool enviarControlSesion(int idNodo, uint8_t tipo, uint16_t sesId, uint16_t reqId,
                                 const uint8_t* datos, uint16_t len,
                                 uint8_t protocolo = CapaTransporte::PROTO_TCP) {
            bufferCtrlTx.prepararParaTx(idNodo);
            if (len > 0 && datos != nullptr) {
                if (!bufferCtrlTx.escribirPayload(datos, len, LimitesPila::OFFSET_PAYLOAD_APP)) {
                    return false;
                }
            }
            return empujarSobreYEnviar(idNodo, tipo, sesId, reqId, bufferCtrlTx, protocolo);
        }

    public:
        CapaSesion(CapaTransporte* capaTransp = nullptr,
                   const char* secreto = "UNTDF_DEFAULT_SECRET",
                   unsigned long timeoutConexionMs = 15000)
            : transporte(capaTransp), timeoutMs(timeoutConexionMs) {
            establecerSecreto(secreto);
        }

        void vincularTransporte(CapaTransporte* capaTransp) {
            transporte = capaTransp;
            metricas.reiniciar();
            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                nodos[i].reiniciar();
            }
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                pendientes[i].activa = false;
            }
        }

        void establecerSecreto(const char* nuevoSecreto) {
            if (nuevoSecreto && nuevoSecreto[0] != '\0') {
                strncpy(secretoCompartido, nuevoSecreto, sizeof(secretoCompartido) - 1);
                secretoCompartido[sizeof(secretoCompartido) - 1] = '\0';
            }
        }

        void fijarSemillasParaTests(uint32_t nonceFijo, uint16_t sessionIdFijo) {
            nonceFijoParaTests     = nonceFijo;
            sessionIdFijoParaTests = sessionIdFijo;
        }

        void configurarBypassSesion(int idNodo, bool activo) {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return;
            nodos[idNodo].autoAutenticar = activo;
            if (activo) {
                nodos[idNodo].estado       = AUTENTICADO;
                nodos[idNodo].sessionId    = 0xFFFF;
                nodos[idNodo].ultimoLatido = millis();
            } else {
                nodos[idNodo].reiniciar();
            }
        }

        void configurarModoDialogo(int idNodo, ModoDialogo modo, bool turnoInicialLocal = true) {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return;
            nodos[idNodo].modoDialogo  = modo;
            nodos[idNodo].poseeTurnoTx = (modo == ModoDialogo::FULL_DUPLEX) ? true : turnoInicialLocal;
        }

        bool tieneTurnoDialogo(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            if (nodos[idNodo].modoDialogo == ModoDialogo::FULL_DUPLEX) return true;
            return nodos[idNodo].poseeTurnoTx;
        }

        bool cederTurnoDialogo(int idNodo) {
            if (!estaAutenticado(idNodo)) return false;
            EstadoNodoSesion& cli = nodos[idNodo];
            if (cli.modoDialogo == ModoDialogo::HALF_DUPLEX && !cli.poseeTurnoTx) return false;
            cli.poseeTurnoTx = false;
            metricas.turnosCedidos++;
            return enviarControlSesion(idNodo, TIPO_TOKEN_GIVE, cli.sessionId, 0, nullptr, 0);
        }

        bool solicitarTurnoDialogo(int idNodo) {
            if (!estaAutenticado(idNodo)) return false;
            return enviarControlSesion(idNodo, TIPO_TOKEN_PLEASE, nodos[idNodo].sessionId, 0, nullptr, 0);
        }

        void calcularHashDesafio(uint32_t nonce, uint8_t* hashSalida32B) const {
            uint8_t bytesNonce[4] = {
                static_cast<uint8_t>(nonce >> 24),
                static_cast<uint8_t>(nonce >> 16),
                static_cast<uint8_t>(nonce >> 8),
                static_cast<uint8_t>(nonce & 0xFF)
            };
            mbedtls_md_context_t ctx;
            mbedtls_md_init(&ctx);
            mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
            mbedtls_md_starts(&ctx);
            mbedtls_md_update(&ctx, bytesNonce, 4);
            mbedtls_md_update(&ctx, reinterpret_cast<const unsigned char*>(secretoCompartido), strlen(secretoCompartido));
            mbedtls_md_finish(&ctx, hashSalida32B);
            mbedtls_md_free(&ctx);
        }

        uint16_t siguienteTicket() {
            contadorTickets = static_cast<uint16_t>((contadorTickets % 65534U) + 1U);
            return contadorTickets;
        }

        bool iniciarSesionCliente(int idNodo) {
            if (!transporte || idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            if (!asegurarTransporteConectado(idNodo)) return false;

            if (nodos[idNodo].autoAutenticar) {
                nodos[idNodo].estado       = AUTENTICADO;
                nodos[idNodo].sessionId    = 0xFFFF;
                nodos[idNodo].ultimoLatido = millis();
                return true;
            }

            nodos[idNodo].reiniciar();
            nodos[idNodo].estado = ESPERANDO_DESAFIO;
            return enviarControlSesion(idNodo, TIPO_HANDSHAKE_INIT, 0, siguienteTicket(), nullptr, 0);
        }

        bool cerrarSesion(int idNodo, bool liberarTransporteSubyacente = true) {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            EstadoNodoSesion& cli = nodos[idNodo];
            if (cli.estado == AUTENTICADO && !cli.autoAutenticar) {
                enviarControlSesion(idNodo, TIPO_SESION_CLOSE, cli.sessionId, 0, nullptr, 0);
            }
            cli.reiniciar();
            cancelarPendientesNodo(idNodo);
            metricas.sesionesCerradas++;
            if (liberarTransporteSubyacente && transporte) {
                transporte->desconectarTCP(idNodo);
            }
            return true;
        }

        uint16_t insertarCheckpoint(int idNodo) {
            if (!estaAutenticado(idNodo)) return 0;
            uint16_t chkId = nodos[idNodo].ultimoCheckpointTx;
            if (chkId == 0) chkId = ++nodos[idNodo].ultimoCheckpointTx;
            if (!enviarControlSesion(idNodo, TIPO_CHECKPOINT_SYNC, nodos[idNodo].sessionId, chkId, nullptr, 0)) {
                return 0;
            }
            return chkId;
        }

        bool solicitarRollbackCheckpoint(int idNodo, uint16_t idUltimoCheckpointValido) {
            if (!estaAutenticado(idNodo)) return false;
            return enviarControlSesion(idNodo, TIPO_CHECKPOINT_ROLLBACK,
                                       nodos[idNodo].sessionId, idUltimoCheckpointValido, nullptr, 0);
        }

        bool estaAutenticado(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            return nodos[idNodo].estado == AUTENTICADO || nodos[idNodo].autoAutenticar;
        }

        uint16_t obtenerSessionId(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return 0;
            return nodos[idNodo].sessionId;
        }

        uint16_t obtenerUltimoCheckpointConfirmado(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return 0;
            return nodos[idNodo].ultimoCheckpointAck;
        }

        uint16_t obtenerUltimoCheckpointRecibido(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return 0;
            return nodos[idNodo].ultimoCheckpointRx;
        }

        int resolverNodoPreferido() const {
            if (estaAutenticado(0)) return 0;
            for (int i = 1; i < LimitesPila::MAX_NODOS_RED; i++) {
                if (estaAutenticado(i)) return i;
            }
            return -1;
        }

        bool haySlotPendienteLibre() const {
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                if (!pendientes[i].activa) return true;
            }
            return false;
        }

        // ==========================================================
        // CAMINO DE SUBIDA (RX)
        // ==========================================================
        bool subirSesion(BufferPaquete& pktSalida, MensajeSesion& metaSalida) {
            if (!transporte) return false;

            SegmentoTransporte seg;
            if (transporte->subirSegmento(pktSalida, seg)) {
                int id = pktSalida.idNodo;
                if (id < 0 || id >= LimitesPila::MAX_NODOS_RED) return false;

                EstadoNodoSesion& cli = nodos[id];

                // 1. BYPASS: Canal pre-autenticado en su propio stack
                if (cli.autoAutenticar) {
                    cli.estado       = AUTENTICADO;
                    cli.ultimoLatido = millis();
                    metaSalida       = MensajeSesion(id, TIPO_RPC_REQ, cli.sessionId, 0,
                                                     static_cast<uint16_t>(pktSalida.longitud),
                                                     nullptr, true, true);
                    metricas.mensajesPassthroughRx++;
                    return true;
                }

                // 2. SOBRE DE SESIÓN PROPIO (8 Bytes)
                const uint8_t* sobre = pktSalida.consumirCabecera(LimitesPila::TAMANO_SOBRE_SESION);
                if (!sobre) {
                    metricas.erroresSobreInvalido++;
                    return false;
                }

                uint8_t  ver        = sobre[0];
                uint8_t  tipo       = sobre[1];
                uint16_t sessionId  = (static_cast<uint16_t>(sobre[2]) << 8) | sobre[3];
                uint16_t reqId      = (static_cast<uint16_t>(sobre[4]) << 8) | sobre[5];
                uint16_t payloadLen = (static_cast<uint16_t>(sobre[6]) << 8) | sobre[7];
                bool     esFiable   = (seg.protocolo == CapaTransporte::PROTO_TCP);

                if (ver != VERSION_PROTOCOLO || payloadLen > pktSalida.longitud) {
                    metricas.erroresSobreInvalido++;
                    uint8_t codErr = ERR_SOBRE_INVALIDO;
                    enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
                    return false;
                }

                pktSalida.longitud        = payloadLen;
                const uint8_t* ptrPayload = pktSalida.ptrActual();

                // --- FASE DE HANDSHAKE CHALLENGE-RESPONSE (SHA-256) ---
                if (tipo == TIPO_HANDSHAKE_INIT) {
                    cli.reiniciar();
                    cli.nonceActual = generarNonce();
                    cli.estado      = ESPERANDO_DESAFIO;

                    uint8_t payloadNonce[4] = {
                        static_cast<uint8_t>(cli.nonceActual >> 24),
                        static_cast<uint8_t>(cli.nonceActual >> 16),
                        static_cast<uint8_t>(cli.nonceActual >> 8),
                        static_cast<uint8_t>(cli.nonceActual & 0xFF)
                    };
                    enviarControlSesion(id, TIPO_HANDSHAKE_CHAL, 0, reqId, payloadNonce, 4);
                    return false;
                }

                if (tipo == TIPO_HANDSHAKE_CHAL) {
                    if (cli.estado != ESPERANDO_DESAFIO || payloadLen != 4) {
                        metricas.fallosAutenticacion++;
                        return false;
                    }
                    uint32_t nonceRecibido = (static_cast<uint32_t>(ptrPayload[0]) << 24) |
                                             (static_cast<uint32_t>(ptrPayload[1]) << 16) |
                                             (static_cast<uint32_t>(ptrPayload[2]) << 8)  |
                                             static_cast<uint32_t>(ptrPayload[3]);
                    uint8_t firmaSha256[32];
                    calcularHashDesafio(nonceRecibido, firmaSha256);
                    cli.estado = ESPERANDO_CONFIRMACION;
                    enviarControlSesion(id, TIPO_HANDSHAKE_AUTH, 0, reqId, firmaSha256, 32);
                    return false;
                }

                if (tipo == TIPO_HANDSHAKE_AUTH) {
                    if (cli.estado != ESPERANDO_DESAFIO || payloadLen != 32) {
                        metricas.fallosAutenticacion++;
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
                        cli.estado       = AUTENTICADO;
                        cli.sessionId    = generarSessionId();
                        cli.ultimoLatido = millis();
                        metricas.sesionesEstablecidas++;
                        enviarControlSesion(id, TIPO_HANDSHAKE_OK, cli.sessionId, reqId, nullptr, 0);
                    } else {
                        cli.reiniciar();
                        metricas.fallosAutenticacion++;
                        uint8_t codErr = ERR_AUTH_FALLIDA;
                        enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
                    }
                    return false;
                }

                if (tipo == TIPO_HANDSHAKE_OK) {
                    if (cli.estado == ESPERANDO_CONFIRMACION && sessionId != 0) {
                        cli.estado       = AUTENTICADO;
                        cli.sessionId    = sessionId;
                        cli.ultimoLatido = millis();
                        metricas.sesionesEstablecidas++;
                    }
                    return false;
                }

                // --- VERIFICACIÓN ESTRICTA DE AUTORIZACIÓN DE SESIÓN ---
                if (cli.estado != AUTENTICADO || sessionId == 0 || sessionId != cli.sessionId) {
                    metricas.rechazosNoAutorizado++;
                    if (tipo != TIPO_ERROR_SESION) {
                        uint8_t codErr = ERR_NO_AUTORIZADO;
                        enviarControlSesion(id, TIPO_ERROR_SESION, 0, reqId, &codErr, 1);
                    }
                    return false;
                }

                cli.ultimoLatido = millis();

                // --- PRIMITIVAS DE CONTROL DE SESIÓN, DIÁLOGO Y RECUPERACIÓN ---
                if (tipo == TIPO_HEARTBEAT) {
                    metricas.latidosProcesados++;
                    if (reqId != 0) {
                        enviarControlSesion(id, TIPO_HEARTBEAT, cli.sessionId, 0, nullptr, 0);
                    }
                    return false;
                }

                if (tipo == TIPO_SESION_CLOSE) {
                    cli.reiniciar();
                    cancelarPendientesNodo(id);
                    metricas.sesionesCerradas++;
                    transporte->desconectarTCP(id);
                    return false;
                }

                if (tipo == TIPO_TOKEN_GIVE) {
                    cli.poseeTurnoTx = true;
                    return false;
                }

                if (tipo == TIPO_TOKEN_PLEASE) {
                    metaSalida = MensajeSesion(id, tipo, sessionId, reqId, 0, nullptr, esFiable, false);
                    return true;
                }

                if (tipo == TIPO_CHECKPOINT_SYNC) {
                    cli.ultimoCheckpointRx = reqId;
                    enviarControlSesion(id, TIPO_CHECKPOINT_ACK, cli.sessionId, reqId, nullptr, 0);
                    return false;
                }

                if (tipo == TIPO_CHECKPOINT_ACK) {
                    cli.ultimoCheckpointAck = reqId;
                    metricas.checkpointsConfirmados++;
                    for (size_t i = 0; i < MAX_VENTANA_CHECKPOINT; i++) {
                        if (cli.historial[i].ocupado && cli.historial[i].idCheckpoint <= reqId) {
                            cli.historial[i].ocupado = false;
                        }
                    }
                    return false;
                }

                if (tipo == TIPO_CHECKPOINT_ROLLBACK) {
                    uint16_t desdeChk = reqId;
                    bool reenviado    = false;
                    for (size_t i = 0; i < MAX_VENTANA_CHECKPOINT; i++) {
                        if (cli.historial[i].ocupado && cli.historial[i].idCheckpoint > desdeChk) {
                            empujarSobreYEnviar(id, cli.historial[i].tipo, cli.sessionId,
                                                cli.historial[i].reqId, cli.historial[i].paquete,
                                                CapaTransporte::PROTO_TCP);
                            metricas.recuperacionesCheckpoint++;
                            reenviado = true;
                        }
                    }
                    if (!reenviado) {
                        uint8_t codErr = ERR_CHECKPOINT_INVALIDO;
                        enviarControlSesion(id, TIPO_ERROR_SESION, cli.sessionId, reqId, &codErr, 1);
                    }
                    return false;
                }

                // Control de diálogo en recepción: si estamos en Half-Duplex y nosotros teníamos el turno, rechazar CALL
                if (tipo == TIPO_RPC_REQ && cli.modoDialogo == ModoDialogo::HALF_DUPLEX && cli.poseeTurnoTx) {
                    metricas.violacionesTurnoDialogo++;
                    uint8_t codErr = ERR_SIN_TURNO_DIALOGO;
                    enviarControlSesion(id, TIPO_ERROR_SESION, cli.sessionId, reqId, &codErr, 1);
                    return false;
                }

                if (tipo == TIPO_RPC_RES || tipo == TIPO_ERROR_SESION) {
                    for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                        if (pendientes[i].activa &&
                            pendientes[i].idNodo == id &&
                            pendientes[i].reqId == reqId) {
                            void* cb             = pendientes[i].callback;
                            pendientes[i].activa = false;
                            if (tipo == TIPO_RPC_RES) {
                                metricas.respuestasRpcRecibidas++;
                            }
                            metaSalida = MensajeSesion(id, tipo, sessionId, reqId, payloadLen, cb, esFiable, false);
                            return true;
                        }
                    }
                    metaSalida = MensajeSesion(id, tipo, sessionId, reqId, payloadLen, nullptr, esFiable, false);
                    return true;
                }

                metaSalida = MensajeSesion(id, tipo, sessionId, reqId, payloadLen, nullptr, esFiable, false);
                return true;
            }

            // 3. Evaluar expiración de peticiones RPC en vuelo cuando no hubo segmentos entrantes
            unsigned long ahora = millis();
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                if (pendientes[i].activa && (ahora - pendientes[i].enviadaMs >= pendientes[i].timeoutMs)) {
                    PeticionPendiente exp = pendientes[i];
                    pendientes[i].activa  = false;
                    metricas.timeoutsPeticionesRpc++;
                    pktSalida.prepararParaRx(exp.idNodo);
                    uint16_t sid = (exp.idNodo >= 0 && exp.idNodo < LimitesPila::MAX_NODOS_RED)
                                     ? nodos[exp.idNodo].sessionId
                                     : 0;
                    metaSalida = MensajeSesion(exp.idNodo, TIPO_TIMEOUT_LOCAL, sid, exp.reqId, 0, exp.callback, true, false);
                    return true;
                }
            }

            return false;
        }

        // ==========================================================
        // CAMINO DE BAJADA (TX)
        // ==========================================================
        uint16_t enviarPeticionRPC(int idNodo, BufferPaquete& pkt, void* callback = nullptr,
                                   unsigned long timeoutPeticionMs = 4000, bool fiable = true) {
            if (!estaAutenticado(idNodo)) return 0;

            EstadoNodoSesion& cli = nodos[idNodo];
            if (cli.modoDialogo == ModoDialogo::HALF_DUPLEX && !cli.poseeTurnoTx) {
                metricas.violacionesTurnoDialogo++;
                return 0;
            }

            int slot = -1;
            for (size_t i = 0; i < MAX_PETICIONES_EN_VUELO; i++) {
                if (!pendientes[i].activa) {
                    slot = static_cast<int>(i);
                    break;
                }
            }
            if (slot < 0) return 0;

            uint8_t  proto = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
            uint16_t reqId = siguienteTicket();

            guardarEnCheckpoint(idNodo, TIPO_RPC_REQ, reqId, pkt);

            if (!empujarSobreYEnviar(idNodo, TIPO_RPC_REQ, cli.sessionId, reqId, pkt, proto)) {
                return 0;
            }

            pendientes[slot] = { true, idNodo, reqId, millis(), timeoutPeticionMs, callback };
            metricas.peticionesRpcEnviadas++;
            return reqId;
        }

        bool bajarRespuesta(int idNodo, uint16_t reqId, BufferPaquete& pkt, bool fiable = true) {
            if (!estaAutenticado(idNodo)) return false;
            uint8_t proto = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
            guardarEnCheckpoint(idNodo, TIPO_RPC_RES, reqId, pkt);
            return empujarSobreYEnviar(idNodo, TIPO_RPC_RES, nodos[idNodo].sessionId, reqId, pkt, proto);
        }

        size_t bajarEventoGlobal(BufferPaquete& pkt, bool fiable = true) {
            uint8_t proto  = fiable ? CapaTransporte::PROTO_TCP : CapaTransporte::PROTO_UDP;
            uint8_t puerto = fiable ? CapaTransporte::PUERTO_RPC : CapaTransporte::PUERTO_EVENTOS;
            size_t enviados = 0;

            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                if (estaAutenticado(i)) {
                    if (empujarSobreYEnviar(i, TIPO_EVENTO, nodos[i].sessionId, 0, pkt, proto, puerto)) {
                        enviados++;
                    }
                }
            }
            return enviados;
        }

        bool enviarLatido(int idNodo) {
            if (!estaAutenticado(idNodo)) return false;
            return enviarControlSesion(idNodo, TIPO_HEARTBEAT, nodos[idNodo].sessionId, siguienteTicket(), nullptr, 0);
        }

        void auditarSesiones() {
            unsigned long ahora = millis();
            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                if (nodos[i].estado == AUTENTICADO && !nodos[i].autoAutenticar) {
                    bool transporteCaido = transporte &&
                                           (transporte->obtenerEstadoTCP(i) != CapaTransporte::TCP_ESTABLECIDO);
                    if (transporteCaido || (ahora - nodos[i].ultimoLatido > timeoutMs)) {
                        nodos[i].reiniciar();
                        cancelarPendientesNodo(i);
                        metricas.sesionesExpiradas++;
                    }
                }
            }
        }

        bool hayClientesAutenticados() const {
            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                if (estaAutenticado(i)) return true;
            }
            return false;
        }

        const MetricasSesion& obtenerMetricas() const { return metricas; }
        void reiniciarMetricas() { metricas.reiniciar(); }
    };

}