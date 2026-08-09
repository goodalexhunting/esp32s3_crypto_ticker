#pragma once

#include <Arduino.h>

namespace cryptoapp {

enum class DisplayPowerState : uint8_t { ON, DIMMED, OFF };

/**
 * Single layer owning the display backlight/panel power state.
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
     * handle() does nothing.
     */
    void begin(bool enabled);

    /**
     * Call from loop(): poll the wake buttons and run the idle timeout
     * state machine.
     */
    void handle();

    /**
     * Register user activity (future touch/motion inputs). Resets the idle
     * timers and wakes the display if it is dimmed or off.
     */
    void notifyActivity();

    /**
     * Returns true exactly once per wake transition (OFF->ON or
     * DIMMED->ON), so the caller can force an immediate data refresh.
     */
    bool consumeWakeEvent();

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

    DisplayPowerState _state       = DisplayPowerState::ON;
    bool              _enabled     = false;
    bool              _wakePending = false;

    // Button debounce state
    bool          _pressEvent   = false;
    bool          _rawLevel     = false;
    bool          _debounced    = false;
    unsigned long _lastChangeMs = 0;

    // millis() when the current state was entered (or when activity
    // restarted the ON idle clock).
    unsigned long _stateChangedAtMs = 0;
};

}  // namespace cryptoapp