#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../common/BufferRed.hpp"
#include "../red/CapaRed.h"

namespace OSI {

    struct SegmentoTransporte {
        int      idNodo        = -1;
        uint8_t  protocolo     = 0;
        uint8_t  puertoOrigen  = 0;
        uint8_t  puertoDestino = 0;
        uint8_t  flags         = 0;
        uint16_t numSecuencia  = 0;
        uint16_t numAcuse      = 0;
        bool     fuePassthrough= false;
    };

    struct MetricasTransporte {
        uint32_t segmentosPassthroughTx     = 0;
        uint32_t segmentosPassthroughRx     = 0;
        uint32_t datagramasUdpTx            = 0;
        uint32_t datagramasUdpRx            = 0;
        uint32_t segmentosTcpDatosTx        = 0;
        uint32_t segmentosTcpDatosRx        = 0;
        uint32_t handshakesEstablecidos     = 0;
        uint32_t cierresSimetricos          = 0;
        uint32_t cierresSemiabiertosTimeout = 0;
        uint32_t retransmisionesRto         = 0;
        uint32_t duplicadosRetrasadosRx     = 0;
        uint32_t erroresCrcTransporte       = 0;
        uint32_t rechazosPuertoCerrado      = 0;
        uint32_t desbordesColaControlFlujo  = 0;
        uint32_t resetsEnviados             = 0;
        uint32_t resetsRecibidos            = 0;

        void reiniciar() { *this = MetricasTransporte{}; }
    };

    class CapaTransporte {
    public:
        static constexpr uint8_t  PROTO_TCP                       = 0x06;
        static constexpr uint8_t  PROTO_UDP                       = 0x11;

        static constexpr uint8_t  FLAG_SYN                        = 0x01;
        static constexpr uint8_t  FLAG_ACK                        = 0x02;
        static constexpr uint8_t  FLAG_PSH                        = 0x04;
        static constexpr uint8_t  FLAG_FIN                        = 0x08;
        static constexpr uint8_t  FLAG_RST                        = 0x10;
        static constexpr uint8_t  FLAG_CRASH_RECOVERY             = 0x20;

        static constexpr uint8_t  PUERTO_RPC                      = 80;
        static constexpr uint8_t  PUERTO_EVENTOS                  = 81;
        static constexpr uint8_t  PUERTO_CONSOLA_SQL              = 82;

        static constexpr size_t   MAX_PUERTOS_TSAP                = 12;
        static constexpr size_t   CAP_COLA_TX_NODO                = 2;
        static constexpr uint32_t RTO_DEFAULT_MS                  = 1000;
        static constexpr uint32_t TIMEOUT_CONEXION_SEMIABIERTA_MS = 30000;
        static constexpr uint8_t  MAX_REINTENTOS                  = 3;

        enum EstadoTCP : uint8_t {
            TCP_CERRADO = 0,
            TCP_SYN_ENVIADO,
            TCP_SYN_RECIBIDO,
            TCP_ESTABLECIDO,
            TCP_FIN_ESPERA
        };

    private:
        struct ItemColaTx {
            bool          ocupado       = false;
            uint8_t       puertoOrigen  = PUERTO_RPC;
            uint8_t       puertoDestino = PUERTO_RPC;
            BufferPaquete paquete;
        };

        struct ControlFlujoNodo {
            EstadoTCP     estado              = TCP_CERRADO;
            uint16_t      seqLocal            = 100;
            uint16_t      seqRemotaEsperada   = 0;

            bool          esperandoAck        = false;
            bool          finRemotoRecibido   = false;
            uint8_t       flagsPendientes     = 0;
            uint16_t      seqPendienteAck     = 0;
            unsigned long ultimoEnvioMs       = 0;
            unsigned long ultimaActividadRxMs = 0;
            uint8_t       reintentos          = 0;
            BufferPaquete ultimoPaqueteTx;

            ItemColaTx    colaTx[CAP_COLA_TX_NODO];
            size_t        headCola            = 0;
            size_t        tailCola            = 0;
            size_t        countCola           = 0;

