#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

// ---------------------------------------------------------------------------
//  Integrator debounce parameters
// ---------------------------------------------------------------------------
//  The scan ISR samples each switch at a fixed rate (2000 Hz here).
//  An "integrator" counter ramps toward the raw level and only commits a
//  debounced state change once it saturates. This rejects contact bounce
//  WITHOUT any fixed time delay / delay() — the effective debounce window is
//  INTEGRATOR_MAX scan periods.
//
//  At 2000 Hz:  INTEGRATOR_MAX = 4  ->  ~2 ms of stability required.
//  MX-style switches typically bounce for <5 ms; raise this value for noisier
//  switches, lower it (min 1) to shave latency on very clean switches.
static constexpr uint8_t INTEGRATOR_MAX = 4;

//  Result of one debounce step.
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
    // the scan ISR at a fixed rate. Returns Press/Release on the debounced
    // state edge, or None when the debounced state did not change.
    // (IRAM_ATTR is applied on the definition in Button.cpp, not here —
    // the macro is not yet available when this header is parsed.)
    ButtonEvent update();

    // Current debounced state (true == pressed).
    bool isPressed() const { return _state; }
    bool rawPressed() const;

    // Change debounce threshold at runtime. Clamped to 1..255.
    void setDebounce(uint8_t samples);
    uint8_t debounce() const { return _maxIntegrator; }

private:
    uint8_t _pin;            // GPIO number
    uint8_t _integrator;     // running count of consecutive stable samples
    uint8_t _maxIntegrator;  // saturation threshold
    bool    _state;          // current debounced state (true = pressed)
};

#endif // BUTTON_H
