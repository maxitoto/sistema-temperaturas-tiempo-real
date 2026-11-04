#pragma once
#include "../DTOs/RpcDto.hpp"
#include "../../../proxy/TransportProxy.hpp"
#include <Arduino.h>
#include <map>
#include <functional>

namespace Messages::Rpc {

class RpcStub {
public:
    using RawCallback = std::function<void(const std::vector<uint8_t>&, bool isEos)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    explicit RpcStub(Connections::TransportProxy& proxy) : proxy_(proxy) {
        // Notificar y limpiar llamadas/streams colgados cuando un medio se desconecta
        proxy_.onMediumDisconnected([this](uint32_t disconnectedClientId, uint32_t, Connections::MediumType, Connections::DisconnectReason) {
            for (auto it = pendingCalls_.begin(); it != pendingCalls_.end(); ) {
                if (it->second.clientId == disconnectedClientId) {
                    if (it->second.onError) it->second.onError("Medio desconectado");
                    it = pendingCalls_.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = streamListeners_.begin(); it != streamListeners_.end(); ) {
                if (it->second.clientId == disconnectedClientId) {
                    it = streamListeners_.erase(it);
                } else {
                    ++it;
                }
            }
        });
    }

    void loadConfig() {
        // cargar de base de datos
        retryTimeoutMs_ = 2500;
        maxRpcRetries_ = 2;
    }

    bool applyConfig(uint32_t retryTimeoutMs, uint8_t maxRetries) {
        if (retryTimeoutMs < 200 || retryTimeoutMs > 30000 || maxRetries > 10) return false;
        // zona base de datos
        retryTimeoutMs_ = retryTimeoutMs;
        maxRpcRetries_ = maxRetries;
        return true;
    }

    uint32_t retryTimeoutMs() const { return retryTimeoutMs_; }
    uint8_t maxRpcRetries() const { return maxRpcRetries_; }

    bool hasActiveOperations(uint32_t clientId) const {
        for (const auto& kv : pendingCalls_) {
            if (kv.second.clientId == clientId) return true;
        }
        for (const auto& kv : streamListeners_) {
            if (kv.second.clientId == clientId) return true;
        }
        return false;
    }

    uint32_t invoke(uint32_t clientId, std::string_view endpointName, RpcFlow flow,
                    RpcSemantics semantics, const std::vector<uint8_t>& payload,
                    bool isEos, RawCallback onData, ErrorCallback onError, uint32_t existingStreamId = 0) {
        uint32_t ticketId = ++nextCallId_;
        uint32_t streamId = (existingStreamId != 0) ? existingStreamId : (++nextStreamId_);
        PacketType pktType = (flow == RpcFlow::Unary || existingStreamId == 0) ? PacketType::Request : PacketType::StreamChunk;
        bool isRt = (semantics == RpcSemantics::Maybe) && (pktType != PacketType::Request);

        RpcEnvelope env;
        env.type = pktType;
        env.flow = flow;
        env.semantics = semantics;
        env.flags = (isEos ? Flags::EOS : Flags::NONE) | (isRt ? Flags::REAL_TIME : Flags::NONE);
        env.callId = ticketId;
        env.streamId = streamId;
        env.methodId = hashEndpoint(endpointName);
        env.payload = payload;

        auto raw = MsgPack::packEnvelope(env);
        if (!proxy_.routeToClient(clientId, raw.data(), raw.size(), isRt)) {
            if (onError) onError("Sin ruta autenticada hacia el cliente");
            return ticketId;
        }

        if (onData || onError) {
            PendingCall call;
            call.clientId = clientId;
            call.callId = ticketId;
            call.streamId = streamId;
            call.semantics = semantics;
            call.rawPacket = std::move(raw);
            call.lastSentMs = millis();
            call.retriesLeft = (semantics == RpcSemantics::Maybe) ? 0 : maxRpcRetries_;
            call.onData = std::move(onData);
            call.onError = std::move(onError);
            pendingCalls_[ticketId] = std::move(call);
        }

        if (flow != RpcFlow::Unary && pendingCalls_.count(ticketId) > 0) {
            streamListeners_[streamId] = {clientId, pendingCalls_[ticketId].onData};
        }
        return ticketId;
    }

    void onIncomingResponse(uint32_t sourceClientId, const RpcEnvelope& env) {
        if (env.type == PacketType::StreamChunk || env.type == PacketType::StreamEnd) {
            auto stIt = streamListeners_.find(env.streamId);
            if (stIt != streamListeners_.end() && stIt->second.onData) {
                if (stIt->second.clientId != 0 && stIt->second.clientId != sourceClientId) return;
                bool eos = (env.type == PacketType::StreamEnd) || ((env.flags & Flags::EOS) != 0);
                stIt->second.onData(env.payload, eos);
                if (eos) streamListeners_.erase(stIt);
            }
            pendingCalls_.erase(env.callId);
            return;
        }

        auto it = pendingCalls_.find(env.callId);
        if (it == pendingCalls_.end()) return;
        // Validar que la respuesta provenga del mismo medio por el que salió el ticket
        if (it->second.clientId != 0 && it->second.clientId != sourceClientId) return;

        PendingCall completed = std::move(it->second);
        pendingCalls_.erase(it);

        if (env.type == PacketType::Error) {
            if (completed.onError) {
                std::string errMsg;
                MsgPack::deserialize(env.payload, errMsg);
                completed.onError(errMsg);
            }
        } else if (completed.onData) {
            completed.onData(env.payload, true);
        }
    }

    void pollRetries() {
        uint32_t now = millis();
        for (auto it = pendingCalls_.begin(); it != pendingCalls_.end(); ) {
            auto& call = it->second;
            if (now - call.lastSentMs >= retryTimeoutMs_) {
                if (call.retriesLeft > 0) {
                    call.retriesLeft--;
                    call.lastSentMs = now;
                    proxy_.routeToClient(call.clientId, call.rawPacket.data(), call.rawPacket.size(), false);
                    ++it;
                } else {
                    if (call.onError) call.onError("RPC Timeout agotado");
                    it = pendingCalls_.erase(it);
                }
            } else {
                ++it;
            }
        }
    }

private:
    struct PendingCall {
        uint32_t clientId = 0;
        uint32_t callId = 0;
        uint32_t streamId = 0;
        RpcSemantics semantics = RpcSemantics::AtMostOnce;
        std::vector<uint8_t> rawPacket;
        uint32_t lastSentMs = 0;
        uint8_t retriesLeft = 0;
        RawCallback onData = nullptr;
        ErrorCallback onError = nullptr;
    };

    struct StreamListener {
        uint32_t clientId = 0;
        RawCallback onData = nullptr;
    };

    Connections::TransportProxy& proxy_;
    std::map<uint32_t, PendingCall> pendingCalls_;
    std::map<uint32_t, StreamListener> streamListeners_;
    uint32_t nextCallId_ = 0;
    uint32_t nextStreamId_ = 0;
    uint32_t retryTimeoutMs_ = 2500;
    uint8_t maxRpcRetries_ = 2;
};

} // namespace Messages::Rpc