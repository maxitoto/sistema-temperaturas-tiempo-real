#pragma once
#include "../../proxy/ITransport.hpp"
#include "UART/UartPhy.hpp"
#include "Security/UartSecurity.hpp"
#include "Reliability/UartReliability.hpp"
#include <esp_system.h>

namespace Connections::UartS {

class UartSTransport : public ITransport {
public:
    static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 3500;

    explicit UartSTransport(HardwareSerial& serial)
        : phy_(serial), reliability_(phy_) {}

    void setBootMetadata(uint32_t bootId, uint8_t resetReason, uint32_t localIdleMs) {
        bootId_ = bootId;
        resetReason_ = resetReason;
        localIdleMs_ = localIdleMs;
    }

    UartReliability& reliability() { return reliability_; }

    void loadConfig() override {
        phy_.loadConfig();
        security_.loadConfig();
        reliability_.loadConfig();
    }

    bool init() override {
        reliability_.setOnMessageReady([this](FrameType type, const std::vector<uint8_t>& msg) {
            handleFrame(type, msg);
        });
        if (!phy_.begin()) return false;
        setState(TransportState::PHY_LINKED, DisconnectReason::NONE);
        return true;
    }

    bool connect() override {
        if (state_ == TransportState::DISCONNECTED && !init()) return false;
        setState(TransportState::HANDSHAKING, DisconnectReason::NONE);
        handshakeStartMs_ = millis();
        reliability_.resetSession();

        std::vector<uint8_t> pubKey;
        if (!security_.generateHandshakePublic(pubKey)) return false;
        reliability_.enqueueMessage(FrameType::SYN, pubKey.data(), pubKey.size());
        return true;
    }

    void disconnect() override {
        if (state_ == TransportState::AUTHENTICATED) {
            reliability_.enqueueMessage(FrameType::DISC, nullptr, 0);
            reliability_.poll();
        }
        reliability_.resetSession();
        setState(TransportState::PHY_LINKED, lastReason_);
    }

    void closeIdle() override {
        reliability_.resetSession();
        setState(TransportState::CLOSED_IDLE, DisconnectReason::IDLE_CLOSE);
    }

    void poll() override {
        if (state_ == TransportState::DISCONNECTED) return;

        if (state_ == TransportState::HANDSHAKING &&
            (millis() - handshakeStartMs_ > HANDSHAKE_TIMEOUT_MS)) {
            reliability_.resetSession();
            setState(TransportState::PHY_LINKED, DisconnectReason::HANDSHAKE_TIMEOUT);
            return;
        }

        if (!reliability_.poll()) {
            reliability_.resetSession();
            setState(TransportState::PHY_LINKED, DisconnectReason::ACK_FAIL);
        }
    }

    bool sendReliable(uint32_t peerId, const uint8_t* data, size_t len) override {
        if (state_ != TransportState::AUTHENTICATED) return false;
        std::vector<uint8_t> cipher;
        if (!security_.encrypt(data, len, cipher)) return false;
        return reliability_.enqueueMessage(FrameType::DATA_RELIABLE, cipher.data(), cipher.size());
    }

    bool sendStream(uint32_t peerId, const uint8_t* data, size_t len) override {
        if (state_ != TransportState::AUTHENTICATED) return false;
        std::vector<uint8_t> cipher;
        if (!security_.encrypt(data, len, cipher)) return false;
        return reliability_.enqueueMessage(FrameType::DATA_STREAM, cipher.data(), cipher.size());
    }

    MediumType getMediumType() const override { return MediumType::UART_S; }

private:
    UartPhy phy_;
    UartSecurity security_;
    UartReliability reliability_;
    uint32_t handshakeStartMs_ = 0;
    uint32_t bootId_ = 1;
    uint8_t resetReason_ = 0;
    uint32_t localIdleMs_ = 10UL * 60UL * 1000UL;

    static void writeU32BE(std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    static uint32_t readU32BE(const uint8_t* ptr) {
        return (static_cast<uint32_t>(ptr[0]) << 24) |
               (static_cast<uint32_t>(ptr[1]) << 16) |
               (static_cast<uint32_t>(ptr[2]) << 8) |
               static_cast<uint32_t>(ptr[3]);
    }

    void handleFrame(FrameType type, const std::vector<uint8_t>& msg) {
        if (type == FrameType::SYN) {
            if (msg.size() < 65) return;
            std::vector<uint8_t> myPub;
            if (security_.generateHandshakePublic(myPub) &&
                security_.computeSharedKey(msg.data(), 65, false, "uart")) {

                uint32_t peerSessionId = (msg.size() >= 69) ? readU32BE(msg.data() + 65) : esp_random();
                uint32_t peerIdleMs = (msg.size() >= 73) ? readU32BE(msg.data() + 69) : localIdleMs_;
                sessionId_ = (peerSessionId != 0) ? peerSessionId : esp_random();
                negotiatedIdleMs_ = std::min(localIdleMs_, peerIdleMs);

                // SYN_ACK: [myPub(65B) | sessionId(4B) | bootId(4B) | resetReason(1B) | negotiatedIdleMs(4B)] = 78B
                std::vector<uint8_t> synAckPayload = myPub;
                writeU32BE(synAckPayload, sessionId_);
                writeU32BE(synAckPayload, bootId_);
                synAckPayload.push_back(resetReason_);
                writeU32BE(synAckPayload, negotiatedIdleMs_);

                reliability_.enqueueMessage(FrameType::SYN_ACK, synAckPayload.data(), synAckPayload.size());
                setState(TransportState::AUTHENTICATED, DisconnectReason::NONE);
            }
        } else if (type == FrameType::SYN_ACK) {
            if (msg.size() >= 65 && security_.computeSharedKey(msg.data(), 65, true, "uart")) {
                if (msg.size() >= 69) sessionId_ = readU32BE(msg.data() + 65);
                if (msg.size() >= 78) negotiatedIdleMs_ = readU32BE(msg.data() + 74);
                setState(TransportState::AUTHENTICATED, DisconnectReason::NONE);
            }
        } else if (type == FrameType::DISC) {
            reliability_.resetSession();
            setState(TransportState::PHY_LINKED, DisconnectReason::DISC_RX);
        } else if (state_ != TransportState::AUTHENTICATED) {
            reliability_.enqueueMessage(FrameType::DISC, nullptr, 0);
        } else if (type == FrameType::DATA_RELIABLE || type == FrameType::DATA_STREAM) {
            std::vector<uint8_t> plain;
            if (security_.decrypt(msg.data(), msg.size(), plain) && onRx_) {
                TransportPacket pkt;
                pkt.endpoint = {MediumType::UART_S, 1};
                pkt.isRealTime = (type == FrameType::DATA_STREAM);
                pkt.payload = std::move(plain);
                onRx_(pkt);
            }
        }
    }
};

} // namespace Connections::UartS