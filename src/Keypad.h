#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>
#include "Button.h"
#include "HidKeycode.h"

//  Event delivered to the HID layer. `keyCode` is the named HID usage to
//  send (see HidKeycode). Keypad does not know how it is consumed by USB.
enum class KeyEventType : uint8_t {
    Press,
    Release
};

struct KeyEvent {
    KeyEventType type;
    HidKeycode   keyCode;
};

//  One physical switch -> one named HID keycode.
struct KeyBinding {
    uint8_t    pin;
    HidKeycode hidCode;
};

// ---------------------------------------------------------------------------
//  Keypad — owns the 6 Buttons, performs the fast GPIO scan from the hardware
//  timer ISR, and pushes debounced transitions into a lock-free SPSC queue.
//  It knows no USB details; main() pulls events and forwards them to HID.
// ---------------------------------------------------------------------------
class Keypad {
public:
    static constexpr uint8_t KEY_COUNT = 6;

    //  GPIO -> HID usage code layout:
    //    GPIO4  -> Z   (0x1D)   GPIO15 -> F13 (0x68)
    //    GPIO5  -> X   (0x1B)   GPIO16 -> F14 (0x69)
    //    GPIO6  -> C   (0x06)
    //    GPIO7  -> V   (0x19)
    void begin();

    //  One scan pass. Call from the hardware-timer ISR at >= 2000 Hz.
    //  Returns the number of events pushed this pass (used to wake main()).
    //  (IRAM_ATTR is applied on the definition in Keypad.cpp, not here.)
    uint8_t scan();

    //  Pull one queued event (called from main loop / task context).
    //  Returns false when the queue is empty.
    bool getEvent(KeyEvent& out);

    // --- runtime bind configuration (see Config) ---
    //  Reassign the HID keycode for one slot. Pins stay fixed (hardware).
    //  HidKeycode is a single byte, so writing it is atomic on the ESP32 —
    //  the scan ISR reads either the old or the new value, never a torn one,
    //  so this is safe to call while scanning runs (no timer pause needed).
    void       setBinding(uint8_t slot, HidKeycode code);
    HidKeycode getBinding(uint8_t slot) const;

    //  Restore the compiled-in default bindings (used by the `reset` command
    //  and as the fallback when NVS has nothing saved).
    void loadDefaultBindings();

    //  Boot-time raw check: true if any switch is currently held (pin LOW).
    //  Used to decide config vs normal mode at power-on. Slow (digitalRead)
    //  is fine here — it is called once, before the scan timer starts.
    bool anySwitchHeld() const;

    // Runtime diagnostics/configuration (called from the main task).
    void setDebounce(uint8_t samples);
    uint8_t debounce() const;
    bool rawPressed(uint8_t slot) const;
    uint32_t scanCount() const { return _scanCount; }
    uint32_t eventCount() const { return _eventCount; }
    uint32_t overflowCount() const { return _overflowCount; }
    uint8_t maxQueueDepth() const { return _maxQueueDepth; }
    void clearStats();

private:
    static constexpr uint8_t QUEUE_SIZE = 32;   // power of two
    static constexpr uint8_t QUEUE_MASK = QUEUE_SIZE - 1;

    Button           _buttons[KEY_COUNT];
    KeyEvent         _queue[QUEUE_SIZE];
    volatile uint8_t _head;   // written by ISR (producer)
    volatile uint8_t _tail;   // written by main loop (consumer)
    volatile uint32_t _scanCount;
    volatile uint32_t _eventCount;
    volatile uint32_t _overflowCount;
    volatile uint8_t _maxQueueDepth; // high-water mark, 0..QUEUE_SIZE-1

};

#endif // KEYPAD_H