            void limpiarCola() {
                headCola  = 0;
                tailCola  = 0;
                countCola = 0;
                for (size_t i = 0; i < CAP_COLA_TX_NODO; i++) {
                    colaTx[i].ocupado = false;
                }
            }

            void cerrar() {
                estado            = TCP_CERRADO;
                esperandoAck      = false;
                finRemotoRecibido = false;
                flagsPendientes   = 0;
                reintentos        = 0;
                limpiarCola();
            }
        };

        CapaRed*           red                                    = nullptr;
        ControlFlujoNodo   nodos[LimitesPila::MAX_NODOS_RED];
        uint8_t            puertosEscucha[MAX_PUERTOS_TSAP]       = {0};
        size_t             totalPuertosEscucha                    = 0;
        uint32_t           rtoConfiguradoMs                       = RTO_DEFAULT_MS;
        uint32_t           timeoutSemiabiertaMs                   = TIMEOUT_CONEXION_SEMIABIERTA_MS;
        uint16_t           isnFijoParaTests                       = 0;
        BufferPaquete      bufferRx;
        BufferPaquete      bufferCtrlTx;
        MetricasTransporte metricas;

        uint16_t generarIsnInicial() const {
            if (isnFijoParaTests != 0) return isnFijoParaTests;
            return static_cast<uint16_t>((esp_random() ^ micros()) & 0x7FFF);
        }

        bool escribirCabeceraYCRC(BufferPaquete& pkt, uint8_t proto, uint8_t ptoOrig, uint8_t ptoDest,
                                  uint8_t flags, uint16_t seq, uint16_t ack) const {
            const uint8_t* payload = pkt.ptrActual();
            size_t lenPayload      = pkt.longitud;

            uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
            if (!hdr) return false;

            hdr[0] = proto;
            hdr[1] = ptoOrig;
            hdr[2] = ptoDest;
            hdr[3] = flags;
            hdr[4] = static_cast<uint8_t>(seq >> 8);
            hdr[5] = static_cast<uint8_t>(seq & 0xFF);
            hdr[6] = static_cast<uint8_t>(ack >> 8);
            hdr[7] = static_cast<uint8_t>(ack & 0xFF);

            uint16_t crc = calcularCRC16(hdr, 8);
            if (lenPayload > 0) {
                crc = calcularCRC16(payload, lenPayload, crc);
            }
            hdr[8] = static_cast<uint8_t>(crc >> 8);
            hdr[9] = static_cast<uint8_t>(crc & 0xFF);
            return true;
        }

        bool enviarControlTCP(int idNodo, uint8_t ptoOrig, uint8_t ptoDest,
                              uint8_t flags, uint16_t seq, uint16_t ack, bool requiereReenvioRto = false) {
            bufferCtrlTx.prepararParaTx(idNodo);
            bufferCtrlTx.inicio = LimitesPila::OFFSET_SESION;
            if (!escribirCabeceraYCRC(bufferCtrlTx, PROTO_TCP, ptoOrig, ptoDest, flags, seq, ack)) {
                return false;
            }

            if (requiereReenvioRto) {
                ControlFlujoNodo& ctrl = nodos[idNodo];
                ctrl.ultimoPaqueteTx   = bufferCtrlTx;
                ctrl.esperandoAck      = true;
                ctrl.flagsPendientes   = flags;
                ctrl.seqPendienteAck   = static_cast<uint16_t>(seq + 1);
                ctrl.ultimoEnvioMs     = millis();
                ctrl.reintentos        = 0;
            }

            if (flags & FLAG_RST) {
                metricas.resetsEnviados++;
            }

            return red->bajarPaqueteUnicast(idNodo, bufferCtrlTx, CapaRed::DEFAULT_TTL, true);
        }

