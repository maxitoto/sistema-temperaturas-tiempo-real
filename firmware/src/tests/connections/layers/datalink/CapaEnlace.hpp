#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../common/BufferRed.hpp"
#include "../physic/CapaFisica.hpp"

namespace OSI {

    struct MetricasEnlace {
        uint32_t tramasPassthroughTx  = 0;
        uint32_t tramasPassthroughRx  = 0;
        uint32_t tramasProtegidasTx   = 0;
        uint32_t tramasProtegidasRx   = 0;
        uint32_t tramasDuplicadasRx   = 0;
        uint32_t chunksTx             = 0;
        uint32_t chunksRxValidos      = 0;
        uint32_t reintentosTx         = 0;
        uint32_t nacksEnviados        = 0;
        uint32_t nacksRecibidos       = 0;
        uint32_t acksRecibidos        = 0;
        uint32_t rechazosOcupadoArq   = 0;
        uint32_t erroresCabecera      = 0;
        uint32_t erroresCrcChunk      = 0;
        uint32_t erroresCrcMensaje    = 0;
        uint32_t erroresSecuencia     = 0;
        uint32_t erroresDesbordeMTU   = 0;
        uint32_t timeoutsEnsamblaje   = 0;

        void reiniciar() { *this = MetricasEnlace{}; }
    };

    class CapaEnlace {
    public:
        static constexpr uint8_t  SOF_BYTE             = 0x7E;
        static constexpr uint8_t  ESC_BYTE             = 0x7D;
        static constexpr uint8_t  IAC_BYTE             = 0xFF;
        static constexpr uint8_t  MASK_ESCAPE          = 0x20;

        static constexpr uint8_t  CTRL_CHUNK_DATOS     = 0x00;
        static constexpr uint8_t  CTRL_TRAMA_ACK       = 0x81;
        static constexpr uint8_t  CTRL_TRAMA_NACK      = 0x82;
        static constexpr uint8_t  CTRL_TRAMA_SYNC      = 0x83;

        static constexpr uint32_t TIMEOUT_TRAMA_MS     = 500;
        static constexpr uint32_t TIMEOUT_ACK_ARQ_MS   = 250;
        static constexpr uint8_t  MAX_REINTENTOS_TRAMA = 3;

    private:
        enum class EstadoRX : uint8_t {
            ESPERANDO_SOF = 0,
            LEYENDO_CABECERA,
            LEYENDO_DATOS,
            LEYENDO_FCS
        };

        CapaFisica*    fisica                    = nullptr;
        int            idMedio                   = -1;
        bool           arqEnlaceActivo           = false;

        EstadoRX       estadoRx                  = EstadoRX::ESPERANDO_SOF;
        bool           escapandoRx               = false;
        bool           descartandoHastaChunkCero = false;
        unsigned long  ultimoByteMs              = 0;

        uint8_t        cabeceraRx[3]             = {0};
        uint8_t        posCabecera               = 0;

        uint8_t        bufferChunkRx[LimitesPila::TAMANO_MAX_CHUNK] = {0};
        uint8_t        posDatosChunk             = 0;

        uint8_t        fcsRx[4]                  = {0};
        uint8_t        posFcs                    = 0;

        uint8_t        bufferEnsamblaje[LimitesPila::MAX_TRAMA_ENLACE] = {0};
        size_t         longitudEnsamblada        = 0;
        uint8_t        totalChunksEsperados      = 0;
        uint8_t        chunkSiguiente            = 0;
        uint8_t        seqMsgEnEnsamblaje        = 0;

        uint8_t        rafagaRx[64]              = {0};
        size_t         lenRafagaRx               = 0;
        size_t         posRafagaRx               = 0;

