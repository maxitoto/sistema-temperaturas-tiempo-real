#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include "proxy/TransportProxy.hpp"
#include "interfaces/UART-S/UartSTransport.hpp"
#include "interfaces/WiFi/WiFiTransport.hpp"
#include "messages/RPC/RpcApp.hpp"

struct SensorStreamRequestDto {
    uint8_t sensorId;
    uint8_t sampleCount;
};

struct TemperatureReadingDto {
    uint8_t sensorId;
    float celsius;
    uint32_t timestampMs;
};

struct FirmwareConfigSnapshotDto {
    std::string staSsid;
    std::string apSsid;
    uint8_t apChannel;
    uint32_t probeIntervalMs;
    uint32_t linkTimeoutMs;
    uint32_t idleTimeoutMs;
    uint32_t gracefulWaitMs;
    uint32_t ackTimeoutMs;
    uint8_t maxRetries;
    uint32_t maxChunkSize;
    uint32_t maxHistoryEntries;
    uint32_t rpcRetryTimeoutMs;
    uint8_t rpcMaxRetries;
};

Connections::TransportProxy proxy;
Connections::UartS::UartSTransport uartS(Serial);
Connections::Wifi::WifiTransport wifi;

rpc::server srv(proxy);
rpc::client c(proxy, &srv);

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

void setup() {
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

    // 1. Sumar dos enteros
    srv.bind("add", [](int a, int b) {
        return a + b;
    });

    // 2. Echo ("hola" -> "hola")
    srv.bind("system.echo", [](std::string msg) {
        return msg;
    });

    // 3. Estado de red completo (con IP DHCP, MAC real, bootId, resetReason y canal)
    srv.bind("network.get_status", []() {
        return wifi.getNetworkStatus();
    });

    // 4. Configurar STA (2 args) de forma asíncrona y persistida en NVS
    srv.bind("network.configure_sta", [](std::string ssid, std::string password) {
        return wifi.reconfigureWifi(ssid, password);
    });

    // 5. Configurar STA + AP (4 args) de forma asíncrona, diferida y persistida en NVS
    srv.bind("network.configure", [](std::string staSsid, std::string staPass, std::string apSsid, std::string apPass) {
        return wifi.reconfigureWifi(staSsid, staPass, apSsid, apPass);
    });

    // 6. Configurar parámetros de liveness e inactividad del Proxy
    srv.bind("link.configure", [](uint32_t probeMs, uint32_t linkMs, uint32_t idleMs, uint32_t graceMs) {
        Connections::LinkConfig cfg = proxy.getConfig();
        cfg.probeIntervalMs = probeMs;
        cfg.linkTimeoutMs = linkMs;
        cfg.idleTimeoutMs = idleMs;
        cfg.gracefulWaitMs = graceMs;
        bool ok = proxy.applyConfig(cfg);
        if (ok) {
            uartS.setBootMetadata(wifi.getNetworkStatus().bootId, wifi.getNetworkStatus().resetReason, cfg.idleTimeoutMs);
            wifi.setBootMetadata(wifi.getNetworkStatus().bootId, wifi.getNetworkStatus().resetReason, cfg.idleTimeoutMs);
        }
        return ok;
    });

    // 7. Configurar parámetros de UartReliability
    srv.bind("reliability.configure", [](uint32_t ackMs, uint8_t retries, uint32_t chunkSz) {
        Connections::UartS::ReliabilityConfig rcfg = uartS.reliability().getConfig();
        rcfg.ackTimeoutMs = ackMs;
        rcfg.maxRetries = retries;
        rcfg.maxChunkSize = chunkSz;
        return uartS.reliability().applyConfig(rcfg, proxy.getConfig().linkTimeoutMs);
    });

    // 8. Server Streaming de temperaturas en Tiempo Real
    srv.bind_stream<SensorStreamRequestDto, TemperatureReadingDto>(
        "sensors.watch_temperature",
        [](auto& ctx) {
            SensorStreamRequestDto req = ctx.request();
            uint8_t count = (req.sampleCount == 0 || req.sampleCount > 10) ? 5 : req.sampleCount;
            for (uint8_t i = 0; i < count; ++i) {
                ctx.emit(TemperatureReadingDto{
                    req.sensorId,
                    22.5f + (i * 0.4f),
                    millis()
                }, true);
            }
            ctx.close();
        }
    );

    srv.bind("rpc.configure", [](uint32_t maxHist, uint32_t /*ttlMs*/, uint32_t retryMs, uint8_t maxRetries) {
        bool ok1 = srv.skeleton().applyConfig(maxHist);
        bool ok2 = c.stub().applyConfig(retryMs, maxRetries);
        return ok1 && ok2;
    });

    srv.bind("config.get", []() {
        const auto& w = wifi.getConfig();
        const auto& l = proxy.getConfig();
        const auto& r = uartS.reliability().getConfig();
        return FirmwareConfigSnapshotDto{
            w.staSsid,
            w.apSsid,
            w.apChannel,
            l.probeIntervalMs,
            l.linkTimeoutMs,
            l.idleTimeoutMs,
            l.gracefulWaitMs,
            r.ackTimeoutMs,
            r.maxRetries,
            static_cast<uint32_t>(r.maxChunkSize),
            static_cast<uint32_t>(srv.skeleton().maxHistoryEntries()),
            c.stub().retryTimeoutMs(),
            c.stub().maxRpcRetries()
        };
    });
}

void loop() {
    srv.run();
    delay(2);
}