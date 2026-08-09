#pragma once

#include <Arduino.h>

#include <cstddef>

#include "app_config.h"
#include "config_mgr.h"


namespace cryptoapp {

/**
 * Single source of truth for the current display cycle.
 *
 * cycle[0] = the existing table view.
 * cycle[i] (i > 0) = configured ticker at index (i - 1).
 *
 * The cycle count is derived dynamically from the number of configured
 * tickers. Navigation wraps in both directions.
 */
class DisplayCycle {
   public:
    explicit DisplayCycle(const ConfigManager& config) : _config(config) {
        clamp();
    }

    /** Total number of cycles: 1 (table) + number of configured tickers. */
    size_t total() const {
        return 1 + _config.count();
    }

    /** Current cycle index, always in [0, total()-1]. */
    size_t current() const {
        return _current;
    }

    /** True if the current cycle is the table view. */
    bool isTable() const {
        return _current == 0;
    }

    /** Ticker index for the current cycle, or 0 if on the table view. */
    size_t tickerIndex() const {
        return _current > 0 ? _current - 1 : 0;
    }

    void next() {
        if (total() <= 1) return;
        _current = (_current + 1) % total();
        logChange("next");
    }

    void previous() {
        if (total() <= 1) return;
        _current = (_current == 0) ? total() - 1 : _current - 1;
        logChange("prev");
    }

    /** Clamp current to a valid range. Call after config changes. */
    void clamp() {
        if (_current >= total()) {
            _current = total() - 1;
            logChange("clamp");
        }
    }

   private:
    void logChange(const char* why) {
        Serial.printf("[CYCLE] %s -> %u/%u\n", why, (unsigned)_current, (unsigned)total());
    }

    const ConfigManager& _config;
    size_t               _current = 0;
};

}  // namespace cryptoapp