        uint8_t        ultimaTramaTx[LimitesPila::MAX_TRAMA_ENLACE] = {0};
        size_t         lenUltimaTramaTx          = 0;
        uint8_t        reintentosActuales        = 0;
        uint8_t        seqMensajeTx              = 1;
        uint8_t        seqUltimoTxEnviado        = 0;
        int16_t        ultimoSeqMensajeRx        = -1;
        bool           esperandoAckArq           = false;
        bool           primerEnvioTrasBoot       = true;
        bool           envioDiferido             = false;
        uint8_t        reintentosSync            = 0;
        unsigned long  ultimoEnvioSyncMs         = 0;
        unsigned long  ultimoEnvioArqMs          = 0;

        uint8_t        crudoTx[3 + LimitesPila::TAMANO_MAX_CHUNK + 4] = {0};
        uint8_t        tramaFisicaTx[1 + (3 + LimitesPila::TAMANO_MAX_CHUNK + 4) * 2] = {0};

        MetricasEnlace metricas;

        void reiniciarEstadoTrama() {
            estadoRx      = EstadoRX::ESPERANDO_SOF;
            escapandoRx   = false;
            posCabecera   = 0;
            posDatosChunk = 0;
            posFcs        = 0;
        }

        void reiniciarEnsamblaje() {
            longitudEnsamblada   = 0;
            totalChunksEsperados = 0;
            chunkSiguiente       = 0;
        }

        void abortarEnsamblajeConNack() {
            uint8_t seqFallido = static_cast<uint8_t>((cabeceraRx[1] >> 4) & 0x0F);
            reiniciarEstadoTrama();
            reiniciarEnsamblaje();
            if (!descartandoHastaChunkCero) {
                descartandoHastaChunkCero = true;
                enviarSenalControl(CTRL_TRAMA_NACK, seqFallido);
            }
        }

        static bool requiereEscape(uint8_t b) {
            return b == SOF_BYTE || b == ESC_BYTE || b == IAC_BYTE;
        }

        static void escribirByteEscapado(uint8_t* dst, size_t& pos, uint8_t b) {
            if (requiereEscape(b)) {
                dst[pos++] = ESC_BYTE;
                dst[pos++] = b ^ MASK_ESCAPE;
            } else {
                dst[pos++] = b;
            }
        }

        void enviarSenalControl(uint8_t tipoCtrl, uint8_t seq = 0) {
            if (!fisica || idMedio < 0 || !arqEnlaceActivo) return;
            uint8_t crudo[7] = { tipoCtrl, static_cast<uint8_t>(((seq & 0x0F) << 4) | 1), 0, 0, 0, 0, 0 };
            uint16_t crc = calcularCRC16(crudo, 3);
            crudo[3] = static_cast<uint8_t>(crc >> 8);
            crudo[4] = static_cast<uint8_t>(crc & 0xFF);

            uint8_t trama[16];
            size_t pos = 0;
            trama[pos++] = SOF_BYTE;
            for (size_t i = 0; i < sizeof(crudo); i++) {
                escribirByteEscapado(trama, pos, crudo[i]);
            }
            fisica->enviarBytes(idMedio, trama, pos);
            if (tipoCtrl == CTRL_TRAMA_NACK) {
                metricas.nacksEnviados++;
            }
        }

