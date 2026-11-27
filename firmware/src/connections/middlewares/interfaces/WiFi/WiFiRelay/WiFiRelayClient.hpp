#ifndef WIFI_RELAY_CLIENT_H
#define WIFI_RELAY_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <mbedtls/md.h>
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum class EstadoRelay : uint8_t {
    DESHABILITADO       = 0,
    SIN_WIFI_STA        = 1,
    DESCONECTADO        = 2,
    ESPERANDO_CHALLENGE = 3,
    ESPERANDO_AUTH_OK   = 4,
    ACTIVO              = 5
};

enum class ErrorRelay : uint8_t {
    NINGUNO          = 0,
    CONEXION_FALLIDA = 1,
    AUTH_RECHAZADA   = 2,
    TIMEOUT_HANDSHAKE = 3,
    TIMEOUT_KEEPALIVE = 4,
    TRAMA_INVALIDA   = 5,
    SOCKET_CAIDO     = 6
};

struct SnapshotEstadoRelay {
    bool        habilitado;
    bool        conectado;
    EstadoRelay estado;
    ErrorRelay  ultimoError;
    uint32_t    uptimeSegundos;
    uint32_t    reconexionesTotales;
    uint32_t    tramasRxTotales;
    uint32_t    tramasTxTotales;
    char        host[64];
    uint16_t    puerto;
    char        nodeId[17];
    uint8_t clientesRelayActivos;
};

class WiFiRelayClient {
public:
    static constexpr size_t   MAX_PAYLOAD_RX_MUX    = 512;    // Cubre cualquier trama RPC entrante AEAD (<= 512 B)
    static constexpr size_t   MAX_PAYLOAD_TX_MUX    = 2048;    // Cubre CAPACIDAD_SALIDA_CIFRADA (<= 1566 B)
    static constexpr uint8_t  MAX_TRAMAS_POR_CICLO  = 4;
    static constexpr uint32_t TIMEOUT_CONNECT_MS    = 600UL;   // Evita congelar TaskComms si el VPS no responde
    static constexpr uint32_t TIMEOUT_HANDSHAKE_MS  = 5000UL;
    static constexpr uint32_t INTERVALO_PING_MS     = 20000UL;
    static constexpr uint32_t TIMEOUT_PONG_MS       = 45000UL;
    static constexpr uint32_t BACKOFF_MIN_MS        = 3000UL;
    static constexpr uint32_t BACKOFF_MAX_MS        = 60000UL;

    inline static WiFiRelayClient* instanciaActiva = nullptr;

    enum OpMux : uint8_t {
        OP_CHALLENGE = 0x00,
        OP_HELLO     = 0x01,
        OP_OPEN      = 0x02,
        OP_DATA      = 0x03,
        OP_CLOSE     = 0x04,
        OP_PING      = 0x05,
        OP_PONG      = 0x06,
        OP_AUTH_OK   = 0x07
    };

    struct Callbacks {
        void (*onAbrirCliente)(uint8_t sid, void* ctx) = nullptr;
        void (*onDatosCliente)(uint8_t sid, const uint8_t* data, size_t len, void* ctx) = nullptr;
        void (*onCerrarCliente)(uint8_t sid, void* ctx) = nullptr;
        void (*onTunelCaido)(void* ctx) = nullptr;
        void* ctx = nullptr;
    };

private:
    struct ConfigRelayInterna {
        bool     habilitado = false;
        char     host[64]   = "";
        uint16_t puerto     = 9443;
        char     nodeId[17] = "ESP32-UNTDF-0001";
        char     secreto[65]= "";
    };

    WiFiClient tcp{};
    Callbacks  cbs{};

    // Configuración activa (solo la lee/escribe Core 0) y staging protegida por spinlock para actualizaciones desde Core 1
    ConfigRelayInterna cfgActiva{};
    ConfigRelayInterna cfgPendiente{};
    portMUX_TYPE       muxConfig = portMUX_INITIALIZER_UNLOCKED;
    std::atomic<bool>  hayConfigPendiente{false};

    // Telemetría atómica legible desde Core 1 sin bloquear el socket
    std::atomic<uint8_t>  estadoAtomico{static_cast<uint8_t>(EstadoRelay::DESHABILITADO)};
    std::atomic<uint8_t>  ultimoErrorAtomico{static_cast<uint8_t>(ErrorRelay::NINGUNO)};
    std::atomic<uint32_t> conectadoDesdeMs{0};
    std::atomic<uint32_t> reconexionesTotales{0};
    std::atomic<uint32_t> tramasRxTotales{0};
    std::atomic<uint32_t> tramasTxTotales{0};

