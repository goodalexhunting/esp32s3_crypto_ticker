#pragma once

// Mocks for the Arduino WiFi/WebServer/DNSServer classes used by
// src/wifi_mgr.cpp. Handlers registered via server.on()/onNotFound() are
// stored and can be invoked from tests; send()/send_P()/sendHeader()/
// redirect() record their arguments; the WiFi singleton is a controllable
// fake with an injectable connection state.

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Arduino.h"

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_CONNECTED,
    WL_DISCONNECTED,
};

enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 1 };

// WiFi radio modes used by wifi_mgr.cpp (WiFi.mode()).
enum WiFiMode : uint8_t { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2 };

struct MockWiFiRecords {
    bool                          lastBeginResult = false;
    bool                          softAPResult    = true;
    std::string                   lastSsid;
    std::string                   lastPass;
    uint8_t                       lastMode = 0;  // 0=none, 1=STA, 2=AP
    wl_status_t                   status   = WL_DISCONNECTED;
    IPAddress                     localIp  = IPAddress(192, 168, 1, 50);
    IPAddress                     apIp     = IPAddress(192, 168, 4, 1);
    std::string                   apName;
    std::vector<std::string>      scanSsids;
    std::vector<int>              scanRssis;
    std::vector<wifi_auth_mode_t> scanEncryption;
    int                           scanResult        = 0;  // -1 = error, else count
    int                           disconnectCount   = 0;
    bool                          disconnectWifiOff = false;
};

static MockWiFiRecords& __mockWifi() {
    static MockWiFiRecords r;
    return r;
}
static MockWiFiRecords& mockWiFi = __mockWifi();

struct MockWiFiClass {
    wl_status_t status() const {
        return mockWiFi.status;
    }

    bool begin(const char* ssid, const char* pass = nullptr) {
        mockWiFi.lastSsid = ssid != nullptr ? ssid : "";
        mockWiFi.lastPass = pass != nullptr ? pass : "";
        return mockWiFi.lastBeginResult;
    }

    bool softAP(
        const char* ssid, const char* pass = nullptr, uint8_t = 1, uint8_t = 0, uint8_t = 4) {
        mockWiFi.apName = ssid != nullptr ? ssid : "";
        return mockWiFi.softAPResult;
    }

    void mode(uint8_t m) {
        mockWiFi.lastMode = m;
    }

    IPAddress localIP() const {
        return mockWiFi.localIp;
    }
    IPAddress softAPIP() const {
        return mockWiFi.apIp;
    }

    int16_t scanNetworks() {
        if (mockWiFi.scanResult < 0) return -1;
        return mockWiFi.scanResult;
    }
    String SSID(int16_t i) const {
        if (i < 0 || (size_t)i >= mockWiFi.scanSsids.size()) return String();
        return String(mockWiFi.scanSsids[(size_t)i].c_str());
    }
    int32_t RSSI(int16_t i) const {
        if (i < 0 || (size_t)i >= mockWiFi.scanRssis.size()) return 0;
        return mockWiFi.scanRssis[(size_t)i];
    }
    wifi_auth_mode_t encryptionType(int16_t i) const {
        if (i < 0 || (size_t)i >= mockWiFi.scanEncryption.size()) return WIFI_AUTH_OPEN;
        return mockWiFi.scanEncryption[(size_t)i];
    }
    void scanDelete() {}

    void disconnect(bool wifioff = false, bool = false) {
        mockWiFi.disconnectCount++;
        mockWiFi.disconnectWifiOff = wifioff;
        mockWiFi.status            = WL_DISCONNECTED;
    }
    void setAutoReconnect(bool) {}

    void softAPdisconnect(bool = false) {
        mockWiFi.softAPResult = true;
    }
};
static MockWiFiClass WiFi;

// ---------------------------------------------------------------------------
// WebServer
// ---------------------------------------------------------------------------
class WebServer {
   public:
    using Handler = std::function<void()>;

    WebServer(int = 80) {
        // Track the most recently constructed instance so tests can drive
        // the handlers registered by WifiManager's private _server member.
        g_current = this;
    }

