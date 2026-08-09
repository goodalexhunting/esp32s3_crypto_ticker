#pragma once

#include <cstddef>

/**
 * Fetch and parse current prices from CoinGecko.
 * Writes the parsed USD values into outValues (one per tracked coin).
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool fetch_prices(float* outValues, size_t count);