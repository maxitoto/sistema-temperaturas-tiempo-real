#pragma once
#include "../UART/UartPhy.hpp"
#include <Preferences.h>
#include <queue>
#include <map>
#include <functional>
#include <string.h>

namespace Connections::UartS {

enum class FrameType : uint8_t {
    SYN = 0x01,
    SYN_ACK = 0x02,
    DATA_RELIABLE = 0x03,
    DATA_STREAM = 0x04,
    ACK = 0x05,
    NACK = 0x06,
    HEARTBEAT = 0x07,
    DISC = 0x08
};

struct ReliabilityConfig {
    uint32_t ackTimeoutMs = 250;
    uint8_t maxRetries = 6;
    size_t maxChunkSize = 240;
    uint32_t reassemblyTimeoutMs = 1500;

    bool validate(uint32_t linkTimeoutMs = 2800) const {
        return ackTimeoutMs >= 50 &&
               maxRetries >= 1 &&
               (ackTimeoutMs * maxRetries) <= linkTimeoutMs &&
               maxChunkSize >= 32 && maxChunkSize <= 240;
    }
};

#pragma pack(push, 1)
struct FrameHeader {
    FrameType type;
    uint8_t seq;
    uint8_t msgId;
    uint8_t chunkIdx;
    uint8_t totalChunks;
    uint8_t payloadLen;
};
#pragma pack(pop)

class UartReliability {
public:
    using MessageReadyCb = std::function<void(FrameType type, const std::vector<uint8_t>& message)>;

    explicit UartReliability(UartPhy& phy) : phy_(phy) {}

    void loadConfig() {
        // cargar de base de datos
        Preferences prefs;
        if (prefs.begin("conn_rel", true)) {
            cfg_.ackTimeoutMs = prefs.getUInt("ackMs", 250);
            cfg_.maxRetries = prefs.getUChar("retries", 6);
            cfg_.maxChunkSize = prefs.getUInt("chunkSz", 240);
            prefs.end();
        }
        if (!cfg_.validate()) {
            cfg_ = ReliabilityConfig{};
        }
    }

    bool applyConfig(const ReliabilityConfig& in, uint32_t linkTimeoutMs = 2800) {
        if (!in.validate(linkTimeoutMs)) return false;
        // zona base de datos
        Preferences prefs;
        if (prefs.begin("conn_rel", false)) {
            prefs.putUInt("ackMs", in.ackTimeoutMs);
            prefs.putUChar("retries", in.maxRetries);
            prefs.putUInt("chunkSz", static_cast<uint32_t>(in.maxChunkSize));
            prefs.end();
        }
        cfg_ = in;
        return true;
    }

    const ReliabilityConfig& getConfig() const { return cfg_; }

    void resetSession() {
        waitingAck_ = false;
        currentRetries_ = 0;
        lastRxSeq_ = 0xFF;
        while (!txQueue_.empty()) txQueue_.pop();
        reassemblyBuffers_.clear();
    }

    void setOnMessageReady(MessageReadyCb cb) { onMessageReady_ = std::move(cb); }

    bool enqueueMessage(FrameType type, const uint8_t* data, size_t len) {
        size_t totalChunks = (len == 0) ? 1 : ((len + cfg_.maxChunkSize - 1) / cfg_.maxChunkSize);
        if (totalChunks > 255) return false; // Evita truncamiento por desborde de uint8_t

        uint8_t msgId = ++nextMsgId_;
        for (size_t i = 0; i < totalChunks; ++i) {
            size_t offset = i * cfg_.maxChunkSize;
            size_t chunkLen = (len > offset) ? std::min(cfg_.maxChunkSize, len - offset) : 0;

            PendingFrame pf;
            pf.header.type = type;
            pf.header.seq = ++nextSeq_;
            pf.header.msgId = msgId;
            pf.header.chunkIdx = static_cast<uint8_t>(i);
            pf.header.totalChunks = static_cast<uint8_t>(totalChunks);
            pf.header.payloadLen = static_cast<uint8_t>(chunkLen);
            pf.requiresAck = (type == FrameType::DATA_RELIABLE ||
                              type == FrameType::SYN ||
                              type == FrameType::SYN_ACK ||
                              type == FrameType::HEARTBEAT);

            if (chunkLen > 0) {
                pf.payload.assign(data + offset, data + offset + chunkLen);
            }
            txQueue_.push(std::move(pf));
        }
        return true;
    }

