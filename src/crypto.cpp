#include "crypto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_config.h"
#include "crypto_utils.h"

namespace cryptoapp {

namespace {

// Bound every network request so a dead or hanging connection can never
// stall the main loop (which would freeze button/display handling).
constexpr int HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr int HTTP_READ_TIMEOUT_MS    = 8000;

bool httpGetJson(const String& url, JsonDocument& doc) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    http.begin(url);
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[HTTP] GET %s -> code: %d\n", url.c_str(), httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[JSON] deserializeJson() failed: %s\n", error.c_str());
        return false;
    }
    return true;
}

bool fetchJson(const String& url, JsonDocument& doc, CryptoHttp* http) {
    if (http != nullptr) {
        return http->getJson(url, doc);
    }
    return httpGetJson(url, doc);
}

}  // namespace

bool fetch_prices(PriceData* outData, size_t count, const ConfigManager& config, CryptoHttp* http) {
    TickerConfig tickers[MAX_TICKERS];
    size_t       tickerCount = config.count();
    for (size_t i = 0; i < tickerCount && i < MAX_TICKERS; i++) {
        tickers[i] = config.get(i);
    }

    String url;
    buildUrl(url, tickers, tickerCount);

    JsonDocument doc;
    if (!fetchJson(url, doc, http)) {
        return false;
    }

    return parsePricesJson(doc, outData, count, tickers, tickerCount);
}

bool fetch_history(const TickerConfig& ticker, HistoryBuffer& history, CryptoHttp* http) {
    String url = buildMarketChartUrl(ticker);

    JsonDocument doc;
    if (!fetchJson(url, doc, http)) {
        return false;
    }

    bool ok = parseHistoryJson(doc, history);
    if (!ok) {
        Serial.println("[HTTP] market_chart returned no prices");
        return false;
    }

    Serial.printf("[HTTP] Fetched %u history points for %s\n",
                  (unsigned)history.size(),
                  ticker.apiId.c_str());
    return history.size() > 0;
}

}  // namespace cryptoapp
