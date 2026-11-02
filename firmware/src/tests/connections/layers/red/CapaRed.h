#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../common/BufferRed.hpp"
#include "../datalink/CapaEnlace.hpp"

namespace OSI {

    struct MetricasRed {
        uint32_t paquetesPassthroughTx         = 0;
        uint32_t paquetesPassthroughRx         = 0;
        uint32_t paquetesRedPropiaTx           = 0;
        uint32_t paquetesRedPropiaRx           = 0;
        uint32_t paquetesBroadcastTx           = 0;
        uint32_t paquetesReenviados            = 0;
        uint32_t erroresTtlAgotado             = 0;
        uint32_t erroresRutaInexistente        = 0;
        uint32_t erroresCabeceraIncompleta     = 0;
        uint32_t paquetesDescartadosCongestion = 0;

        void reiniciar() { *this = MetricasRed{}; }
    };

    struct MetadatosRedRx {
        int     idMedioEntrada = -1;
        uint8_t origenLogico   = 0;
        uint8_t destinoLogico  = 0;
        uint8_t ttlRecibido    = 0;
        bool    fuePassthrough = false;
    };

    class CapaRed {
    public:
        static constexpr uint8_t  NODO_SERVIDOR_LOCAL         = 0x00;
        static constexpr uint8_t  NODO_BROADCAST              = 0x0F;
        static constexpr uint8_t  DEFAULT_TTL                 = 8;
        static constexpr size_t   MAX_DIRECCIONES_LOGICAS     = 16;
        static constexpr uint32_t VENTANA_CONGESTION_MS       = 1000;
        static constexpr uint16_t MAX_PAQUETES_POR_VENTANA_DEF= 64;

    private:
        struct EstadoCongestionNodo {
            unsigned long inicioVentanaMs   = 0;
            uint16_t      paquetesEnVentana = 0;
            uint16_t      limitePorVentana  = MAX_PAQUETES_POR_VENTANA_DEF;
        };

        CapaFisica*          fisicaRef                           = nullptr;
        CapaEnlace           enlaces[LimitesPila::MAX_NODOS_RED];
        EstadoCongestionNodo congestion[LimitesPila::MAX_NODOS_RED];
        int8_t               tablaRutas[MAX_DIRECCIONES_LOGICAS];

        uint8_t              direccionLocal                      = NODO_SERVIDOR_LOCAL;
        int                  cantidadNodos                       = 0;
        int                  ultimoNodoConsultado                = 0;
        bool                 reenvioEntreRedesActivo             = true;

        MetadatosRedRx       ultimoMetaRx;
        MetricasRed          metricas;

        bool verificarCuotaCongestion(int idMedio) {
            if (idMedio < 0 || idMedio >= cantidadNodos) return false;
            EstadoCongestionNodo& c = congestion[idMedio];
            unsigned long ahora = millis();

            if (ahora - c.inicioVentanaMs >= VENTANA_CONGESTION_MS) {
                c.inicioVentanaMs   = ahora;
                c.paquetesEnVentana = 0;
            }

            if (c.paquetesEnVentana >= c.limitePorVentana) {
                metricas.paquetesDescartadosCongestion++;
                return false;
            }

            c.paquetesEnVentana++;
            return true;
        }

        bool reenviarPaqueteEnTransito(uint8_t origen, uint8_t destino, uint8_t ttlActual, BufferPaquete& pktDatos) {
            if (!reenvioEntreRedesActivo) return false;
            if (ttlActual <= 1) {
                metricas.erroresTtlAgotado++;
                return false;
            }

            int medioSalida = resolverRuta(destino);
            if (medioSalida < 0 || medioSalida >= cantidadNodos || !estaNodoActivo(medioSalida)) {
                metricas.erroresRutaInexistente++;
                return false;
            }

            if (!verificarCuotaCongestion(medioSalida)) return false;

            int nodoPrevio    = pktDatos.idNodo;
            size_t iniPrevio  = pktDatos.inicio;
            size_t lenPrevia  = pktDatos.longitud;
            pktDatos.idNodo   = medioSalida;

            uint8_t respaldo[LimitesPila::TAMANO_CABECERA_RED] = {0};
            if (!esMedioPassthrough(medioSalida)) {
                if (pktDatos.inicio < LimitesPila::TAMANO_CABECERA_RED) {
                    pktDatos.idNodo = nodoPrevio;
                    return false;
                }
                memcpy(respaldo, &pktDatos.raw[pktDatos.inicio - LimitesPila::TAMANO_CABECERA_RED], LimitesPila::TAMANO_CABECERA_RED);
                uint8_t* hdr = pktDatos.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
                hdr[0] = static_cast<uint8_t>(((origen & 0x0F) << 4) | (destino & 0x0F));
                hdr[1] = ttlActual - 1;
            }

            bool ok = enlaces[medioSalida].bajarTrama(pktDatos);

            if (!esMedioPassthrough(medioSalida)) {
                pktDatos.inicio   = iniPrevio;
                pktDatos.longitud = lenPrevia;
                memcpy(&pktDatos.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_RED], respaldo, LimitesPila::TAMANO_CABECERA_RED);
            }
            pktDatos.idNodo = nodoPrevio;

