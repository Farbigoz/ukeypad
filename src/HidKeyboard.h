#ifndef HIDKEYBOARD_H
#define HIDKEYBOARD_H

#include <USB.h>
#include <USBHIDKeyboard.h>
#include "Keypad.h"

// ---------------------------------------------------------------------------
//  HidKeyboard — thin adapter over the ESP32 core's TinyUSB USBHIDKeyboard.
//  Responsibilities:
//    * start the USB HID device;
//    * translate KeyEvent(s) into raw HID reports.
//  It owns no scanning / debounce logic and knows nothing about GPIOs.
//  USBHIDKeyboard maintains an internal 6-key rollover report; pressRaw()/
//  releaseRaw() accept raw USB HID usage codes and send one report per
//  change -> minimal end-to-end latency.
//
//  `_refCount` tracks how many physical buttons currently hold each logical
//  usage. Duplicate bindings are therefore supported correctly: a logical
//  key is released only after its last physical button is released. The array
//  is 256 bytes so normal usages (0x01..0xA4) and modifiers (0xE0..0xE7) use
//  the same path.
// ---------------------------------------------------------------------------
class HidKeyboard {
public:
    // Register the HID interface and bring the USB device up.
    void begin();

    // Apply one key event to the HID report and (if needed) send it.
    void handleEvent(const KeyEvent& e);

    // Clear the HID report and all duplicate-binding ownership counts. Called
    // when the USB host disconnects so no key can remain logically stuck.
    void releaseAll();

private:
    static void onUsbEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    USBHIDKeyboard _kbd;
    uint8_t        _refCount[256];
};

#endif // HIDKEYBOARD_H
