#pragma once
#include "../../proxy/ITransport.hpp"
#include "../UART-S/Security/UartSecurity.hpp"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <esp_system.h>
#include <string>
#include <vector>

namespace Connections::Wifi {

struct WifiConfig {
    std::string staSsid = "";
    std::string staPassword = "";
    std::string apSsid = "ESP32-SecureAP";
    std::string apPassword = "Esp@12345678";
    uint8_t apChannel = 1;

    bool validate() const {
        if (apSsid.empty() || apSsid.size() > 32) return false;
        if (!apPassword.empty() && (apPassword.size() < 8 || apPassword.size() > 64)) return false;
        if (staSsid.size() > 32) return false;
        if (!staPassword.empty() && staPassword.size() > 64) return false;
        return apChannel >= 1 && apChannel <= 13;
    }
};

struct NetworkStatusDto {
    bool staConnected;
    std::string staSsid;
    std::string staDhcpIp;
    std::string staGatewayIp;
    std::string staSubnetMask;
    int32_t staRssi;
    bool apActive;
    std::string apSsid;
    std::string apIp;
    std::string macAddress;
    uint32_t bootId;
    uint32_t uptimeMs;
    uint8_t resetReason;
    int32_t channel;
};

class WifiTransport : public ITransport {
public:
    static constexpr uint16_t TCP_PORT = 8443;
    static constexpr uint16_t UDP_PORT = 8444;
    static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 10000;
    static constexpr uint32_t HANDSHAKE_TIMEOUT_MS   = 3500;

    WifiTransport() : tcpServer_(TCP_PORT) {}

    void setBootMetadata(uint32_t bootId, uint8_t resetReason, uint32_t localIdleMs) {
        bootId_ = bootId;
        resetReason_ = resetReason;
        localIdleMs_ = localIdleMs;
    }

    void loadConfig() override {
        // cargar de base de datos
        Preferences prefs;
        if (prefs.begin("conn_wifi", true)) {
            cfg_.staSsid = prefs.getString("staSsid", "").c_str();
            cfg_.staPassword = prefs.getString("staPass", "").c_str();
            cfg_.apSsid = prefs.getString("apSsid", "ESP32-SecureAP").c_str();
            cfg_.apPassword = prefs.getString("apPass", "12345678").c_str();
            cfg_.apChannel = prefs.getUChar("apChan", 1);
            prefs.end();
        }
        if (!cfg_.validate()) {
            cfg_ = WifiConfig{};
        }
        tcpCrypto_.loadConfig();
        udpCrypto_.loadConfig();
    }

    bool applyConfig(const WifiConfig& in) {
        if (!in.validate()) return false;
        // zona base de datos
        Preferences prefs;
        if (prefs.begin("conn_wifi", false)) {
            prefs.putString("staSsid", in.staSsid.c_str());
            prefs.putString("staPass", in.staPassword.c_str());
            prefs.putString("apSsid", in.apSsid.c_str());
            prefs.putString("apPass", in.apPassword.c_str());
            prefs.putUChar("apChan", in.apChannel);
            prefs.end();
        }
        cfg_ = in;
        // Aplicación diferida: permite enviar la respuesta RPC antes de reconfigurar el AP/STA en poll()
        reinitPending_ = true;
        reinitScheduledMs_ = millis();
        return true;
    }

    const WifiConfig& getConfig() const { return cfg_; }

    bool init() override {
        WiFi.persistent(false);
        WiFi.mode(cfg_.staSsid.empty() ? WIFI_AP : WIFI_AP_STA);
        WiFi.setSleep(false);

        IPAddress apIp(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIp, gateway, subnet);
        apActive_ = WiFi.softAP(cfg_.apSsid.c_str(), cfg_.apPassword.c_str(), cfg_.apChannel, 0, 4);

        if (!cfg_.staSsid.empty()) {
            startAsyncStaConnect();
        }

        tcpServer_.begin();
        tcpServer_.setNoDelay(true);
        udp_.begin(UDP_PORT);

        setState(TransportState::PHY_LINKED, DisconnectReason::NONE);
        return true;
    }

    // Configuración no bloqueante (sin delay ni schedule reentrante) con persistencia en NVS
    NetworkStatusDto reconfigureWifi(const std::string& staSsid, const std::string& staPass,
                                     const std::string& apSsid = "", const std::string& apPass = "") {
        WifiConfig next = cfg_;
        next.staSsid = staSsid;
        next.staPassword = staPass;
        if (!apSsid.empty()) next.apSsid = apSsid;
        if (!apPass.empty()) next.apPassword = apPass;
        applyConfig(next);
        return getNetworkStatus();
    }

