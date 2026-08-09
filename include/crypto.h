#pragma once

#include <Arduino.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>

/** Global display instance, defined in main.cpp. */
extern LGFX tft;

/**
 * Fetch and parse current prices, then redraw the coin cells.
 * Returns true if the fetch and parse succeeded (HTTP 200 + valid JSON).
 */
bool update_crypto();

/** Draw the static layout (header/footer/coin backgrounds). */
void render_layout(LovyanGFX& display);