        bool transmitirPayloadFragmentado(const uint8_t* payload, size_t longitud, uint8_t seqMsg) {
            uint16_t crcMensajeUnificado = calcularCRC16(payload, longitud);
            uint8_t totalChunks = static_cast<uint8_t>(
                (longitud + LimitesPila::TAMANO_MAX_CHUNK - 1) / LimitesPila::TAMANO_MAX_CHUNK
            );
            uint8_t byteTotalYSeq = static_cast<uint8_t>(((seqMsg & 0x0F) << 4) | (totalChunks & 0x0F));

            for (uint8_t i = 0; i < totalChunks; i++) {
                size_t offset = static_cast<size_t>(i) * LimitesPila::TAMANO_MAX_CHUNK;
                size_t restantes = longitud - offset;
                uint8_t lenSegmento = (restantes > LimitesPila::TAMANO_MAX_CHUNK)
                                        ? static_cast<uint8_t>(LimitesPila::TAMANO_MAX_CHUNK)
                                        : static_cast<uint8_t>(restantes);

                crudoTx[0] = i;
                crudoTx[1] = byteTotalYSeq;
                crudoTx[2] = lenSegmento;
                memcpy(&crudoTx[3], &payload[offset], lenSegmento);

                uint16_t crcTrama = calcularCRC16(crudoTx, 3 + lenSegmento);
                crudoTx[3 + lenSegmento] = static_cast<uint8_t>(crcTrama >> 8);
                crudoTx[4 + lenSegmento] = static_cast<uint8_t>(crcTrama & 0xFF);

                uint16_t crcFinal = (i == totalChunks - 1) ? crcMensajeUnificado : 0x0000;
                crudoTx[5 + lenSegmento] = static_cast<uint8_t>(crcFinal >> 8);
                crudoTx[6 + lenSegmento] = static_cast<uint8_t>(crcFinal & 0xFF);

                size_t totalCrudo = 7 + lenSegmento;
                size_t posTx = 0;
                tramaFisicaTx[posTx++] = SOF_BYTE;
                for (size_t k = 0; k < totalCrudo; k++) {
                    escribirByteEscapado(tramaFisicaTx, posTx, crudoTx[k]);
                }

                bool enviadoOk = false;
                for (uint8_t intento = 0; intento <= MAX_REINTENTOS_TRAMA; intento++) {
                    if (fisica->enviarBytes(idMedio, tramaFisicaTx, posTx) == posTx) {
                        enviadoOk = true;
                        break;
                    }
                    metricas.reintentosTx++;
                }

                if (!enviadoOk) return false;
                metricas.chunksTx++;
            }

            if (arqEnlaceActivo) {
                esperandoAckArq  = true;
                ultimoEnvioArqMs = millis();
            }
            metricas.tramasProtegidasTx++;
            return true;
        }

    public:
        CapaEnlace() = default;

        void configurarEnlace(CapaFisica* capaFisica, int identificadorMedio, bool activarArqEnlace = false) {
            fisica                    = capaFisica;
            idMedio                   = identificadorMedio;
            arqEnlaceActivo           = activarArqEnlace;
            lenRafagaRx               = 0;
            posRafagaRx               = 0;
            lenUltimaTramaTx          = 0;
            reintentosActuales        = 0;
            seqMensajeTx              = 1;
            seqUltimoTxEnviado        = 0;
            ultimoSeqMensajeRx        = -1;
            esperandoAckArq           = false;
            primerEnvioTrasBoot       = true;
            envioDiferido             = false;
            reintentosSync            = 0;
            ultimoEnvioSyncMs         = 0;
            descartandoHastaChunkCero = false;
            reiniciarEstadoTrama();
            reiniciarEnsamblaje();
            metricas.reiniciar();
        }

        void habilitarArqEnlace(bool activo) { arqEnlaceActivo = activo; }

        void reiniciarEmisor() {
            lenUltimaTramaTx    = 0;
            reintentosActuales  = 0;
            seqMensajeTx        = 1;
            seqUltimoTxEnviado  = 0;
            esperandoAckArq     = false;
            primerEnvioTrasBoot = true;
            envioDiferido       = false;
            reintentosSync      = 0;
            ultimoEnvioSyncMs   = 0;
        }

        bool estaEsperandoAck() const { return arqEnlaceActivo && esperandoAckArq; }

        bool reenviarUltimaTrama() {
            if (!fisica || idMedio < 0 || lenUltimaTramaTx == 0) return false;
            if (reintentosActuales >= MAX_REINTENTOS_TRAMA) {
                esperandoAckArq  = false;
                lenUltimaTramaTx = 0;
                return false;
            }
            reintentosActuales++;
            metricas.reintentosTx++;
            return transmitirPayloadFragmentado(ultimaTramaTx, lenUltimaTramaTx, seqUltimoTxEnviado);
        }