    EstadoRelay estadoLocal       = EstadoRelay::DESHABILITADO;
    uint32_t    backoffActualMs   = BACKOFF_MIN_MS;
    uint32_t    proximoIntentoMs  = 0;
    uint32_t    ultimoPingTxMs    = 0;
    uint32_t    ultimoRxMs        = 0;

    // Buffer estático de recepción Mux (cero heap en tiempo de ejecución)
    uint8_t rxBuf[4 + MAX_PAYLOAD_RX_MUX]{0};
    size_t  rxPos = 0;

    uint16_t             mascaraSidsActivos{0};
    std::atomic<uint8_t> clientesActivosAtomico{0};

    void actualizarConteoSids() {
        uint8_t c = 0;
        uint16_t m = mascaraSidsActivos;
        while (m) {
            c += static_cast<uint8_t>(m & 1u);
            m >>= 1;
        }
        clientesActivosAtomico.store(c, std::memory_order_relaxed);
    }

    static void copiarCadenaSegura(char* dst, size_t cap, const char* src) {
        if (!dst || cap == 0) return;
        if (!src) {
            dst[0] = '\0';
            return;
        }
        strncpy(dst, src, cap - 1);
        dst[cap - 1] = '\0';
    }

    void actualizarEstado(EstadoRelay nuevo) {
        estadoLocal = nuevo;
        estadoAtomico.store(static_cast<uint8_t>(nuevo), std::memory_order_relaxed);
    }