    NetworkStatusDto getNetworkStatus() const {
        bool staOk = (WiFi.status() == WL_CONNECTED);
        std::string realStaSsid = staOk ? WiFi.SSID().c_str() : cfg_.staSsid;
        std::string mac = WiFi.macAddress().c_str();
        if (mac.empty() || mac == "00:00:00:00:00:00") {
            mac = WiFi.softAPmacAddress().c_str();
        }
        return NetworkStatusDto{
            staOk,
            realStaSsid,
            staOk ? WiFi.localIP().toString().c_str() : "0.0.0.0",
            staOk ? WiFi.gatewayIP().toString().c_str() : "0.0.0.0",
            staOk ? WiFi.subnetMask().toString().c_str() : "0.0.0.0",
            staOk ? static_cast<int32_t>(WiFi.RSSI()) : 0,
            apActive_,
            cfg_.apSsid,
            WiFi.softAPIP().toString().c_str(),
            mac,
            bootId_,
            millis(),
            resetReason_,
            static_cast<int32_t>(WiFi.channel())
        };
    }

    bool connect() override {
        if (state_ == TransportState::DISCONNECTED) return init();
        return true;
    }

    void disconnect() override {
        if (tcpClient_) tcpClient_.stop();
        if (pendingClient_) pendingClient_.stop();
        rxTcpBuffer_.clear();
        pendingRxBuffer_.clear();
        handshakeCompleted_ = false;
        setState(TransportState::PHY_LINKED, lastReason_);
    }

    void closeIdle() override {
        if (tcpClient_) tcpClient_.stop();
        if (pendingClient_) pendingClient_.stop();
        rxTcpBuffer_.clear();
        pendingRxBuffer_.clear();
        handshakeCompleted_ = false;
        // No activamos setSleep(true) si el SoftAP está encendido, porque el driver de ESP-IDF hace abort() si hay AP activo
        setState(TransportState::CLOSED_IDLE, DisconnectReason::IDLE_CLOSE);
    }

