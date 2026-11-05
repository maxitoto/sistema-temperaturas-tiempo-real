#pragma once
#include "ITransport.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <map>
#include <queue>
#include <vector>

namespace Connections {

struct LinkConfig {
    uint32_t probeIntervalMs = 1000;
    uint32_t linkTimeoutMs = 2800;
    uint32_t idleTimeoutMs = 10UL * 60UL * 1000UL; // 10 minutos por defecto
    uint32_t gracefulWaitMs = 1000;
    size_t maxPacketsPerTick = 16;

    bool validate() const {
        return probeIntervalMs >= 250 &&
               linkTimeoutMs >= (probeIntervalMs * 2) &&
               idleTimeoutMs >= 5000 &&
               gracefulWaitMs >= 200 &&
               maxPacketsPerTick >= 1 && maxPacketsPerTick <= 64;
    }
};

struct ClientSession {
    uint32_t clientId = 0;
    MediumType medium = MediumType::NONE;
    uint32_t peerId = 1;
    uint32_t sessionId = 0;
    uint32_t lastRxMs = 0;
    uint32_t lastProbeTxMs = 0;
    uint32_t lastRpcActivityMs = 0;
    uint32_t negotiatedIdleMs = 10UL * 60UL * 1000UL;
    bool closingGracefully = false;
    bool pendingIdleClose = false;
    uint32_t gracefulCloseStartMs = 0;
    bool wasAuthenticated = false;
    DisconnectReason lastReason = DisconnectReason::NONE;
};

class TransportProxy {
public:
    static constexpr uint8_t PROXY_CTRL_MAGIC      = 0xF0;
    static constexpr uint8_t PROXY_PING            = 0x01;
    static constexpr uint8_t PROXY_PONG            = 0x02;
    static constexpr uint8_t PROXY_IDLE_CLOSE      = 0x03;
    static constexpr uint8_t PROXY_IDLE_CLOSE_ACK  = 0x04;
    static constexpr uint8_t PROXY_IDLE_CLOSE_NACK = 0x05;

    using RpcLayerHandler = std::function<void(uint32_t clientId, uint32_t sessionId, const std::vector<uint8_t>& data, bool isRealTime)>;
    using DisconnectListener = std::function<void(uint32_t clientId, uint32_t sessionId, MediumType medium, DisconnectReason reason)>;
    using BusyCheckCallback = std::function<bool(uint32_t clientId)>;

    void loadConfig() {
        // cargar de base de datos
        Preferences prefs;
        if (prefs.begin("conn_link", true)) {
            cfg_.probeIntervalMs = prefs.getUInt("probeMs", 1000);
            cfg_.linkTimeoutMs = prefs.getUInt("linkMs", 2800);
            cfg_.idleTimeoutMs = prefs.getUInt("idleMs", 10UL * 60UL * 1000UL);
            cfg_.gracefulWaitMs = prefs.getUInt("graceMs", 1000);
            cfg_.maxPacketsPerTick = prefs.getUInt("maxPkt", 16);
            prefs.end();
        }
        if (!cfg_.validate()) {
            cfg_ = LinkConfig{};
        }
    }

    bool applyConfig(const LinkConfig& in) {
        if (!in.validate()) return false;
        // zona base de datos
        Preferences prefs;
        if (prefs.begin("conn_link", false)) {
            prefs.putUInt("probeMs", in.probeIntervalMs);
            prefs.putUInt("linkMs", in.linkTimeoutMs);
            prefs.putUInt("idleMs", in.idleTimeoutMs);
            prefs.putUInt("graceMs", in.gracefulWaitMs);
            prefs.putUInt("maxPkt", static_cast<uint32_t>(in.maxPacketsPerTick));
            prefs.end();
        }
        cfg_ = in;
        for (auto& kv : sessions_) {
            kv.second.negotiatedIdleMs = cfg_.idleTimeoutMs;
        }
        return true;
    }

    const LinkConfig& getConfig() const { return cfg_; }

