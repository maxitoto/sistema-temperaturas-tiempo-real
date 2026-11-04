#pragma once
#include "conexionDTO.hpp"
#include "../../connections/CommunicationContainer.hpp"

namespace Apis::Conexion {

inline void registrarConexionApi() {
    auto& comm = Connections::comm;

    // 1. Estado de red completo (IP DHCP, MAC real, bootId, resetReason y canal)
    comm.srv.bind("network.get_status", [&comm]() -> NetworkStatusDto {
        return comm.wifi.getNetworkStatus();
    });

    // 2. Configurar STA (2 args) de forma asíncrona y persistida
    comm.srv.bind("network.configure_sta", [&comm](std::string ssid, std::string password) -> NetworkStatusDto {
        return comm.wifi.reconfigureWifi(ssid, password);
    });

    // 3. Configurar STA + AP (4 args) de forma asíncrona, diferida y persistida
    comm.srv.bind("network.configure", [&comm](std::string staSsid, std::string staPass, std::string apSsid, std::string apPass) -> NetworkStatusDto {
        return comm.wifi.reconfigureWifi(staSsid, staPass, apSsid, apPass);
    });

    // 4. Configurar parámetros de liveness e inactividad del Proxy
    comm.srv.bind("link.configure", [&comm](uint32_t probeMs, uint32_t linkMs, uint32_t idleMs, uint32_t graceMs) -> bool {
        Connections::LinkConfig cfg = comm.proxy.getConfig();
        cfg.probeIntervalMs = probeMs;
        cfg.linkTimeoutMs = linkMs;
        cfg.idleTimeoutMs = idleMs;
        cfg.gracefulWaitMs = graceMs;
        bool ok = comm.proxy.applyConfig(cfg);
        if (ok) {
            auto net = comm.wifi.getNetworkStatus();
            comm.uartS.setBootMetadata(net.bootId, net.resetReason, cfg.idleTimeoutMs);
            comm.wifi.setBootMetadata(net.bootId, net.resetReason, cfg.idleTimeoutMs);
        }
        return ok;
    });

    // 5. Configurar parámetros de fiabilidad en UART-S
    comm.srv.bind("reliability.configure", [&comm](uint32_t ackMs, uint8_t retries, uint32_t chunkSz) -> bool {
        Connections::UartS::ReliabilityConfig rcfg = comm.uartS.reliability().getConfig();
        rcfg.ackTimeoutMs = ackMs;
        rcfg.maxRetries = retries;
        rcfg.maxChunkSize = chunkSz;
        return comm.uartS.reliability().applyConfig(rcfg, comm.proxy.getConfig().linkTimeoutMs);
    });

    // 6. Configurar parámetros de la capa RPC (Skeleton + Stub)
    comm.srv.bind("rpc.configure", [&comm](uint32_t maxHist, uint32_t /*ttlMs*/, uint32_t retryMs, uint8_t maxRetries) -> bool {
        bool ok1 = comm.srv.skeleton().applyConfig(maxHist);
        bool ok2 = comm.c.stub().applyConfig(retryMs, maxRetries);
        return ok1 && ok2;
    });

    // 7. Lectura consolidada de configuración activa sin exponer claves
    comm.srv.bind("config.get", [&comm]() -> FirmwareConfigSnapshotDto {
        const auto& w = comm.wifi.getConfig();
        const auto& l = comm.proxy.getConfig();
        const auto& r = comm.uartS.reliability().getConfig();
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
            static_cast<uint32_t>(comm.srv.skeleton().maxHistoryEntries()),
            comm.c.stub().retryTimeoutMs(),
            comm.c.stub().maxRpcRetries()
        };
    });
}

} // namespace Apis::Conexion