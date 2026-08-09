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
    _btn1Raw        = digitalRead(PIN_BUTTON_1) == LOW;
    _btn1Active     = _btn1Raw;
    _btn1LastChange = millis();

    _btn2Raw        = digitalRead(PIN_BUTTON_2) == LOW;
    _btn2Active     = _btn2Raw;
    _btn2LastChange = millis();

    _buttonEvent = false;
    _buttonWhich = ButtonEvent::NONE;

    _stateChangedAtMs = millis();

    // Put the display into the full-brightness ON state. When disabled
    // (AP mode) the QR setup screen stays permanently lit.
    setState(DisplayPowerState::ON);
    Serial.printf("[POWER] Display power management %s\n",
                  _enabled ? "enabled" : "disabled (AP mode)");
}

void DisplayPower::handle() {
    // Always poll the buttons, even when power management is disabled
    // (AP mode), so navigation events are still available.
    const unsigned long now = millis();
    pollButtons(now);

    if (!_enabled) {
        return;
    }

    // A button press counts as user activity: wake the display.
    if (_buttonEvent) {
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

bool DisplayPower::debouncePin(
    uint8_t pin, bool& active, bool& lastRaw, unsigned long& lastChange, unsigned long now) {
    const bool raw = digitalRead(pin) == LOW;  // active-low

    if (raw != lastRaw) {
        lastRaw    = raw;
        lastChange = now;
    }

    if (raw != active && (now - lastChange) >= BUTTON_DEBOUNCE_MS) {
        active = raw;
        if (active) {
            // Rising edge (button pressed).
            return true;
        }
    }
    return false;
}

void DisplayPower::pollButtons(unsigned long now) {
    if (debouncePin(PIN_BUTTON_1, _btn1Active, _btn1Raw, _btn1LastChange, now)) {
        _buttonEvent = true;
        _buttonWhich = ButtonEvent::BUTTON_1;
        Serial.println("[POWER] Button 1 pressed");
    }
    if (debouncePin(PIN_BUTTON_2, _btn2Active, _btn2Raw, _btn2LastChange, now)) {
        _buttonEvent = true;
        _buttonWhich = ButtonEvent::BUTTON_2;
        Serial.println("[POWER] Button 2 pressed");
    }
}

bool DisplayPower::consumeButtonEvent(ButtonEvent& event) {
    if (!_buttonEvent) {
        event = ButtonEvent::NONE;
        return false;
    }
    event        = _buttonWhich;
    _buttonEvent = false;
    _buttonWhich = ButtonEvent::NONE;
    return true;
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

}  // namespace cryptoapp