        bool transmitirSegmentoTCPInmediato(int idDestino, uint8_t puertoOrigen,
                                            uint8_t puertoDestino, const BufferPaquete& pktBase) {
            ControlFlujoNodo& ctrl = nodos[idDestino];
            uint16_t seqActual     = ctrl.seqLocal++;
            uint8_t  flags         = FLAG_PSH | FLAG_ACK;

            ctrl.ultimoPaqueteTx = pktBase;
            if (!escribirCabeceraYCRC(ctrl.ultimoPaqueteTx, PROTO_TCP, puertoOrigen, puertoDestino,
                                      flags, seqActual, ctrl.seqRemotaEsperada)) {
                return false;
            }

            ctrl.esperandoAck    = true;
            ctrl.flagsPendientes = flags;
            ctrl.seqPendienteAck = static_cast<uint16_t>(seqActual + 1);
            ctrl.ultimoEnvioMs   = millis();
            ctrl.reintentos      = 0;

            if (red->bajarPaqueteUnicast(idDestino, ctrl.ultimoPaqueteTx)) {
                metricas.segmentosTcpDatosTx++;
                return true;
            }
            return false;
        }

        void despacharSiguienteEnCola(int idNodo) {
            ControlFlujoNodo& ctrl = nodos[idNodo];
            if (ctrl.esperandoAck || ctrl.countCola == 0 || ctrl.estado != TCP_ESTABLECIDO) {
                if (!ctrl.esperandoAck && ctrl.countCola == 0 && ctrl.finRemotoRecibido) {
                    ctrl.cerrar();
                    metricas.cierresSimetricos++;
                }
                return;
            }

            ItemColaTx& item = ctrl.colaTx[ctrl.tailCola];
            uint8_t pOrig    = item.puertoOrigen;
            uint8_t pDest    = item.puertoDestino;

            item.ocupado  = false;
            ctrl.tailCola = (ctrl.tailCola + 1) % CAP_COLA_TX_NODO;
            ctrl.countCola--;

            transmitirSegmentoTCPInmediato(idNodo, pOrig, pDest, item.paquete);
        }

    public:
        explicit CapaTransporte(CapaRed* capaRed = nullptr) : red(capaRed) {
            listen(PUERTO_RPC);
            listen(PUERTO_EVENTOS);
            listen(PUERTO_CONSOLA_SQL);
        }

        void fijarIsnParaTests(uint16_t isn) { isnFijoParaTests = isn; }

        void vincularRed(CapaRed* capaRed) {
            red = capaRed;
            metricas.reiniciar();
            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                nodos[i].cerrar();
            }
        }

        bool listen(uint8_t puertoTsap) {
            if (estaPuertoEnEscucha(puertoTsap)) return true;
            if (totalPuertosEscucha >= MAX_PUERTOS_TSAP) return false;
            puertosEscucha[totalPuertosEscucha++] = puertoTsap;
            return true;
        }

        bool cerrarPuerto(uint8_t puertoTsap) {
            for (size_t i = 0; i < totalPuertosEscucha; i++) {
                if (puertosEscucha[i] == puertoTsap) {
                    puertosEscucha[i] = puertosEscucha[totalPuertosEscucha - 1];
                    totalPuertosEscucha--;
                    return true;
                }
            }
            return false;
        }

        bool estaPuertoEnEscucha(uint8_t puertoTsap) const {
            for (size_t i = 0; i < totalPuertosEscucha; i++) {
                if (puertosEscucha[i] == puertoTsap) return true;
            }
            return false;
        }

