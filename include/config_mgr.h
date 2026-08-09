#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "app_config.h"


namespace cryptoapp {

/**
 * A single configured ticker.
 */
struct TickerConfig {
    String   label;  // Display label, e.g. "BTC"
    String   apiId;  // CoinGecko id, e.g. "bitcoin"
    String   quote;  // Quote currency, e.g. "usd" or "usdc"
    uint16_t color;  // Brand colour for the label (RGB565)
};

/**
 * Central configuration manager.
 *
 * Owns the list of configured tickers and persists it to NVS.
 * The display and market-data layers read their ticker list from here,
 * so the web configuration can modify it at runtime.
 */
class ConfigManager {
   public:
    ConfigManager();

    /**
     * Load the ticker configuration from NVS. If none exists, seeds
     * with the compile-time defaults. Safe to call multiple times.
     */
    void begin();

    /** Number of currently configured tickers. */
    size_t count() const {
        return _count;
    }

    /** Maximum number of tickers that can be configured. */
    size_t maxCount() const {
        return MAX_TICKERS;
    }

    /** Get the ticker at index i. Assumes i < count(). */
    const TickerConfig& get(size_t i) const {
        return _tickers[i];
    }

    /**
     * Add a ticker. Returns true on success, false if the list is full
     * or the ticker already exists (same apiId+quote).
     */
    bool add(const String& label, const String& apiId, const String& quote, uint16_t color);

    /** Remove the ticker at index i. Returns true on success. */
    bool remove(size_t i);

    /**
     * Move a ticker from index from to index to (0-based).
     * Returns true on success.
     */
    bool move(size_t from, size_t to);

    /** Persist the current configuration to NVS. */
    void save();

    /** Reset to the compile-time default tickers. */
    void resetToDefaults();

   private:
    TickerConfig _tickers[MAX_TICKERS];
    size_t       _count = 0;

    void load();
    void seedDefaults();
};

}  // namespace cryptoapp