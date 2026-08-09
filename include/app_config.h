#pragma once

#include <cstddef>
#include <cstdint>

//
// Central configuration for the ESP32-S3 Crypto Ticker.
// Adjust these values to customise the display, the CoinGecko
// endpoint, and the cryptocurrencies being tracked.
//

// ---------------------------------------------------------------------------
// Screen size (px)
// ---------------------------------------------------------------------------
// The T-Display S3 panel is 170x320 portrait; with setRotation(3) the
// effective landscape resolution is 320x170. Update these if the firmware
// is reused on a panel with different dimensions.
constexpr uint16_t SCREEN_WIDTH  = 320;
constexpr uint16_t SCREEN_HEIGHT = 170;

// ---------------------------------------------------------------------------
// CoinGecko API
// ---------------------------------------------------------------------------
constexpr char COINGECKO_URL[] = "https://api.coingecko.com/api/v3/simple/price?ids=";

// ---------------------------------------------------------------------------
// Tracked cryptocurrencies
// ---------------------------------------------------------------------------
// {label, CoinGecko API id, display colour (RGB565)}
// RGB565: 0bRRRRRGGGGGGBBBBB
struct CoinConfig {
    const char* label;  // Display label, e.g. "BTC"
    const char* apiId;  // CoinGecko id, e.g. "bitcoin"
    uint16_t    color;  // Brand colour for the label (RGB565)
};

constexpr CoinConfig COINS[] = {
    {"BTC", "bitcoin", 0xFFE0},  // yellow
    {"SOL", "solana", 0xF81F},   // purple
    {"SUI", "sui", 0x07FF},      // cyan
};

constexpr size_t NUM_COINS = sizeof(COINS) / sizeof(COINS[0]);

// ---------------------------------------------------------------------------
// Display power management
// ---------------------------------------------------------------------------
// The T-Display S3 backlight is PWM-driven. To prevent burn-in from a
// constantly-lit panel, the display dims after an idle period and turns
// fully off (panel sleep + zero backlight) after a longer idle period.
// Any of the wake buttons turns it back on and restarts the timers.
constexpr bool     DISPLAY_POWER_ENABLED   = true;
constexpr uint8_t  DISPLAY_FULL_BRIGHTNESS = 150;  // matches the previous hardcoded value
constexpr uint8_t  DISPLAY_DIM_BRIGHTNESS  = 10;
constexpr uint32_t DISPLAY_DIM_TIMEOUT_MS  = 30UL * 1000UL;   // ON -> DIMMED
constexpr uint32_t DISPLAY_OFF_TIMEOUT_MS  = 180UL * 1000UL;  // DIMMED -> OFF (3 min)

// T-Display S3 wake buttons (active-low, internal pull-ups).
constexpr uint8_t PIN_BUTTON_1 = 0;
constexpr uint8_t PIN_BUTTON_2 = 14;
