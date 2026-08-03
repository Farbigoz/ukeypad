#include "Keypad.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Default (compiled-in) bindings. `reset` / first boot copy these into the
//  live BINDINGS table. Kept separate so the defaults are always recoverable.
//    GPIO4 -> Z   GPIO5 -> X   GPIO6 -> C   GPIO7 -> V
//    GPIO15 -> F13   GPIO16 -> F14
//  Order must match the scan loop; index == button index.
// ---------------------------------------------------------------------------
static const KeyBinding DEFAULT_BINDINGS[Keypad::KEY_COUNT] = {
    { 4u,  HidKeycode::Z   }, // GPIO4  -> Z
    { 5u,  HidKeycode::X   }, // GPIO5  -> X
    { 6u,  HidKeycode::C   }, // GPIO6  -> C
    { 7u,  HidKeycode::V   }, // GPIO7  -> V
    { 15u, HidKeycode::F13 }, // GPIO15 -> F13
    { 16u, HidKeycode::F14 }, // GPIO16 -> F14
};

//  Live bindings — mutable so the config channel can reassign keycodes at
//  runtime. Pins are fixed (hardware); only hidCode is ever changed, and a
//  uint8_t write is atomic on the ESP32, so the scan ISR can read this
//  concurrently without tearing. Resides in RAM (not const) but is only
//  mutated by `bind`/`reset` from the main loop, never from the ISR.
static KeyBinding BINDINGS[Keypad::KEY_COUNT];

void Keypad::begin()
{
    _head = 0;
    _tail = 0;
    _scanCount = 0;
    _eventCount = 0;
    _overflowCount = 0;
    _maxQueueDepth = 0;
    // Start from the compiled-in defaults; Config may overwrite hidCodes
    // from NVS immediately after this.
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        BINDINGS[i] = DEFAULT_BINDINGS[i];
        _buttons[i].begin(BINDINGS[i].pin);
    }
}

// IRAM_ATTR repeated on the definition for the same reason as Button::update:
// this runs from the scan ISR.
IRAM_ATTR uint8_t Keypad::scan()
{
    ++_scanCount;
    uint8_t pushed = 0;

    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        const ButtonEvent ev = _buttons[i].update();
        if (ev == ButtonEvent::None) {
            continue;
        }

        // Lock-free SPSC enqueue. Single producer (this ISR) / single
        // consumer (main loop). A 32-slot ring with only 6 keys can never
        // fill in practice, but we still guard against overflow.
        const uint8_t next = static_cast<uint8_t>((_head + 1) & QUEUE_MASK);
        if (next == _tail) {
            // Queue full: drop this event. Keep a diagnostic counter.
            ++_overflowCount;
            continue;
        }

        _queue[_head].type    = (ev == ButtonEvent::Press)
                                 ? KeyEventType::Press
                                 : KeyEventType::Release;
        _queue[_head].keyCode  = BINDINGS[i].hidCode;

        // Publish the entry before moving the head (full memory barrier) so
        // the consumer never observes an advanced head without its payload.
        __sync_synchronize();
        _head = next;
        ++_eventCount;
        ++pushed;

        const uint8_t depth = static_cast<uint8_t>((_head - _tail) & QUEUE_MASK);
        if (depth > _maxQueueDepth) {
            _maxQueueDepth = depth;
        }
    }

    return pushed;
}

bool Keypad::getEvent(KeyEvent& out)
{
    if (_head == _tail) {
        return false; // empty
    }

    out = _queue[_tail];

    // Acknowledge consumption before advancing tail (barrier matches the
    // producer side in scan()).
    __sync_synchronize();
    _tail = static_cast<uint8_t>((_tail + 1) & QUEUE_MASK);
    return true;
}

// --- runtime bind configuration (called from main loop, never from ISR) -----

void Keypad::setBinding(uint8_t slot, HidKeycode code)
{
    if (slot < KEY_COUNT) {
        BINDINGS[slot].hidCode = code;  // atomic single-byte write
    }
}

HidKeycode Keypad::getBinding(uint8_t slot) const
{
    return (slot < KEY_COUNT) ? BINDINGS[slot].hidCode : HidKeycode::None;
}

void Keypad::loadDefaultBindings()
{
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        BINDINGS[i] = DEFAULT_BINDINGS[i];
    }
}

bool Keypad::anySwitchHeld() const
{
    // digitalRead is fine at boot — fast enough for a one-time check.
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        if (digitalRead(BINDINGS[i].pin) == LOW) {
            return true;
        }
    }
    return false;
}

void Keypad::setDebounce(uint8_t samples)
{
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        _buttons[i].setDebounce(samples);
    }
}

uint8_t Keypad::debounce() const
{
    return _buttons[0].debounce();
}

bool Keypad::rawPressed(uint8_t slot) const
{
    return slot < KEY_COUNT && _buttons[slot].rawPressed();
}

void Keypad::clearStats()
{
    _scanCount = 0;
    _eventCount = 0;
    _overflowCount = 0;
    _maxQueueDepth = 0;
}
