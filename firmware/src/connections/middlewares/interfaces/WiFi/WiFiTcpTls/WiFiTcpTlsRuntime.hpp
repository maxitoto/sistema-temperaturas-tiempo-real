#ifndef WIFI_TCP_TLS_RUNTIME_H
#define WIFI_TCP_TLS_RUNTIME_H

#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../WiFiRelay/WiFiRelayClient.hpp"

struct NuevoClienteAceptado {
    uint8_t slotAsignado;
    char ip[16];
    uint16_t puerto;
};

class WiFiTcpTlsRuntime {
public:
    static constexpr uint16_t PUERTO_SERVIDOR_TLS     = 8443;
    static constexpr uint8_t  MAXIMO_CLIENTES_TLS     = 4;
    static constexpr uint32_t INTERVALO_MIN_ACCEPT_MS = 50;
    

private:

    WiFiServer servidorTcp{PUERTO_SERVIDOR_TLS};
    WiFiClient clientesTcp[MAXIMO_CLIENTES_TLS]{};
    bool       slotOcupado[MAXIMO_CLIENTES_TLS]{false};
    uint32_t   ultimoHandshakeMs{0};

    // Estado por slot para clientes remotos multiplexados sobre WiFiRelayClient
    WiFiRelayClient* refRelay{nullptr};
    bool             esRelay[MAXIMO_CLIENTES_TLS]{false};
    bool             pendienteAceptarRelay[MAXIMO_CLIENTES_TLS]{false};
    bool             caidoRelay[MAXIMO_CLIENTES_TLS]{false};
    uint8_t          sidRelay[MAXIMO_CLIENTES_TLS]{0};
    void (*onDatosSlotRelay)(uint8_t slot, const uint8_t* data, size_t len, void* ctx){nullptr};
    void* ctxDatosSlotRelay{nullptr};
    

    int8_t buscarIndicePorSid(uint8_t sid) const {
        if (sid == 0) return -1;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && esRelay[i] && !caidoRelay[i] && sidRelay[i] == sid) {
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    // ¿Cómo lo hace?: Asigna el primer slot libre a un cliente remoto entrante o devuelve OP_CLOSE si los 4 slots están llenos.
    void manejarAbrirClienteRelay(uint8_t sid) {
        if (sid == 0) return;

        // Si el VPS reutiliza un sid que ya estaba mapeado, se cierra primero para evitar duplicados
        const int8_t existente = buscarIndicePorSid(sid);
        if (existente >= 0) {
            caidoRelay[existente]            = true;
            pendienteAceptarRelay[existente] = false;
            sidRelay[existente]              = 0;
        }

        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (!slotOcupado[i] || (!esRelay[i] && !clientesTcp[i].connected())) {
                if (!esRelay[i]) {
                    clientesTcp[i].stop();
                }
                slotOcupado[i]           = true;
                esRelay[i]               = true;
                pendienteAceptarRelay[i] = true;
                caidoRelay[i]            = false;
                sidRelay[i]              = sid;
                return;
            }
        }

        // Sin cupo disponible en los 4 slots: rechazar limpiamente al cliente remoto
        if (refRelay) {
            refRelay->cerrarClienteRemoto(sid);
        }
    }

    // ¿Cómo lo hace?: Inserta los bytes cifrados del cliente remoto en su cola circular RX; si desborda, marca el slot caído.
    void manejarDatosClienteRelay(uint8_t sid, const uint8_t* data, size_t len) {
        const int8_t idx = buscarIndicePorSid(sid);
        if (idx < 0 || !data || len == 0) return;
        if (onDatosSlotRelay) {
            onDatosSlotRelay(static_cast<uint8_t>(idx + 1), data, len, ctxDatosSlotRelay);
        }
    }

    void manejarCerrarClienteRelay(uint8_t sid) {
        const int8_t idx = buscarIndicePorSid(sid);
        if (idx < 0) return;
        caidoRelay[idx]            = true;
        pendienteAceptarRelay[idx] = false;
        sidRelay[idx]              = 0;
    }

    void manejarTunelRelayCaido() {
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && esRelay[i]) {
                caidoRelay[i]            = true;
                pendienteAceptarRelay[i] = false;
                sidRelay[i]              = 0;
            }
        }
    }

    static void cbAbrirRelay(uint8_t sid, void* ctx) {
        if (ctx) static_cast<WiFiTcpTlsRuntime*>(ctx)->manejarAbrirClienteRelay(sid);
    }
    static void cbDatosRelay(uint8_t sid, const uint8_t* data, size_t len, void* ctx) {
        if (ctx) static_cast<WiFiTcpTlsRuntime*>(ctx)->manejarDatosClienteRelay(sid, data, len);
    }
    static void cbCerrarRelay(uint8_t sid, void* ctx) {
        if (ctx) static_cast<WiFiTcpTlsRuntime*>(ctx)->manejarCerrarClienteRelay(sid);
    }
    static void cbTunelCaido(void* ctx) {
        if (ctx) static_cast<WiFiTcpTlsRuntime*>(ctx)->manejarTunelRelayCaido();
    }

