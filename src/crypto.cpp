#include "crypto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_config.h"

namespace cryptoapp {

namespace {

void buildUrl(String& url, const ConfigManager& config) {
    url = COINGECKO_URL;
    for (size_t i = 0; i < config.count(); i++) {
        if (i > 0) url += ",";
        url += config.get(i).apiId;
    }
    url += "&vs_currencies=";
    // Use the quote currency of the first ticker. All tickers share the
    // same quote currency in practice, but we build the URL from the
    // first configured ticker's quote.
    if (config.count() > 0) {
        url += config.get(0).quote;
    } else {
        url += "usd";
    }
}

}  // namespace

bool fetch_prices(float* outValues, size_t count, const ConfigManager& config) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String url;
    buildUrl(url, config);

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

            size_t n = count < config.count() ? count : config.count();
            for (size_t i = 0; i < n; i++) {
                const TickerConfig& t = config.get(i);
                outValues[i]          = doc[t.apiId][t.quote] | 0.0f;
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

}  // namespace cryptoapp