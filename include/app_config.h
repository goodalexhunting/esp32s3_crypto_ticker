#pragma once

#include <cstddef>
#include <cstdint>

//
// Central configuration for the ESP32-S3 Crypto Ticker.
// Adjust these values to customise the display, the CoinGecko
// endpoint, and the default cryptocurrencies being tracked.
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
// mDNS hostname
// ---------------------------------------------------------------------------
// The device advertises itself as <MDNS_HOSTNAME>.local on the local network.
// This is the single authoritative location for the hostname.
constexpr char MDNS_HOSTNAME[] = "crypto-ticker";

// ---------------------------------------------------------------------------
// CoinGecko API
// ---------------------------------------------------------------------------
constexpr char COINGECKO_URL[] = "https://api.coingecko.com/api/v3/simple/price?ids=";

// ---------------------------------------------------------------------------
// Default tracked cryptocurrencies
// ---------------------------------------------------------------------------
// {label, CoinGecko API id, quote currency, display colour (RGB565)}
// RGB565: 0bRRRRRGGGGGGBBBBB
struct DefaultTicker {
    const char* label;  // Display label, e.g. "BTC"
    const char* apiId;  // CoinGecko id, e.g. "bitcoin"
    const char* quote;  // Quote currency, e.g. "usd" or "usdc"
    uint16_t    color;  // Brand colour for the label (RGB565)
};

constexpr DefaultTicker DEFAULT_TICKERS[] = {
    {"BTC", "bitcoin", "usd", 0xFFE0},  // yellow
    {"SOL", "solana", "usd", 0xF81F},   // purple
    {"SUI", "sui", "usd", 0x07FF},      // cyan
};

constexpr size_t NUM_DEFAULT_TICKERS = sizeof(DEFAULT_TICKERS) / sizeof(DEFAULT_TICKERS[0]);
constexpr size_t MAX_TICKERS         = 8;

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

// ---------------------------------------------------------------------------
// Device deep sleep
// ---------------------------------------------------------------------------
// When the display turns fully OFF, the whole device enters deep sleep:
// the WiFi radio and the main loop are powered down to minimise draw.
// A press of either wake button reboots the ESP32, which reconnects to
// WiFi and refreshes the crypto prices via the normal boot path.
constexpr bool DEVICE_DEEP_SLEEP_ENABLED = true;

// EXT1 wake mask for the wake buttons (both are RTC GPIOs on the S3).
// GPIO0 and GPIO14, active-low with internal pull-ups.
constexpr uint64_t DEEP_SLEEP_WAKEUP_MASK = (1ULL << PIN_BUTTON_1) | (1ULL << PIN_BUTTON_2);