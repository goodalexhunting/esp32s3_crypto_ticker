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