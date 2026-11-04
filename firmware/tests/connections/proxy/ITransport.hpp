#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <functional>

namespace Connections {

enum class MediumType : uint8_t {
    NONE = 0,
    UART_S = 1,
    WIFI = 2
};

enum class TransportState : uint8_t {
    DISCONNECTED = 0,
    PHY_LINKED = 1,
    HANDSHAKING = 2,
    AUTHENTICATED = 3,
    CLOSED_IDLE = 4,
    ERROR_RECOVERY = 5,
    CLOSING = 6
};

enum class DisconnectReason : uint8_t {
    NONE = 0,
    USER_CLOSE = 1,
    IDLE_CLOSE = 2,
    IDLE_NO_AGREEMENT = 3,
    LINK_TIMEOUT = 4,
    ACK_FAIL = 5,
    DISC_RX = 6,
    PORT_CLOSED = 7,
    HANDSHAKE_TIMEOUT = 8
};

struct EndpointId {
    MediumType medium = MediumType::NONE;
    uint32_t peerId = 0;

    bool operator==(const EndpointId& o) const {
        return medium == o.medium && peerId == o.peerId;
    }
    bool operator<(const EndpointId& o) const {
        return (medium != o.medium) ? (medium < o.medium) : (peerId < o.peerId);
    }
};

struct TransportPacket {
    EndpointId endpoint;
    bool isRealTime = false;
    std::vector<uint8_t> payload;
};

using RxCallback = std::function<void(const TransportPacket&)>;
using StateChangeCallback = std::function<void(MediumType, TransportState, DisconnectReason)>;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual void loadConfig() = 0;
    virtual bool init() = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual void closeIdle() { disconnect(); }
    virtual void poll() = 0;

    virtual bool sendReliable(uint32_t peerId, const uint8_t* data, size_t len) = 0;
    virtual bool sendStream(uint32_t peerId, const uint8_t* data, size_t len) = 0;

    virtual MediumType getMediumType() const = 0;
    virtual TransportState getState() const { return state_; }
    virtual DisconnectReason lastDisconnectReason() const { return lastReason_; }

    uint32_t sessionId() const { return sessionId_; }
    uint32_t negotiatedIdleMs() const { return negotiatedIdleMs_; }

    void setRxCallback(RxCallback cb) { onRx_ = std::move(cb); }
    void setStateChangeCallback(StateChangeCallback cb) { onStateChange_ = std::move(cb); }

protected:
    TransportState state_ = TransportState::DISCONNECTED;
    DisconnectReason lastReason_ = DisconnectReason::NONE;
    uint32_t sessionId_ = 0;
    uint32_t negotiatedIdleMs_ = 600000UL;
    RxCallback onRx_ = nullptr;
    StateChangeCallback onStateChange_ = nullptr;

    void setState(TransportState nextState, DisconnectReason reason = DisconnectReason::NONE) {
        if (reason != DisconnectReason::NONE) {
            lastReason_ = reason;
        }
        if (state_ != nextState) {
            state_ = nextState;
            if (onStateChange_) {
                onStateChange_(getMediumType(), state_, lastReason_);
            }
        }
    }
};

} // namespace Connections