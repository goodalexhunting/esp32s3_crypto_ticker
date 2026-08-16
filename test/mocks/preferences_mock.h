#pragma once

// In-memory mock of the Arduino Preferences (NVS) API used by
// src/wifi_mgr.cpp (wifi_creds namespace) and src/config_mgr.cpp
// (ticker_cfg namespace). Backed by a single global map so tests can
// pre-seed persisted state and verify what the code wrote.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"

class Preferences {
   public:
    Preferences() = default;

    bool begin(const char* ns, bool readOnly = false) {
        _ns        = ns != nullptr ? ns : "";
        _readOnly  = readOnly;
        _hasNbegin = true;
        return true;
    }

    void end() {
        _hasNbegin = false;
    }

    // --- Read ---
    String getString(const char* key, const String& defaultValue) const {
        auto it = store().find(makeKey(key));
        if (it == store().end()) return defaultValue;
        return String(it->second.c_str());
    }
    uint8_t getUChar(const char* key, uint8_t defaultValue) const {
        auto it = store().find(makeKey(key));
        if (it == store().end()) return defaultValue;
        return (uint8_t)std::stoul(it->second);
    }
    uint16_t getUShort(const char* key, uint16_t defaultValue) const {
        auto it = store().find(makeKey(key));
        if (it == store().end()) return defaultValue;
        return (uint16_t)std::stoul(it->second);
    }

    // --- Write ---
    size_t putString(const char* key, const String& value) {
        if (_readOnly) return 0;
        store()[makeKey(key)] = value.c_str();
        return value.length();
    }
    size_t putUChar(const char* key, uint8_t value) {
        if (_readOnly) return 0;
        store()[makeKey(key)] = std::to_string(value);
        return 1;
    }
    size_t putUShort(const char* key, uint16_t value) {
        if (_readOnly) return 0;
        store()[makeKey(key)] = std::to_string(value);
        return 2;
    }

    void remove(const char* key) {
        if (_readOnly) return;
        store().erase(makeKey(key));
    }

    // --- Test helpers ---
    static std::map<std::string, std::string>& store() {
        static std::map<std::string, std::string> s;
        return s;
    }
    static void clear() {
        store().clear();
    }

    // Direct namespace-prefixed access for seeding/verifying.
    static void put(const std::string& ns, const std::string& key, const std::string& value) {
        store()[ns + "::" + key] = value;
    }
    static std::string get(const std::string& ns, const std::string& key) {
        auto it = store().find(ns + "::" + key);
        return it != store().end() ? it->second : std::string();
    }

   private:
    std::string makeKey(const char* key) const {
        return _ns + "::" + (key != nullptr ? key : "");
    }

    std::string _ns;
    bool        _readOnly  = false;
    bool        _hasNbegin = false;
};