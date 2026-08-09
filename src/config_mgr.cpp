#include "config_mgr.h"

#include <Preferences.h>

namespace cryptoapp {

namespace {
constexpr char   NVS_NAMESPACE[]  = "ticker_cfg";
constexpr char   NVS_KEY_COUNT[]  = "count";
constexpr char   NVS_KEY_PREFIX[] = "t";
constexpr char   NVS_KEY_LABEL[]  = "label";
constexpr char   NVS_KEY_APIID[]  = "apiid";
constexpr char   NVS_KEY_QUOTE[]  = "quote";
constexpr char   NVS_KEY_COLOR[]  = "color";
constexpr size_t MAX_LABEL_LEN    = 16;
constexpr size_t MAX_APIID_LEN    = 32;
constexpr size_t MAX_QUOTE_LEN    = 8;
}  // namespace

ConfigManager::ConfigManager() = default;

void ConfigManager::begin() {
    load();
}

void ConfigManager::load() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        Serial.println("[CFG] NVS open (read) failed - using defaults");
        seedDefaults();
        return;
    }

    _count = prefs.getUChar(NVS_KEY_COUNT, 0);
    if (_count == 0 || _count > MAX_TICKERS) {
        prefs.end();
        seedDefaults();
        return;
    }

    bool valid = true;
    for (size_t i = 0; i < _count; i++) {
        String key        = String(NVS_KEY_PREFIX) + i;
        _tickers[i].label = prefs.getString((key + NVS_KEY_LABEL).c_str(), "");
        _tickers[i].apiId = prefs.getString((key + NVS_KEY_APIID).c_str(), "");
        _tickers[i].quote = prefs.getString((key + NVS_KEY_QUOTE).c_str(), "");
        _tickers[i].color = prefs.getUShort((key + NVS_KEY_COLOR).c_str(), 0xFFFF);

        if (_tickers[i].label.length() == 0 || _tickers[i].apiId.length() == 0 ||
            _tickers[i].quote.length() == 0) {
            valid = false;
            break;
        }
    }
    prefs.end();

    if (!valid) {
        Serial.println("[CFG] Stored config invalid - using defaults");
        seedDefaults();
    } else {
        Serial.printf("[CFG] Loaded %u tickers from NVS\n", (unsigned)_count);
    }
}

void ConfigManager::seedDefaults() {
    _count = NUM_DEFAULT_TICKERS;
    for (size_t i = 0; i < _count; i++) {
        _tickers[i].label = DEFAULT_TICKERS[i].label;
        _tickers[i].apiId = DEFAULT_TICKERS[i].apiId;
        _tickers[i].quote = DEFAULT_TICKERS[i].quote;
        _tickers[i].color = DEFAULT_TICKERS[i].color;
    }
    Serial.printf("[CFG] Seeded %u default tickers\n", (unsigned)_count);
}

bool ConfigManager::add(const String& label,
                        const String& apiId,
                        const String& quote,
                        uint16_t      color) {
    if (_count >= MAX_TICKERS) {
        Serial.println("[CFG] Add failed: ticker list full");
        return false;
    }

    String trimmedLabel = label;
    trimmedLabel.trim();
    String trimmedApiId = apiId;
    trimmedApiId.trim();
    String trimmedQuote = quote;
    trimmedQuote.trim();

    if (trimmedLabel.length() == 0 || trimmedApiId.length() == 0 || trimmedQuote.length() == 0) {
        Serial.println("[CFG] Add failed: empty field");
        return false;
    }

    // Reject duplicates (same apiId + quote).
    for (size_t i = 0; i < _count; i++) {
        if (_tickers[i].apiId.equalsIgnoreCase(trimmedApiId) &&
            _tickers[i].quote.equalsIgnoreCase(trimmedQuote)) {
            Serial.println("[CFG] Add failed: duplicate ticker");
            return false;
        }
    }

    _tickers[_count].label = trimmedLabel.substring(0, MAX_LABEL_LEN);
    _tickers[_count].apiId = trimmedApiId.substring(0, MAX_APIID_LEN);
    _tickers[_count].quote = trimmedQuote.substring(0, MAX_QUOTE_LEN);
    _tickers[_count].color = color;
    _count++;

    Serial.printf("[CFG] Added ticker: %s (%s/%s)\n",
                  _tickers[_count - 1].label.c_str(),
                  _tickers[_count - 1].apiId.c_str(),
                  _tickers[_count - 1].quote.c_str());
    return true;
}

bool ConfigManager::remove(size_t i) {
    if (i >= _count) {
        return false;
    }

    for (size_t j = i; j < _count - 1; j++) {
        _tickers[j] = _tickers[j + 1];
    }
    _count--;

    Serial.printf("[CFG] Removed ticker at index %u\n", (unsigned)i);
    return true;
}

bool ConfigManager::move(size_t from, size_t to) {
    if (from >= _count || to >= _count || from == to) {
        return false;
    }

    TickerConfig tmp = _tickers[from];
    if (from < to) {
        for (size_t j = from; j < to; j++) {
            _tickers[j] = _tickers[j + 1];
        }
    } else {
        for (size_t j = from; j > to; j--) {
            _tickers[j] = _tickers[j - 1];
        }
    }
    _tickers[to] = tmp;

    Serial.printf("[CFG] Moved ticker %u -> %u\n", (unsigned)from, (unsigned)to);
    return true;
}

void ConfigManager::save() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[CFG] NVS open (write) failed");
        return;
    }

    prefs.putUChar(NVS_KEY_COUNT, _count);
    for (size_t i = 0; i < _count; i++) {
        String key = String(NVS_KEY_PREFIX) + i;
        prefs.putString((key + NVS_KEY_LABEL).c_str(), _tickers[i].label);
        prefs.putString((key + NVS_KEY_APIID).c_str(), _tickers[i].apiId);
        prefs.putString((key + NVS_KEY_QUOTE).c_str(), _tickers[i].quote);
        prefs.putUShort((key + NVS_KEY_COLOR).c_str(), _tickers[i].color);
    }
    // Remove any stale entries beyond the new count.
    for (size_t i = _count; i < MAX_TICKERS; i++) {
        String key = String(NVS_KEY_PREFIX) + i;
        prefs.remove((key + NVS_KEY_LABEL).c_str());
        prefs.remove((key + NVS_KEY_APIID).c_str());
        prefs.remove((key + NVS_KEY_QUOTE).c_str());
        prefs.remove((key + NVS_KEY_COLOR).c_str());
    }
    prefs.end();

    Serial.printf("[CFG] Saved %u tickers to NVS\n", (unsigned)_count);
}

void ConfigManager::resetToDefaults() {
    seedDefaults();
    save();
}

}  // namespace cryptoapp