public:
    WiFiTcpTlsRuntime() = default;

    // ¿Cómo lo hace?: Vincula la instancia global de WiFiRelayClient y registra los 4 callbacks de multiplexación.
    void vincularRelay(WiFiRelayClient* relay) {
        refRelay = relay;
        if (refRelay) {
            WiFiRelayClient::Callbacks cbs{};
            cbs.onAbrirCliente  = &WiFiTcpTlsRuntime::cbAbrirRelay;
            cbs.onDatosCliente  = &WiFiTcpTlsRuntime::cbDatosRelay;
            cbs.onCerrarCliente = &WiFiTcpTlsRuntime::cbCerrarRelay;
            cbs.onTunelCaido    = &WiFiTcpTlsRuntime::cbTunelCaido;
            cbs.ctx             = this;
            refRelay->vincularCallbacks(cbs);
        }
    }

    void configurarCallbackDatosRelay(void (*cb)(uint8_t slot, const uint8_t* data, size_t len, void* ctx), void* ctx) {
        onDatosSlotRelay  = cb;
        ctxDatosSlotRelay = ctx;
    }

    // ¿Cómo lo hace?: Ejecuta un paso no bloqueante del cliente Reverse TCP Relay en Core 0.
    void procesarCicloRelay() {
        if (refRelay) {
            refRelay->procesarCiclo();
        }
    }

    // ¿Cómo lo hace?: Inicia el servidor de sockets WiFiServer en el puerto fijo 8443 sobre todas las interfaces activas (AP y STA).
    bool iniciarServidorSeguro(const char* certPem = nullptr, const char* keyPem = nullptr) {
        (void)certPem;
        (void)keyPem;
        servidorTcp.begin(PUERTO_SERVIDOR_TLS);
        servidorTcp.setNoDelay(true);
        return true;
    }

    // ¿Cómo lo hace?: Entrega primero cualquier cliente remoto pendiente del Relay y luego acepta conexiones locales TCP en :8443.
    NuevoClienteAceptado aceptarConexionEntrante() {
        NuevoClienteAceptado resultado{0, "", 0};

        // 1. Clientes remotos entrantes desde el Reverse TCP Relay
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && esRelay[i] && pendienteAceptarRelay[i] && !caidoRelay[i]) {
                pendienteAceptarRelay[i] = false;
                resultado.slotAsignado   = static_cast<uint8_t>(i + 1);
                snprintf(resultado.ip, sizeof(resultado.ip), "RELAY-%u", static_cast<unsigned>(sidRelay[i]));
                resultado.puerto         = static_cast<uint16_t>(9440u + sidRelay[i]);
                return resultado;
            }
        }

        // 2. Clientes locales SoftAP / STA en :8443 con control de tasa
        const uint32_t ahoraMs = millis();
        if (static_cast<int32_t>(ahoraMs - ultimoHandshakeMs) < static_cast<int32_t>(INTERVALO_MIN_ACCEPT_MS)) {
            return resultado;
        }

        WiFiClient entrante = servidorTcp.accept();
        if (!entrante) {
            return resultado;
        }
        ultimoHandshakeMs = ahoraMs;

        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (!slotOcupado[i] || (!esRelay[i] && !clientesTcp[i].connected())) {
                if (!esRelay[i]) {
                    clientesTcp[i].stop();
                }
                clientesTcp[i]           = entrante;
                clientesTcp[i].setNoDelay(true);
                slotOcupado[i]           = true;
                esRelay[i]               = false;
                pendienteAceptarRelay[i] = false;
                caidoRelay[i]            = false;
                sidRelay[i]              = 0;

                resultado.slotAsignado = static_cast<uint8_t>(i + 1);
                IPAddress ipRemota = clientesTcp[i].remoteIP();
                snprintf(resultado.ip, sizeof(resultado.ip), "%u.%u.%u.%u",
                         ipRemota[0], ipRemota[1], ipRemota[2], ipRemota[3]);
                resultado.puerto = clientesTcp[i].remotePort();
                return resultado;
            }
        }

        entrante.stop();
        return resultado;
    }

    // ¿Cómo lo hace?: Escribe los bytes cifrados en el socket local o los encapsula en OP_DATA si el slot pertenece al Relay.
    bool enviarBytesACliente(uint8_t slotCliente, const uint8_t* datos, size_t longitud) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS || datos == nullptr || longitud == 0) {
            return false;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!slotOcupado[idx]) {
            return false;
        }

        if (esRelay[idx]) {
            if (caidoRelay[idx] || !refRelay || sidRelay[idx] == 0) {
                return false;
            }
            return refRelay->enviarDatosACliente(sidRelay[idx], datos, longitud);
        }

        if (!clientesTcp[idx].connected()) {
            return false;
        }

        const size_t escritos = clientesTcp[idx].write(datos, longitud);
        return escritos == longitud;
    }

    // ¿Cómo lo hace?: Lee de forma no bloqueante del socket TCP local o de la cola circular RX si es un cliente Relay.
    size_t recibirBytesDeCliente(uint8_t slotCliente, uint8_t* bufferDestino, size_t capacidadMaxima) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS || bufferDestino == nullptr || capacidadMaxima == 0) {
            return 0;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!slotOcupado[idx]) {
            return 0;
        }

        if (esRelay[idx]) {
            return 0;
        }

        if (!clientesTcp[idx].connected()) {
            return 0;
        }

        const int disponibles = clientesTcp[idx].available();
        if (disponibles <= 0) {
            return 0;
        }

        const size_t aLeer = (static_cast<size_t>(disponibles) < capacidadMaxima)
                             ? static_cast<size_t>(disponibles)
                             : capacidadMaxima;

        const int leidos = clientesTcp[idx].read(bufferDestino, aLeer);
        return (leidos > 0) ? static_cast<size_t>(leidos) : 0;
    }

    // ¿Cómo lo hace?: Recorre los 4 slots (locales y remotos) y envía el mismo buffer a todos los activos.
    uint8_t difundirBytesATodos(const uint8_t* datos, size_t longitud) {
        if (datos == nullptr || longitud == 0) {
            return 0;
        }

        uint8_t exitosos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (enviarBytesACliente(static_cast<uint8_t>(i + 1), datos, longitud)) {
                exitosos++;
            }
        }
        return exitosos;
    }

    // ¿Cómo lo hace?: Cierra el socket local o notifica OP_CLOSE al VPS y libera la ranura.
    void desconectarCliente(uint8_t slotCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS) {
            return;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!slotOcupado[idx]) {
            return;
        }

        if (esRelay[idx]) {
            if (!caidoRelay[idx] && refRelay && sidRelay[idx] != 0) {
                refRelay->cerrarClienteRemoto(sidRelay[idx]);
            }
            esRelay[idx]               = false;
            pendienteAceptarRelay[idx] = false;
            caidoRelay[idx]            = false;
            sidRelay[idx]              = 0;
        } else {
            clientesTcp[idx].stop();
        }
        slotOcupado[idx] = false;
    }

    // ¿Cómo lo hace?: Devuelve y libera al instante solo los slots Relay cerrados desde el VPS (sin esperar el ciclo de 500 ms).
    uint16_t limpiarSocketsCaidosRelay() {
        uint16_t mascaraCaidos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && esRelay[i] && caidoRelay[i]) {
                slotOcupado[i]           = false;
                esRelay[i]               = false;
                pendienteAceptarRelay[i] = false;
                caidoRelay[i]            = false;
                sidRelay[i]              = 0;
                mascaraCaidos |= static_cast<uint16_t>(1u << (i + 1));
            }
        }
        return mascaraCaidos;
    }

    // ¿Cómo lo hace?: Detecta qué sockets locales o remotos perdieron conexión física, los libera y devuelve su máscara.
    uint16_t limpiarSocketsCaidos() {
        uint16_t mascaraCaidos = limpiarSocketsCaidosRelay();
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && !esRelay[i] && !clientesTcp[i].connected()) {
                clientesTcp[i].stop();
                slotOcupado[i] = false;
                mascaraCaidos |= static_cast<uint16_t>(1u << (i + 1));
            }
        }
        return mascaraCaidos;
    }

    bool esClienteViaRelay(uint8_t slotCliente) const {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS) return false;
        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        return slotOcupado[idx] && esRelay[idx];
    }

    uint8_t obtenerCantidadClientesConectados() const {
        uint8_t activos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && (!esRelay[i] || !caidoRelay[i])) {
                activos++;
            }
        }
        return activos;
    }

    uint8_t obtenerCantidadClientesRelayConectados() const {
        uint8_t activos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && esRelay[i] && !caidoRelay[i]) {
                activos++;
            }
        }
        return activos;
    }
};

#endif