    void poll() override {
        if (state_ == TransportState::DISCONNECTED) return;

        uint32_t now = millis();

        // 1. Aplicar reconfiguración diferida una vez que salió la respuesta RPC
        if (reinitPending_ && (now - reinitScheduledMs_ >= 60)) {
            reinitPending_ = false;
            apActive_ = WiFi.softAP(cfg_.apSsid.c_str(), cfg_.apPassword.c_str(), cfg_.apChannel, 0, 4);
            if (!cfg_.staSsid.empty()) {
                startAsyncStaConnect();
            } else {
                staConnecting_ = false;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                apActive_ = WiFi.softAP(cfg_.apSsid.c_str(), cfg_.apPassword.c_str(), cfg_.apChannel, 0, 4);
                WiFi.setSleep(false);
            }
        }

        // 2. Máquina de estados asíncrona de conexión STA (nunca bloquea el loop ni reentra a schedule)
        if (staConnecting_) {
            if (WiFi.status() == WL_CONNECTED) {
                staConnecting_ = false;
                int32_t ch = (WiFi.channel() > 0) ? WiFi.channel() : cfg_.apChannel;
                apActive_ = WiFi.softAP(cfg_.apSsid.c_str(), cfg_.apPassword.c_str(), ch, 0, 4);
                WiFi.setSleep(false);
            } else if (now - staConnectStartMs_ >= STA_CONNECT_TIMEOUT_MS) {
                staConnecting_ = false;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                apActive_ = WiFi.softAP(cfg_.apSsid.c_str(), cfg_.apPassword.c_str(), cfg_.apChannel, 0, 4);
                WiFi.setSleep(false);
            }
        }

        // 3. Aceptar nuevo socket en candidato separado sin expulsar una sesión autenticada viva
        if (tcpServer_.hasClient()) {
            WiFiClient incoming = tcpServer_.accept();
            if (incoming) {
                incoming.setNoDelay(true);
                if (state_ == TransportState::AUTHENTICATED && tcpClient_ && tcpClient_.connected()) {
                    if (pendingClient_ && pendingClient_.connected()) {
                        pendingClient_.stop();
                    }
                    pendingClient_ = incoming;
                    pendingRxBuffer_.clear();
                    pendingHandshakeStartMs_ = now;
                } else {
                    if (tcpClient_) tcpClient_.stop();
                    tcpClient_ = incoming;
                    rxTcpBuffer_.clear();
                    handshakeCompleted_ = false;
                    handshakeStartMs_ = now;
                    remoteUdpIp_ = tcpClient_.remoteIP();
                    remoteUdpPort_ = UDP_PORT;
                    WiFi.setSleep(false);
                    setState(TransportState::HANDSHAKING, DisconnectReason::NONE);
                }
            }
        }

        // 4. Procesar candidato pendiente (solo reemplaza la sesión viva si completa el handshake ECDH)
        if (pendingClient_ && pendingClient_.connected()) {
            while (pendingClient_.available()) {
                pendingRxBuffer_.push_back(static_cast<uint8_t>(pendingClient_.read()));
            }
            if (!tryCompleteHandshake(pendingClient_, pendingRxBuffer_)) {
                if (now - pendingHandshakeStartMs_ > HANDSHAKE_TIMEOUT_MS) {
                    pendingClient_.stop();
                    pendingRxBuffer_.clear();
                }
            } else {
                if (tcpClient_) tcpClient_.stop();
                tcpClient_ = pendingClient_;
                rxTcpBuffer_ = std::move(pendingRxBuffer_);
                pendingRxBuffer_.clear();
                handshakeCompleted_ = true;
                setState(TransportState::AUTHENTICATED, DisconnectReason::NONE);
            }
        }

        // 5. Procesar socket principal y su timeout de handshake
        if (tcpClient_ && tcpClient_.connected()) {
            while (tcpClient_.available()) {
                rxTcpBuffer_.push_back(static_cast<uint8_t>(tcpClient_.read()));
            }
            if (!handshakeCompleted_) {
                if (tryCompleteHandshake(tcpClient_, rxTcpBuffer_)) {
                    handshakeCompleted_ = true;
                    setState(TransportState::AUTHENTICATED, DisconnectReason::NONE);
                } else if (now - handshakeStartMs_ > HANDSHAKE_TIMEOUT_MS) {
                    tcpClient_.stop();
                    rxTcpBuffer_.clear();
                    setState(TransportState::PHY_LINKED, DisconnectReason::HANDSHAKE_TIMEOUT);
                }
            }
            if (handshakeCompleted_) {
                processEncryptedTcpFrames();
            }
        } else if (state_ == TransportState::AUTHENTICATED || state_ == TransportState::HANDSHAKING) {
            rxTcpBuffer_.clear();
            handshakeCompleted_ = false;
            setState(TransportState::PHY_LINKED, DisconnectReason::PORT_CLOSED);
        }

        // 6. Canal UDP de streaming cifrado con clave independiente "udp"
        int packetSize = udp_.parsePacket();
        while (packetSize > 0) {
            std::vector<uint8_t> udpBuf(packetSize);
            udp_.read(udpBuf.data(), packetSize);

            if (state_ == TransportState::AUTHENTICATED) {
                remoteUdpIp_ = udp_.remoteIP();
                remoteUdpPort_ = udp_.remotePort();
                std::vector<uint8_t> plain;
                if (udpCrypto_.decrypt(udpBuf.data(), udpBuf.size(), plain) && onRx_) {
                    TransportPacket pkt;
                    pkt.endpoint = {MediumType::WIFI, 1};
                    pkt.isRealTime = true;
                    pkt.payload = std::move(plain);
                    onRx_(pkt);
                }
            }
            packetSize = udp_.parsePacket();
        }
    }

    bool sendReliable(uint32_t peerId, const uint8_t* data, size_t len) override {
        if (state_ != TransportState::AUTHENTICATED || !tcpClient_ || !tcpClient_.connected()) return false;
        std::vector<uint8_t> cipher;
        if (!tcpCrypto_.encrypt(data, len, cipher)) return false;

        uint16_t cLen = static_cast<uint16_t>(cipher.size());
        uint8_t hdr[2] = {static_cast<uint8_t>((cLen >> 8) & 0xFF), static_cast<uint8_t>(cLen & 0xFF)};
        tcpClient_.write(hdr, 2);
        return tcpClient_.write(cipher.data(), cipher.size()) == cipher.size();
    }

    bool sendStream(uint32_t peerId, const uint8_t* data, size_t len) override {
        if (state_ != TransportState::AUTHENTICATED) return false;
        std::vector<uint8_t> cipher;
        if (!udpCrypto_.encrypt(data, len, cipher)) return false;

        IPAddress targetIp = (remoteUdpIp_ != IPAddress(0, 0, 0, 0)) ? remoteUdpIp_ : tcpClient_.remoteIP();
        udp_.beginPacket(targetIp, remoteUdpPort_);
        udp_.write(cipher.data(), cipher.size());
        return udp_.endPacket() == 1;
    }

