#ifndef BUTTON_DEBOUNCE_H
#define BUTTON_DEBOUNCE_H

#include <stdint.h>

enum class ButtonEvent : uint8_t;

ButtonEvent debounceStep(bool raw, uint8_t& integrator, uint8_t maxIntegrator, bool& state);

#endif // BUTTON_DEBOUNCE_H
