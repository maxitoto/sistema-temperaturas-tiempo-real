// Archivo: ./src/communication/layers/CapaEnlace.h
#pragma once
#include <Arduino.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaFisica.h"
#include "debug/DebugOLED.h"

class CapaEnlace {
  public:
    static constexpr uint8_t SOF_BYTE = 0x7E;
    static constexpr uint8_t ESC_BYTE = 0x7D;
    static constexpr uint8_t IAC_BYTE = 0xFF;
    static constexpr unsigned long TIMEOUT_TRAMA_MS = 500;

  private:
    CapaFisica* fisica = nullptr;
    int idMedio = -1;

    enum EstadoRX {
        ESPERANDO_SOF,
        LEYENDO_CABECERA,
        LEYENDO_DATOS,
        LEYENDO_FCS
    };

    EstadoRX estadoRx = ESPERANDO_SOF;
    bool escapandoRx = false;
    unsigned long ultimoByteMs = 0;

    uint8_t cabeceraRx[3];
    uint8_t posCabecera = 0;

    uint8_t bufferChunkRx[LimitesPila::TAMANO_MAX_CHUNK];
    uint8_t posDatosChunk = 0;

    uint8_t fcsRx[4];
    uint8_t posFcs = 0;

    uint8_t bufferEnsamblaje[LimitesPila::MAX_TRAMA_ENLACE];
    size_t longitudEnsamblada = 0;
    uint8_t totalChunksEsperados = 0;
    uint8_t chunkSiguiente = 0;

    uint8_t rafagaRx[32];
    size_t lenRafagaRx = 0;
    size_t posRafagaRx = 0;

    void reiniciarEstadoTrama() {
        estadoRx = ESPERANDO_SOF;
        escapandoRx = false;
        posCabecera = 0;
        posDatosChunk = 0;
        posFcs = 0;
    }

    void reiniciarEnsamblaje() {
        longitudEnsamblada = 0;
        totalChunksEsperados = 0;
        chunkSiguiente = 0;
    }

    static bool requiereEscape(uint8_t b) {
        return b == SOF_BYTE || b == ESC_BYTE || b == IAC_BYTE;
    }

    static void escribirByteEscapado(uint8_t* dst, size_t& pos, uint8_t b) {
        if (requiereEscape(b)) {
            dst[pos++] = ESC_BYTE;
            dst[pos++] = b ^ 0x20;
        } else {
            dst[pos++] = b;
        }
    }

  public:
    CapaEnlace() {}

    void configurarEnlace(CapaFisica* capaFisica, int identificadorMedio) {
        fisica = capaFisica;
        idMedio = identificadorMedio;
        lenRafagaRx = 0;
        posRafagaRx = 0;
        reiniciarEstadoTrama();
        reiniciarEnsamblaje();
    }

