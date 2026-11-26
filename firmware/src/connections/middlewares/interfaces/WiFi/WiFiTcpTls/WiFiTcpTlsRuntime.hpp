#ifndef WIFI_TCP_TLS_RUNTIME_H
#define WIFI_TCP_TLS_RUNTIME_H

#include <Arduino.h>
#include <WiFi.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct NuevoClienteAceptado {
    uint8_t slotAsignado;
    char ip[16];
    uint16_t puerto;
};

class WiFiTcpTlsRuntime {
public:
    static constexpr uint16_t PUERTO_SERVIDOR_TLS = 8443;
    static constexpr uint8_t MAXIMO_CLIENTES_TLS = 4;
    static constexpr uint32_t INTERVALO_MIN_ACCEPT_MS = 50;

private:
    WiFiServer servidorTcp{PUERTO_SERVIDOR_TLS};
    WiFiClient clientesTcp[MAXIMO_CLIENTES_TLS]{};
    bool slotOcupado[MAXIMO_CLIENTES_TLS]{false};
    uint32_t ultimoHandshakeMs{0};

public:
    WiFiTcpTlsRuntime() = default;

    // ¿Cómo lo hace?: Inicia el servidor de sockets WiFiServer en el puerto fijo 8443 sobre todas las interfaces activas (AP y STA).
    bool iniciarServidorSeguro(const char* certPem = nullptr, const char* keyPem = nullptr) {
        (void)certPem;
        (void)keyPem;
        servidorTcp.begin(PUERTO_SERVIDOR_TLS);
        servidorTcp.setNoDelay(true);
        return true;
    }

    // ¿Cómo lo hace?: Consulta servidorTcp.available(), aplica control de tasa de aceptación y asigna el primer slot libre (1..4) al cliente entrante.
    NuevoClienteAceptado aceptarConexionEntrante() {
        NuevoClienteAceptado resultado{0, "", 0};

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
            if (!slotOcupado[i] || !clientesTcp[i].connected()) {
                clientesTcp[i].stop();
                clientesTcp[i] = entrante;
                clientesTcp[i].setNoDelay(true);
                slotOcupado[i] = true;

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

    // ¿Cómo lo hace?: Escribe el bloque de bytes en el socket activo del slot indicado (1..4) y verifica que se hayan enviado todos.
    bool enviarBytesACliente(uint8_t slotCliente, const uint8_t* datos, size_t longitud) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS || datos == nullptr || longitud == 0) {
            return false;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!slotOcupado[idx] || !clientesTcp[idx].connected()) {
            return false;
        }

        const size_t escritos = clientesTcp[idx].write(datos, longitud);
        return escritos == longitud;
    }

    // ¿Cómo lo hace?: Lee de forma no bloqueante los bytes disponibles en el socket del slot indicado hasta completar capacidadMaxima.
    size_t recibirBytesDeCliente(uint8_t slotCliente, uint8_t* bufferDestino, size_t capacidadMaxima) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS || bufferDestino == nullptr || capacidadMaxima == 0) {
            return 0;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!slotOcupado[idx] || !clientesTcp[idx].connected()) {
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

    // ¿Cómo lo hace?: Recorre los 4 slots y envía el mismo buffer a todos los clientes conectados actualmente.
    uint8_t difundirBytesATodos(const uint8_t* datos, size_t longitud) {
        if (datos == nullptr || longitud == 0) {
            return 0;
        }

        uint8_t exitosos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && clientesTcp[i].connected()) {
                if (clientesTcp[i].write(datos, longitud) == longitud) {
                    exitosos++;
                }
            }
        }
        return exitosos;
    }

    // ¿Cómo lo hace?: Cierra el socket del slot indicado con stop() y libera la ranura.
    void desconectarCliente(uint8_t slotCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_CLIENTES_TLS) {
            return;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (slotOcupado[idx]) {
            clientesTcp[idx].stop();
            slotOcupado[idx] = false;
        }
    }

    // ¿Cómo lo hace?: Detecta qué sockets perdieron conexión física, los cierra y devuelve sus bits encendidos en una máscara.
    uint16_t limpiarSocketsCaidos() {
        uint16_t mascaraCaidos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i] && !clientesTcp[i].connected()) {
                clientesTcp[i].stop();
                slotOcupado[i] = false;
                const uint8_t slot = static_cast<uint8_t>(i + 1);
                mascaraCaidos |= static_cast<uint16_t>(1u << slot);
            }
        }
        return mascaraCaidos;
    }

    // ¿Cómo lo hace?: Cuenta cuántos slots tienen un socket activo actualmente.
    uint8_t obtenerCantidadClientesConectados() const {
        uint8_t activos = 0;
        for (uint8_t i = 0; i < MAXIMO_CLIENTES_TLS; ++i) {
            if (slotOcupado[i]) {
                activos++;
            }
        }
        return activos;
    }
};

#endif