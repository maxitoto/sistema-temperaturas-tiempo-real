#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include "proxy/TransportProxy.hpp"
#include "interfaces/UART-S/UartSTransport.hpp"
#include "interfaces/WiFi/WiFiTransport.hpp"
#include "messages/RPC/RpcApp.hpp"

namespace Connections {

class CommunicationContainer {
public:
    TransportProxy proxy;
    UartS::UartSTransport uartS;
    Wifi::WifiTransport wifi;
    rpc::server srv;
    rpc::client c;

    static CommunicationContainer& instance() {
        static CommunicationContainer inst;
        return inst;
    }

    void begin() {
        uint32_t bootId = loadAndIncrementBootId();
        uint8_t resetReason = static_cast<uint8_t>(esp_reset_reason());

        proxy.loadConfig();
        uartS.loadConfig();
        wifi.loadConfig();

        uartS.setBootMetadata(bootId, resetReason, proxy.getConfig().idleTimeoutMs);
        wifi.setBootMetadata(bootId, resetReason, proxy.getConfig().idleTimeoutMs);

        proxy.registerMedium(&uartS);
        proxy.registerMedium(&wifi);

        uartS.init();
        wifi.init();
    }

    void tick() {
        srv.run();
    }

private:
    CommunicationContainer()
        : uartS(Serial), srv(proxy), c(proxy, &srv) {}

    CommunicationContainer(const CommunicationContainer&) = delete;
    CommunicationContainer& operator=(const CommunicationContainer&) = delete;

    static uint32_t loadAndIncrementBootId() {
        // zona base de datos
        Preferences prefs;
        uint32_t bootId = 1;
        if (prefs.begin("conn_boot", false)) {
            bootId = prefs.getUInt("bootId", 0) + 1;
            prefs.putUInt("bootId", bootId);
            prefs.end();
        }
        return bootId;
    }
};

inline CommunicationContainer& comm = CommunicationContainer::instance();

} // namespace Connections