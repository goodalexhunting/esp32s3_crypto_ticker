#include "crypto.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_config.h"

namespace cryptoapp {

namespace {

constexpr char COINGECKO_MARKET_CHART_URL[] =
    "https://api.coingecko.com/api/v3/coins/{id}/"
    "market_chart?vs_currency={quote}&days={days}";

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

String buildMarketChartUrl(const TickerConfig& ticker) {
    String url = COINGECKO_MARKET_CHART_URL;
    url.replace("{id}", ticker.apiId);
    url.replace("{quote}", ticker.quote);
    url.replace("{days}", String(HISTORY_BACKFILL_DAYS));
    return url;
}

bool httpGetJson(const String& url, JsonDocument& doc) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    http.begin(url);
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

}  // namespace

bool fetch_prices(float* outValues, size_t count, const ConfigManager& config) {
    String url;
    buildUrl(url, config);

    JsonDocument doc;
    if (!httpGetJson(url, doc)) {
        return false;
    }

    size_t n = count < config.count() ? count : config.count();
    for (size_t i = 0; i < n; i++) {
        const TickerConfig& t = config.get(i);
        outValues[i]          = doc[t.apiId][t.quote] | 0.0f;
    }
    return true;
}

bool fetch_history(const TickerConfig& ticker, HistoryBuffer& history) {
    String url = buildMarketChartUrl(ticker);

    JsonDocument doc;
    if (!httpGetJson(url, doc)) {
        return false;
    }

    JsonArray prices = doc["prices"].as<JsonArray>();
    if (prices.isNull() || prices.size() == 0) {
        Serial.println("[HTTP] market_chart returned no prices");
        return false;
    }

    history.reset();

    // The market_chart endpoint returns an array of [timestamp_ms, price].
    // We downsample to HISTORY_POINTS by taking every Nth point so the
    // buffer contains a representative time series without exceeding
    // the fixed allocation.
    size_t n    = prices.size();
    size_t step = (n > HISTORY_POINTS) ? (n / HISTORY_POINTS) : 1;
    for (size_t i = 0; i < n; i += step) {
        JsonArray point = prices[i].as<JsonArray>();
        if (point.size() >= 2) {
            history.push(point[1].as<float>());
        }
    }

    // If the step skipped the final (most recent) point, append it.
    if (n > 0 && (n - 1) % step != 0) {
        JsonArray last = prices[n - 1].as<JsonArray>();
        if (last.size() >= 2) {
            history.push(last[1].as<float>());
        }
    }

    Serial.printf("[HTTP] Fetched %u history points for %s\n",
                  (unsigned)history.size(),
                  ticker.apiId.c_str());
    return history.size() > 0;
}

}  // namespace cryptoapp