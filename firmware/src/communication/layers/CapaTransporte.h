// Archivo: ./src/communication/layers/CapaTransporte.h
#pragma once
#include <Arduino.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaRed.h"

struct SegmentoTransporte {
    int idNodo;
    uint8_t protocolo;
    uint8_t puertoOrigen;
    uint8_t puertoDestino;
    uint8_t flags;
    uint16_t numSecuencia;
    uint16_t numAcuse;
};

class CapaTransporte {
  public:
    static const uint8_t PROTO_TCP = 0x06;
    static const uint8_t PROTO_UDP = 0x11;

    static const uint8_t FLAG_SYN = 0x01;
    static const uint8_t FLAG_ACK = 0x02;
    static const uint8_t FLAG_PSH = 0x04;
    static const uint8_t FLAG_FIN = 0x08;
    static const uint8_t FLAG_RST = 0x10;

    static const uint8_t PUERTO_RPC     = 80;
    static const uint8_t PUERTO_EVENTOS = 81;

    static const unsigned long RTO_MS = 1000;
    static const uint8_t MAX_REINTENTOS = 3;

    enum EstadoTCP {
        TCP_CERRADO,
        TCP_SYN_ENVIADO,
        TCP_SYN_RECIBIDO,
        TCP_ESTABLECIDO
    };

  private:
    CapaRed* red;

    struct ControlFlujoNodo {
        EstadoTCP estado = TCP_CERRADO;
        uint16_t seqLocal = 100;
        uint16_t seqRemotaEsperada = 0;

        bool esperandoAck = false;
        uint16_t seqPendienteAck = 0;
        unsigned long ultimoEnvioMs = 0;
        uint8_t reintentos = 0;
        BufferPaquete ultimoPaqueteTx;

        // Cola estática de 1 slot para serializar envíos simultáneos (CALL + REPLY)
        bool hayPaqueteEnCola = false;
        uint8_t ptoOrigCola = PUERTO_RPC;
        uint8_t ptoDestCola = PUERTO_RPC;
        BufferPaquete paqueteEnColaTx;
    };

    ControlFlujoNodo nodos[LimitesPila::MAX_NODOS_RED];
    BufferPaquete bufferRx;

    void escribirCabeceraYCRC(BufferPaquete& pkt, uint8_t proto, uint8_t ptoOrig, uint8_t ptoDest,
                              uint8_t flags, uint16_t seq, uint16_t ack) const {
        const uint8_t* payload = pkt.ptrActual();
        size_t lenPayload = pkt.longitud;

        uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
        if (!hdr) return;

        hdr[0] = proto;
        hdr[1] = ptoOrig;
        hdr[2] = ptoDest;
        hdr[3] = flags;
        hdr[4] = (uint8_t)(seq >> 8);
        hdr[5] = (uint8_t)(seq & 0xFF);
        hdr[6] = (uint8_t)(ack >> 8);
        hdr[7] = (uint8_t)(ack & 0xFF);

        uint16_t crc = calcularCRC16(hdr, 8);
        if (lenPayload > 0) {
            crc = calcularCRC16(payload, lenPayload, crc);
        }
        hdr[8] = (uint8_t)(crc >> 8);
        hdr[9] = (uint8_t)(crc & 0xFF);
    }

    void enviarControlTCP(int idNodo, uint8_t ptoOrig, uint8_t ptoDest,
                          uint8_t flags, uint16_t seq, uint16_t ack) {
        BufferPaquete pktCtrl;
        pktCtrl.prepararParaTx(idNodo);
        escribirCabeceraYCRC(pktCtrl, PROTO_TCP, ptoOrig, ptoDest, flags, seq, ack);
        red->bajarPaqueteUnicast(idNodo, pktCtrl);
    }

    bool transmitirSegmentoTCPInmediato(int idDestino, uint8_t puertoOrigen,
                                        uint8_t puertoDestino, BufferPaquete& pkt) {
        ControlFlujoNodo& ctrl = nodos[idDestino];
        uint16_t seqActual = ctrl.seqLocal++;
        uint8_t flags = FLAG_PSH | FLAG_ACK;

        escribirCabeceraYCRC(pkt, PROTO_TCP, puertoOrigen, puertoDestino,
                             flags, seqActual, ctrl.seqRemotaEsperada);

        ctrl.ultimoPaqueteTx = pkt;
        ctrl.esperandoAck = true;
        ctrl.seqPendienteAck = seqActual + 1;
        ctrl.ultimoEnvioMs = millis();
        ctrl.reintentos = 0;

        return red->bajarPaqueteUnicast(idDestino, pkt);
    }

