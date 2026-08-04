#include "Button.h"
#include "hw/HwApi.h"
#include "ButtonDebounce.h"

static inline bool fastReadPin(uint8_t pin)
{
    return Hw::gpioReadPressed(pin);
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
    Hw::gpioBeginInputPullup(pin);
}

// IRAM_ATTR is repeated on the definition (not only on the declaration in
// the header) to guarantee placement in the IRAM section: the function runs
// from the profile-defined scan ISR and must not depend on flash access.
ButtonEvent Button::update()
{
    // fastReadPin() already returns true while the switch is physically
    // actuated (pin pulled low).
    const bool raw = fastReadPin(_pin);

    return debounceStep(raw, _integrator, _maxIntegrator, _state);
}

bool Button::rawPressed() const
{
    return Hw::gpioReadPressed(_pin);
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
