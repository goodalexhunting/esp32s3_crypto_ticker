#pragma once

#include <Arduino.h>

namespace cryptoapp {

enum class DisplayPowerState : uint8_t { ON, DIMMED, OFF };

enum class ButtonEvent : uint8_t {
    BUTTON_1,  // GPIO0  (previous)
    BUTTON_2,  // GPIO14 (next)
    NONE,
};

/**
 * Single layer owning the display backlight/panel power state and the
 * debounced button input.
 *
 * State machine: ON -> DIMMED -> OFF based on idle time since the last
 * user input, and back to ON on a button press (or any future input via
 * notifyActivity()). While OFF the panel is sent to sleep and the PWM
 * backlight is set to 0, which prevents burn-in from a constant-on
 * display.
 *
 * The layer is a no-op when begun with enabled=false (used for AP mode,
 * where the QR setup screen must stay fully lit).
 */
class DisplayPower {
   public:
    /**
     * Configure the layer and put the display into the full-brightness ON
     * state. When `enabled` is false the display stays permanently on and
     * handle() does nothing (but buttons are still polled).
     */
    void begin(bool enabled);

    /**
     * Call from loop(): poll the buttons and run the idle timeout
     * state machine.
     */
    void handle();

    /**
     * Returns true exactly once per button press. The pressed button is
     * written to `event`. Use this to detect navigation events.
     */
    bool consumeButtonEvent(ButtonEvent& event);

    /**
     * Register user activity (e.g. touch). Resets the idle timers and
     * wakes the display if it is dimmed or off.
     */
    void notifyActivity();

    /**
     * Returns true exactly once per wake transition (OFF->ON or
     * DIMMED->ON), so the caller can force an immediate data refresh.
     */
    bool consumeWakeEvent();

    /**
     * Returns true exactly once when the display transitions to OFF,
     * so the caller can put the whole device into deep sleep.
     */
    bool consumeSleepEvent();

    bool isOff() const {
        return _state == DisplayPowerState::OFF;
    }

    bool isDimmed() const {
        return _state == DisplayPowerState::DIMMED;
    }

    DisplayPowerState state() const {
        return _state;
    }

   private:
    void setState(DisplayPowerState newState);
    void pollButtons(unsigned long now);
    bool debouncePin(
        uint8_t pin, bool& active, bool& lastRaw, unsigned long& lastChange, unsigned long now);

    DisplayPowerState _state        = DisplayPowerState::ON;
    bool              _enabled      = false;
    bool              _wakePending  = false;
    bool              _sleepPending = false;

    // Debounce state per button
    bool          _btn1Raw        = false;
    bool          _btn1Active     = false;
    unsigned long _btn1LastChange = 0;

    bool          _btn2Raw        = false;
    bool          _btn2Active     = false;
    unsigned long _btn2LastChange = 0;

    // Pending button event
    bool        _buttonEvent = false;
    ButtonEvent _buttonWhich = ButtonEvent::NONE;

    // millis() when the current state was entered (or when activity
    // restarted the ON idle clock).
    unsigned long _stateChangedAtMs = 0;
};

}  // namespace cryptoapp