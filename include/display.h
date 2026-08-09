#pragma once

#include <Arduino.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>

#include "api_health.h"
#include "config_mgr.h"
#include "crypto.h"
#include "history.h"

/** Global display instance, defined in main.cpp. */
extern LGFX tft;

namespace cryptoapp {

/**
 * Draw the static layout (header/footer/coin backgrounds).
 */
void render_layout(LovyanGFX& display);

/**
 * Store the latest prices and redraw the coin table.
 * Assumes prices are already fetched and valid.
 */
void update_prices_display(const PriceData* data, size_t count, const ConfigManager& config);

/**
 * Draw a single-ticker detail view: label, current price, 24h change,
 * and a historical price graph.
 */
void update_ticker_display(const TickerConfig&  ticker,
                           const PriceData&     data,
                           const HistoryBuffer& history);

/**
 * Draw the API status indicator in the top-left corner.
 */
void draw_api_status(ApiStatus status);

/**
 * Clear the content area and draw a red message (e.g. error text).
 */
void show_message(const char* msg);

}  // namespace cryptoapp