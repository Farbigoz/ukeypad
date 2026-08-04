#include "HidKeyboard.h"
#include "hw/HwApi.h"
#include <string.h>

static HidKeyboard* s_hidInstance = nullptr;

void HidKeyboard::begin()
{
    // The object may be reused after a soft restart in a test harness; clear
    // all ownership counts before accepting events.
    memset(_refCount, 0, sizeof(_refCount));
    s_hidInstance = this;

    Hw::hidOnDisconnect(&HidKeyboard::onUsbDisconnect);
    Hw::hidBegin();
}

void HidKeyboard::onUsbDisconnect()
{
    if (s_hidInstance != nullptr) s_hidInstance->releaseAll();
}

void HidKeyboard::releaseAll()
{
    Hw::hidReleaseAll();
    memset(_refCount, 0, sizeof(_refCount));
}

void HidKeyboard::handleEvent(const KeyEvent& e)
{
    // pressRaw()/releaseRaw() take a raw USB HID usage code. KeyEvent carries
    // a typed HidKeycode, so cast it to the underlying uint8_t here.
    const uint8_t code = static_cast<uint8_t>(e.keyCode);

    if (e.type == KeyEventType::Press) {
        // USBHIDKeyboard itself suppresses duplicate keycodes. We also keep
        // ownership explicitly so the matching Release from one physical
        // button cannot release a logical key still held by another button.
        if (_refCount[code] == 0) {
            // pressRaw() returns 0 if its six non-modifier slots are full or
            // if the usage is invalid. Do not increment ownership in that
            // case: otherwise a later Release could leave state inconsistent.
            if (!Hw::hidPress(code)) return;
        }
        if (_refCount[code] != 0xFF) {
            ++_refCount[code];
        }
        return;
    }

    // Ignore an unmatched Release. This protects against stale events after
    // a queue overflow and avoids emitting a spurious HID release.
    if (_refCount[code] == 0) {
        return;
    }

    --_refCount[code];
    if (_refCount[code] == 0) {
        Hw::hidRelease(code);
    }
}