        void mantenerEnlace() {
            if (envioDiferido) {
                if (millis() - ultimoEnvioSyncMs >= TIMEOUT_ACK_ARQ_MS) {
                    if (++reintentosSync > MAX_REINTENTOS_TRAMA) {
                        envioDiferido = false;
                        transmitirPayloadFragmentado(ultimaTramaTx, lenUltimaTramaTx, seqUltimoTxEnviado);
                    } else {
                        ultimoEnvioSyncMs = millis();
                        enviarSenalControl(CTRL_TRAMA_SYNC, 0);
                    }
                }
                return;
            }
            if (!arqEnlaceActivo || !esperandoAckArq || lenUltimaTramaTx == 0) return;
            if (millis() - ultimoEnvioArqMs >= TIMEOUT_ACK_ARQ_MS) {
                reenviarUltimaTrama();
            }
        }

        bool bajarTrama(const BufferPaquete& pkt) {
            if (!fisica || idMedio < 0 || pkt.longitud == 0) return false;

            if (pkt.longitud > LimitesPila::MAX_TRAMA_ENLACE) {
                metricas.erroresDesbordeMTU++;
                return false;
            }

            const uint8_t* payload = pkt.ptrActual();
            size_t longitud        = pkt.longitud;

            if (!fisica->esMedioCrudo(idMedio)) {
                if (fisica->enviarBytes(idMedio, payload, longitud) == longitud) {
                    metricas.tramasPassthroughTx++;
                    return true;
                }
                return false;
            }

            if (arqEnlaceActivo && esperandoAckArq) {
                metricas.rechazosOcupadoArq++;
                return false;
            }

            bool hacerSync = arqEnlaceActivo && primerEnvioTrasBoot;
            if (arqEnlaceActivo) {
                primerEnvioTrasBoot = false;
            }

            memcpy(ultimaTramaTx, payload, longitud);
            lenUltimaTramaTx   = longitud;
            reintentosActuales = 0;
            seqUltimoTxEnviado = seqMensajeTx;
            seqMensajeTx       = static_cast<uint8_t>((seqMensajeTx % 15) + 1);

            if (hacerSync) {
                envioDiferido     = true;
                esperandoAckArq   = true;
                reintentosSync    = 0;
                ultimoEnvioSyncMs = millis();
                enviarSenalControl(CTRL_TRAMA_SYNC, 0);
                return true;
            }
            return transmitirPayloadFragmentado(payload, longitud, seqUltimoTxEnviado);
        }

