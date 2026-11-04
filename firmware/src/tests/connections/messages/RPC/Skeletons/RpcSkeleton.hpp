#pragma once
#include "../DTOs/RpcDto.hpp"
#include "../../../proxy/TransportProxy.hpp"
#include <map>
#include <tuple>
#include <functional>

namespace Messages::Rpc {

template <typename TIn, typename TOut>
class StreamContext {
public:
    StreamContext(uint32_t clientId, const RpcEnvelope& reqEnv, Connections::TransportProxy& proxy)
        : clientId_(clientId), env_(reqEnv), proxy_(proxy) {}

    uint32_t clientId() const { return clientId_; }
    uint32_t streamId() const { return env_.streamId; }
    bool isEndOfStream() const { return (env_.flags & Flags::EOS) != 0; }

    TIn request() const {
        TIn data{};
        std::tuple<TIn> tup{};
        if (MsgPack::deserialize(env_.payload, tup)) {
            return std::get<0>(tup);
        }
        MsgPack::deserialize(env_.payload, data);
        return data;
    }

    bool emit(const TOut& item, bool realTimeOverride = false) {
        RpcEnvelope outEnv;
        outEnv.type = PacketType::StreamChunk;
        outEnv.flow = env_.flow;
        outEnv.semantics = env_.semantics;
        bool useRt = realTimeOverride || (env_.semantics == RpcSemantics::Maybe) || ((env_.flags & Flags::REAL_TIME) != 0);
        outEnv.flags = useRt ? Flags::REAL_TIME : Flags::NONE;
        outEnv.callId = env_.callId;
        outEnv.streamId = env_.streamId;
        outEnv.methodId = env_.methodId;
        outEnv.payload = MsgPack::serialize(item);

        auto raw = MsgPack::packEnvelope(outEnv);
        return proxy_.routeToClient(clientId_, raw.data(), raw.size(), useRt);
    }

    bool close(const TOut* finalResponse = nullptr) {
        RpcEnvelope outEnv;
        outEnv.type = PacketType::StreamEnd;
        outEnv.flow = env_.flow;
        outEnv.semantics = env_.semantics;
        outEnv.flags = Flags::EOS;
        outEnv.callId = env_.callId;
        outEnv.streamId = env_.streamId;
        outEnv.methodId = env_.methodId;
        if (finalResponse) {
            outEnv.payload = MsgPack::serialize(*finalResponse);
        }
        auto raw = MsgPack::packEnvelope(outEnv);
        return proxy_.routeToClient(clientId_, raw.data(), raw.size(), false);
    }

private:
    uint32_t clientId_;
    RpcEnvelope env_;
    Connections::TransportProxy& proxy_;
};

class RpcSkeleton {
public:
    using RawHandler = std::function<std::vector<uint8_t>(uint32_t clientId, const RpcEnvelope& env)>;

    explicit RpcSkeleton(Connections::TransportProxy& proxy) : proxy_(proxy) {
        // Limpiar caché AtMostOnce al caer o cerrar la sesión
        proxy_.onMediumDisconnected([this](uint32_t clientId, uint32_t, Connections::MediumType, Connections::DisconnectReason) {
            clearClientSession(clientId);
        });
    }

    void loadConfig() {
        // cargar de base de datos
        maxHistoryEntries_ = 64;
    }

    bool applyConfig(size_t maxEntries) {
        if (maxEntries < 8 || maxEntries > 512) return false;
        // zona base de datos
        maxHistoryEntries_ = maxEntries;
        return true;
    }

    size_t maxHistoryEntries() const { return maxHistoryEntries_; }

    void clearClientSession(uint32_t clientId) {
        for (auto it = responseHistory_.begin(); it != responseHistory_.end(); ) {
            if (std::get<0>(it->first) == clientId) {
                it = responseHistory_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void bindRaw(uint32_t methodId, RpcFlow flow, RpcSemantics defaultSemantics, RawHandler handler) {
        routes_[methodId] = {flow, defaultSemantics, std::move(handler)};
    }

    void onIncomingPacket(uint32_t clientId, uint32_t sessionId, const RpcEnvelope& env) {
        auto it = routes_.find(env.methodId);
        if (it == routes_.end()) {
            sendError(clientId, env, "Endpoint no registrado");
            return;
        }

        // Clave de deduplicación AtMostOnce aislada por (clientId, sessionId, callId, methodId)
        DedupKey key{clientId, sessionId, env.callId, env.methodId};
        if (env.semantics == RpcSemantics::AtMostOnce && env.type == PacketType::Request) {
            auto histIt = responseHistory_.find(key);
            if (histIt != responseHistory_.end()) {
                if (!histIt->second.inProgress && !histIt->second.rawResponse.empty()) {
                    proxy_.routeToClient(clientId, histIt->second.rawResponse.data(), histIt->second.rawResponse.size(), false);
                }
                return;
            }
            if (responseHistory_.size() >= maxHistoryEntries_) {
                responseHistory_.erase(responseHistory_.begin());
            }
            responseHistory_[key] = HistoryEntry{true, {}};
        }

        std::vector<uint8_t> resultPayload = it->second.handler(clientId, env);

        if (env.flow == RpcFlow::Unary ||
           (env.flow == RpcFlow::ClientStream && (env.flags & Flags::EOS))) {
            RpcEnvelope resp;
            resp.type = PacketType::Response;
            resp.flow = env.flow;
            resp.semantics = env.semantics;
            resp.flags = Flags::EOS;
            resp.callId = env.callId;
            resp.streamId = env.streamId;
            resp.methodId = env.methodId;
            resp.payload = std::move(resultPayload);

            bool isRt = (env.semantics == RpcSemantics::Maybe) || ((env.flags & Flags::REAL_TIME) != 0);
            auto rawResp = MsgPack::packEnvelope(resp);

            if (env.semantics == RpcSemantics::AtMostOnce) {
                responseHistory_[key] = HistoryEntry{false, rawResp};
            }

            proxy_.routeToClient(clientId, rawResp.data(), rawResp.size(), isRt);
        }
    }

private:
    using DedupKey = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;

    struct HistoryEntry {
        bool inProgress = false;
        std::vector<uint8_t> rawResponse;
    };

    struct RouteEntry {
        RpcFlow flow;
        RpcSemantics semantics;
        RawHandler handler;
    };

    Connections::TransportProxy& proxy_;
    std::map<uint32_t, RouteEntry> routes_;
    std::map<DedupKey, HistoryEntry> responseHistory_;
    size_t maxHistoryEntries_ = 64;

    void sendError(uint32_t clientId, const RpcEnvelope& req, const std::string& msg) {
        RpcEnvelope err;
        err.type = PacketType::Error;
        err.callId = req.callId;
        err.streamId = req.streamId;
        err.methodId = req.methodId;
        err.payload = MsgPack::serialize(msg);
        auto raw = MsgPack::packEnvelope(err);
        proxy_.routeToClient(clientId, raw.data(), raw.size(), false);
    }
};

} // namespace Messages::Rpc