    void registerMedium(ITransport* transport) {
        if (!transport) return;
        MediumType m = transport->getMediumType();
        transports_[m] = transport;

        uint32_t cid = bindDefaultEndpoint(m, 1);
        ClientSession& sess = sessions_[m];
        sess.clientId = cid;
        sess.medium = m;
        sess.peerId = 1;
        sess.negotiatedIdleMs = cfg_.idleTimeoutMs;

        transport->setRxCallback([this](const TransportPacket& pkt) {
            uint32_t now = millis();
            MediumType med = pkt.endpoint.medium;
            auto it = sessions_.find(med);
            if (it == sessions_.end()) return;
            ClientSession& s = it->second;
            s.lastRxMs = now;

            if (pkt.payload.size() == 2 && pkt.payload[0] == PROXY_CTRL_MAGIC) {
                uint8_t cmd = pkt.payload[1];
                if (cmd == PROXY_PING) {
                    sendControlFrame(med, pkt.endpoint.peerId, PROXY_PONG);
                } else if (cmd == PROXY_IDLE_CLOSE) {
                    bool isBusy = (busyCheck_ && busyCheck_(s.clientId)) ||
                                  (now >= s.lastRpcActivityMs && (now - s.lastRpcActivityMs < (s.negotiatedIdleMs / 2)));
                    if (isBusy) {
                        sendControlFrame(med, pkt.endpoint.peerId, PROXY_IDLE_CLOSE_NACK);
                    } else {
                        sendControlFrame(med, pkt.endpoint.peerId, PROXY_IDLE_CLOSE_ACK);
                        s.pendingIdleClose = true;
                        s.lastReason = DisconnectReason::IDLE_CLOSE;
                    }
                } else if (cmd == PROXY_IDLE_CLOSE_ACK) {
                    s.pendingIdleClose = true;
                    s.lastReason = DisconnectReason::IDLE_CLOSE;
                } else if (cmd == PROXY_IDLE_CLOSE_NACK) {
                    s.closingGracefully = false;
                    s.lastRpcActivityMs = now;
                }
                return;
            }

            s.lastRpcActivityMs = now;
            s.closingGracefully = false;
            lastActiveClientId_ = s.clientId;
            rxQueue_.push({s.clientId, s.sessionId, pkt});
        });
    }

    void setRpcHandler(RpcLayerHandler handler) { rpcHandler_ = std::move(handler); }
    void onMediumDisconnected(DisconnectListener listener) { disconnectListeners_.push_back(std::move(listener)); }
    void setBusyCheck(BusyCheckCallback cb) { busyCheck_ = std::move(cb); }

    void schedule() {
        for (auto& kv : transports_) {
            MediumType m = kv.first;
            ITransport* t = kv.second;
            ClientSession& s = sessions_[m];

            t->poll();
            uint32_t now = millis();

            if (s.pendingIdleClose) {
                t->poll();
                t->closeIdle();
                s.pendingIdleClose = false;
                s.wasAuthenticated = false;
                s.closingGracefully = false;
                notifyDisconnected(s, DisconnectReason::IDLE_CLOSE);
                continue;
            }

            bool isAuth = (t->getState() == TransportState::AUTHENTICATED);
            if (isAuth && !s.wasAuthenticated) {
                s.sessionId = t->sessionId();
                s.negotiatedIdleMs = (t->negotiatedIdleMs() > 0)
                                     ? std::min(cfg_.idleTimeoutMs, t->negotiatedIdleMs())
                                     : cfg_.idleTimeoutMs;
                s.lastRxMs = now;
                s.lastProbeTxMs = now;
                s.lastRpcActivityMs = now;
                s.closingGracefully = false;
                s.lastReason = DisconnectReason::NONE;
            }
            if (!isAuth && s.wasAuthenticated) {
                s.closingGracefully = false;
                s.wasAuthenticated = false;
                DisconnectReason r = (t->lastDisconnectReason() != DisconnectReason::NONE)
                                     ? t->lastDisconnectReason()
                                     : DisconnectReason::DISC_RX;
                s.lastReason = r;
                notifyDisconnected(s, r);
            }
            s.wasAuthenticated = isAuth;

        }

        size_t processed = 0;
        while (!rxQueue_.empty() && processed < cfg_.maxPacketsPerTick) {
            QueuedRx item = rxQueue_.front();
            rxQueue_.pop();
            if (rpcHandler_) {
                rpcHandler_(item.clientId, item.sessionId, item.packet.payload, item.packet.isRealTime);
            }
            processed++;
        }

        processed = 0;
        while (!txQueue_.empty() && processed < cfg_.maxPacketsPerTick) {
            TransportPacket pkt = txQueue_.front();
            txQueue_.pop();
            dispatchToMedium(pkt);
            processed++;
        }
    }