    // ==========================================================
    // CAMINO DE BAJADA (TX): Bypass en medios nativos / HDLC en USB
    // ==========================================================
    bool bajarTrama(const BufferPaquete& pkt) {
        if (!fisica || idMedio < 0 || pkt.longitud == 0 || pkt.longitud > LimitesPila::MAX_TRAMA_ENLACE) {
            return false;
        }

        const uint8_t* payload = pkt.ptrActual();
        size_t longitud = pkt.longitud;

        // BYPASS CAPA 2 (Wi-Fi / Medios Nativos): La MAC 802.11 ya delimita y aplica FCS-32
        if (fisica->esMedioNativo(idMedio)) {
            return fisica->enviarBytes(idMedio, payload, longitud, pkt.esDatagramaNativo) == longitud;
        }

        // CAMINO USB (idMedio == 0): Fragmentación 64B + Byte-Stuffing HDLC + Doble CRC16
        uint16_t crcMensajeUnificado = calcularCRC16(payload, longitud);
        uint8_t totalChunks = (longitud + LimitesPila::TAMANO_MAX_CHUNK - 1) / LimitesPila::TAMANO_MAX_CHUNK;

        uint8_t crudo[3 + LimitesPila::TAMANO_MAX_CHUNK + 4];
        uint8_t tramaFisica[1 + (3 + LimitesPila::TAMANO_MAX_CHUNK + 4) * 2];

        for (uint8_t i = 0; i < totalChunks; i++) {
            size_t offset = (size_t)i * LimitesPila::TAMANO_MAX_CHUNK;
            size_t restantes = longitud - offset;
            uint8_t lenSegmento = (restantes > LimitesPila::TAMANO_MAX_CHUNK)
                                    ? LimitesPila::TAMANO_MAX_CHUNK
                                    : (uint8_t)restantes;

            crudo[0] = i;
            crudo[1] = totalChunks;
            crudo[2] = lenSegmento;
            memcpy(&crudo[3], &payload[offset], lenSegmento);

            uint16_t crcTrama = calcularCRC16(crudo, 3 + lenSegmento);
            crudo[3 + lenSegmento] = (uint8_t)(crcTrama >> 8);
            crudo[4 + lenSegmento] = (uint8_t)(crcTrama & 0xFF);

            uint16_t crcFinal = (i == totalChunks - 1) ? crcMensajeUnificado : 0x0000;
            crudo[5 + lenSegmento] = (uint8_t)(crcFinal >> 8);
            crudo[6 + lenSegmento] = (uint8_t)(crcFinal & 0xFF);

            size_t totalCrudo = 7 + lenSegmento;
            size_t posTx = 0;
            tramaFisica[posTx++] = SOF_BYTE;
            for (size_t k = 0; k < totalCrudo; k++) {
                escribirByteEscapado(tramaFisica, posTx, crudo[k]);
            }

            if (fisica->enviarBytes(idMedio, tramaFisica, posTx) != posTx) {
                DebugOLED::registrarError("L2 TX ERR");
                return false;
            }
        }
        return true;
    }

