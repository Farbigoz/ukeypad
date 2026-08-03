#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "DeviceProfile.h"

// ---------------------------------------------------------------------------
//  Integrator debounce
// ---------------------------------------------------------------------------
// The scan ISR samples each switch at DeviceProfile::SCAN_FREQUENCY_HZ.
// An integrator counter ramps toward the raw level and commits a debounced
// state change only after it reaches the configured threshold. The default
// threshold comes from DeviceProfile; CDC config may override it at runtime.
//
// Default stability window:
//   DEFAULT_DEBOUNCE_SAMPLES / SCAN_FREQUENCY_HZ seconds.

// Result of one debounce step.
enum class ButtonEvent : uint8_t {
    None    = 0,
    Press   = 1,
    Release = 2
};

// ---------------------------------------------------------------------------
//  Button — handles exactly ONE physical switch:
//    * GPIO sampling
//    * integrator debounce
//  It knows NOTHING about USB / HID. It only reports debounced transitions.
// ---------------------------------------------------------------------------
class Button {
public:
    Button();

    // Configure the GPIO. Called once from Keypad::begin().
    void begin(uint8_t pin);

    // Sample the pin and run one integrator step. Intended to be called from
    // the scan ISR at the profile-defined frequency.
    ButtonEvent update();

    // Current debounced state (true == pressed).
    bool isPressed() const { return _state; }
    bool rawPressed() const;

    // Change debounce threshold at runtime. Clamped to 1..255.
    void setDebounce(uint8_t samples);
    uint8_t debounce() const { return _maxIntegrator; }

private:
    uint8_t _pin;
    uint8_t _integrator;
    uint8_t _maxIntegrator;
    bool _state;
};

#endif // BUTTON_H
