// ============================================================================
//  USB HID keypad — main
//
//  Two modes, one firmware, selected at boot:
//
//    NORMAL MODE  (plug in without holding any switch):
//      hardware timer (DeviceProfile::SCAN_FREQUENCY_HZ Hz ISR)  ->  Keypad::scan()  ->  lock-free queue
//           ->  main loop: semaphore-wake  ->  HidKeyboard (immediate report)
//      CDC Serial port is inert; config commands are silently ignored.
//
//    CONFIG MODE  (hold ANY switch while plugging in):
//      scan timer is OFF (no keypresses sent).
//      CDC Serial port accepts text commands: bind / list / save / reset / help.
//      Saved binds live in NVS and survive power cycles.
//      Reboot (tap RESET without holding a key) to enter normal mode.
//
//  End-to-end latency in normal mode: contact closure -> scan (<=0.5 ms) ->
//  debounce (<=2 ms) -> USB report (host polls every 1 ms).
// ============================================================================

#include <Arduino.h>
#include <freertos/semphr.h>

#include "Keypad.h"
#include "HidKeyboard.h"
#include "Config.h"
#include "DeviceProfile.h"

// Hardware timer settings come from the selected device profile. The legacy
// Arduino timer API remains intentionally unchanged for this environment.
//  Semaphore: signalled from the scan ISR when >= 1 event was queued.
//  Lets the main loop sleep instead of busy-spinning, without adding latency.
static SemaphoreHandle_t s_eventSem = nullptr;

static Keypad      g_keypad;
static HidKeyboard g_hid;
static Config      g_config;
static hw_timer_t* g_scanTimer = nullptr;

// ---------------------------------------------------------------------------
//  Scan ISR — runs in hardware-timer interrupt context.
//  Only samples GPIOs, debounces and queues events. Never touches USB here
//  (TinyUSB is not ISR-safe); USB reports are sent from the main loop.
// ---------------------------------------------------------------------------
void ARDUINO_ISR_ATTR onScanTimer()
{
    if (g_keypad.scan() > 0) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(s_eventSem, &higherPriorityTaskWoken);
        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

void setup()
{
    // Bring up USB first (composite CDC + HID) so the host enumerates the
    // device before we decide mode and start producing events.
    g_hid.begin();

    // Init buttons + load default bindings into the live table.
    g_keypad.begin();

    // One unified operating mode: CDC configuration and HID output are
    // available together. A held button no longer changes the boot behavior.
    // Pass false so the old config-mode banner is not shown; Config::poll()
    // still services the CDC command channel in this unified mode.
    g_config.begin(g_keypad, false); // unified mode, not boot-selected config mode

    // The legacy boot gesture is intentionally no longer consulted.

    // Event semaphore (binary). Created before the timer starts so the ISR
    // never runs against a null handle.
    s_eventSem = xSemaphoreCreateBinary();

    // Start the same profile-defined scan timer in both modes. In config mode the
    // resulting debounced events are consumed by Config::processKeyEvents()
    // and never forwarded to HidKeyboard; this keeps diagnostics identical
    // to normal operation while preventing host keypresses.
    g_scanTimer = timerBegin(DeviceProfile::TIMER_NUMBER,
                             DeviceProfile::TIMER_DIVIDER,
                             true);
    timerAttachInterrupt(g_scanTimer, &onScanTimer, true);
    timerAlarmWrite(g_scanTimer, DeviceProfile::SCAN_ALARM_TICKS, true);
    timerAlarmEnable(g_scanTimer); // DeviceProfile::SCAN_FREQUENCY_HZ Hz
}

void loop()
{
    // Unified mode: CDC commands, test events, and HID output are available
    // at the same time. Configuration no longer suppresses keyboard reports.
    g_config.poll();

    // Block until the scan ISR signals an event (or 2 ms safety timeout), then
    // deliver every event to both the HID adapter and optional CDC test output.
    xSemaphoreTake(s_eventSem, pdMS_TO_TICKS(2));
    KeyEvent ev;
    while (g_keypad.getEvent(ev)) {
        g_hid.handleEvent(ev);
        g_config.processKeyEvent(ev);
    }
}
