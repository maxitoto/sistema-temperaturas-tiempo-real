#pragma once
#include <stdint.h>
#include <string>
#include "../../connections/interfaces/WiFi/WiFiTransport.hpp"

namespace Apis::Conexion {

using NetworkStatusDto = Connections::Wifi::NetworkStatusDto;

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

} // namespace Apis::Conexion