        bool subirTrama(BufferPaquete& pktSalida) {
            if (!fisica || idMedio < 0) return false;

            mantenerEnlace();

            if (!fisica->esMedioCrudo(idMedio)) {
                int disponibles = fisica->bytesDisponibles(idMedio);
                if (disponibles <= 0) return false;

                if (static_cast<size_t>(disponibles) > LimitesPila::MAX_TRAMA_ENLACE) {
                    uint8_t descarte[64];
                    while (fisica->bytesDisponibles(idMedio) > 0) {
                        fisica->recibirBytes(idMedio, descarte, sizeof(descarte));
                    }
                    metricas.erroresDesbordeMTU++;
                    return false;
                }

                pktSalida.prepararParaRx(idMedio);
                size_t leidos = fisica->recibirBytes(idMedio, pktSalida.raw, LimitesPila::MAX_TRAMA_ENLACE);
                if (leidos == 0) return false;

                pktSalida.longitud = leidos;
                metricas.tramasPassthroughRx++;
                return true;
            }

            unsigned long ahora = millis();
            if ((estadoRx != EstadoRX::ESPERANDO_SOF || totalChunksEsperados > 0) &&
                (ahora - ultimoByteMs > TIMEOUT_TRAMA_MS)) {
                metricas.timeoutsEnsamblaje++;
                descartandoHastaChunkCero = false;
                abortarEnsamblajeConNack();
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
                        posCabecera   = 0;
                        posDatosChunk = 0;
                        posFcs        = 0;
                        escapandoRx   = false;
                        estadoRx      = EstadoRX::LEYENDO_CABECERA;
                        continue;
                    }

                    if (estadoRx == EstadoRX::ESPERANDO_SOF) continue;

                    if (byteCrudo == ESC_BYTE) {
                        escapandoRx = true;
                        continue;
                    }

                    uint8_t byteLeido = escapandoRx ? (byteCrudo ^ MASK_ESCAPE) : byteCrudo;
                    escapandoRx = false;

                    switch (estadoRx) {
                        case EstadoRX::ESPERANDO_SOF:
                            break;

                        case EstadoRX::LEYENDO_CABECERA:
                            cabeceraRx[posCabecera++] = byteLeido;
                            if (posCabecera == 3) {
                                uint8_t indice = cabeceraRx[0];
                                uint8_t total  = cabeceraRx[1] & 0x0F;
                                uint8_t len    = cabeceraRx[2];

                                if ((indice == CTRL_TRAMA_ACK || indice == CTRL_TRAMA_NACK || indice == CTRL_TRAMA_SYNC) &&
                                    total == 1 && len == 0) {
                                    posFcs   = 0;
                                    estadoRx = EstadoRX::LEYENDO_FCS;
                                    break;
                                }

                                if (descartandoHastaChunkCero && indice != 0) {
                                    reiniciarEstadoTrama();
                                    break;
                                }

                                if (len == 0 || len > LimitesPila::TAMANO_MAX_CHUNK || total == 0 || indice >= total) {
                                    metricas.erroresCabecera++;
                                    abortarEnsamblajeConNack();
                                } else {
                                    posDatosChunk = 0;
                                    estadoRx      = EstadoRX::LEYENDO_DATOS;
                                }
                            }
                            break;

                        case EstadoRX::LEYENDO_DATOS:
                            bufferChunkRx[posDatosChunk++] = byteLeido;
                            if (posDatosChunk == cabeceraRx[2]) {
                                posFcs   = 0;
                                estadoRx = EstadoRX::LEYENDO_FCS;
                            }
                            break;

                        case EstadoRX::LEYENDO_FCS:
                            fcsRx[posFcs++] = byteLeido;
                            if (posFcs == 4) {
                                uint8_t indice         = cabeceraRx[0];
                                uint8_t seqMsg         = (cabeceraRx[1] >> 4) & 0x0F;
                                uint8_t total          = cabeceraRx[1] & 0x0F;
                                uint8_t len            = cabeceraRx[2];
                                uint16_t crcTramaRecib = (static_cast<uint16_t>(fcsRx[0]) << 8) | fcsRx[1];
                                uint16_t crcMsgRecib   = (static_cast<uint16_t>(fcsRx[2]) << 8) | fcsRx[3];

                                reiniciarEstadoTrama();

                                uint16_t crcCalc = calcularCRC16(cabeceraRx, 3);
                                if (len > 0) {
                                    crcCalc = calcularCRC16(bufferChunkRx, len, crcCalc);
                                }

                                if (crcCalc != crcTramaRecib) {
                                    metricas.erroresCrcChunk++;
                                    abortarEnsamblajeConNack();
                                    continue;
                                }

                                if (indice == CTRL_TRAMA_SYNC && len == 0) {
                                    ultimoSeqMensajeRx        = -1;
                                    descartandoHastaChunkCero = false;
                                    reiniciarEnsamblaje();
                                    enviarSenalControl(CTRL_TRAMA_ACK, 0);
                                    continue;
                                }
                                if (indice == CTRL_TRAMA_ACK && len == 0 && seqMsg == 0) {
                                    if (envioDiferido) {
                                        envioDiferido = false;
                                        transmitirPayloadFragmentado(ultimaTramaTx, lenUltimaTramaTx, seqUltimoTxEnviado);
                                    }
                                    continue;
                                }
                                if ((indice == CTRL_TRAMA_ACK || indice == CTRL_TRAMA_NACK) && len == 0 &&
                                    (!esperandoAckArq || envioDiferido || (seqMsg != 0 && seqMsg != seqUltimoTxEnviado))) {
                                    continue;
                                }
                                if (indice == CTRL_TRAMA_ACK && len == 0) {
                                    metricas.acksRecibidos++;
                                    esperandoAckArq    = false;
                                    lenUltimaTramaTx   = 0;
                                    reintentosActuales = 0;
                                    continue;
                                }
                                if (indice == CTRL_TRAMA_NACK && len == 0) {
                                    metricas.nacksRecibidos++;
                                    reenviarUltimaTrama();
                                    continue;
                                }

                                if (indice == 0) {
                                    descartandoHastaChunkCero = false;
                                    reiniciarEnsamblaje();
                                    totalChunksEsperados = total;
                                    seqMsgEnEnsamblaje   = seqMsg;
                                    chunkSiguiente       = 1;
                                    memcpy(bufferEnsamblaje, bufferChunkRx, len);
                                    longitudEnsamblada   = len;
                                    metricas.chunksRxValidos++;
                                } else if (!descartandoHastaChunkCero &&
                                           indice == chunkSiguiente &&
                                           total == totalChunksEsperados &&
                                           seqMsg == seqMsgEnEnsamblaje) {
                                    if (longitudEnsamblada + len > LimitesPila::MAX_TRAMA_ENLACE) {
                                        metricas.erroresDesbordeMTU++;
                                        abortarEnsamblajeConNack();
                                        continue;
                                    }
                                    memcpy(&bufferEnsamblaje[longitudEnsamblada], bufferChunkRx, len);
                                    longitudEnsamblada += len;
                                    chunkSiguiente++;
                                    metricas.chunksRxValidos++;
                                } else {
                                    if (!descartandoHastaChunkCero) {
                                        metricas.erroresSecuencia++;
                                        abortarEnsamblajeConNack();
                                    }
                                    continue;
                                }

                                if (chunkSiguiente == totalChunksEsperados) {
                                    uint16_t crcTotalCalc = calcularCRC16(bufferEnsamblaje, longitudEnsamblada);
                                    if (crcTotalCalc != crcMsgRecib) {
                                        metricas.erroresCrcMensaje++;
                                        abortarEnsamblajeConNack();
                                        continue;
                                    }

                                    enviarSenalControl(CTRL_TRAMA_ACK, seqMsgEnEnsamblaje);

                                    if (arqEnlaceActivo && ultimoSeqMensajeRx == static_cast<int16_t>(seqMsgEnEnsamblaje)) {
                                        metricas.tramasDuplicadasRx++;
                                        reiniciarEnsamblaje();
                                        continue;
                                    }
                                    ultimoSeqMensajeRx = static_cast<int16_t>(seqMsgEnEnsamblaje);

                                    pktSalida.prepararParaRx(idMedio);
                                    memcpy(pktSalida.raw, bufferEnsamblaje, longitudEnsamblada);
                                    pktSalida.longitud = longitudEnsamblada;
                                    reiniciarEnsamblaje();
                                    metricas.tramasProtegidasRx++;
                                    return true;
                                }
                            }
                            break;
                    }
                }
            }
            return false;
        }

        const MetricasEnlace& obtenerMetricas() const { return metricas; }
        void reiniciarMetricas() {
            metricas.reiniciar();
            ultimoSeqMensajeRx        = -1;
            descartandoHastaChunkCero = false;
            esperandoAckArq           = false;
            envioDiferido             = false;
            reintentosSync            = 0;
        }
    };

}