        bool iniciarHandshakeTCP(int idNodo, uint8_t puerto = PUERTO_RPC) {
            if (!red || idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
            listen(puerto);

            ControlFlujoNodo& ctrl = nodos[idNodo];
            ctrl.cerrar();
            ctrl.seqLocal            = generarIsnInicial();
            ctrl.ultimaActividadRxMs = millis();

            if (red->esMedioPassthrough(idNodo)) {
                ctrl.estado = TCP_ESTABLECIDO;
                metricas.handshakesEstablecidos++;
                return true;
            }

            ctrl.estado         = TCP_SYN_ENVIADO;
            uint16_t seqInicial = ctrl.seqLocal++;
            return enviarControlTCP(idNodo, puerto, puerto, FLAG_SYN, seqInicial, 0, true);
        }

        bool desconectarTCP(int idNodo, uint8_t puerto = PUERTO_RPC) {
            if (!red || idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;

            ControlFlujoNodo& ctrl = nodos[idNodo];
            if (red->esMedioPassthrough(idNodo)) {
                ctrl.cerrar();
                return true;
            }

            if (ctrl.estado != TCP_ESTABLECIDO && ctrl.estado != TCP_SYN_RECIBIDO) {
                ctrl.cerrar();
                return false;
            }

            ctrl.estado     = TCP_FIN_ESPERA;
            uint16_t seqFin = ctrl.seqLocal++;
            return enviarControlTCP(idNodo, puerto, puerto, FLAG_FIN | FLAG_ACK, seqFin, ctrl.seqRemotaEsperada, true);
        }

        void anunciarRecuperacionCaida() {
            if (!red) return;
            bufferCtrlTx.prepararParaTx();
            bufferCtrlTx.inicio = LimitesPila::OFFSET_SESION;
            if (escribirCabeceraYCRC(bufferCtrlTx, PROTO_UDP, PUERTO_EVENTOS, PUERTO_EVENTOS, FLAG_CRASH_RECOVERY, 0, 0)) {
                red->bajarPaqueteBroadcast(bufferCtrlTx);
            }
        }

        bool subirSegmento(BufferPaquete& pktSalida, SegmentoTransporte& metaSalida) {
            if (!red) return false;

            if (!red->subirPaquete(bufferRx)) {
                mantenerTransporte();
                return false;
            }

            int id = bufferRx.idNodo;
            if (id < 0 || id >= LimitesPila::MAX_NODOS_RED) return false;

            ControlFlujoNodo& ctrl   = nodos[id];
            ctrl.ultimaActividadRxMs = millis();

            if (red->esMedioPassthrough(id)) {
                ctrl.estado = TCP_ESTABLECIDO;
                pktSalida   = bufferRx;
                metaSalida  = { id, PROTO_TCP, PUERTO_RPC, PUERTO_RPC, FLAG_PSH | FLAG_ACK, 0, 0, true };
                metricas.segmentosPassthroughRx++;
                return true;
            }

            const uint8_t* hdr = bufferRx.consumirCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
            if (!hdr) return false;

            uint8_t  proto      = hdr[0];
            uint8_t  ptoOrigen  = hdr[1];
            uint8_t  ptoDestino = hdr[2];
            uint8_t  flags      = hdr[3];
            uint16_t seq        = (static_cast<uint16_t>(hdr[4]) << 8) | hdr[5];
            uint16_t ack        = (static_cast<uint16_t>(hdr[6]) << 8) | hdr[7];
            uint16_t crcRecib   = (static_cast<uint16_t>(hdr[8]) << 8) | hdr[9];

            const uint8_t* payload = bufferRx.ptrActual();
            size_t lenPayload      = bufferRx.longitud;

            uint16_t crcCalc = calcularCRC16(hdr, 8);
            if (lenPayload > 0) {
                crcCalc = calcularCRC16(payload, lenPayload, crcCalc);
            }
            if (crcCalc != crcRecib) {
                metricas.erroresCrcTransporte++;
                return false;
            }

            if (!estaPuertoEnEscucha(ptoDestino)) {
                metricas.rechazosPuertoCerrado++;
                if (proto == PROTO_TCP && !(flags & FLAG_RST)) {
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_RST, 0, static_cast<uint16_t>(seq + 1), false);
                }
                return false;
            }

            if (proto == PROTO_UDP) {
                if (flags & FLAG_CRASH_RECOVERY) {
                    ctrl.cerrar();
                    return false;
                }
                if (lenPayload == 0) return false;
                pktSalida  = bufferRx;
                metaSalida = { id, proto, ptoOrigen, ptoDestino, flags, seq, ack, false };
                metricas.datagramasUdpRx++;
                return true;
            }

            if (proto == PROTO_TCP) {
                if (flags & FLAG_RST) {
                    metricas.resetsRecibidos++;
                    ctrl.cerrar();
                    return false;
                }

                if (flags == FLAG_SYN) {
                    uint16_t esperado = static_cast<uint16_t>(seq + 1);
                    if ((ctrl.estado == TCP_SYN_RECIBIDO || ctrl.estado == TCP_ESTABLECIDO) &&
                        ctrl.seqRemotaEsperada == esperado) {
                        uint16_t miSeqAnterior = static_cast<uint16_t>(ctrl.seqLocal - 1);
                        enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_SYN | FLAG_ACK, miSeqAnterior, ctrl.seqRemotaEsperada, false);
                        return false;
                    }
                    ctrl.cerrar();
                    ctrl.seqLocal            = generarIsnInicial();
                    ctrl.estado              = TCP_SYN_RECIBIDO;
                    ctrl.seqRemotaEsperada   = esperado;
                    ctrl.ultimaActividadRxMs = millis();
                    uint16_t miSeq           = ctrl.seqLocal++;
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_SYN | FLAG_ACK, miSeq, ctrl.seqRemotaEsperada, true);
                    return false;
                }

                if (ctrl.estado == TCP_SYN_ENVIADO && flags == (FLAG_SYN | FLAG_ACK)) {
                    if (ctrl.esperandoAck && ack == ctrl.seqPendienteAck) {
                        ctrl.esperandoAck = false;
                        ctrl.reintentos   = 0;
                    }
                    ctrl.seqRemotaEsperada = static_cast<uint16_t>(seq + 1);
                    ctrl.estado            = TCP_ESTABLECIDO;
                    metricas.handshakesEstablecidos++;
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada, false);
                    despacharSiguienteEnCola(id);
                    return false;
                }

