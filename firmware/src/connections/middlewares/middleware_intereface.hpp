#ifndef MIDDLEWARE_INTERFACE_H
#define MIDDLEWARE_INTERFACE_H

#include <Arduino.h>
#include "../remote/Controller/abstract/RPC_middleware_interface.hpp"
#include "../remote/Controller/unmarshalling/decode.hpp"
#include "interfaces/UART-S/UARTSRuntime.hpp"
#include "interfaces/WiFi/WiFiBase/WiFiRuntime.hpp"
#include "interfaces/WiFi/WiFiTcpTls/WiFiTcpTlsRuntime.hpp"
#include "interfaces/WiFi/WiFiUdpPlain/WiFiUdpPlainRuntime.hpp"
#include "interfaces/WiFi/utils/security/encrypt.hpp"
#include "interfaces/WiFi/utils/security/RateLimiter.hpp"
#include "ColaTrabajoRPC.hpp"
#include "middleware_sesion.hpp"

struct AcumuladorFramingTcp {
    static constexpr size_t CAPACIDAD_RX_TCP = 512; // Cubre ConsultaSqlTerminalDTO (384B) + cabecera + AEAD
    uint8_t  buffer[CAPACIDAD_RX_TCP];
    size_t   bytesAcumulados;
    uint32_t longitudEsperada;
};

class MiddlewareInterface {
public:
    static constexpr uint32_t TIEMPO_EXPIRACION_KEEPALIVE_MS = WiFiRuntime::TIEMPO_EXPIRACION_PEER_MS; // 2 min autenticado
    static constexpr size_t   MAX_PLAINTEXT_PRE_AUTH         = 128; // Suficiente para KEEPALIVE y LOGIN (68B)
    static constexpr size_t   MAX_PLAINTEXT_AUTH_TCP         = 480; // Cubre cualquier DTO de entrada válido
    static constexpr size_t   CAPACIDAD_SALIDA_CIFRADA       =
        RPCBase::CAPACIDAD_BUFFER_COMPARTIDO + WiFiSecurity::CifradorAesCcm::OVERHEAD_INTERNO;

private:
    UARTSRuntime&        uarts;
    WiFiRuntime&         wifiBase;
    WiFiTcpTlsRuntime&   wifiTcp;
    WiFiUdpPlainRuntime& wifiUdp;
    ColaTrabajoRPC&      colaRpc;

    uint8_t              bufferEntradaSerie[RPCBase::CAPACIDAD_BUFFER_COMPARTIDO]{0};
    AcumuladorFramingTcp acumuladoresTcp[WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS]{};

    // Estado de seguridad y control de tráfico (100% estático, cero heap)
    WiFiSecurity::CifradorAesCcm      cifradorWiFi{};
    WiFiSecurity::EstadoCanalCcmSlot  estadoCryptoTcp[WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS]{};
    WiFiSecurity::EstadoCanalCcmSlot  estadoCryptoUdp{};
    WiFiSecurity::EstadoRateLimitSlot rateLimitTcp[WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS]{};
    uint8_t                           bufferDescifradoRx[MAX_PLAINTEXT_AUTH_TCP]{0};
    uint8_t                           bufferSalidaCifrada[CAPACIDAD_SALIDA_CIFRADA]{0};
    uint8_t                           bufferErrorRapido[128]{0};

    uint8_t  turnoSlotTcp{0};
    uint32_t ultimoContactoSlot0Ms{0};
    uint32_t ultimaVerificacionInactividadMs{0};

    inline static MiddlewareInterface* instanciaActiva = nullptr;

    // Avisa a Core 1 que limpie tickets/streams e invalida auth al instante sin tocar L2/L3 directamente
    void solicitarLimpiezaSesionDesdeCore0(uint8_t slot) {
        RPAuth::invalidarAutenticacionInmediata(slot);
        colaRpc.solicitarLimpiezaSesion(slot);
    }