    MediumType getMediumType() const override { return MediumType::WIFI; }

private:
    WifiConfig cfg_;
    bool apActive_ = false;
    bool reinitPending_ = false;
    uint32_t reinitScheduledMs_ = 0;

    bool staConnecting_ = false;
    uint32_t staConnectStartMs_ = 0;

    WiFiServer tcpServer_;
    WiFiClient tcpClient_;
    WiFiClient pendingClient_;
    WiFiUDP udp_;
    IPAddress remoteUdpIp_{0, 0, 0, 0};
    uint16_t remoteUdpPort_ = UDP_PORT;

    std::vector<uint8_t> rxTcpBuffer_;
    std::vector<uint8_t> pendingRxBuffer_;
    bool handshakeCompleted_ = false;
    uint32_t handshakeStartMs_ = 0;
    uint32_t pendingHandshakeStartMs_ = 0;

    uint32_t bootId_ = 1;
    uint8_t resetReason_ = 0;
    uint32_t localIdleMs_ = 10UL * 60UL * 1000UL;

    UartS::UartSecurity tcpCrypto_;
    UartS::UartSecurity udpCrypto_;

    void startAsyncStaConnect() {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);
        WiFi.disconnect();
        WiFi.begin(cfg_.staSsid.c_str(), cfg_.staPassword.c_str());
        staConnecting_ = true;
        staConnectStartMs_ = millis();
    }

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

    bool tryCompleteHandshake(WiFiClient& client, std::vector<uint8_t>& buf) {
        if (buf.size() < 2 || buf[0] != 0xFE) return false;
        uint8_t payloadLen = buf[1];
        if (payloadLen < 65 || buf.size() < static_cast<size_t>(2 + payloadLen)) return false;

        if (payloadLen >= 67) {
            remoteUdpPort_ = (static_cast<uint16_t>(buf[2 + 65]) << 8) | buf[2 + 66];
        }
        uint32_t peerSessionId = (payloadLen >= 71) ? readU32BE(buf.data() + 2 + 67) : esp_random();
        uint32_t peerIdleMs = (payloadLen >= 75) ? readU32BE(buf.data() + 2 + 71) : localIdleMs_;

        sessionId_ = (peerSessionId != 0) ? peerSessionId : esp_random();
        negotiatedIdleMs_ = std::min(localIdleMs_, peerIdleMs);
        remoteUdpIp_ = client.remoteIP();

        std::vector<uint8_t> myPub;
        if (!tcpCrypto_.generateHandshakePublic(myPub) ||
            !tcpCrypto_.computeSharedKey(buf.data() + 2, 65, false, "tcp") ||
            !udpCrypto_.deriveChannelKeysFrom(tcpCrypto_, false, "udp")) {
            client.stop();
            buf.clear();
            return false;
        }

        // Respuesta: [0xFE, 78, myPub(65B) | sessionId(4B) | bootId(4B) | resetReason(1B) | negotiatedIdleMs(4B)]
        std::vector<uint8_t> resp;
        resp.push_back(0xFE);
        resp.push_back(78);
        resp.insert(resp.end(), myPub.begin(), myPub.end());
        writeU32BE(resp, sessionId_);
        writeU32BE(resp, bootId_);
        resp.push_back(resetReason_);
        writeU32BE(resp, negotiatedIdleMs_);
        client.write(resp.data(), resp.size());

        buf.erase(buf.begin(), buf.begin() + 2 + payloadLen);
        return true;
    }

    void processEncryptedTcpFrames() {
        while (rxTcpBuffer_.size() >= 2) {
            uint16_t pktLen = (static_cast<uint16_t>(rxTcpBuffer_[0]) << 8) | rxTcpBuffer_[1];
            if (rxTcpBuffer_.size() < static_cast<size_t>(2 + pktLen)) break;

            std::vector<uint8_t> plain;
            if (tcpCrypto_.decrypt(rxTcpBuffer_.data() + 2, pktLen, plain) && onRx_) {
                TransportPacket pkt;
                pkt.endpoint = {MediumType::WIFI, 1};
                pkt.isRealTime = false;
                pkt.payload = std::move(plain);
                onRx_(pkt);
            }
            rxTcpBuffer_.erase(rxTcpBuffer_.begin(), rxTcpBuffer_.begin() + 2 + pktLen);
        }
    }
};

} // namespace Connections::Wifi