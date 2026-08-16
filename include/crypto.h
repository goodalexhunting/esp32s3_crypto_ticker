#pragma once

#include <ArduinoJson.h>

#include <cstddef>

#include "config_mgr.h"
#include "history.h"

namespace cryptoapp {

/**
 * Result of a price fetch: current price and 24h percentage change
 * for each configured ticker.
 */
struct PriceData {
    float price;      // Current price in the ticker's quote currency
    float change24h;  // 24h percentage change (e.g. 2.31 = +2.31%)
};

/**
 * Injectable HTTP transport used to fetch and parse a JSON document.
 *
 * The firmware passes nullptr (the default) and fetch_prices/fetch_history
 * use the real HTTPClient + WiFi stack. Host-side tests pass a fake
 * implementation returning fixture JSON, so the URL building + JSON
 * parsing path can be tested end-to-end without network access.
 */
struct CryptoHttp {
    virtual ~CryptoHttp() = default;

    /**
     * Perform a GET on url and deserialize the response body into doc.
     * Returns true only if the request succeeded (HTTP 200) and the body
     * was valid JSON.
     */
    virtual bool getJson(const String& url, JsonDocument& doc) = 0;
};

/**
 * Fetch current prices and 24h percentage changes from CoinGecko.
 * Writes the parsed values into outData (one per configured ticker).
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool fetch_prices(PriceData*           outData,
                  size_t               count,
                  const ConfigManager& config,
                  CryptoHttp*          http = nullptr);

/**
 * Fetch historical price data for a single ticker from CoinGecko's
 * market_chart endpoint and populate a HistoryBuffer.
 * Returns true on success, false on any failure.
 */
bool fetch_history(const TickerConfig& ticker, HistoryBuffer& history, CryptoHttp* http = nullptr);

}  // namespace cryptoapp