  public:
    explicit CapaTransporte(CapaRed* capaRed) : red(capaRed) {}

    EstadoTCP obtenerEstadoTCP(int idNodo) const {
        if (idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return TCP_CERRADO;
        return nodos[idNodo].estado;
    }

    bool iniciarHandshakeTCP(int idNodo, uint8_t puerto = PUERTO_RPC) {
        if (!red || idNodo < 0 || idNodo >= LimitesPila::MAX_NODOS_RED) return false;
        ControlFlujoNodo& ctrl = nodos[idNodo];
        ctrl.estado = TCP_SYN_ENVIADO;
        ctrl.esperandoAck = false;
        ctrl.hayPaqueteEnCola = false;
        uint16_t seqInicial = ctrl.seqLocal++;
        enviarControlTCP(idNodo, puerto, puerto, FLAG_SYN, seqInicial, 0);
        return true;
    }

    bool subirSegmento(BufferPaquete& pktSalida, SegmentoTransporte& metaSalida) {
        if (!red) return false;

        mantenerTransporte();

        if (!red->subirPaquete(bufferRx)) return false;

        int id = bufferRx.idNodo;
        if (id < 0 || id >= LimitesPila::MAX_NODOS_RED) return false;

        const uint8_t* hdr = bufferRx.consumirCabecera(LimitesPila::TAMANO_CABECERA_TRANS);
        if (!hdr) return false;

        uint8_t proto      = hdr[0];
        uint8_t ptoOrigen  = hdr[1];
        uint8_t ptoDestino = hdr[2];
        uint8_t flags      = hdr[3];
        uint16_t seq       = ((uint16_t)hdr[4] << 8) | hdr[5];
        uint16_t ack       = ((uint16_t)hdr[6] << 8) | hdr[7];
        uint16_t crcRecib  = ((uint16_t)hdr[8] << 8) | hdr[9];

        const uint8_t* payload = bufferRx.ptrActual();
        size_t lenPayload = bufferRx.longitud;

        uint16_t crcCalc = calcularCRC16(hdr, 8);
        if (lenPayload > 0) {
            crcCalc = calcularCRC16(payload, lenPayload, crcCalc);
        }
        if (crcCalc != crcRecib) {
            return false;
        }

        if (proto == PROTO_UDP) {
            if (lenPayload == 0) return false;
            pktSalida = bufferRx;
            metaSalida = { id, proto, ptoOrigen, ptoDestino, flags, seq, ack };
            return true;
        }

        if (proto == PROTO_TCP) {
            ControlFlujoNodo& ctrl = nodos[id];

            if (flags & FLAG_RST) {
                ctrl.estado = TCP_CERRADO;
                ctrl.esperandoAck = false;
                ctrl.hayPaqueteEnCola = false;
                return false;
            }

            if (flags == FLAG_SYN) {
                uint16_t esperado = (uint16_t)(seq + 1);
                // Si es una retransmisión del mismo SYN, no incrementamos seqLocal otra vez
                if ((ctrl.estado == TCP_SYN_RECIBIDO || ctrl.estado == TCP_ESTABLECIDO) &&
                    ctrl.seqRemotaEsperada == esperado) {
                    uint16_t miSeqAnterior = (uint16_t)(ctrl.seqLocal - 1);
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_SYN | FLAG_ACK, miSeqAnterior, ctrl.seqRemotaEsperada);
                    return false;
                }
                ctrl.estado = TCP_SYN_RECIBIDO;
                ctrl.seqRemotaEsperada = esperado;
                ctrl.esperandoAck = false;
                ctrl.hayPaqueteEnCola = false;
                uint16_t miSeq = ctrl.seqLocal++;
                enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_SYN | FLAG_ACK, miSeq, ctrl.seqRemotaEsperada);
                return false;
            }

            if (ctrl.estado == TCP_SYN_ENVIADO && flags == (FLAG_SYN | FLAG_ACK)) {
                ctrl.seqRemotaEsperada = seq + 1;
                ctrl.estado = TCP_ESTABLECIDO;
                enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada);
                return false;
            }

