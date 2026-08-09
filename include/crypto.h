#pragma once

#include <cstddef>

#include "config_mgr.h"

namespace cryptoapp {

/**
 * Fetch and parse current prices from CoinGecko.
 * Writes the parsed values into outValues (one per configured ticker).
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool fetch_prices(float* outValues, size_t count, const ConfigManager& config);

}  // namespace cryptoapp