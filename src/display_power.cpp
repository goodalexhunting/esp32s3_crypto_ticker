#include "display_power.h"

#include <Arduino.h>

#include "app_config.h"
#include "display.h"

namespace cryptoapp {
namespace {

// Button debounce window in milliseconds.
constexpr uint8_t BUTTON_DEBOUNCE_MS = 30;

}  // namespace

void DisplayPower::begin(bool enabled) {
    _enabled = enabled;

    // The wake buttons are active-low with internal pull-ups.
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);

    // Seed the debounce state so the initial level never looks like a press.
    const bool level  = (digitalRead(PIN_BUTTON_1) == LOW) || (digitalRead(PIN_BUTTON_2) == LOW);
    _rawLevel         = level;
    _debounced        = level;
    _pressEvent       = false;
    _lastChangeMs     = millis();
    _stateChangedAtMs = millis();

    // Put the display into the full-brightness ON state. When disabled
    // (AP mode) the QR setup screen stays permanently lit.
    setState(DisplayPowerState::ON);
    Serial.printf("[POWER] Display power management %s\n",
                  _enabled ? "enabled" : "disabled (AP mode)");
}

void DisplayPower::handle() {
    if (!_enabled) {
        return;
    }

    const unsigned long now = millis();
    pollButtons(now);

    if (_pressEvent) {
        _pressEvent = false;
        notifyActivity();
        return;
    }

    // Idle timeout ladder. millis() subtraction is rollover-safe.
    if (_state == DisplayPowerState::ON && (now - _stateChangedAtMs) >= DISPLAY_DIM_TIMEOUT_MS) {
        setState(DisplayPowerState::DIMMED);
    } else if (_state == DisplayPowerState::DIMMED &&
               (now - _stateChangedAtMs) >= DISPLAY_OFF_TIMEOUT_MS) {
        setState(DisplayPowerState::OFF);
    }
}

void DisplayPower::notifyActivity() {
    if (!_enabled) {
        return;
    }

    if (_state != DisplayPowerState::ON) {
        // Waking from dimmed or off: mark a one-shot wake event so the
        // caller can trigger an immediate refresh with fresh data.
        _wakePending = true;
    }

    setState(DisplayPowerState::ON);
}

bool DisplayPower::consumeWakeEvent() {
    const bool pending = _wakePending;
    _wakePending       = false;
    return pending;
}

bool DisplayPower::consumeSleepEvent() {
    const bool pending = _sleepPending;
    _sleepPending      = false;
    return pending;
}

void DisplayPower::setState(DisplayPowerState newState) {
    if (_state == newState && newState != DisplayPowerState::ON) {
        return;
    }

    _state            = newState;
    _stateChangedAtMs = millis();

    // Any transition away from OFF cancels a pending deep-sleep request,
    // so a button press that lands on the same cycle as the OFF timeout
    // always wins.
    if (newState != DisplayPowerState::OFF) {
        _sleepPending = false;
    }

    switch (newState) {
        case DisplayPowerState::ON:
            tft.wakeup();
            tft.setBrightness(DISPLAY_FULL_BRIGHTNESS);
            Serial.println("[POWER] Display ON");
            break;
        case DisplayPowerState::DIMMED:
            tft.setBrightness(DISPLAY_DIM_BRIGHTNESS);
            Serial.println("[POWER] Display dimmed");
            break;
        case DisplayPowerState::OFF:
            tft.setBrightness(0);
            tft.sleep();
            _sleepPending = true;
            Serial.println("[POWER] Display OFF");
            break;
    }
}

void DisplayPower::pollButtons(unsigned long now) {
    const bool raw = (digitalRead(PIN_BUTTON_1) == LOW) || (digitalRead(PIN_BUTTON_2) == LOW);

    if (raw != _rawLevel) {
        _rawLevel     = raw;
        _lastChangeMs = now;
    }

    if (raw != _debounced && (now - _lastChangeMs) >= BUTTON_DEBOUNCE_MS) {
        _debounced = raw;
        if (_debounced) {
            _pressEvent = true;
        }
    }
}

}  // namespace cryptoapp