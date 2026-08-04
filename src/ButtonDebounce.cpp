#include "ButtonDebounce.h"
#include "Button.h"

ButtonEvent debounceStep(bool raw, uint8_t& integrator, uint8_t maxIntegrator, bool& state)
{
    ButtonEvent event = ButtonEvent::None;
    if (raw) {
        if (integrator < maxIntegrator) ++integrator;
        if (integrator == maxIntegrator && !state) {
            state = true;
            event = ButtonEvent::Press;
        }
    } else {
        if (integrator > 0) --integrator;
        if (integrator == 0 && state) {
            state = false;
            event = ButtonEvent::Release;
        }
    }
    return event;
}