    bool routeToClient(uint32_t clientId, const uint8_t* data, size_t len, bool isRealTime) {
        uint32_t targetId = (clientId == 0) ? resolveDefaultClientId() : clientId;
        auto it = clientRoutes_.find(targetId);
        if (it == clientRoutes_.end()) return false;

        MediumType m = it->second.medium;
        if (!isMediumConnected(m)) return false;

        sessions_[m].lastRpcActivityMs = millis();
        sessions_[m].closingGracefully = false;

        TransportPacket pkt;
        pkt.endpoint = it->second;
        pkt.isRealTime = isRealTime;
        pkt.payload.assign(data, data + len);
        txQueue_.push(std::move(pkt));
        return true;
    }

    bool isConnected() const {
        for (const auto& kv : transports_) {
            if (kv.second->getState() == TransportState::AUTHENTICATED) return true;
        }
        return false;
    }

    bool isMediumConnected(MediumType m) const {
        auto it = transports_.find(m);
        return (it != transports_.end() && it->second->getState() == TransportState::AUTHENTICATED);
    }

    uint32_t getSessionIdForClient(uint32_t clientId) const {
        auto it = clientRoutes_.find(clientId);
        if (it == clientRoutes_.end()) return 0;
        auto sit = sessions_.find(it->second.medium);
        return (sit != sessions_.end()) ? sit->second.sessionId : 0;
    }

    uint32_t bindDefaultEndpoint(MediumType medium, uint32_t peerId = 1) {
        return resolveOrCreateClientId({medium, peerId});
    }

private:
    struct QueuedRx {
        uint32_t clientId;
        uint32_t sessionId;
        TransportPacket packet;
    };

    LinkConfig cfg_;
    std::map<MediumType, ITransport*> transports_;
    std::map<MediumType, ClientSession> sessions_;
    std::map<uint32_t, EndpointId> clientRoutes_;
    std::map<EndpointId, uint32_t> reverseRoutes_;
    std::queue<QueuedRx> rxQueue_;
    std::queue<TransportPacket> txQueue_;

    RpcLayerHandler rpcHandler_ = nullptr;
    std::vector<DisconnectListener> disconnectListeners_;
    BusyCheckCallback busyCheck_ = nullptr;
    uint32_t nextClientId_ = 1;
    uint32_t lastActiveClientId_ = 1;

    void sendControlFrame(MediumType m, uint32_t peerId, uint8_t cmd) {
        auto it = transports_.find(m);
        if (it == transports_.end() || it->second->getState() != TransportState::AUTHENTICATED) return;
        uint8_t probe[2] = {PROXY_CTRL_MAGIC, cmd};
        it->second->sendReliable(peerId, probe, 2);
    }

    void notifyDisconnected(const ClientSession& s, DisconnectReason reason) {
        // Vaciar tramas encoladas de ese medio
        std::queue<TransportPacket> cleanTx;
        while (!txQueue_.empty()) {
            TransportPacket p = std::move(txQueue_.front());
            txQueue_.pop();
            if (p.endpoint.medium != s.medium) {
                cleanTx.push(std::move(p));
            }
        }
        txQueue_ = std::move(cleanTx);

        for (const auto& fn : disconnectListeners_) {
            if (fn) fn(s.clientId, s.sessionId, s.medium, reason);
        }
    }

    uint32_t resolveDefaultClientId() {
        auto it = clientRoutes_.find(lastActiveClientId_);
        if (it != clientRoutes_.end() && isMediumConnected(it->second.medium)) {
            return lastActiveClientId_;
        }
        for (const auto& kv : clientRoutes_) {
            if (isMediumConnected(kv.second.medium)) return kv.first;
        }
        return lastActiveClientId_;
    }

    uint32_t resolveOrCreateClientId(const EndpointId& ep) {
        auto it = reverseRoutes_.find(ep);
        if (it != reverseRoutes_.end()) return it->second;

        uint32_t newId = nextClientId_++;
        clientRoutes_[newId] = ep;
        reverseRoutes_[ep] = newId;
        return newId;
    }

    bool dispatchToMedium(const TransportPacket& pkt) {
        auto it = transports_.find(pkt.endpoint.medium);
        if (it == transports_.end()) return false;

        ITransport* medium = it->second;
        if (medium->getState() != TransportState::AUTHENTICATED) return false;

        if (pkt.isRealTime) {
            return medium->sendStream(pkt.endpoint.peerId, pkt.payload.data(), pkt.payload.size());
        }
        return medium->sendReliable(pkt.endpoint.peerId, pkt.payload.data(), pkt.payload.size());
    }
};

} // namespace Connections