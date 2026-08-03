#include "Button.h"
#include <Arduino.h>
#include "soc/gpio_struct.h"

// ---------------------------------------------------------------------------
//  fastReadPin
//  Reads a GPIO pin state. digitalRead() is used here instead of direct
//  register access because the GPIO register struct layout varies across
//  ESP32 Arduino core versions (GPIO.in is uint32_t on some, a union on
//  others; GPIO.in1 likewise). At 2000 Hz × 6 pins = 12000 calls/sec,
//  each digitalRead costs ~1 µs; the total remains negligible for the ISR.
//  flash cache is always on at runtime (we never erase/write flash from the
//  ISR), so digitalRead from IRAM context is safe.
// ---------------------------------------------------------------------------
static inline IRAM_ATTR bool fastReadPin(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

Button::Button()
    : _pin(0)
    , _integrator(0)
    , _maxIntegrator(DeviceProfile::DEFAULT_DEBOUNCE_SAMPLES)
    , _state(false)
{
}

void Button::begin(uint8_t pin)
{
    _pin        = pin;
    _integrator = 0;
    _maxIntegrator = DeviceProfile::DEFAULT_DEBOUNCE_SAMPLES;
    _state      = false;

    // Each switch is wired between its GPIO and GND. The internal pull-up
    // holds the pin HIGH while open; pressing the switch pulls it LOW.
    //   released  -> pin reads HIGH (1)
    //   pressed  -> pin reads LOW  (0)
    pinMode(pin, INPUT_PULLUP);
}

// IRAM_ATTR is repeated on the definition (not only on the declaration in
// the header) to guarantee placement in the IRAM section: the function runs
// from the profile-defined scan ISR and must not depend on flash access.
IRAM_ATTR ButtonEvent Button::update()
{
    // raw == true while the switch is physically actuated (pin pulled low).
    const bool raw = (fastReadPin(_pin) == 0);

    ButtonEvent event = ButtonEvent::None;

    if (raw) {
        // Saturate the integrator upwards.
        if (_integrator < _maxIntegrator) {
            ++_integrator;
        }
        // Emit Press exactly once, on the rising edge of the debounced state.
        if (_integrator == _maxIntegrator && !_state) {
            _state = true;
            event = ButtonEvent::Press;
        }
    } else {
        // Saturate the integrator downwards.
        if (_integrator > 0) {
            --_integrator;
        }
        // Emit Release exactly once, on the falling edge.
        if (_integrator == 0 && _state) {
            _state = false;
            event = ButtonEvent::Release;
        }
    }

    return event;
}

bool Button::rawPressed() const
{
    return digitalRead(_pin) == LOW;
}

void Button::setDebounce(uint8_t samples)
{
    if (samples == 0) {
        samples = 1;
    }
    _maxIntegrator = samples;
    if (_integrator > _maxIntegrator) {
        _integrator = _maxIntegrator;
    }
}