            if (ok) {
                metricas.paquetesReenviados++;
            }
            return ok;
        }

    public:
        CapaRed() {
            for (size_t i = 0; i < MAX_DIRECCIONES_LOGICAS; i++) {
                tablaRutas[i] = -1;
            }
        }

        void vincularMedios(CapaFisica* capaFisica, uint8_t dirLocal = NODO_SERVIDOR_LOCAL) {
            if (!capaFisica) return;
            fisicaRef            = capaFisica;
            direccionLocal       = dirLocal & 0x0F;
            ultimoNodoConsultado = 0;
            metricas.reiniciar();

            int disponibles = capaFisica->obtenerTotalMedios();
            cantidadNodos = (disponibles > LimitesPila::MAX_NODOS_RED)
                              ? LimitesPila::MAX_NODOS_RED
                              : disponibles;

            for (size_t i = 0; i < MAX_DIRECCIONES_LOGICAS; i++) {
                tablaRutas[i] = -1;
            }

            for (int i = 0; i < cantidadNodos; i++) {
                enlaces[i].configurarEnlace(capaFisica, i, false);
                congestion[i] = { millis(), 0, MAX_PAQUETES_POR_VENTANA_DEF };
                if (i < static_cast<int>(MAX_DIRECCIONES_LOGICAS)) {
                    tablaRutas[i] = static_cast<int8_t>(i);
                }
            }
        }

        void establecerDireccionLocal(uint8_t nuevaDirLocal) {
            direccionLocal = nuevaDirLocal & 0x0F;
        }

        uint8_t obtenerDireccionLocal() const { return direccionLocal; }

        bool configurarRuta(uint8_t destinoLogico, int idMedioSalida) {
            if (destinoLogico >= MAX_DIRECCIONES_LOGICAS || idMedioSalida < 0 || idMedioSalida >= cantidadNodos) {
                return false;
            }
            tablaRutas[destinoLogico] = static_cast<int8_t>(idMedioSalida);
            return true;
        }

        int resolverRuta(uint8_t destinoLogico) const {
            if (destinoLogico >= MAX_DIRECCIONES_LOGICAS) return -1;
            int idMedio = tablaRutas[destinoLogico];
            if (idMedio >= 0 && idMedio < cantidadNodos) return idMedio;
            if (destinoLogico < cantidadNodos) return destinoLogico;
            return -1;
        }

        void configurarLimiteCongestion(int idMedio, uint16_t maxPaquetesPorSegundo) {
            if (idMedio >= 0 && idMedio < cantidadNodos) {
                congestion[idMedio].limitePorVentana  = maxPaquetesPorSegundo;
                congestion[idMedio].paquetesEnVentana = 0;
                congestion[idMedio].inicioVentanaMs   = millis();
            }
        }

        void habilitarReenvioEntreRedes(bool activo) { reenvioEntreRedesActivo = activo; }

        bool esMedioPassthrough(int idMedio) const {
            return fisicaRef ? !fisicaRef->esMedioCrudo(idMedio) : false;
        }

        bool estaNodoActivo(int idMedio) const {
            return fisicaRef ? fisicaRef->estaActivo(idMedio) : false;
        }

        void desconectarNodo(int idNodo) {
            if (fisicaRef && idNodo >= 0 && idNodo < cantidadNodos) {
                fisicaRef->desconectarMedio(idNodo);
            }
        }

        int obtenerCantidadNodos() const { return cantidadNodos; }
        CapaEnlace* obtenerEnlace(int idMedio) {
            return (idMedio >= 0 && idMedio < cantidadNodos) ? &enlaces[idMedio] : nullptr;
        }

        bool subirPaquete(BufferPaquete& pktSalida) {
            if (cantidadNodos <= 0) return false;

            for (int i = 0; i < cantidadNodos; i++) {
                int idx = (ultimoNodoConsultado + i) % cantidadNodos;
                if (!enlaces[idx].subirTrama(pktSalida)) continue;

                ultimoNodoConsultado = (idx + 1) % cantidadNodos;
                pktSalida.idNodo     = idx;

                if (esMedioPassthrough(idx)) {
                    ultimoMetaRx = { idx, static_cast<uint8_t>(idx & 0x0F), direccionLocal, DEFAULT_TTL, true };
                    metricas.paquetesPassthroughRx++;
                    return true;
                }

                const uint8_t* hdr = pktSalida.consumirCabecera(LimitesPila::TAMANO_CABECERA_RED);
                if (!hdr) {
                    metricas.erroresCabeceraIncompleta++;
                    continue;
                }

                uint8_t origen  = (hdr[0] >> 4) & 0x0F;
                uint8_t destino = hdr[0] & 0x0F;
                uint8_t ttl     = hdr[1];

                if (ttl == 0) {
                    metricas.erroresTtlAgotado++;
                    continue;
                }

                ultimoMetaRx = { idx, origen, destino, ttl, false };

                if (destino == direccionLocal || destino == NODO_BROADCAST) {
                    metricas.paquetesRedPropiaRx++;
                    return true;
                }

                reenviarPaqueteEnTransito(origen, destino, ttl, pktSalida);
            }
            return false;
        }

