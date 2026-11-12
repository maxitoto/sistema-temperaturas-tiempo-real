#ifndef MIDDLEWARE_INTERFACE_H
#define MIDDLEWARE_INTERFACE_H

#include <Arduino.h>
#include "../remote/Controller/abstract/RPC_middleware_interface.hpp"
#include "../remote/Controller/unmarshalling/decode.hpp"
#include "interfaces/UART-S/UARTSRuntime.hpp"
#include "interfaces/WiFi/WiFiBase/WiFiRuntime.hpp"
#include "interfaces/WiFi/WiFiTcpTls/WiFiTcpTlsRuntime.hpp"
#include "interfaces/WiFi/WiFiUdpPlain/WiFiUdpPlainRuntime.hpp"
#include "middleware_sesion.hpp"

struct AcumuladorFramingTcp {
    uint8_t buffer[RPCBase::CAPACIDAD_BUFFER_COMPARTIDO];
    size_t bytesAcumulados;
    uint32_t longitudEsperada;
};

class MiddlewareInterface {
public:
    static constexpr uint32_t TIEMPO_EXPIRACION_KEEPALIVE_MS = 120000; // 2 minutos

private:
    UARTSRuntime& uarts;
    WiFiRuntime& wifiBase;
    WiFiTcpTlsRuntime& wifiTcp;
    WiFiUdpPlainRuntime& wifiUdp;
    MiddlewareSesion& capaSesionL2;

    uint8_t bufferEntradaSerie[RPCBase::CAPACIDAD_BUFFER_COMPARTIDO]{0};
    AcumuladorFramingTcp acumuladoresTcp[WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS]{};
    uint32_t ultimoContactoSlot0Ms{0};
    uint32_t ultimaVerificacionInactividadMs{0};

    inline static MiddlewareInterface* instanciaActiva = nullptr;

    void enrutarPaqueteCompleto(uint8_t slotOrigen, const uint8_t* datosSinPrefijoLen, size_t longitud) {
        uint16_t idTicket = 0;
        uint16_t idProc = 0;
        const uint8_t* ptrArgs = nullptr;
        size_t lenArgs = 0;

        if (!DecodificadorRPC::extraerCabeceraSinCopia(datosSinPrefijoLen, longitud, idTicket, idProc, ptrArgs, lenArgs)) {
            return;
        }

        if (slotOrigen == 0) {
            ultimoContactoSlot0Ms = millis();
        } else {
            wifiBase.refrescarActividadPeer(slotOrigen);
        }

        SobreMensajeRPC sobre{
            slotOrigen,
            idProc,
            idTicket,
            RPCBase::calcularHashPeticion(idProc, ptrArgs, lenArgs),
            RolUsuario::NINGUNO,
            ptrArgs,
            lenArgs
        };

        uint16_t puertoUdpSolicitado = 0;
        if (RPCMiddlewareInterface::despacharProcedimientoConexion(sobre, puertoUdpSolicitado)) {
            if (idProc == PROC_L1_HANDSHAKE_UARTS && slotOrigen == 0) {
                uarts.marcarClienteConectado();
            } else if (idProc == PROC_L1_SUSCRIBIR_UDP_WIFI && slotOrigen >= 1 && puertoUdpSolicitado > 0) {
                const IdentidadPeerWiFi& peer = wifiBase.obtenerIdentidadPeer(slotOrigen);
                wifiUdp.suscribirClienteAStream(slotOrigen, peer.ip, puertoUdpSolicitado);
            }

            if (RPCBase::obtenerLongitudSalidaCompartida() > 0) {
                difundirBufferCompartido(
                    RPCBase::obtenerBufferSalidaCompartido(),
                    RPCBase::obtenerLongitudSalidaCompartida(),
                    static_cast<MascaraClientes>(1u << slotOrigen),
                    CanalSalida::RESPUESTA_RPC_CONFIABLE
                );
            }

            if (idProc == PROC_L1_CERRAR_UARTS) {
                capaSesionL2.forzarCierreSesionPorDesconexion(0);
                uarts.cerrarConexionCliente(false);
            } else if (idProc == PROC_L1_CERRAR_WIFI) {
                capaSesionL2.forzarCierreSesionPorDesconexion(slotOrigen);
                cerrarConexionFisicaPorSlot(slotOrigen);
            }
            return;
        }

        capaSesionL2.procesarMensajeEntrante(sobre);
    }

public:
    MiddlewareInterface(
        UARTSRuntime& uartsRt,
        WiFiRuntime& wifiBaseRt,
        WiFiTcpTlsRuntime& wifiTcpRt,
        WiFiUdpPlainRuntime& wifiUdpRt,
        MiddlewareSesion& sesionL2,
        MiddlewareMessage& mensajeL3
    ) : uarts(uartsRt),
        wifiBase(wifiBaseRt),
        wifiTcp(wifiTcpRt),
        wifiUdp(wifiUdpRt),
        capaSesionL2(sesionL2) {
        instanciaActiva = this;
        sesionL2.configurarCallbacksL1(&MiddlewareInterface::callbackDifusionEstatico, &MiddlewareInterface::callbackCierreEstatico);
        mensajeL3.configurarCallbackDifusion(&MiddlewareInterface::callbackDifusionEstatico);
    }