    bool poll() {
        std::vector<uint8_t> raw;
        while (phy_.pollFrame(raw)) {
            processIncomingFrame(raw);
        }

        uint32_t now = millis();
        for (auto it = reassemblyBuffers_.begin(); it != reassemblyBuffers_.end(); ) {
            if (now >= it->second.startedMs && (now - it->second.startedMs > cfg_.reassemblyTimeoutMs)) {
                it = reassemblyBuffers_.erase(it);
            } else {
                ++it;
            }
        }

        if (waitingAck_) {
            if (now - lastTxTime_ >= cfg_.ackTimeoutMs) {
                if (currentRetries_ >= cfg_.maxRetries) {
                    waitingAck_ = false;
                    return false;
                }
                currentRetries_++;
                transmitFrame(inFlight_);
            }
        } else if (!txQueue_.empty()) {
            inFlight_ = txQueue_.front();
            txQueue_.pop();
            currentRetries_ = 0;
            transmitFrame(inFlight_);
            waitingAck_ = inFlight_.requiresAck;
        }
        return true;
    }

private:
    struct PendingFrame {
        FrameHeader header;
        std::vector<uint8_t> payload;
        bool requiresAck = true;
    };

    struct ReassemblyEntry {
        std::vector<uint8_t> data;
        uint8_t count = 0;
        uint32_t startedMs = 0;
    };

    UartPhy& phy_;
    ReliabilityConfig cfg_;
    MessageReadyCb onMessageReady_ = nullptr;
    std::queue<PendingFrame> txQueue_;
    PendingFrame inFlight_;
    bool waitingAck_ = false;
    uint32_t lastTxTime_ = 0;
    uint8_t currentRetries_ = 0;

    uint8_t nextSeq_ = 0;
    uint8_t nextMsgId_ = 0;
    uint8_t lastRxSeq_ = 0xFF;
    uint32_t lastSynRxMs_ = 0;

    std::map<uint8_t, ReassemblyEntry> reassemblyBuffers_;

    void transmitFrame(const PendingFrame& pf) {
        std::vector<uint8_t> buf(sizeof(FrameHeader) + pf.payload.size() + 2);
        memcpy(buf.data(), &pf.header, sizeof(FrameHeader));
        if (!pf.payload.empty()) {
            memcpy(buf.data() + sizeof(FrameHeader), pf.payload.data(), pf.payload.size());
        }
        uint16_t crc = crc16CcittFalse(buf.data(), sizeof(FrameHeader) + pf.payload.size());
        buf[buf.size() - 2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
        buf[buf.size() - 1] = static_cast<uint8_t>(crc & 0xFF);

        phy_.sendFrame(buf.data(), buf.size());
        lastTxTime_ = millis();
    }

    void sendControlAck(FrameType ackType, uint8_t seq) {
        PendingFrame ctrl{};
        ctrl.header.type = ackType;
        ctrl.header.seq = seq;
        ctrl.requiresAck = false;
        transmitFrame(ctrl);
    }

    void processIncomingFrame(const std::vector<uint8_t>& raw) {
        if (raw.size() < sizeof(FrameHeader) + 2) return;

        size_t dataLen = raw.size() - 2;
        uint16_t rxCrc = (static_cast<uint16_t>(raw[dataLen]) << 8) | raw[dataLen + 1];
        if (crc16CcittFalse(raw.data(), dataLen) != rxCrc) return;

        FrameHeader hdr;
        memcpy(&hdr, raw.data(), sizeof(FrameHeader));
        if (hdr.payloadLen != (dataLen - sizeof(FrameHeader))) return;

        if (hdr.type == FrameType::ACK) {
            if (waitingAck_ && hdr.seq == inFlight_.header.seq) {
                waitingAck_ = false;
            }
            return;
        }

        uint32_t now = millis();
        if (hdr.type == FrameType::SYN) {
            sendControlAck(FrameType::ACK, hdr.seq);
            if (hdr.seq == lastRxSeq_ && (now - lastSynRxMs_ < 400)) return;
            lastSynRxMs_ = now;
            resetSession();
            lastRxSeq_ = hdr.seq;
        } else if (hdr.type == FrameType::SYN_ACK ||
                   hdr.type == FrameType::DATA_RELIABLE ||
                   hdr.type == FrameType::HEARTBEAT) {
            sendControlAck(FrameType::ACK, hdr.seq);
            if (hdr.seq == lastRxSeq_) return;
            lastRxSeq_ = hdr.seq;
        }

        if (hdr.chunkIdx == 0) {
            reassemblyBuffers_[hdr.msgId] = ReassemblyEntry{{}, 0, now};
        }
        auto it = reassemblyBuffers_.find(hdr.msgId);
        if (it == reassemblyBuffers_.end()) return;

        const uint8_t* chunkPtr = raw.data() + sizeof(FrameHeader);
        it->second.data.insert(it->second.data.end(), chunkPtr, chunkPtr + hdr.payloadLen);
        it->second.count++;

        if (it->second.count >= hdr.totalChunks) {
            std::vector<uint8_t> completedMsg = std::move(it->second.data);
            reassemblyBuffers_.erase(it);
            if (onMessageReady_) onMessageReady_(hdr.type, completedMsg);
        }
    }

    static uint16_t crc16CcittFalse(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (uint8_t b = 0; b < 8; ++b) {
                crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
            }
        }
        return crc;
    }
};

} // namespace Connections::UartS