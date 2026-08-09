#pragma once

#include <Arduino.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>

/** Global display instance, defined in main.cpp. */
extern LGFX tft;

/**
 * Draw the static layout (header/footer/coin backgrounds).
 */
void render_layout(LovyanGFX& display);

/**
 * Store the latest prices and redraw the coin table.
 * Assumes prices are already fetched and valid.
 */
void update_prices_display(const float* values, size_t count);

/**
 * Clear the content area and draw a red message (e.g. error text).
 */
void show_message(const char* msg);