#include "crypto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_config.h"

namespace {

void buildUrl(String& url) {
    url = COINGECKO_URL;
    for (int i = 0; i < NUM_COINS; i++) {
        if (i > 0) url += ",";
        url += COINS[i].apiId;
    }
    url += "&vs_currencies=usd";
}

}  // namespace

bool fetch_prices(float* outValues, size_t count) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String url;
    buildUrl(url);

    HTTPClient http;
    http.begin(url);
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            JsonDocument         doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                http.end();
                return false;
            }

            size_t n = count < NUM_COINS ? count : NUM_COINS;
            for (size_t i = 0; i < n; i++) {
                outValues[i] = doc[COINS[i].apiId]["usd"] | 0.0f;
            }

            http.end();
            return true;
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}