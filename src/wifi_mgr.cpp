#include "wifi_mgr.h"

#include <WiFi.h>

#include "wifi_secrets.h"

namespace cryptoapp {

int WifiManager::connectWifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        Serial.print(".");
        tries++;
    }
    Serial.println();

    bool connected = (WiFi.status() == WL_CONNECTED);
    Serial.println(connected ? "WiFi OK" : "WiFi FAIL");
    return connected ? 0 : 1;
};

}  // namespace cryptoapp