#include "wifi_mgr.h"

#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

namespace cryptoapp {

namespace {
constexpr char         NVS_NAMESPACE[]       = "wifi_creds";
constexpr char         NVS_KEY_SSID[]        = "ssid";
constexpr char         NVS_KEY_PASS[]        = "pass";
constexpr int          AP_TIMEOUT_MS         = 5 * 60 * 1000;  // 5 min
constexpr int          RECONNECT_INTERVAL_MS = 10 * 1000;
constexpr int          WIFI_CONNECT_TIMEOUT  = 10 * 1000;  // 10 s
constexpr uint8_t      DEFAULT_AP_CHANNEL    = 1;
constexpr uint8_t      MAX_AP_CLIENTS        = 4;
constexpr unsigned int CRED_MAX_SSID_LEN     = 32;
constexpr unsigned int CRED_MAX_PASS_LEN     = 64;
}  // namespace

WifiManager::WifiManager()
    : _state(State::DISCONNECTED),
      _server(80),
      _apStartTime(0),
      _lastReconnectAttempt(0),
      _fsMounted(false) {
    _server.on("/", HTTP_GET, std::bind(&WifiManager::handleRoot, this));
    _server.on("/scan", HTTP_GET, std::bind(&WifiManager::handleScan, this));
    _server.on("/connect", HTTP_POST, std::bind(&WifiManager::handleConnect, this));
    _server.onNotFound(std::bind(&WifiManager::handleNotFound, this));
}

bool WifiManager::begin() {
    mountFileSystem();
    loadCredentials();

    if (_ssid.length() > 0) {
        Serial.printf("[WiFi] Connecting to '%s'", _ssid.c_str());
        if (tryConnect(_ssid, _password, WIFI_CONNECT_TIMEOUT)) {
            _state = State::CONNECTED;
            Serial.println("\n[WiFi] Connected. IP: " + WiFi.localIP().toString());
            WiFi.setAutoReconnect(true);
            return true;
        }
        Serial.println();
    }

    Serial.println("[WiFi] No credentials or connection failed - starting AP mode");
    startAP();
    return false;
}

void WifiManager::handle() {
    if (_state == State::AP_MODE) {
        _dns.processNextRequest();
        _server.handleClient();

        // Auto-close AP after timeout
        if (millis() - _apStartTime > AP_TIMEOUT_MS) {
            Serial.println("[WiFi] AP timeout - attempting stored credentials again");
            stopAP();
            loadCredentials();
            if (_ssid.length() > 0 && tryConnect(_ssid, _password, WIFI_CONNECT_TIMEOUT)) {
                _state = State::CONNECTED;
                Serial.println("[WiFi] Reconnected. IP: " + WiFi.localIP().toString());
            } else {
                startAP();  // If reconnect fails, keep AP alive
            }
        }
    } else if (_state == State::CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            _state                = State::DISCONNECTED;
            _lastReconnectAttempt = 0;
            Serial.println("[WiFi] Connection lost");
        }
    }

    if (_state == State::DISCONNECTED) {
        handleReconnect();
    }
}

void WifiManager::forceAPMode() {
    Serial.println("[WiFi] Forcing AP mode");
    WiFi.disconnect();
    if (_state == State::AP_MODE) {
        stopAP();
    }
    startAP();
}

bool WifiManager::mountFileSystem() {
    if (_fsMounted) {
        return true;
    }
    _fsMounted = LittleFS.begin();
    if (!_fsMounted) {
        Serial.println("[WiFi] LittleFS mount failed");
        return false;
    }
    Serial.println("[WiFi] LittleFS mounted");
    return true;
}

void WifiManager::unmountFileSystem() {
    if (!_fsMounted) {
        return;
    }
    LittleFS.end();
    _fsMounted = false;
    Serial.println("[WiFi] LittleFS unmounted");
}

