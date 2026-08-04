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
    // Bring up USB first (composite CDC + HID) before producing events.
    g_hid.begin();

    // Init buttons + load default bindings into the live table.
    g_keypad.begin();

    // CDC commands and HID output are available together.
    g_config.begin(g_keypad);

    // Event semaphore (binary). Created before the timer starts so the ISR
    // never runs against a null handle.
    s_eventSem = xSemaphoreCreateBinary();

    // Start the profile-defined scan timer. Debounced events are forwarded to
    // both HID and the optional CDC test logger from the main loop.
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