    void procesarCicloEntrada() {
        const bool estabaConectadoSerie = uarts.hayClienteConectado();
        uarts.ejecutarCicloProtocolo();
        if (!estabaConectadoSerie && uarts.hayClienteConectado()) {
            ultimoContactoSlot0Ms = millis();
        }

        const size_t leidosSerie = uarts.extraerMensajeRecibido(bufferEntradaSerie, sizeof(bufferEntradaSerie));
        if (leidosSerie >= 8) {
            enrutarPaqueteCompleto(0, bufferEntradaSerie + 4, leidosSerie - 4);
        }

        NuevoClienteAceptado nuevo = wifiTcp.aceptarConexionEntrante();
        if (nuevo.slotAsignado >= 1) {
            wifiBase.registrarPeerEnSlotEspecifico(nuevo.slotAsignado, nuevo.ip, nuevo.puerto);
            acumuladoresTcp[nuevo.slotAsignado - 1].bytesAcumulados = 0;
            acumuladoresTcp[nuevo.slotAsignado - 1].longitudEsperada = 0;
        }

        for (uint8_t slot = 1; slot <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS; ++slot) {
            AcumuladorFramingTcp& acc = acumuladoresTcp[slot - 1];
            const size_t espacioLibre = sizeof(acc.buffer) - acc.bytesAcumulados;
            if (espacioLibre == 0) {
                acc.bytesAcumulados = 0;
                acc.longitudEsperada = 0;
                continue;
            }

            const size_t rec = wifiTcp.recibirBytesDeCliente(slot, acc.buffer + acc.bytesAcumulados, espacioLibre);
            if (rec == 0) {
                continue;
            }
            acc.bytesAcumulados += rec;

            while (acc.bytesAcumulados >= 4) {
                if (acc.longitudEsperada == 0) {
                    acc.longitudEsperada = (static_cast<uint32_t>(acc.buffer[0]) << 24) |
                                           (static_cast<uint32_t>(acc.buffer[1]) << 16) |
                                           (static_cast<uint32_t>(acc.buffer[2]) << 8)  |
                                           static_cast<uint32_t>(acc.buffer[3]);

                    if (acc.longitudEsperada < 4 || (acc.longitudEsperada + 4) > sizeof(acc.buffer)) {
                        acc.bytesAcumulados = 0;
                        acc.longitudEsperada = 0;
                        break;
                    }
                }

                const size_t tamanoTotalFrame = 4 + acc.longitudEsperada;
                if (acc.bytesAcumulados < tamanoTotalFrame) {
                    break;
                }

                enrutarPaqueteCompleto(slot, acc.buffer + 4, acc.longitudEsperada);

                const size_t sobrantes = acc.bytesAcumulados - tamanoTotalFrame;
                if (sobrantes > 0) {
                    memmove(acc.buffer, acc.buffer + tamanoTotalFrame, sobrantes);
                }
                acc.bytesAcumulados = sobrantes;
                acc.longitudEsperada = 0;
            }
        }

        const uint32_t ahoraMs = millis();
        if (ahoraMs - ultimaVerificacionInactividadMs >= 500) {
            ultimaVerificacionInactividadMs = ahoraMs;
            verificarKeepalivesYExpulsarInactivos();
            capaSesionL2.verificarExpiracionSesionesPorOcio();
        }
    }

