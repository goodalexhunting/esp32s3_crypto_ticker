#pragma once

#include <ArduinoJson.h>

#include <cstddef>

#include "app_config.h"
#include "config_mgr.h"
#include "crypto.h"
#include "history.h"

namespace cryptoapp {

// CoinGecko market_chart endpoint template for a single ticker.
// Substituted by buildMarketChartUrl().
constexpr char COINGECKO_MARKET_CHART_URL[] =
    "https://api.coingecko.com/api/v3/coins/{id}/"
    "market_chart?vs_currency={quote}&days={days}";

/**
 * Build the CoinGecko simple/price API URL for the given ticker list.
 * Tickers are comma-joined, the quote currency of the first ticker is
 * used (falling back to "usd" for an empty list), and 24h percentage
 * change is always requested.
 */
void buildUrl(String& url, const TickerConfig* tickers, size_t count);

/** Build the CoinGecko market_chart URL for a single ticker. */
String buildMarketChartUrl(const TickerConfig& ticker);

/**
 * Parse a CoinGecko simple/price response into outData (one entry per
 * ticker). Missing coins/fields default to 0.0. Writes at most
 * min(count, tickerCount) entries. Always returns true once the JSON
 * document is a valid object (matching fetch_prices behaviour).
 */
bool parsePricesJson(const JsonDocument& doc,
                     PriceData*          outData,
                     size_t              count,
                     const TickerConfig* tickers,
                     size_t              tickerCount);

/**
 * Parse a CoinGecko market_chart response into a HistoryBuffer.
 * Downsampling: when the response has more than HISTORY_POINTS samples,
 * every Nth point is taken and the final (most recent) point is always
 * appended. Returns false if the "prices" array is missing/empty.
 */
bool parseHistoryJson(const JsonDocument& doc, HistoryBuffer& history);

}  // namespace cryptoapp