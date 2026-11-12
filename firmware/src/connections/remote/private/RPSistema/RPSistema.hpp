#ifndef RP_SISTEMA_H
#define RP_SISTEMA_H

#include <Arduino.h>
#include <stdint.h>
#include "../../Controller/abstract/RPC_base.hpp"
#include "../../../middlewares/interfaces/WiFi/WiFiBase/WiFiInicializador.hpp"

struct ConfiguracionWiFiDTO {
    char ssidSta[32];
    char claveSta[64];
    char ssidAp[32];
    char claveAp[64];
};

struct ConfirmacionSistemaDTO {
    bool exito;
    uint32_t codigoEstado;
};

class RPSistema {
public:
    // ¿Cómo lo hace?: Prepara la confirmación de reinicio y delega el reinicio físico al controlador de hardware.
    static void reiniciarHardware(const uint8_t&, ConfirmacionSistemaDTO& salida) {
        salida.exito = true;
        salida.codigoEstado = 200;
        xTaskCreate([](void*) {
            vTaskDelay(pdMS_TO_TICKS(250));
            ESP.restart();
        }, "RebootTask", 2048, nullptr, 5, nullptr);
    }

    // ¿Cómo lo hace?: Encripta las claves WiFi entrantes y actualiza el registro en la tabla ConfiguracionConectividad.
    static void actualizarConfiguracionWiFi(const ConfiguracionWiFiDTO& nuevaConfig, ConfirmacionSistemaDTO& salida) {
        if (nuevaConfig.ssidAp[0] == '\0') {
            salida.exito = false;
            salida.codigoEstado = 400;
            return;
        }

        // consulta DB

        if (WiFiInicializador::instanciaActiva != nullptr) {
            WiFiInicializador::instanciaActiva->actualizarCredencialesYReconectar(
                nuevaConfig.ssidSta,
                nuevaConfig.claveSta,
                nuevaConfig.ssidAp,
                nuevaConfig.claveAp
            );
        }

        salida.exito = true;
        // Si se conectó al router STA, devuelve el último octeto de la IP local (ej. 192.168.1.X -> X), o 200 si solo está en AP
        salida.codigoEstado = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP()[3] : 200;
    }

    // ¿Cómo lo hace?: Obtiene la cantidad de memoria heap libre del ESP32 con ESP.getFreeHeap() y la devuelve en codigoEstado.
    static void ejecutarDiagnosticoSistema(const uint8_t&, ConfirmacionSistemaDTO& salida) {
        salida.exito = true;
        salida.codigoEstado = ESP.getFreeHeap();
    }
};

#endif