    void verificarKeepalivesYExpulsarInactivos() {
        const uint32_t ahoraMs = millis();

        if (uarts.hayClienteConectado()) {
            if (static_cast<int32_t>(ahoraMs - ultimoContactoSlot0Ms) > static_cast<int32_t>(TIEMPO_EXPIRACION_KEEPALIVE_MS)) {
                capaSesionL2.forzarCierreSesionPorDesconexion(0);
                cerrarConexionFisicaPorSlot(0);
            }
        }

        const uint16_t mascaraSocketsCaidos = wifiTcp.limpiarSocketsCaidos();
        wifiBase.actualizarEstadoYExpirarPeers();

        for (uint8_t slot = 1; slot <= WiFiRuntime::MAXIMO_PEERS_WIFI; ++slot) {
            const bool socketMurio = (mascaraSocketsCaidos & (1u << slot)) != 0;
            const bool keepaliveVencio = !wifiBase.estaSlotActivo(slot);

            if (socketMurio || keepaliveVencio) {
                capaSesionL2.forzarCierreSesionPorDesconexion(slot);
                cerrarConexionFisicaPorSlot(slot);
            }
        }
    }

    void difundirBufferCompartido(
        const uint8_t* bufferCompartido,
        size_t longitud,
        MascaraClientes mascaraDestinatarios,
        CanalSalida canal
    ) {
        if (bufferCompartido == nullptr || longitud == 0 || mascaraDestinatarios == 0) {
            return;
        }

        if ((mascaraDestinatarios & 0x0001u) && uarts.hayClienteConectado()) {
            uarts.enviarMensajeConfiable(bufferCompartido, longitud);
        }

        const uint16_t mascaraWiFi = static_cast<uint16_t>(mascaraDestinatarios & ~0x0001u);
        if (mascaraWiFi == 0) {
            return;
        }

        if (canal == CanalSalida::STREAMING_TELEMETRIA) {
            wifiUdp.emitirDatagramaASuscriptores(bufferCompartido, longitud, mascaraWiFi);
        } else {
            for (uint8_t slot = 1; slot <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS; ++slot) {
                if (mascaraWiFi & (1u << slot)) {
                    wifiTcp.enviarBytesACliente(slot, bufferCompartido, longitud);
                }
            }
        }
    }

    void cerrarConexionFisicaPorSlot(uint8_t slotCliente) {
        if (slotCliente == 0) {
            uarts.cerrarConexionCliente();
            return;
        }

        if (slotCliente >= 1 && slotCliente <= WiFiRuntime::MAXIMO_PEERS_WIFI) {
            wifiUdp.desuscribirClienteDeStream(slotCliente);
            if (slotCliente <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS) {
                wifiTcp.desconectarCliente(slotCliente);
                acumuladoresTcp[slotCliente - 1].bytesAcumulados = 0;
                acumuladoresTcp[slotCliente - 1].longitudEsperada = 0;
            }
            wifiBase.eliminarPeerPorSlot(slotCliente);
        }
    }

private:
    static void callbackDifusionEstatico(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal) {
        if (instanciaActiva != nullptr) {
            instanciaActiva->difundirBufferCompartido(buffer, longitud, destinatarios, canal);
        }
    }

    static void callbackCierreEstatico(uint8_t slotCliente) {
        if (instanciaActiva != nullptr) {
            instanciaActiva->cerrarConexionFisicaPorSlot(slotCliente);
        }
    }
};

#endif