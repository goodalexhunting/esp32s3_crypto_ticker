#pragma once

#include <cstddef>

#include "config_mgr.h"
#include "history.h"

namespace cryptoapp {

/**
 * Fetch and parse current prices from CoinGecko.
 * Writes the parsed values into outValues (one per configured ticker).
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool fetch_prices(float* outValues, size_t count, const ConfigManager& config);

/**
 * Fetch historical price data for a single ticker from CoinGecko's
 * market_chart endpoint and populate a HistoryBuffer.
 * Returns true on success, false on any failure.
 */
bool fetch_history(const TickerConfig& ticker, HistoryBuffer& history);

}  // namespace cryptoapp