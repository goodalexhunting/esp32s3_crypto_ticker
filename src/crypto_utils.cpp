#include "crypto_utils.h"

#include <Arduino.h>

namespace cryptoapp {

void buildUrl(String& url, const TickerConfig* tickers, size_t count) {
    url = COINGECKO_URL;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) url += ",";
        url += tickers[i].apiId;
    }
    url += "&vs_currencies=";
    // Use the quote currency of the first ticker. All tickers share the
    // same quote currency in practice, but we build the URL from the
    // first configured ticker's quote.
    if (count > 0) {
        url += tickers[0].quote;
    } else {
        url += "usd";
    }
    // Include 24h percentage change in the response.
    url += "&include_24hr_change=true";
}

String buildMarketChartUrl(const TickerConfig& ticker) {
    String url = COINGECKO_MARKET_CHART_URL;
    url.replace("{id}", ticker.apiId);
    url.replace("{quote}", ticker.quote);
    url.replace("{days}", String(HISTORY_BACKFILL_DAYS));
    return url;
}

bool parsePricesJson(const JsonDocument& doc,
                     PriceData*          outData,
                     size_t              count,
                     const TickerConfig* tickers,
                     size_t              tickerCount) {
    size_t n = count < tickerCount ? count : tickerCount;
    for (size_t i = 0; i < n; i++) {
        const TickerConfig& t    = tickers[i];
        JsonObjectConst     coin = doc[t.apiId].as<JsonObjectConst>();
        outData[i].price         = coin[t.quote] | 0.0f;
        outData[i].change24h     = coin[t.quote + "_24h_change"] | 0.0f;
    }
    return true;
}

bool parseHistoryJson(const JsonDocument& doc, HistoryBuffer& history) {
    JsonArrayConst prices = doc["prices"].as<JsonArrayConst>();
    if (prices.isNull() || prices.size() == 0) {
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
        JsonArrayConst point = prices[i].as<JsonArrayConst>();
        if (point.size() >= 2) {
            history.push(point[1].as<float>());
        }
    }

    // If the step skipped the final (most recent) point, append it.
    if (n > 0 && (n - 1) % step != 0) {
        JsonArrayConst last = prices[n - 1].as<JsonArrayConst>();
        if (last.size() >= 2) {
            history.push(last[1].as<float>());
        }
    }

    return history.size() > 0;
}

}  // namespace cryptoapp