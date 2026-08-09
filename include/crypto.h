#pragma once

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
 * Fetch current prices and 24h percentage changes from CoinGecko.
 * Writes the parsed values into outData (one per configured ticker).
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool fetch_prices(PriceData* outData, size_t count, const ConfigManager& config);

/**
 * Fetch historical price data for a single ticker from CoinGecko's
 * market_chart endpoint and populate a HistoryBuffer.
 * Returns true on success, false on any failure.
 */
bool fetch_history(const TickerConfig& ticker, HistoryBuffer& history);

}  // namespace cryptoapp