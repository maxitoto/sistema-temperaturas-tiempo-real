#include <Arduino.h>
#include <WiFi.h>

// ============== CONFIG ==============
static const char* WIFI_SSID = "Zhone_96F8";
static const char* WIFI_PASS = "Ushuaia30";
static const uint16_t RPC_PORT = 8080;
static const size_t MAX_MSG = 1024;
// ====================================

WiFiServer server(RPC_PORT);

// ---------- MARSHALLING: escribe un response completo ----------
static void sendResponse(WiFiClient& c, bool ok, const String& result) {
    uint16_t rlen = (uint16_t)result.length();
    uint16_t total = 1 + 2 + rlen;

    uint8_t buf[2 + 1 + 2 + 512];
    size_t idx = 0;
    buf[idx++] = total & 0xFF;
    buf[idx++] = total >> 8;
    buf[idx++] = ok ? 0x01 : 0x00;
    buf[idx++] = rlen & 0xFF;
    buf[idx++] = rlen >> 8;
    memcpy(&buf[idx], result.c_str(), rlen);
    idx += rlen;

    c.write(buf, idx);
}

// ---------- READ EXACT (con timeout) ----------
static bool readExact(WiFiClient& c, uint8_t* buf, size_t n, uint32_t timeoutMs = 3000) {
    size_t got = 0;
    uint32_t t0 = millis();
    while (got < n) {
        if (!c.connected()) return false;
        int avail = c.available();
        if (avail > 0) {
            int r = c.read(buf + got, n - got);
            if (r > 0) { got += r; t0 = millis(); }
        } else {
            // Solo matamos la conexión si ya empezó a mandar bytes y se cortó a mitad.
            // Si todavía no mandó nada (got == 0), esperamos indefinidamente.
            if (got > 0 && millis() - t0 > timeoutMs) return false;
            delay(1);
        }
    }
    return true;
}

// ---------- UNMARSHALLING: lee un request completo ----------
struct Request { String method; String args; };

static bool recvRequest(WiFiClient& c, Request& out) {
    uint8_t lenHdr[2];
    if (!readExact(c, lenHdr, 2)) return false;
    uint16_t total = (uint16_t)lenHdr[0] | ((uint16_t)lenHdr[1] << 8);
    if (total == 0 || total > MAX_MSG) 
    {
        sendResponse(c, false, "request too large (max " + String(MAX_MSG) + " bytes)");
        return false;
    }

    uint8_t body[MAX_MSG];
    if (!readExact(c, body, total)) return false;

    uint8_t mlen = body[0];
    if (total < 1 + mlen + 2) return false;

    out.method = String((const char*)&body[1], mlen);

    uint16_t alen = (uint16_t)body[1 + mlen] | ((uint16_t)body[2 + mlen] << 8);
    if (total < 1 + mlen + 2 + alen) return false;

    out.args = String((const char*)&body[3 + mlen], alen);
    return true;
}

// ---------- SKELETON: el "stub" del servidor ----------
// Cada método es una función. Registrar uno nuevo = un if más.
static void skeleton_echo(WiFiClient& c, const String& args) {
    String respuesta = "servidor echo: ";
    respuesta += args;
    sendResponse(c, true, respuesta);
}

// ---------- DISPATCH ----------
static void dispatch(WiFiClient& c, const Request& req) {
    Serial.printf("[RPC] -> %s(\"%s\")\n", req.method.c_str(), req.args.c_str());

    if (req.method == "echo") {
        skeleton_echo(c, req.args);
    } else if (req.method == "ping") {
        sendResponse(c, true, "pong");
    } else {
        sendResponse(c, false, "unknown method: " + req.method);
    }
}

// ---------- SESIÓN POR CLIENTE ----------
static void handleClient(WiFiClient& c) {
    String peer = c.remoteIP().toString();
    Serial.printf("[RPC] cliente conectado: %s\n", peer.c_str());

    while (c.connected()) {
        Request req;
        if (!recvRequest(c, req)) break;   // ahora solo sale si el cliente cerró
        dispatch(c, req);
    }

    Serial.printf("[RPC] cliente desconectado: %s\n", peer.c_str());
    c.stop();
}

// ---------- SETUP ----------
void setup() {
    // 0. Serial libre para ver la terminal (no lo usa el protocolo)
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[boot] ESP32 RPC server");

    // 1. WiFi STA — mostrar host asignado por DHCP
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[wifi] conectando a %s", WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.printf("[wifi] host asignado por DHCP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[wifi] puerto RPC: %u\n", RPC_PORT);

    server.begin();
    Serial.println("[rpc] escuchando...");
}

// ---------- LOOP ----------
void loop() {
    WiFiClient client = server.available();
    if (client) handleClient(client);
}