    /** The most recently constructed WebServer (for test access). */
    static WebServer* current() {
        return g_current;
    }

    void on(const char* uri, uint8_t method, Handler h) {
        // First registration wins (matches ESP32 WebServer semantics).
        std::string key = std::string(uri) + "|" + std::to_string(method);
        if (_handlers.find(key) == _handlers.end()) {
            _handlers[key] = std::move(h);
        }
    }
    void onNotFound(Handler h) {
        _notFound = std::move(h);
    }
    void begin() {
        _begun = true;
    }
    void stop() {
        _begun = false;
    }
    void handleClient() {
        // Tests invoke the handlers directly via call(); handleClient is a
        // no-op so the firmware's loop() behaviour is deterministic.
    }

    // --- Request state (what tests provide before invoking a handler) ---
    void setArgs(const std::vector<std::pair<std::string, std::string>>& args) {
        _args = args;
    }
    bool hasArg(const char* name) const {
        std::string key(name);
        for (const auto& a : _args) {
            if (a.first == key) return true;
        }
        return false;
    }
    String arg(const char* name) const {
        std::string key(name);
        for (const auto& a : _args) {
            if (a.first == key) return String(a.second.c_str());
        }
        return String();
    }

    // --- Response recording ---
    struct Response {
        int                                code = 0;
        std::string                        contentType;
        std::string                        content;
        bool                               isRedirect = false;
        std::string                        location;
        std::map<std::string, std::string> headers;
    };

    Response response;
    bool     _begun = false;

    void send(int code, const char* contentType, const char* content) {
        // A 3xx with a Location header (set via sendHeader) is treated as a
        // redirect by the real ESP32 WebServer; reflect that here.
        auto loc            = response.headers.find("Location");
        response.isRedirect = (code / 100 == 3) && (loc != response.headers.end());
        if (loc != response.headers.end()) {
            response.location = loc->second;
        }
        response.code        = code;
        response.contentType = contentType != nullptr ? contentType : "";
        response.content     = content != nullptr ? content : "";
    }
    void send(int code, const char* contentType, const String& content) {
        send(code, contentType, content.c_str());
    }
    void send(int code, const String& contentType, const String& content) {
        send(code, contentType.c_str(), content.c_str());
    }
    void send_P(int code, const char* contentType, const char* content, size_t = 0) {
        send(code, contentType, content);
    }
    void sendHeader(const char* name, const char* value, bool = false) {
        response.headers[name] = value;
    }
    void sendHeader(const char* name, const String& value, bool first = false) {
        sendHeader(name, value.c_str(), first);
    }
    void redirect(const String& url) {
        response.isRedirect = true;
        response.location   = url.c_str();
        response.code       = 302;
    }

    // Invoke the registered handler for uri+method, or the notFound handler.
    bool call(const char* uri, uint8_t method) {
        // Fresh response so tests never leak state from a previous handler.
        response = Response();
        Handler     h;
        std::string key = std::string(uri) + "|" + std::to_string(method);
        auto        it  = _handlers.find(key);
        if (it != _handlers.end()) {
            h = it->second;
        } else if (_notFound) {
            h = _notFound;
        } else {
            return false;
        }
        h();
        return true;
    }

   private:
    std::map<std::string, Handler>                   _handlers;
    Handler                                          _notFound;
    std::vector<std::pair<std::string, std::string>> _args;

    static WebServer* g_current;
};

// HTTP method constants matching the ESP32 WebServer API used by the code.
#define HTTP_GET 0x01
#define HTTP_POST 0x02

// Out-of-line definition of the static (so it has one address per binary).
WebServer* WebServer::g_current = nullptr;

// ---------------------------------------------------------------------------
// DNSServer
// ---------------------------------------------------------------------------
class DNSServer {
   public:
    bool _started = false;
    void start(uint8_t, const char*, const IPAddress&) {
        _started = true;
    }
    bool start(uint8_t, const char*, const IPAddress&, uint16_t) {
        _started = true;
        return true;
    }
    void stop() {
        _started = false;
    }
    void processNextRequest() {}
    bool isStarted() const {
        return _started;
    }
};