                if (ctrl.estado == TCP_SYN_RECIBIDO && flags == FLAG_ACK) {
                    ctrl.estado       = TCP_ESTABLECIDO;
                    ctrl.esperandoAck = false;
                    ctrl.reintentos   = 0;
                    metricas.handshakesEstablecidos++;
                    return false;
                }

                if (flags & FLAG_FIN) {
                    ctrl.seqRemotaEsperada = static_cast<uint16_t>(seq + 1);
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada, false);
                    if (!ctrl.esperandoAck && ctrl.countCola == 0) {
                        ctrl.cerrar();
                        metricas.cierresSimetricos++;
                    } else {
                        ctrl.finRemotoRecibido = true;
                    }
                    return false;
                }

                if (flags & FLAG_ACK) {
                    if (ctrl.estado == TCP_FIN_ESPERA && ack == ctrl.seqPendienteAck) {
                        ctrl.cerrar();
                        metricas.cierresSimetricos++;
                        return false;
                    } else if (ctrl.esperandoAck && ack == ctrl.seqPendienteAck) {
                        ctrl.esperandoAck = false;
                        ctrl.reintentos   = 0;
                        despacharSiguienteEnCola(id);
                    }
                }

                if (ctrl.estado != TCP_ESTABLECIDO) {
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_RST, 0, 0, false);
                    return false;
                }

                if ((flags & FLAG_PSH) && lenPayload > 0) {
                    if (seq == ctrl.seqRemotaEsperada) {
                        ctrl.seqRemotaEsperada = static_cast<uint16_t>(seq + 1);
                        enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada, false);

                        pktSalida  = bufferRx;
                        metaSalida = { id, proto, ptoOrigen, ptoDestino, flags, seq, ack, false };
                        metricas.segmentosTcpDatosRx++;
                        return true;
                    } else {
                        metricas.duplicadosRetrasadosRx++;
                        enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada, false);
                        return false;
                    }
                }
            }
            return false;
        }

        bool bajarSegmentoUnicast(int idDestino, uint8_t protocolo, uint8_t puertoOrigen,
                                  uint8_t puertoDestino, BufferPaquete& pkt) {
            if (!red || idDestino < 0 || idDestino >= LimitesPila::MAX_NODOS_RED) return false;
            listen(puertoOrigen);

            if (red->esMedioPassthrough(idDestino)) {
                if (red->bajarPaqueteUnicast(idDestino, pkt)) {
                    metricas.segmentosPassthroughTx++;
                    return true;
                }
                return false;
            }

            if (protocolo == PROTO_UDP) {
                if (pkt.inicio < LimitesPila::TAMANO_CABECERA_TRANS) return false;

                size_t iniPrevio = pkt.inicio;
                size_t lenPrevia = pkt.longitud;
                uint8_t respaldo[LimitesPila::TAMANO_CABECERA_TRANS];
                memcpy(respaldo, &pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_TRANS], LimitesPila::TAMANO_CABECERA_TRANS);

                bool ok = escribirCabeceraYCRC(pkt, PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0) &&
                          red->bajarPaqueteUnicast(idDestino, pkt);

                pkt.inicio   = iniPrevio;
                pkt.longitud = lenPrevia;
                memcpy(&pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_TRANS], respaldo, LimitesPila::TAMANO_CABECERA_TRANS);

                if (ok) metricas.datagramasUdpTx++;
                return ok;
            }

            ControlFlujoNodo& ctrl = nodos[idDestino];
            if (ctrl.estado != TCP_ESTABLECIDO) return false;

            if (ctrl.esperandoAck) {
                if (ctrl.countCola >= CAP_COLA_TX_NODO) {
                    metricas.desbordesColaControlFlujo++;
                    return false;
                }
                ItemColaTx& slot   = ctrl.colaTx[ctrl.headCola];
                slot.ocupado       = true;
                slot.puertoOrigen  = puertoOrigen;
                slot.puertoDestino = puertoDestino;
                slot.paquete       = pkt;
                ctrl.headCola      = (ctrl.headCola + 1) % CAP_COLA_TX_NODO;
                ctrl.countCola++;
                return true;
            }

            return transmitirSegmentoTCPInmediato(idDestino, puertoOrigen, puertoDestino, pkt);
        }

        size_t bajarDatagramaBroadcast(uint8_t puertoOrigen, uint8_t puertoDestino, BufferPaquete& pkt) {
            if (!red) return 0;
            listen(puertoOrigen);

            if (pkt.inicio < LimitesPila::TAMANO_CABECERA_TRANS) return 0;

            size_t iniPrevio = pkt.inicio;
            size_t lenPrevia = pkt.longitud;
            uint8_t respaldo[LimitesPila::TAMANO_CABECERA_TRANS];
            memcpy(respaldo, &pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_TRANS], LimitesPila::TAMANO_CABECERA_TRANS);

            size_t enviados = 0;
            if (escribirCabeceraYCRC(pkt, PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0)) {
                enviados = red->bajarPaqueteBroadcast(pkt);
                metricas.datagramasUdpTx += enviados;
            }

            pkt.inicio   = iniPrevio;
            pkt.longitud = lenPrevia;
            memcpy(&pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_TRANS], respaldo, LimitesPila::TAMANO_CABECERA_TRANS);
            return enviados;
        }

        void mantenerTransporte() {
            if (!red) return;
            unsigned long ahora = millis();

            for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
                if (red->esMedioPassthrough(i)) continue;
                ControlFlujoNodo& ctrl = nodos[i];

                if (ctrl.estado != TCP_CERRADO && ctrl.esperandoAck) {
                    if (ahora - ctrl.ultimoEnvioMs >= rtoConfiguradoMs) {
                        if (ctrl.reintentos < MAX_REINTENTOS) {
                            ctrl.reintentos++;
                            ctrl.ultimoEnvioMs = ahora;
                            metricas.retransmisionesRto++;
                            red->bajarPaqueteUnicast(i, ctrl.ultimoPaqueteTx);
                        } else {
                            bool eraFin = (ctrl.estado == TCP_FIN_ESPERA);
                            ctrl.cerrar();
                            if (eraFin) {
                                metricas.cierresSimetricos++;
                            }
                        }
                    }
                }

                if (ctrl.estado == TCP_ESTABLECIDO && !ctrl.esperandoAck) {
                    if (ahora - ctrl.ultimaActividadRxMs >= timeoutSemiabiertaMs) {
                        ctrl.cerrar();
                        metricas.cierresSemiabiertosTimeout++;
                    }
                }
            }
        }

        void configurarTemporizadores(uint32_t nuevoRtoMs, uint32_t nuevoTimeoutSemiabiertaMs) {
            rtoConfiguradoMs     = nuevoRtoMs;
            timeoutSemiabiertaMs = nuevoTimeoutSemiabiertaMs;
        }

        EstadoTCP obtenerEstadoTCP(int idNodo) const {
            if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return TCP_CERRADO;
            return nodos[idNodo].estado;
        }

        const MetricasTransporte& obtenerMetricas() const { return metricas; }
        void reiniciarMetricas() { metricas.reiniciar(); }
    };

}