    void registrarIncidenteRedThrottled(uint8_t slot, const char* codigoEvento, CriticidadEvento crit) {
        if (slot < 1 || slot > WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS) return;
        const uint32_t ahoraMs = millis();
        if (!rateLimitTcp[slot - 1].puedeAuditarIncidente(ahoraMs)) {
            return;
        }
        const IdentidadPeerWiFi& peer = wifiBase.obtenerIdentidadPeer(slot);
        int32_t idUsr = 0;
        char nombreUsr[20]{};
        RPAuth::obtenerSnapshotUsuarioSlot(slot, idUsr, nombreUsr, sizeof(nombreUsr));

        RTOS::encolarAuditoria(
            RTOS::TipoRegistroDB::AUDITORIA_RED,
            idUsr,
            nombreUsr[0] != '\0' ? nombreUsr : "ANONIMO",
            peer.ip[0] != '\0' ? peer.ip : "WIFI",
            codigoEvento,
            static_cast<uint8_t>(ResultadoAuditoria::INTRUSION_TERMINAL),
            static_cast<uint8_t>(crit)
        );
    }

    void enviarErrorRapido(uint8_t slot, uint16_t idTicket, uint16_t idProc, const char* mensaje) {
        const size_t lenPlain = CodificadorRPC::serializarExcepcionCruda(
            idTicket, idProc, mensaje, bufferErrorRapido, sizeof(bufferErrorRapido)
        );
        if (lenPlain > 4) {
            difundirBufferCompartido(
                bufferErrorRapido,
                lenPlain,
                static_cast<MascaraClientes>(1u << slot),
                CanalSalida::RESPUESTA_RPC_CONFIABLE
            );
        }
    }

    bool enviarFrameSeguroATcpSlot(uint8_t slot, const uint8_t* frameConLen, size_t longitudTotal) {
        if (frameConLen == nullptr || longitudTotal <= 4) return false;
#if WIFI_CIFRADO_CCM_ACTIVO
        const size_t lenCifrada = cifradorWiFi.empaquetarYCifrarFrame(
            WiFiSecurity::DIR_S2C_TCP,
            estadoCryptoTcp[slot - 1],
            frameConLen + 4,
            longitudTotal - 4,
            bufferSalidaCifrada,
            sizeof(bufferSalidaCifrada)
        );
        if (lenCifrada == 0) {
            solicitarLimpiezaSesionDesdeCore0(slot);
            cerrarConexionFisicaPorSlot(slot);
            return false;
        }
        return wifiTcp.enviarBytesACliente(slot, bufferSalidaCifrada, lenCifrada);
#else
        return wifiTcp.enviarBytesACliente(slot, frameConLen, longitudTotal);
#endif
    }

