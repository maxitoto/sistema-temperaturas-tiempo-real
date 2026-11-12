#ifndef WIFI_RUNTIME_H
#define WIFI_RUNTIME_H

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <stdint.h>
#include <string.h>
#include "WiFiInicializador.hpp"

struct IdentidadPeerWiFi {
    char ip[16];
    uint16_t puerto;
};

struct RegistroPeerWiFi {
    IdentidadPeerWiFi identidad;
    uint32_t ultimoContactoMs;
    bool slotOcupado;
};

class WiFiRuntime {
public:
    static constexpr uint8_t MAXIMO_PEERS_WIFI = 8;
    static constexpr uint32_t TIEMPO_EXPIRACION_PEER_MS = 120000; // 2 minutos

private:
    WiFiInicializador& inicializador;
    RegistroPeerWiFi tablaPeers[MAXIMO_PEERS_WIFI]{};
    std::atomic<uint8_t> contadorClientesActivos{0};
    std::atomic<bool> routerConectado{false};

public:
    explicit WiFiRuntime(WiFiInicializador& init) : inicializador(init) {
        for (uint8_t i = 0; i < MAXIMO_PEERS_WIFI; ++i) {
            tablaPeers[i].slotOcupado = false;
            tablaPeers[i].identidad.ip[0] = '\0';
            tablaPeers[i].identidad.puerto = 0;
            tablaPeers[i].ultimoContactoMs = 0;
        }
    }

    void actualizarEstadoYExpirarPeers() {
        const bool staOk = (WiFi.status() == WL_CONNECTED);
        routerConectado.store(staOk, std::memory_order_relaxed);
        inicializador.actualizarEstadoEnCaliente(staOk);

        const uint32_t ahoraMs = millis();
        uint8_t activos = 0;

        for (uint8_t i = 0; i < MAXIMO_PEERS_WIFI; ++i) {
            if (tablaPeers[i].slotOcupado) {
                if (static_cast<int32_t>(ahoraMs - tablaPeers[i].ultimoContactoMs) > static_cast<int32_t>(TIEMPO_EXPIRACION_PEER_MS)) {
                    tablaPeers[i].slotOcupado = false;
                    tablaPeers[i].identidad.ip[0] = '\0';
                    tablaPeers[i].identidad.puerto = 0;
                } else {
                    activos++;
                }
            }
        }

        contadorClientesActivos.store(activos, std::memory_order_relaxed);
    }

    uint8_t registrarNuevoPeer(const char* ip, uint16_t puerto) {
        if (ip == nullptr || ip[0] == '\0' || puerto == 0) {
            return 0;
        }

        const uint32_t ahoraMs = millis();

        for (uint8_t i = 0; i < MAXIMO_PEERS_WIFI; ++i) {
            if (tablaPeers[i].slotOcupado &&
                tablaPeers[i].identidad.puerto == puerto &&
                strncmp(tablaPeers[i].identidad.ip, ip, sizeof(tablaPeers[i].identidad.ip)) == 0) {
                tablaPeers[i].ultimoContactoMs = ahoraMs;
                return static_cast<uint8_t>(i + 1);
            }
        }

        for (uint8_t i = 0; i < MAXIMO_PEERS_WIFI; ++i) {
            if (!tablaPeers[i].slotOcupado) {
                strncpy(tablaPeers[i].identidad.ip, ip, sizeof(tablaPeers[i].identidad.ip) - 1);
                tablaPeers[i].identidad.ip[sizeof(tablaPeers[i].identidad.ip) - 1] = '\0';
                tablaPeers[i].identidad.puerto = puerto;
                tablaPeers[i].ultimoContactoMs = ahoraMs;
                tablaPeers[i].slotOcupado = true;

                contadorClientesActivos.fetch_add(1, std::memory_order_relaxed);
                return static_cast<uint8_t>(i + 1);
            }
        }

        return 0;
    }

    bool registrarPeerEnSlotEspecifico(uint8_t slotCliente, const char* ip, uint16_t puerto) {
        if (slotCliente < 1 || slotCliente > MAXIMO_PEERS_WIFI || ip == nullptr || ip[0] == '\0' || puerto == 0) {
            return false;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        const bool estabaLibre = !tablaPeers[idx].slotOcupado;

        strncpy(tablaPeers[idx].identidad.ip, ip, sizeof(tablaPeers[idx].identidad.ip) - 1);
        tablaPeers[idx].identidad.ip[sizeof(tablaPeers[idx].identidad.ip) - 1] = '\0';
        tablaPeers[idx].identidad.puerto = puerto;
        tablaPeers[idx].ultimoContactoMs = millis();
        tablaPeers[idx].slotOcupado = true;

        if (estabaLibre) {
            contadorClientesActivos.fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }

    void eliminarPeerPorSlot(uint8_t slotCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_PEERS_WIFI) {
            return;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (tablaPeers[idx].slotOcupado) {
            tablaPeers[idx].slotOcupado = false;
            tablaPeers[idx].identidad.ip[0] = '\0';
            tablaPeers[idx].identidad.puerto = 0;
            tablaPeers[idx].ultimoContactoMs = 0;

            uint8_t actual = contadorClientesActivos.load(std::memory_order_relaxed);
            if (actual > 0) {
                contadorClientesActivos.store(static_cast<uint8_t>(actual - 1), std::memory_order_relaxed);
            }
        }
    }

    void refrescarActividadPeer(uint8_t slotCliente) {
        if (slotCliente < 1 || slotCliente > MAXIMO_PEERS_WIFI) {
            return;
        }

        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (tablaPeers[idx].slotOcupado) {
            tablaPeers[idx].ultimoContactoMs = millis();
        }
    }

    uint8_t obtenerCantidadClientesActivos() const {
        return contadorClientesActivos.load(std::memory_order_relaxed);
    }

    bool hayConexionConRouter() const {
        return routerConectado.load(std::memory_order_relaxed);
    }

    bool estaSlotActivo(uint8_t slotCliente) const {
        if (slotCliente < 1 || slotCliente > MAXIMO_PEERS_WIFI) {
            return false;
        }
        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        if (!tablaPeers[idx].slotOcupado) {
            return false;
        }
        return static_cast<int32_t>(millis() - tablaPeers[idx].ultimoContactoMs) <= static_cast<int32_t>(TIEMPO_EXPIRACION_PEER_MS);
    }

    const IdentidadPeerWiFi& obtenerIdentidadPeer(uint8_t slotCliente) const {
        static const IdentidadPeerWiFi identidadVacia{"", 0};
        if (slotCliente < 1 || slotCliente > MAXIMO_PEERS_WIFI) {
            return identidadVacia;
        }
        const uint8_t idx = static_cast<uint8_t>(slotCliente - 1);
        return tablaPeers[idx].slotOcupado ? tablaPeers[idx].identidad : identidadVacia;
    }
};

#endif