    // ==========================================================
    // CAMINO DE SUBIDA (RX): Bypass en medios nativos / HDLC en USB
    // ==========================================================
    bool subirTrama(BufferPaquete& pktSalida) {
        if (!fisica || idMedio < 0) return false;

        // BYPASS CAPA 2 (Wi-Fi / Medios Nativos): Entrega directa a Capa 3
        if (fisica->esMedioNativo(idMedio)) {
            if (fisica->bytesDisponibles(idMedio) <= 0) return false;
            pktSalida.prepararParaRx(idMedio);
            bool esDatagrama = false;
            size_t leidos = fisica->recibirBytes(idMedio, pktSalida.raw, LimitesPila::MAX_TRAMA_ENLACE, esDatagrama);
            if (leidos == 0) return false;
            pktSalida.longitud = leidos;
            pktSalida.esDatagramaNativo = esDatagrama;
            return true;
        }

        // CAMINO USB (idMedio == 0): Máquina de estados HDLC
        unsigned long ahora = millis();
        if ((estadoRx != ESPERANDO_SOF || totalChunksEsperados > 0) &&
            (ahora - ultimoByteMs > TIMEOUT_TRAMA_MS)) {
            DebugOLED::registrarError("L2 TMO RX");
            reiniciarEstadoTrama();
            reiniciarEnsamblaje();
        }

        while (posRafagaRx < lenRafagaRx || fisica->bytesDisponibles(idMedio) > 0) {
            if (posRafagaRx >= lenRafagaRx) {
                lenRafagaRx = fisica->recibirBytes(idMedio, rafagaRx, sizeof(rafagaRx));
                posRafagaRx = 0;
                if (lenRafagaRx == 0) break;
                ultimoByteMs = millis();
            }

            while (posRafagaRx < lenRafagaRx) {
                uint8_t byteCrudo = rafagaRx[posRafagaRx++];

                if (byteCrudo == SOF_BYTE) {
                    posCabecera = 0;
                    posDatosChunk = 0;
                    posFcs = 0;
                    escapandoRx = false;
                    estadoRx = LEYENDO_CABECERA;
                    continue;
                }

                if (estadoRx == ESPERANDO_SOF) continue;

                if (byteCrudo == ESC_BYTE) {
                    escapandoRx = true;
                    continue;
                }

                uint8_t byteLeido = escapandoRx ? (byteCrudo ^ 0x20) : byteCrudo;
                escapandoRx = false;

                switch (estadoRx) {
                    case ESPERANDO_SOF:
                        break;

                    case LEYENDO_CABECERA:
                        cabeceraRx[posCabecera++] = byteLeido;
                        if (posCabecera == 3) {
                            uint8_t indice = cabeceraRx[0];
                            uint8_t total  = cabeceraRx[1];
                            uint8_t len    = cabeceraRx[2];
                            if (len == 0 || len > LimitesPila::TAMANO_MAX_CHUNK || total == 0 || indice >= total) {
                                DebugOLED::registrarError("L2 HDR ERR");
                                reiniciarEstadoTrama();
                                reiniciarEnsamblaje();
                            } else {
                                posDatosChunk = 0;
                                estadoRx = LEYENDO_DATOS;
                            }
                        }
                        break;

                    case LEYENDO_DATOS:
                        bufferChunkRx[posDatosChunk++] = byteLeido;
                        if (posDatosChunk == cabeceraRx[2]) {
                            posFcs = 0;
                            estadoRx = LEYENDO_FCS;
                        }
                        break;

                    case LEYENDO_FCS:
                        fcsRx[posFcs++] = byteLeido;
                        if (posFcs == 4) {
                            uint8_t indice = cabeceraRx[0];
                            uint8_t total  = cabeceraRx[1];
                            uint8_t len    = cabeceraRx[2];
                            uint16_t crcTramaRecib = ((uint16_t)fcsRx[0] << 8) | fcsRx[1];
                            uint16_t crcMsgRecib   = ((uint16_t)fcsRx[2] << 8) | fcsRx[3];

                            reiniciarEstadoTrama();

                            uint16_t crcCalc = calcularCRC16(cabeceraRx, 3);
                            crcCalc = calcularCRC16(bufferChunkRx, len, crcCalc);
                            if (crcCalc != crcTramaRecib) {
                                DebugOLED::registrarError("L2 CRC1 ERR");
                                reiniciarEnsamblaje();
                                continue;
                            }

                            if (indice == 0) {
                                reiniciarEnsamblaje();
                                totalChunksEsperados = total;
                                chunkSiguiente = 1;
                                memcpy(bufferEnsamblaje, bufferChunkRx, len);
                                longitudEnsamblada = len;
                            } else if (indice == chunkSiguiente && total == totalChunksEsperados) {
                                if (longitudEnsamblada + len > LimitesPila::MAX_TRAMA_ENLACE) {
                                    reiniciarEnsamblaje();
                                    continue;
                                }
                                memcpy(&bufferEnsamblaje[longitudEnsamblada], bufferChunkRx, len);
                                longitudEnsamblada += len;
                                chunkSiguiente++;
                            } else {
                                DebugOLED::registrarError("L2 SEQ ERR");
                                reiniciarEnsamblaje();
                                continue;
                            }

                            if (chunkSiguiente == totalChunksEsperados) {
                                uint16_t crcTotalCalc = calcularCRC16(bufferEnsamblaje, longitudEnsamblada);
                                if (crcTotalCalc != crcMsgRecib) {
                                    DebugOLED::registrarError("L2 CRC2 ERR");
                                    reiniciarEnsamblaje();
                                    continue;
                                }
                                pktSalida.prepararParaRx(idMedio);
                                memcpy(pktSalida.raw, bufferEnsamblaje, longitudEnsamblada);
                                pktSalida.longitud = longitudEnsamblada;
                                reiniciarEnsamblaje();
                                return true;
                            }
                        }
                        break;
                }
            }
        }
        return false;
    }
};