    // ¿Cómo lo hace?: Escribe la cabecera Mux Big-Endian de 4 bytes seguida del payload directo sobre el socket TCP.
    bool enviarPaqueteMux(uint8_t op, uint8_t sid, const uint8_t* payload, uint16_t len) {
        if (!tcp.connected()) return false;

        uint8_t hdr[4] = {
            op,
            sid,
            static_cast<uint8_t>((len >> 8) & 0xFF),
            static_cast<uint8_t>(len & 0xFF)
        };
        if (tcp.write(hdr, 4) != 4) {
            return false;
        }
        if (len > 0 && payload != nullptr) {
            if (tcp.write(payload, len) != len) {
                return false;
            }
        }
        tramasTxTotales.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // ¿Cómo lo hace?: Calcula HMAC-SHA256(secreto, nodeId[16] || nonce[16]) con mbedtls nativo y envía OP_HELLO.
    bool responderChallenge(const uint8_t nonce16[16]) {
        uint8_t entrada[32]{0};
        memcpy(entrada, cfgActiva.nodeId, 16);
        memcpy(entrada + 16, nonce16, 16);

        uint8_t hmacOut[32]{0};
        const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (!md) return false;

        if (mbedtls_md_hmac(
                md,
                reinterpret_cast<const unsigned char*>(cfgActiva.secreto),
                strlen(cfgActiva.secreto),
                entrada,
                sizeof(entrada),
                hmacOut) != 0) {
            return false;
        }

        uint8_t hello[48]{0};
        memcpy(hello, cfgActiva.nodeId, 16);
        memcpy(hello + 16, hmacOut, 32);
        return enviarPaqueteMux(OP_HELLO, 0, hello, sizeof(hello));
    }

    // ¿Cómo lo hace?: Cierra el socket del túnel, notifica caída a los slots remotos activos y programa backoff exponencial.
    void cerrarTunel(uint32_t ahoraMs, ErrorRelay motivo, bool aplicarBackoff = true) {
        const bool estabaActivo = (estadoLocal == EstadoRelay::ACTIVO);
        mascaraSidsActivos = 0; actualizarConteoSids();
        tcp.stop();
        rxPos = 0;
        conectadoDesdeMs.store(0, std::memory_order_relaxed);

        if (motivo != ErrorRelay::NINGUNO) {
            ultimoErrorAtomico.store(static_cast<uint8_t>(motivo), std::memory_order_relaxed);
        }

        actualizarEstado(cfgActiva.habilitado ? EstadoRelay::DESCONECTADO : EstadoRelay::DESHABILITADO);

        if (estabaActivo && cbs.onTunelCaido) {
            cbs.onTunelCaido(cbs.ctx);
        }

        if (aplicarBackoff) {
            proximoIntentoMs = ahoraMs + backoffActualMs;
            backoffActualMs  = (backoffActualMs * 2 <= BACKOFF_MAX_MS)
                ? (backoffActualMs * 2)
                : BACKOFF_MAX_MS;
        } else {
            backoffActualMs  = BACKOFF_MIN_MS;
            proximoIntentoMs = ahoraMs;
        }
    }

    // ¿Cómo lo hace?: Aplica en Core 0 cualquier cambio de configuración recibido desde Core 1 sin competir por el socket.
    void aplicarConfiguracionPendienteSiExiste(uint32_t ahoraMs) {
        if (!hayConfigPendiente.load(std::memory_order_acquire)) {
            return;
        }

        ConfigRelayInterna nueva{};
        portENTER_CRITICAL(&muxConfig);
        nueva = cfgPendiente;
        hayConfigPendiente.store(false, std::memory_order_release);
        portEXIT_CRITICAL(&muxConfig);

        const bool cambioRelevante =
            (nueva.habilitado != cfgActiva.habilitado) ||
            (nueva.puerto != cfgActiva.puerto) ||
            (strncmp(nueva.host, cfgActiva.host, sizeof(cfgActiva.host)) != 0) ||
            (strncmp(nueva.nodeId, cfgActiva.nodeId, sizeof(cfgActiva.nodeId)) != 0) ||
            (strncmp(nueva.secreto, cfgActiva.secreto, sizeof(cfgActiva.secreto)) != 0);

        cfgActiva = nueva;
        if (cambioRelevante) {
            ultimoErrorAtomico.store(static_cast<uint8_t>(ErrorRelay::NINGUNO), std::memory_order_relaxed);
            cerrarTunel(ahoraMs, ErrorRelay::NINGUNO, false);
        }
    }

    void procesarTramaCompleta(uint8_t op, uint8_t sid, const uint8_t* payload, uint16_t len, uint32_t ahoraMs) {
        tramasRxTotales.fetch_add(1, std::memory_order_relaxed);

        if (estadoLocal == EstadoRelay::ESPERANDO_CHALLENGE) {
            if (op == OP_CHALLENGE && len == 16 && responderChallenge(payload)) {
                actualizarEstado(EstadoRelay::ESPERANDO_AUTH_OK);
                ultimoRxMs = ahoraMs;
            } else {
                cerrarTunel(ahoraMs, ErrorRelay::AUTH_RECHAZADA);
            }
            return;
        }

        if (estadoLocal == EstadoRelay::ESPERANDO_AUTH_OK) {
            if (op == OP_AUTH_OK) {
                actualizarEstado(EstadoRelay::ACTIVO);
                ultimoErrorAtomico.store(static_cast<uint8_t>(ErrorRelay::NINGUNO), std::memory_order_relaxed);
                backoffActualMs = BACKOFF_MIN_MS;
                ultimoPingTxMs  = ahoraMs;
                ultimoRxMs      = ahoraMs;
                conectadoDesdeMs.store(ahoraMs, std::memory_order_relaxed);
                reconexionesTotales.fetch_add(1, std::memory_order_relaxed);
            } else {
                cerrarTunel(ahoraMs, ErrorRelay::AUTH_RECHAZADA);
            }
            return;
        }

        switch (op) {
            case OP_OPEN:
                if (sid >= 1 && sid <= 15) { mascaraSidsActivos |= static_cast<uint16_t>(1u << sid); actualizarConteoSids(); }
                if (cbs.onAbrirCliente) cbs.onAbrirCliente(sid, cbs.ctx);
                break;
            case OP_DATA:
                if (cbs.onDatosCliente && len > 0) cbs.onDatosCliente(sid, payload, len, cbs.ctx);
                break;
            case OP_CLOSE:
                if (sid >= 1 && sid <= 15) { mascaraSidsActivos &= static_cast<uint16_t>(~(1u << sid)); actualizarConteoSids(); }
                if (cbs.onCerrarCliente) cbs.onCerrarCliente(sid, cbs.ctx);
                break;
            case OP_PING:
                enviarPaqueteMux(OP_PONG, 0, nullptr, 0);
                break;
            case OP_PONG:
                break;
            default:
                break;
        }
    }

public:
    WiFiRelayClient() {
        instanciaActiva = this;
    }

    // ¿Cómo lo hace?: Registra los callbacks de entrega hacia WiFiTcpTlsRuntime durante setup().
    void vincularCallbacks(const Callbacks& callbacks) {
        cbs = callbacks;
    }

    // ¿Cómo lo hace?: Copia parámetros en staging bajo spinlock; es seguro llamarlo desde Core 1 (RPC/setup).
    void configurar(
        bool habilitar,
        const char* hostRelay,
        uint16_t puertoRelay,
        const char* idNodo16,
        const char* secretoCompartido
    ) {
        ConfigRelayInterna nueva{};
        copiarCadenaSegura(nueva.host, sizeof(nueva.host), hostRelay);
        nueva.puerto = (puertoRelay > 0) ? puertoRelay : 9443;

        memset(nueva.nodeId, 0, sizeof(nueva.nodeId));
        if (idNodo16 && idNodo16[0] != '\0') {
            strncpy(nueva.nodeId, idNodo16, 16);
        } else {
            strncpy(nueva.nodeId, "ESP32-UNTDF-0001", 16);
        }

        copiarCadenaSegura(nueva.secreto, sizeof(nueva.secreto), secretoCompartido);
        nueva.habilitado = habilitar && (nueva.host[0] != '\0') && (nueva.secreto[0] != '\0');

        portENTER_CRITICAL(&muxConfig);
        cfgPendiente = nueva;
        hayConfigPendiente.store(true, std::memory_order_release);
        portEXIT_CRITICAL(&muxConfig);
    }

    // ¿Cómo lo hace?: Envía bytes cifrados AEAD de un cliente hacia el VPS encapsulados en OP_DATA (solo Core 0).
    bool enviarDatosACliente(uint8_t sid, const uint8_t* data, size_t len) {
        if (estadoLocal != EstadoRelay::ACTIVO || sid == 0 || !data || len == 0 || len > MAX_PAYLOAD_TX_MUX) {
            return false;
        }
        return enviarPaqueteMux(OP_DATA, sid, data, static_cast<uint16_t>(len));
    }

    // ¿Cómo lo hace?: Solicita al VPS el cierre inmediato del socket TCP del cliente remoto indicado (solo Core 0).
    void cerrarClienteRemoto(uint8_t sid) {
        if (estadoLocal == EstadoRelay::ACTIVO && sid != 0) {
            if (sid >= 1 && sid <= 15) { mascaraSidsActivos &= static_cast<uint16_t>(~(1u << sid)); actualizarConteoSids(); }
            enviarPaqueteMux(OP_CLOSE, sid, nullptr, 0);
        }
    }

    // ¿Cómo lo hace?: Ejecuta máquina de estados no bloqueante, lectura RX, despacho Mux y keepalive en Core 0.
    void procesarCiclo() {
        const uint32_t ahoraMs = millis();
        aplicarConfiguracionPendienteSiExiste(ahoraMs);

        if (!cfgActiva.habilitado) {
            if (estadoLocal != EstadoRelay::DESHABILITADO) {
                cerrarTunel(ahoraMs, ErrorRelay::NINGUNO, false);
            }
            return;
        }

        if (WiFi.status() != WL_CONNECTED) {
            if (estadoLocal != EstadoRelay::SIN_WIFI_STA) {
                if (tcp.connected() || estadoLocal == EstadoRelay::ACTIVO) {
                    cerrarTunel(ahoraMs, ErrorRelay::SOCKET_CAIDO, false);
                }
                actualizarEstado(EstadoRelay::SIN_WIFI_STA);
            }
            return;
        }

        if (estadoLocal == EstadoRelay::SIN_WIFI_STA) {
            actualizarEstado(EstadoRelay::DESCONECTADO);
            proximoIntentoMs = ahoraMs;
        }

        if (estadoLocal == EstadoRelay::DESCONECTADO) {
            if (static_cast<int32_t>(ahoraMs - proximoIntentoMs) < 0) {
                return;
            }
            if (tcp.connect(cfgActiva.host, cfgActiva.puerto, static_cast<int32_t>(TIMEOUT_CONNECT_MS))) {
                tcp.setNoDelay(true);
                rxPos      = 0;
                ultimoRxMs = ahoraMs;
                actualizarEstado(EstadoRelay::ESPERANDO_CHALLENGE);
            } else {
                cerrarTunel(ahoraMs, ErrorRelay::CONEXION_FALLIDA);
            }
            return;
        }

        if (!tcp.connected()) {
            const ErrorRelay motivo = (estadoLocal == EstadoRelay::ESPERANDO_AUTH_OK)
                ? ErrorRelay::AUTH_RECHAZADA
                : ErrorRelay::SOCKET_CAIDO;
            cerrarTunel(ahoraMs, motivo);
            return;
        }

        // 1. Leer bytes disponibles sin bloquear
        while (tcp.available() > 0 && rxPos < sizeof(rxBuf)) {
            const int leidos = tcp.read(rxBuf + rxPos, sizeof(rxBuf) - rxPos);
            if (leidos <= 0) break;
            rxPos     += static_cast<size_t>(leidos);
            ultimoRxMs = ahoraMs;
        }

        // 2. Despachar hasta MAX_TRAMAS_POR_CICLO tramas completas ya presentes en rxBuf
        uint8_t procesadas = 0;
        while (rxPos >= 4 && procesadas < MAX_TRAMAS_POR_CICLO) {
            const uint8_t  op  = rxBuf[0];
            const uint8_t  sid = rxBuf[1];
            const uint16_t len = static_cast<uint16_t>((rxBuf[2] << 8) | rxBuf[3]);

            if (len > MAX_PAYLOAD_RX_MUX) {
                cerrarTunel(ahoraMs, ErrorRelay::TRAMA_INVALIDA);
                return;
            }
            const size_t total = 4u + len;
            if (rxPos < total) break;

            procesarTramaCompleta(op, sid, rxBuf + 4, len, ahoraMs);
            if (estadoLocal == EstadoRelay::DESCONECTADO || estadoLocal == EstadoRelay::DESHABILITADO) {
                return;
            }

            const size_t sobrante = rxPos - total;
            if (sobrante > 0) {
                memmove(rxBuf, rxBuf + total, sobrante);
            }
            rxPos = sobrante;
            procesadas++;

            // Cede el ciclo a MiddlewareInterface para liberar la sesión del slot antes de aceptar otro OP_OPEN
            if (op == OP_CLOSE) {
                break;
            }
        }

        // 3. Control de keepalive (CGNAT NAT-table refresh) y timeouts
        if (estadoLocal == EstadoRelay::ACTIVO) {
            if (static_cast<int32_t>(ahoraMs - ultimoPingTxMs) >= static_cast<int32_t>(INTERVALO_PING_MS)) {
                ultimoPingTxMs = ahoraMs;
                if (!enviarPaqueteMux(OP_PING, 0, nullptr, 0)) {
                    cerrarTunel(ahoraMs, ErrorRelay::SOCKET_CAIDO);
                    return;
                }
            }
            if (static_cast<int32_t>(ahoraMs - ultimoRxMs) >= static_cast<int32_t>(TIMEOUT_PONG_MS)) {
                cerrarTunel(ahoraMs, ErrorRelay::TIMEOUT_KEEPALIVE);
            }
        } else if (static_cast<int32_t>(ahoraMs - ultimoRxMs) >= static_cast<int32_t>(TIMEOUT_HANDSHAKE_MS)) {
            cerrarTunel(ahoraMs, ErrorRelay::TIMEOUT_HANDSHAKE);
        }
    }

    bool estaActivo() const {
        return static_cast<EstadoRelay>(estadoAtomico.load(std::memory_order_relaxed)) == EstadoRelay::ACTIVO;
    }

    // ¿Cómo lo hace?: Arma una fotografía segura desde cualquier núcleo (Core 0 o Core 1) para diagnóstico RPC.
    SnapshotEstadoRelay obtenerSnapshotEstado() const {
        SnapshotEstadoRelay snap{};
        snap.estado              = static_cast<EstadoRelay>(estadoAtomico.load(std::memory_order_relaxed));
        snap.ultimoError         = static_cast<ErrorRelay>(ultimoErrorAtomico.load(std::memory_order_relaxed));
        snap.conectado           = (snap.estado == EstadoRelay::ACTIVO);
        snap.reconexionesTotales = reconexionesTotales.load(std::memory_order_relaxed);
        snap.tramasRxTotales     = tramasRxTotales.load(std::memory_order_relaxed);
        snap.tramasTxTotales     = tramasTxTotales.load(std::memory_order_relaxed);
        snap.clientesRelayActivos = clientesActivosAtomico.load(std::memory_order_relaxed);

        const uint32_t inicioMs = conectadoDesdeMs.load(std::memory_order_relaxed);
        if (snap.conectado && inicioMs != 0) {
            const int32_t diff = static_cast<int32_t>(millis() - inicioMs);
            snap.uptimeSegundos = (diff > 0) ? static_cast<uint32_t>(diff) / 1000UL : 0UL;
        }

        portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&muxConfig));
        const ConfigRelayInterna& src = hayConfigPendiente.load(std::memory_order_relaxed) ? cfgPendiente : cfgActiva;
        snap.habilitado = src.habilitado;
        snap.puerto     = src.puerto;
        copiarCadenaSegura(snap.host, sizeof(snap.host), src.host);
        copiarCadenaSegura(snap.nodeId, sizeof(snap.nodeId), src.nodeId);
        portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&muxConfig));

        return snap;
    }
};

#endif