            if (flags & FLAG_FIN) {
                ctrl.estado = TCP_CERRADO;
                ctrl.esperandoAck = false;
                ctrl.hayPaqueteEnCola = false;
                ctrl.seqRemotaEsperada = seq + 1;
                enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada);
                red->desconectarNodo(id);
                return false;
            }

            if (flags & FLAG_ACK) {
                if (ctrl.estado == TCP_SYN_RECIBIDO) {
                    ctrl.estado = TCP_ESTABLECIDO;
                }
                if (ctrl.esperandoAck && ack == ctrl.seqPendienteAck) {
                    ctrl.esperandoAck = false;
                    ctrl.reintentos = 0;

                    // Si había un paquete esperando turno en la cola Stop-and-Wait, enviarlo ahora
                    if (ctrl.hayPaqueteEnCola) {
                        ctrl.hayPaqueteEnCola = false;
                        BufferPaquete pktSig = ctrl.paqueteEnColaTx;
                        transmitirSegmentoTCPInmediato(id, ctrl.ptoOrigCola, ctrl.ptoDestCola, pktSig);
                    }
                }
            }

            if (ctrl.estado != TCP_ESTABLECIDO) {
                enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_RST, 0, 0);
                return false;
            }

            if ((flags & FLAG_PSH) && lenPayload > 0) {
                if (seq == ctrl.seqRemotaEsperada) {
                    ctrl.seqRemotaEsperada = seq + 1;
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada);

                    pktSalida = bufferRx;
                    metaSalida = { id, proto, ptoOrigen, ptoDestino, flags, seq, ack };
                    return true;
                } else {
                    enviarControlTCP(id, ptoDestino, ptoOrigen, FLAG_ACK, ctrl.seqLocal, ctrl.seqRemotaEsperada);
                    return false;
                }
            }
        }
        return false;
    }

    bool bajarSegmentoUnicast(int idDestino, uint8_t protocolo, uint8_t puertoOrigen,
                              uint8_t puertoDestino, BufferPaquete& pkt) {
        if (!red || idDestino < 0 || idDestino >= LimitesPila::MAX_NODOS_RED) return false;

        if (protocolo == PROTO_UDP) {
            escribirCabeceraYCRC(pkt, PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0);
            return red->bajarPaqueteUnicast(idDestino, pkt);
        }

        ControlFlujoNodo& ctrl = nodos[idDestino];
        if (ctrl.estado != TCP_ESTABLECIDO) return false;

        // Si ya hay un segmento esperando ACK, encolamos este sin pisar el paquete en vuelo
        if (ctrl.esperandoAck) {
            if (ctrl.hayPaqueteEnCola) return false; // Cola llena
            ctrl.paqueteEnColaTx = pkt;
            ctrl.ptoOrigCola = puertoOrigen;
            ctrl.ptoDestCola = puertoDestino;
            ctrl.hayPaqueteEnCola = true;
            return true;
        }

        return transmitirSegmentoTCPInmediato(idDestino, puertoOrigen, puertoDestino, pkt);
    }

    void bajarDatagramaBroadcast(uint8_t puertoOrigen, uint8_t puertoDestino, BufferPaquete& pkt) {
        if (!red) return;
        escribirCabeceraYCRC(pkt, PROTO_UDP, puertoOrigen, puertoDestino, 0, 0, 0);
        red->bajarPaqueteBroadcast(pkt);
    }

    void mantenerTransporte() {
        unsigned long ahora = millis();
        for (int i = 0; i < LimitesPila::MAX_NODOS_RED; i++) {
            ControlFlujoNodo& ctrl = nodos[i];
            if (ctrl.estado == TCP_ESTABLECIDO && ctrl.esperandoAck) {
                if (ahora - ctrl.ultimoEnvioMs >= RTO_MS) {
                    if (ctrl.reintentos < MAX_REINTENTOS) {
                        ctrl.reintentos++;
                        ctrl.ultimoEnvioMs = ahora;
                        BufferPaquete copiaReenvio = ctrl.ultimoPaqueteTx;
                        red->bajarPaqueteUnicast(i, copiaReenvio);
                    } else {
                        ctrl.esperandoAck = false;
                        ctrl.hayPaqueteEnCola = false;
                        ctrl.estado = TCP_CERRADO;
                    }
                }
            }
        }
    }
};