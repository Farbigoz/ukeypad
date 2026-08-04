#ifndef UKEYPAD_TEST_ARDUINO_H
#define UKEYPAD_TEST_ARDUINO_H

#include <cctype>
#include <cstddef>
#include <cstdint>

#define IRAM_ATTR
#define ARDUINO_ISR_ATTR
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2

extern uint8_t g_mockPinState;
inline int digitalRead(uint8_t) { return g_mockPinState; }
inline void pinMode(uint8_t, uint8_t) {}

#endif