    void enrutarPaqueteCompleto(uint8_t slotOrigen, const uint8_t* datosSinPrefijoLen, size_t longitud) {
        uint16_t idTicket = 0;
        uint16_t idProc = 0;
        const uint8_t* ptrArgs = nullptr;
        size_t lenArgs = 0;

        if (!DecodificadorRPC::extraerCabeceraSinCopia(datosSinPrefijoLen, longitud, idTicket, idProc, ptrArgs, lenArgs)) {
            if (slotOrigen >= 1) {
                registrarIncidenteRedThrottled(slotOrigen, "WIFI_CABECERA_RPC_INVALIDA", CriticidadEvento::MEDIA);
                solicitarLimpiezaSesionDesdeCore0(slotOrigen);
                cerrarConexionFisicaPorSlot(slotOrigen);
            }
            return;
        }

        // Control de tasa por slot WiFi (se exime cierre explícito)
        if (slotOrigen >= 1 && slotOrigen <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS && idProc != PROC_L1_CERRAR_WIFI) {
            const bool autenticado = RPAuth::estaClienteAutenticado(slotOrigen);
            const auto decision = rateLimitTcp[slotOrigen - 1].evaluarPeticion(autenticado, millis());

            if (decision == WiFiSecurity::DecisionTrafico::EXPULSAR_CONEXION) {
                registrarIncidenteRedThrottled(slotOrigen, "WIFI_FLOOD_EXPULSION", CriticidadEvento::ALTA);
                enviarErrorRapido(slotOrigen, idTicket, idProc, "RateLimitError: exceso de peticiones, conexion cerrada");
                solicitarLimpiezaSesionDesdeCore0(slotOrigen);
                cerrarConexionFisicaPorSlot(slotOrigen);
                return;
            }
            if (decision == WiFiSecurity::DecisionTrafico::LIMITADO_RECHAZO_SUAVE) {
                registrarIncidenteRedThrottled(slotOrigen, "WIFI_RATE_LIMIT_SOFT", CriticidadEvento::MEDIA);
                enviarErrorRapido(slotOrigen, idTicket, idProc, "BusyError: limite de tasa temporal alcanzado, reintente");
                return;
            }
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

        // 1. Procedimientos L1 se responden en el acto en Core 0 con bufferSalidaL1 propio
        uint16_t puertoUdpSolicitado = 0;
        if (RPCMiddlewareInterface::despacharProcedimientoConexion(sobre, puertoUdpSolicitado)) {
            if (idProc == PROC_L1_HANDSHAKE_UARTS && slotOrigen == 0) {
                uarts.marcarClienteConectado();
            } else if (idProc == PROC_L1_SUSCRIBIR_UDP_WIFI && slotOrigen >= 1 && puertoUdpSolicitado > 0) {
                if (RPAuth::estaClienteAutenticado(slotOrigen)) {
                    const IdentidadPeerWiFi& peer = wifiBase.obtenerIdentidadPeer(slotOrigen);
                    wifiUdp.suscribirClienteAStream(slotOrigen, peer.ip, puertoUdpSolicitado);
                }
            }

            if (RPCMiddlewareInterface::obtenerLongitudSalidaL1() > 0) {
                difundirBufferCompartido(
                    RPCMiddlewareInterface::obtenerBufferSalidaL1(),
                    RPCMiddlewareInterface::obtenerLongitudSalidaL1(),
                    static_cast<MascaraClientes>(1u << slotOrigen),
                    CanalSalida::RESPUESTA_RPC_CONFIABLE
                );
            }

            if (idProc == PROC_L1_CERRAR_UARTS) {
                solicitarLimpiezaSesionDesdeCore0(0);
                uarts.cerrarConexionCliente(false);
            } else if (idProc == PROC_L1_CERRAR_WIFI) {
                solicitarLimpiezaSesionDesdeCore0(slotOrigen);
                cerrarConexionFisicaPorSlot(slotOrigen);
            }
            return;
        }

        // 2. Procedimientos L2/L3 van a la cola acotada de Core 1 (tail-drop si está llena)
        if (!colaRpc.intentarEncolarPeticion(sobre)) {
            enviarErrorRapido(slotOrigen, idTicket, idProc, "BusyError: cola del servidor llena, reintente");
        }
    }

    void procesarUnFrameDeSlotTcp(uint8_t slot) {
        AcumuladorFramingTcp& acc = acumuladoresTcp[slot - 1];
        const size_t espacioLibre = sizeof(acc.buffer) - acc.bytesAcumulados;
        if (espacioLibre == 0) {
            registrarIncidenteRedThrottled(slot, "WIFI_OVERFLOW_BUFFER_RX", CriticidadEvento::ALTA);
            solicitarLimpiezaSesionDesdeCore0(slot);
            cerrarConexionFisicaPorSlot(slot);
            return;
        }

        const size_t rec = wifiTcp.recibirBytesDeCliente(slot, acc.buffer + acc.bytesAcumulados, espacioLibre);
        acc.bytesAcumulados += rec;

        if (acc.bytesAcumulados < 4) {
            return;
        }

        if (acc.longitudEsperada == 0) {
            acc.longitudEsperada = (static_cast<uint32_t>(acc.buffer[0]) << 24) |
                                   (static_cast<uint32_t>(acc.buffer[1]) << 16) |
                                   (static_cast<uint32_t>(acc.buffer[2]) << 8)  |
                                   static_cast<uint32_t>(acc.buffer[3]);

            const bool autenticado = RPAuth::estaClienteAutenticado(slot);
#if WIFI_CIFRADO_CCM_ACTIVO
            constexpr size_t minFrame = 4 + WiFiSecurity::CifradorAesCcm::OVERHEAD_INTERNO; // 22 B
            const size_t maxFrame = (autenticado ? MAX_PLAINTEXT_AUTH_TCP : MAX_PLAINTEXT_PRE_AUTH)
                                  + WiFiSecurity::CifradorAesCcm::OVERHEAD_INTERNO;
#else
            constexpr size_t minFrame = 4;
            const size_t maxFrame = autenticado ? MAX_PLAINTEXT_AUTH_TCP : MAX_PLAINTEXT_PRE_AUTH;
#endif
            if (acc.longitudEsperada < minFrame ||
                acc.longitudEsperada > maxFrame ||
                (acc.longitudEsperada + 4) > sizeof(acc.buffer)) {
                registrarIncidenteRedThrottled(slot, "WIFI_FRAMING_INVALIDO", CriticidadEvento::ALTA);
                solicitarLimpiezaSesionDesdeCore0(slot);
                cerrarConexionFisicaPorSlot(slot);
                return;
            }
        }

        const size_t tamanoTotalFrame = 4 + acc.longitudEsperada;
        if (acc.bytesAcumulados < tamanoTotalFrame) {
            return;
        }

        size_t lenLista = 0;

#if WIFI_CIFRADO_CCM_ACTIVO
        const auto resCrypto = cifradorWiFi.descifrarYValidar(
            WiFiSecurity::DIR_C2S_TCP,
            estadoCryptoTcp[slot - 1],
            acc.buffer + 4,
            acc.longitudEsperada,
            bufferDescifradoRx,
            sizeof(bufferDescifradoRx),
            lenLista
        );

        if (resCrypto != WiFiSecurity::ResultadoDescifradoCcm::OK) {
            const char* motivo = (resCrypto == WiFiSecurity::ResultadoDescifradoCcm::ERROR_REPLAY)
                                 ? "WIFI_ATAQUE_REPLAY"
                                 : "WIFI_AEAD_TAG_INVALIDO";
            registrarIncidenteRedThrottled(slot, motivo, CriticidadEvento::ALTA);
            solicitarLimpiezaSesionDesdeCore0(slot);
            cerrarConexionFisicaPorSlot(slot);
            return;
        }
#else
        if (acc.longitudEsperada > sizeof(bufferDescifradoRx)) {
            solicitarLimpiezaSesionDesdeCore0(slot);
            cerrarConexionFisicaPorSlot(slot);
            return;
        }
        lenLista = acc.longitudEsperada;
        memcpy(bufferDescifradoRx, acc.buffer + 4, lenLista);
#endif

        // Compactamos el acumulador después de copiar/descifrar en bufferDescifradoRx
        const size_t sobrantes = acc.bytesAcumulados - tamanoTotalFrame;
        if (sobrantes > 0) {
            memmove(acc.buffer, acc.buffer + tamanoTotalFrame, sobrantes);
        }
        acc.bytesAcumulados  = sobrantes;
        acc.longitudEsperada = 0;

        enrutarPaqueteCompleto(slot, bufferDescifradoRx, lenLista);
    }

    void drenarSalidaYCierresDesdeCore1() {
        // 1. Emitir respuestas RPC y streams UDP/UART producidos por Core 1
        uint8_t idxRes = 0;
        const RespuestaEncoladaRPC* resp = nullptr;
        while (colaRpc.tomarRespuestaLista(idxRes, resp)) {
            difundirBufferCompartido(resp->buffer, resp->longitud, resp->destinatarios, resp->canal);
            colaRpc.liberarRespuesta(idxRes);
        }

        // 2. Ejecutar cierres físicos solicitados por Core 1 (ej. sesión expirada por ocio)
        const uint16_t cierres = colaRpc.extraerCierresFisicosPendientes();
        if (cierres != 0) {
            for (uint8_t slot = 0; slot <= WiFiRuntime::MAXIMO_PEERS_WIFI; ++slot) {
                if (cierres & (1u << slot)) {
                    cerrarConexionFisicaPorSlot(slot);
                }
            }
        }
    }

public:
    MiddlewareInterface(
        UARTSRuntime& uartsRt,
        WiFiRuntime& wifiBaseRt,
        WiFiTcpTlsRuntime& wifiTcpRt,
        WiFiUdpPlainRuntime& wifiUdpRt,
        ColaTrabajoRPC& colaTrabajoRt,
        MiddlewareSesion& sesionL2,
        MiddlewareMessage& mensajeL3,
        const uint8_t* clavePsk16 = WiFiSecurity::PSK_DEFECTO_128
    ) : uarts(uartsRt),
        wifiBase(wifiBaseRt),
        wifiTcp(wifiTcpRt),
        wifiUdp(wifiUdpRt),
        colaRpc(colaTrabajoRt) {
        instanciaActiva = this;
        cifradorWiFi.inicializarConPsk(clavePsk16);
        estadoCryptoUdp.reiniciar();
        // L2 y L3 corren en Core 1: sus callbacks publican hacia Core 0 a través de ColaTrabajoRPC
        sesionL2.configurarCallbacksL1(&MiddlewareInterface::callbackDifusionDesdeWorker, &MiddlewareInterface::callbackCierreDesdeWorker);
        mensajeL3.configurarCallbackDifusion(&MiddlewareInterface::callbackDifusionDesdeWorker);
    }

    bool configurarClavePsk(const uint8_t clavePsk16[WiFiSecurity::CifradorAesCcm::TAMANO_CLAVE]) {
        return cifradorWiFi.inicializarConPsk(clavePsk16);
    }

    void procesarCicloEntrada() {
        // 0. Despachar primero cualquier respuesta lista de Core 1 y cierres pendientes
        drenarSalidaYCierresDesdeCore1();

        // 1. Canal UART-S (slot 0)
        const bool estabaConectadoSerie = uarts.hayClienteConectado();
        uarts.ejecutarCicloProtocolo();
        if (!estabaConectadoSerie && uarts.hayClienteConectado()) {
            ultimoContactoSlot0Ms = millis();
            colaRpc.avanzarGeneracionSlot(0);
        }

        const size_t leidosSerie = uarts.extraerMensajeRecibido(bufferEntradaSerie, sizeof(bufferEntradaSerie));
        if (leidosSerie >= 8) {
            enrutarPaqueteCompleto(0, bufferEntradaSerie + 4, leidosSerie - 4);
        }

        // 2. Aceptación protegida de nuevas conexiones WiFi TCP
        NuevoClienteAceptado nuevo = wifiTcp.aceptarConexionEntrante();
        if (nuevo.slotAsignado >= 1 && nuevo.slotAsignado <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS) {
            const uint8_t idx = static_cast<uint8_t>(nuevo.slotAsignado - 1);
            solicitarLimpiezaSesionDesdeCore0(nuevo.slotAsignado);
            wifiUdp.desuscribirClienteDeStream(nuevo.slotAsignado);

            wifiBase.registrarPeerEnSlotEspecifico(nuevo.slotAsignado, nuevo.ip, nuevo.puerto);
            acumuladoresTcp[idx].bytesAcumulados  = 0;
            acumuladoresTcp[idx].longitudEsperada = 0;
            estadoCryptoTcp[idx].reiniciar();
            rateLimitTcp[idx].iniciarConexion(millis());
        }

        // 3. Presupuesto Round-Robin: máximo 1 trama por slot por ciclo
        for (uint8_t k = 0; k < WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS; ++k) {
            const uint8_t idx  = static_cast<uint8_t>((turnoSlotTcp + k) % WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS);
            const uint8_t slot = static_cast<uint8_t>(idx + 1);
            if (!rateLimitTcp[idx].activo) continue;
            procesarUnFrameDeSlotTcp(slot);
        }
        turnoSlotTcp = static_cast<uint8_t>((turnoSlotTcp + 1) % WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS);

        // 4. Verificación de keepalives y timeouts pre-login (5 s) en Core 0
        const uint32_t ahoraMs = millis();
        if (ahoraMs - ultimaVerificacionInactividadMs >= 500) {
            ultimaVerificacionInactividadMs = ahoraMs;
            verificarKeepalivesYExpulsarInactivos();
        }
    }

    void verificarKeepalivesYExpulsarInactivos() {
        const uint32_t ahoraMs = millis();

        if (uarts.hayClienteConectado()) {
            if (static_cast<int32_t>(ahoraMs - ultimoContactoSlot0Ms) > static_cast<int32_t>(TIEMPO_EXPIRACION_KEEPALIVE_MS)) {
                solicitarLimpiezaSesionDesdeCore0(0);
                cerrarConexionFisicaPorSlot(0);
            }
        }

        const uint16_t mascaraSocketsCaidos = wifiTcp.limpiarSocketsCaidos();
        wifiBase.actualizarEstadoYExpirarPeers();

        for (uint8_t slot = 1; slot <= WiFiRuntime::MAXIMO_PEERS_WIFI; ++slot) {
            const bool socketMurio     = (mascaraSocketsCaidos & (1u << slot)) != 0;
            const bool keepaliveVencio = !wifiBase.estaSlotActivo(slot);
            const bool preAuthVencio   = (slot <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS) &&
                rateLimitTcp[slot - 1].expiroVentanaPreAuth(RPAuth::estaClienteAutenticado(slot), ahoraMs);

            if (socketMurio || keepaliveVencio || preAuthVencio) {
                if (preAuthVencio) {
                    registrarIncidenteRedThrottled(slot, "WIFI_TIMEOUT_PRE_AUTH", CriticidadEvento::BAJA);
                }
                solicitarLimpiezaSesionDesdeCore0(slot);
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
        if (bufferCompartido == nullptr || longitud <= 4 || mascaraDestinatarios == 0) {
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
#if WIFI_CIFRADO_CCM_ACTIVO
            if (estadoCryptoUdp.txSeq >= 0xFFFFFF00u) {
                estadoCryptoUdp.reiniciar();
            }
            const size_t lenCifrada = cifradorWiFi.empaquetarYCifrarFrame(
                WiFiSecurity::DIR_S2C_UDP,
                estadoCryptoUdp,
                bufferCompartido + 4,
                longitud - 4,
                bufferSalidaCifrada,
                sizeof(bufferSalidaCifrada)
            );
            if (lenCifrada > 0 && lenCifrada <= WiFiUdpPlainRuntime::TAMANO_MAXIMO_DATAGRAMA) {
                wifiUdp.emitirDatagramaASuscriptores(bufferSalidaCifrada, lenCifrada, mascaraWiFi);
            }
#else
            wifiUdp.emitirDatagramaASuscriptores(bufferCompartido, longitud, mascaraWiFi);
#endif
        } else {
            for (uint8_t slot = 1; slot <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS; ++slot) {
                if (mascaraWiFi & (1u << slot)) {
                    enviarFrameSeguroATcpSlot(slot, bufferCompartido, longitud);
                }
            }
        }
    }

    void cerrarConexionFisicaPorSlot(uint8_t slotCliente) {
        if (slotCliente == 0) {
            colaRpc.avanzarGeneracionSlot(0);
            uarts.cerrarConexionCliente();
            return;
        }

        if (slotCliente >= 1 && slotCliente <= WiFiRuntime::MAXIMO_PEERS_WIFI) {
            colaRpc.avanzarGeneracionSlot(slotCliente);
            wifiUdp.desuscribirClienteDeStream(slotCliente);
            if (slotCliente <= WiFiTcpTlsRuntime::MAXIMO_CLIENTES_TLS) {
                const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
                wifiTcp.desconectarCliente(slotCliente);
                acumuladoresTcp[idx].bytesAcumulados  = 0;
                acumuladoresTcp[idx].longitudEsperada = 0;
                estadoCryptoTcp[idx].reiniciar();
                rateLimitTcp[idx].reiniciar();
            }
            wifiBase.eliminarPeerPorSlot(slotCliente);
        }
    }

private:
    static void callbackDifusionDesdeWorker(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal) {
        if (instanciaActiva != nullptr) {
            instanciaActiva->colaRpc.publicarRespuesta(buffer, longitud, destinatarios, canal);
        }
    }

    static void callbackCierreDesdeWorker(uint8_t slotCliente) {
        if (instanciaActiva != nullptr) {
            instanciaActiva->colaRpc.solicitarCierreFisico(slotCliente);
        }
    }
};

#endif