void WifiManager::sleep() {
    if (_state == State::AP_MODE) {
        stopAP();
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    _state = State::DISCONNECTED;
    Serial.println("[WiFi] Radio powered down for deep sleep");
}

// ---------------------------------------------------------------------------
// Connection helpers
// ---------------------------------------------------------------------------

bool WifiManager::tryConnect(const String& ssid, const String& pass, int timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (millis() - start < (unsigned long)timeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        delay(250);
        Serial.print(".");
    }
    WiFi.disconnect();
    return false;
}

void WifiManager::handleReconnect() {
    if (millis() - _lastReconnectAttempt < RECONNECT_INTERVAL_MS) {
        return;
    }
    _lastReconnectAttempt = millis();

    if (_ssid.length() == 0) {
        startAP();
        return;
    }

    Serial.println("[WiFi] Attempting reconnect...");
    if (tryConnect(_ssid, _password, WIFI_CONNECT_TIMEOUT)) {
        _state = State::CONNECTED;
        Serial.println("[WiFi] Reconnected. IP: " + WiFi.localIP().toString());
    } else {
        startAP();
    }
}

// ---------------------------------------------------------------------------
// AP mode + captive portal
// ---------------------------------------------------------------------------

void WifiManager::startAP() {
    // Ensure the config page is available even if the FS was unmounted.
    mountFileSystem();

    String apSuffix = String((uint32_t)ESP.getEfuseMac(), HEX);
    apSuffix.toUpperCase();
    _apName = "CryptoTicker-" + apSuffix;

    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(_apName.c_str(), nullptr, DEFAULT_AP_CHANNEL, 0, MAX_AP_CLIENTS);
    if (!ok) {
        Serial.println("[WiFi] AP setup failed");
        return;
    }

    _dns.start(53, "*", WiFi.softAPIP());
    _server.begin();

    _apStartTime = millis();
    _state       = State::AP_MODE;
    Serial.printf(
        "[WiFi] AP mode: '%s' at http://%s\n", _apName.c_str(), WiFi.softAPIP().toString().c_str());
}

void WifiManager::stopAP() {
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true);
    // Switch to station mode only. Do NOT call WiFi.mode(WIFI_OFF) here:
    // that would tear down a just-established STA connection (e.g. right
    // after the user submits credentials via the captive portal).
    WiFi.mode(WIFI_STA);
}

// ---------------------------------------------------------------------------
// NVS credentials
// ---------------------------------------------------------------------------

void WifiManager::loadCredentials() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("[WiFi] NVS open (read) failed");
        return;
    }
    _ssid     = prefs.getString(NVS_KEY_SSID, "");
    _password = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();
}

void WifiManager::saveCredentials(const String& ssid, const String& pass) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[WiFi] NVS open (write) failed");
        return;
    }
    prefs.putString(NVS_KEY_SSID, ssid.substring(0, CRED_MAX_SSID_LEN));
    prefs.putString(NVS_KEY_PASS, pass.substring(0, CRED_MAX_PASS_LEN));
    prefs.end();
}

void WifiManager::clearCredentials() {
    Serial.println("[WiFi] Clearing stored credentials");
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.remove(NVS_KEY_SSID);
        prefs.remove(NVS_KEY_PASS);
        prefs.end();
    }
    _ssid     = "";
    _password = "";
    WiFi.disconnect(true, true);
    Serial.println("[WiFi] Credentials cleared");
}

// ---------------------------------------------------------------------------
// Web handlers
// ---------------------------------------------------------------------------

String WifiManager::buildScanJson() {
    int16_t networks = WiFi.scanNetworks();
    String  json     = "[";
    for (int16_t i = 0; i < networks; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                ",\"encrypt\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") +
                "}";
    }
    WiFi.scanDelete();
    json += "]";
    return json;
}

void WifiManager::handleRoot() {
    File file = LittleFS.open("/wifi_config.html", "r");
    if (!file) {
        _server.send(500, "text/plain", "Config page not found");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(file, "text/html");
    file.close();
}

void WifiManager::handleScan() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", buildScanJson());
}

void WifiManager::handleConnect() {
    if (!_server.hasArg("ssid")) {
        _server.send(400, "text/plain", "Missing SSID");
        return;
    }
    String ssid = _server.arg("ssid");
    String pass = _server.hasArg("pass") ? _server.arg("pass") : "";
    ssid.trim();

    if (ssid.length() == 0) {
        _server.send(400, "text/plain", "Empty SSID");
        return;
    }

    Serial.printf("[WiFi] Attempting to join '%s'\n", ssid.c_str());
    if (tryConnect(ssid, pass, WIFI_CONNECT_TIMEOUT)) {
        saveCredentials(ssid, pass);
        _ssid     = ssid;
        _password = pass;
        _state    = State::CONNECTED;
        Serial.println("[WiFi] Connected. IP: " + WiFi.localIP().toString());
        _server.send(200, "text/plain", "OK");
        stopAP();
    } else {
        _server.send(401, "text/plain", "Wrong credentials or unreachable network");
    }
}

void WifiManager::handleNotFound() {
    // Captive portal: any request for another host redirects to the config page
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
}

}  // namespace cryptoapp