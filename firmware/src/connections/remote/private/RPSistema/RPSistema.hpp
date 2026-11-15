#ifndef RP_SISTEMA_H
#define RP_SISTEMA_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../middlewares/interfaces/WiFi/WiFiBase/WiFiInicializador.hpp"

#pragma once
#include <stdint.h>

struct ConfiguracionWiFiDTO {
    char ssidSta[32];   // 0..31
    char claveSta[64];  // 32..95
    char ssidAp[32];    // 96..127
    char claveAp[64];   // 128..191
};
static_assert(sizeof(ConfiguracionWiFiDTO) == 192, "ConfiguracionWiFiDTO debe medir 192 bytes");

struct ResConfiguracionWiFiDTO {
    char ssidSta[32];          // 0..31
    char ssidAp[32];           // 32..63
    char ipSta[16];            // 64..79
    char ipAp[16];             // 80..95
    uint8_t conectadoARouter;  // 96
    uint8_t puntoAccesoActivo; // 97
};
static_assert(sizeof(ResConfiguracionWiFiDTO) == 98, "ResConfiguracionWiFiDTO debe medir 98 bytes");

struct DiagnosticoSistemaDTO {
    uint32_t heapLibreBytes;
    uint32_t uptimeSegundos;
};

namespace RPSistema {

    inline auto reiniciarHardware = llamadoSemantico.exactamenteUnaVez<void, void>(
        "reiniciarHardware",
        []() {
            xTaskCreate([](void*) {
                vTaskDelay(pdMS_TO_TICKS(250));
                ESP.restart();
            }, "RebootTask", 2048, nullptr, 5, nullptr);
        }
    );

    inline auto actualizarConfiguracionWiFi = llamadoSemantico.exactamenteUnaVez<ConfiguracionWiFiDTO, ResConfiguracionWiFiDTO>(
        "actualizarConfiguracionWiFi",
        [](const ConfiguracionWiFiDTO& nuevaConfig) -> ResConfiguracionWiFiDTO {
            if (nuevaConfig.ssidAp[0] == '\0') {
                throw std::invalid_argument("ValueError: el SSID del Punto de Acceso no puede estar vacio");
            }
            const size_t lenClaveAp = strnlen(nuevaConfig.claveAp, sizeof(nuevaConfig.claveAp));
            if (lenClaveAp > 0 && lenClaveAp < 8) {
                throw std::invalid_argument("ValueError: la clave WPA2 del AP requiere al menos 8 caracteres");
            }

            if (WiFiInicializador::instanciaActiva == nullptr ||
                !WiFiInicializador::instanciaActiva->actualizarCredencialesYReconectar(
                    nuevaConfig.ssidSta, nuevaConfig.claveSta, nuevaConfig.ssidAp, nuevaConfig.claveAp)) {
                throw std::runtime_error("RuntimeError: fallo al reiniciar la interfaz WiFi con las nuevas credenciales");
            }

            DbRuntime::Repositories::ConfiguracionRedDTO cfgDb{};
            if (!DbRuntime::Repositories::RepoConfiguracionRed::obtenerOInicializar(cfgDb)) {
                throw std::runtime_error("RuntimeError: no se pudo leer ConfiguracionRed desde la base de datos");
            }

            const EstadoRedWiFi& estado = WiFiInicializador::instanciaActiva->obtenerEstadoRed();
            ResConfiguracionWiFiDTO salida{};
            strncpy(salida.ssidSta, cfgDb.ssid_sta, sizeof(salida.ssidSta) - 1);
            strncpy(salida.ssidAp,  cfgDb.ssid_ap,  sizeof(salida.ssidAp) - 1);
            strncpy(salida.ipSta,   estado.ipEstacionLAN, sizeof(salida.ipSta) - 1);
            strncpy(salida.ipAp,    estado.ipPuntoAcceso, sizeof(salida.ipAp) - 1);
            salida.conectadoARouter  = estado.conectadoARouter ? 1 : 0;
            salida.puntoAccesoActivo = estado.puntoAccesoActivo ? 1 : 0;
            return salida;
        }
    );

    inline auto obtenerConfiguracionWiFi = llamadoSemantico.alMenosUnaVez<void, ResConfiguracionWiFiDTO>(
        "obtenerConfiguracionWiFi",
        []() -> ResConfiguracionWiFiDTO {
            DbRuntime::Repositories::ConfiguracionRedDTO cfgDb{};
            if (!DbRuntime::Repositories::RepoConfiguracionRed::obtenerOInicializar(cfgDb)) {
                throw std::runtime_error("RuntimeError: no se pudo leer ConfiguracionRed desde la base de datos");
            }

            ResConfiguracionWiFiDTO salida{};
            strncpy(salida.ssidSta, cfgDb.ssid_sta, sizeof(salida.ssidSta) - 1);
            strncpy(salida.ssidAp,  cfgDb.ssid_ap,  sizeof(salida.ssidAp) - 1);

            if (WiFiInicializador::instanciaActiva != nullptr) {
                const EstadoRedWiFi& estado = WiFiInicializador::instanciaActiva->obtenerEstadoRed();
                strncpy(salida.ipSta, estado.ipEstacionLAN, sizeof(salida.ipSta) - 1);
                strncpy(salida.ipAp,  estado.ipPuntoAcceso, sizeof(salida.ipAp) - 1);
                salida.conectadoARouter  = estado.conectadoARouter ? 1 : 0;
                salida.puntoAccesoActivo = estado.puntoAccesoActivo ? 1 : 0;
            }
            return salida;
        }
    );

    inline auto ejecutarDiagnosticoSistema = llamadoSemantico.alMenosUnaVez<void, DiagnosticoSistemaDTO>(
        "ejecutarDiagnosticoSistema",
        []() -> DiagnosticoSistemaDTO {
            return DiagnosticoSistemaDTO{
                ESP.getFreeHeap(),
                static_cast<uint32_t>(millis() / 1000UL)
            };
        }
    );

} // namespace RPSistema

#endif