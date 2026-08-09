#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

namespace cryptoapp {

class WifiManager {
   public:
    enum class State { CONNECTED, AP_MODE, CONNECTING, DISCONNECTED };

    WifiManager();

    /**
     * Attempts to connect using stored credentials. If none exist or
     * connection fails, starts AP mode with a captive portal.
     * Returns true if connected to a network, false if in AP mode.
     */
    bool begin();

    /**
     * Must be called from loop(). Handles the web server (AP mode)
     * and reconnection attempts (station mode).
     */
    void handle();

    State getState() const {
        return _state;
    }
    bool isConnected() const {
        return _state == State::CONNECTED;
    }

    /** Name of the soft-AP SSID when in AP mode (e.g. "CryptoTicker-XXXX"). */
    const String& getAPName() const {
        return _apName;
    }

    /** Force AP mode (e.g. from a button long-press). */
    void forceAPMode();

    /** Erase stored WiFi credentials so the device will boot into AP mode. */
    void clearCredentials();

    /**
     * Power down the WiFi radio (e.g. before entering deep sleep).
     * The next begin() re-initialises the radio and reconnects.
     */
    void sleep();

   private:
    State         _state;
    WebServer     _server;
    DNSServer     _dns;
    unsigned long _apStartTime;
    unsigned long _lastReconnectAttempt;
    String        _ssid;
    String        _password;
    String        _apName;

    void loadCredentials();
    void saveCredentials(const String& ssid, const String& pass);
    bool tryConnect(const String& ssid, const String& pass, int timeoutMs);
    void startAP();
    void stopAP();
    void handleReconnect();

    // Web server handlers
    void handleRoot();
    void handleScan();
    void handleConnect();
    void handleNotFound();

    String buildScanJson();
};

}  // namespace cryptoapp