        bool bajarPaqueteUnicast(int destinoLogico, BufferPaquete& pkt,
                                 uint8_t ttlInicial = DEFAULT_TTL,
                                 bool esControlPrioritario = false) {
            if (ttlInicial == 0) {
                metricas.erroresTtlAgotado++;
                return false;
            }

            if (destinoLogico < 0) {
                metricas.erroresRutaInexistente++;
                return false;
            }

            int idMedio = resolverRuta(static_cast<uint8_t>(destinoLogico));
            if (idMedio < 0 || idMedio >= cantidadNodos || !estaNodoActivo(idMedio)) {
                metricas.erroresRutaInexistente++;
                return false;
            }

            if (!esControlPrioritario && !verificarCuotaCongestion(idMedio)) return false;

            int nodoPrevio = pkt.idNodo;
            pkt.idNodo     = idMedio;

            if (esMedioPassthrough(idMedio)) {
                bool ok = enlaces[idMedio].bajarTrama(pkt);
                pkt.idNodo = nodoPrevio;
                if (ok) metricas.paquetesPassthroughTx++;
                return ok;
            }

            if (pkt.inicio < LimitesPila::TAMANO_CABECERA_RED) {
                pkt.idNodo = nodoPrevio;
                metricas.erroresCabeceraIncompleta++;
                return false;
            }

            size_t iniPrevio = pkt.inicio;
            size_t lenPrevia = pkt.longitud;
            uint8_t respaldo[LimitesPila::TAMANO_CABECERA_RED];
            memcpy(respaldo, &pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_RED], LimitesPila::TAMANO_CABECERA_RED);

            uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
            hdr[0] = static_cast<uint8_t>(((direccionLocal & 0x0F) << 4) | (static_cast<uint8_t>(destinoLogico) & 0x0F));
            hdr[1] = ttlInicial;

            bool ok = enlaces[idMedio].bajarTrama(pkt);

            pkt.inicio   = iniPrevio;
            pkt.longitud = lenPrevia;
            pkt.idNodo   = nodoPrevio;
            memcpy(&pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_RED], respaldo, LimitesPila::TAMANO_CABECERA_RED);

            if (ok) {
                metricas.paquetesRedPropiaTx++;
            }
            return ok;
        }

        size_t bajarPaqueteBroadcast(BufferPaquete& pkt, uint8_t ttlInicial = DEFAULT_TTL) {
            if (ttlInicial == 0) {
                metricas.erroresTtlAgotado++;
                return 0;
            }

            int nodoPrevio   = pkt.idNodo;
            size_t iniPrevio = pkt.inicio;
            size_t lenPrevia = pkt.longitud;

            size_t enviados = 0;
            for (int i = 0; i < cantidadNodos; i++) {
                if (!estaNodoActivo(i)) continue;
                if (!verificarCuotaCongestion(i)) continue;

                pkt.idNodo = i;

                if (esMedioPassthrough(i)) {
                    if (enlaces[i].bajarTrama(pkt)) {
                        metricas.paquetesPassthroughTx++;
                        metricas.paquetesBroadcastTx++;
                        enviados++;
                    }
                } else {
                    if (iniPrevio < LimitesPila::TAMANO_CABECERA_RED) continue;
                    uint8_t respaldo[LimitesPila::TAMANO_CABECERA_RED];
                    memcpy(respaldo, &pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_RED], LimitesPila::TAMANO_CABECERA_RED);

                    uint8_t* hdr = pkt.empujarCabecera(LimitesPila::TAMANO_CABECERA_RED);
                    hdr[0] = static_cast<uint8_t>(((direccionLocal & 0x0F) << 4) | NODO_BROADCAST);
                    hdr[1] = ttlInicial;

                    if (enlaces[i].bajarTrama(pkt)) {
                        metricas.paquetesRedPropiaTx++;
                        metricas.paquetesBroadcastTx++;
                        enviados++;
                    }

                    pkt.inicio   = iniPrevio;
                    pkt.longitud = lenPrevia;
                    memcpy(&pkt.raw[iniPrevio - LimitesPila::TAMANO_CABECERA_RED], respaldo, LimitesPila::TAMANO_CABECERA_RED);
                }
            }
            pkt.idNodo = nodoPrevio;
            return enviados;
        }

        const MetadatosRedRx& obtenerUltimoMetaRx() const { return ultimoMetaRx; }
        const MetricasRed&    obtenerMetricas()     const { return metricas; }
        void                  reiniciarMetricas()         { metricas.reiniciar(); }
    };

}