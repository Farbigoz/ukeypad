#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>
#include "Button.h"
#include "HidKeycode.h"
#include "DeviceProfile.h"

enum class KeyEventType : uint8_t {
    Press,
    Release
};

struct KeyEvent {
    KeyEventType type;
    HidKeycode keyCode;
};

struct KeyBinding {
    uint8_t pin;
    HidKeycode hidCode;
};

// Keypad owns the profile-defined physical inputs, scan pass, and event queue.
// It knows no USB details; main() pulls events and forwards them to HID.
class Keypad {
public:
    static constexpr uint8_t KEY_COUNT = DeviceProfile::INPUT_COUNT;

    void begin();
    uint8_t scan();
    bool getEvent(KeyEvent& out);

    void setBinding(uint8_t slot, HidKeycode code);
    HidKeycode getBinding(uint8_t slot) const;
    uint8_t getPin(uint8_t slot) const;
    void loadDefaultBindings();

    void setDebounce(uint8_t samples);
    uint8_t debounce() const;
    bool rawPressed(uint8_t slot) const;
    uint32_t scanCount() const { return _scanCount; }
    uint32_t eventCount() const { return _eventCount; }
    uint32_t overflowCount() const { return _overflowCount; }
    uint8_t maxQueueDepth() const { return _maxQueueDepth; }
    void clearStats();

private:
    static constexpr uint8_t QUEUE_SIZE = 32;
    static constexpr uint8_t QUEUE_MASK = QUEUE_SIZE - 1;

    Button _buttons[KEY_COUNT];
    KeyEvent _queue[QUEUE_SIZE];
    volatile uint8_t _head;
    volatile uint8_t _tail;
    volatile uint32_t _scanCount;
    volatile uint32_t _eventCount;
    volatile uint32_t _overflowCount;
    volatile uint8_t _maxQueueDepth;
};

#endif // KEYPAD_H
