// ============================================================================
//  USB HID keypad — main
//
//  Unified operation: the profile-defined timer scans and debounces inputs,
//  the SPSC queue wakes the main loop, and each event is sent to HID plus the
//  optional CDC test logger. CDC commands are always available alongside HID.
// ============================================================================

// Contact closure -> scan (<=0.5 ms) -> debounce (about 2 ms by default) ->
// USB report (host polls every 1 ms).
// ============================================================================

//
#include <Arduino.h>

#include "Keypad.h"
#include "HidKeyboard.h"
#include "Config.h"
#include "DeviceProfile.h"
#include "hw/HwApi.h"

static Keypad      g_keypad;
static HidKeyboard g_hid;
static Config      g_config;

// ---------------------------------------------------------------------------
//  Scan ISR — runs in hardware-timer interrupt context.
//  Only samples GPIOs, debounces and queues events. Never touches USB here
//  (TinyUSB is not ISR-safe); USB reports are sent from the main loop.
// ---------------------------------------------------------------------------
void onScanTimer()
{
    g_keypad.scan();
}

void setup()
{
    // Bring up USB first (composite CDC + HID) before producing events.
    g_hid.begin();

    // Init buttons + load default bindings into the live table.
    g_keypad.begin();

    // CDC commands and HID output are available together.
    g_config.begin(g_keypad);

    // Start the profile-defined scan timer through the selected hardware
    // backend. Debounced events are forwarded from the main loop.
    Hw::beginScanTimer(DeviceProfile::TIMER_NUMBER,
                       DeviceProfile::TIMER_DIVIDER,
                       DeviceProfile::SCAN_ALARM_TICKS,
                       &onScanTimer);
}

void loop()
{
    // Unified mode: CDC commands, test events, and HID output are available
    // at the same time. Configuration no longer suppresses keyboard reports.
    g_config.poll();

    // Block until the scan ISR signals an event (or 2 ms safety timeout), then
    // deliver every event to both the HID adapter and optional CDC test output.
    Hw::waitForScanEvent(2);
    KeyEvent ev;
    while (g_keypad.getEvent(ev)) {
        g_hid.handleEvent(ev);
        g_config.processKeyEvent(ev);
    }
}
