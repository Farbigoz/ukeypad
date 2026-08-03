// ============================================================================
//  USB HID keypad — main
//
//  Two modes, one firmware, selected at boot:
//
//    NORMAL MODE  (plug in without holding any switch):
//      hardware timer (2000 Hz ISR)  ->  Keypad::scan()  ->  lock-free queue
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

//  Hardware timer (core 2.x API): APB clock = 80 MHz, divider = 80 gives a
//  1 MHz tick (1 tick = 1 us). Alarm every 500 ticks -> 2000 Hz scan.
static constexpr uint8_t  TIMER_NUM   = 0;     // hardware timer 0
static constexpr uint16_t TIMER_DIV   = 80;    // 80 MHz / 80 = 1 MHz
static constexpr uint64_t SCAN_ALARM  = 500;   // 500 us -> 2000 Hz

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

    // Boot mode: hold ANY switch while plugging in -> config mode (CDC
    // command channel, HID output suppressed). Otherwise -> normal keyboard mode.
    const bool configMode = g_keypad.anySwitchHeld();

    // Load saved bindings from NVS (overrides defaults) in both modes, and
    // prepare the Serial config interface if in config mode.
    g_config.begin(g_keypad, configMode);

    // Event semaphore (binary). Created before the timer starts so the ISR
    // never runs against a null handle.
    s_eventSem = xSemaphoreCreateBinary();

    // Start the same 2000 Hz scan timer in both modes. In config mode the
    // resulting debounced events are consumed by Config::processKeyEvents()
    // and never forwarded to HidKeyboard; this keeps diagnostics identical
    // to normal operation while preventing host keypresses.
    g_scanTimer = timerBegin(TIMER_NUM, TIMER_DIV, true); // timer0, 1 MHz, countUp
    timerAttachInterrupt(g_scanTimer, &onScanTimer, true); // edge=true
    timerAlarmWrite(g_scanTimer, SCAN_ALARM, true);        // 500 us, autoreload
    timerAlarmEnable(g_scanTimer);                        // 2000 Hz
}

void loop()
{
    if (g_config.isConfigMode()) {
        // Config mode uses the same timer, GPIO scan and integrator debounce
        // as normal mode. Consume every event here, but do not send it to HID.
        g_config.processKeyEvents();
        g_config.poll();
        xSemaphoreTake(s_eventSem, pdMS_TO_TICKS(2));
        g_config.processKeyEvents();
        return;
    }

    // Normal mode: block until the scan ISR signals an event (or 2 ms safety
    // timeout), then drain the queue into HID reports.
    if (xSemaphoreTake(s_eventSem, pdMS_TO_TICKS(2)) == pdTRUE) {
        KeyEvent ev;
        while (g_keypad.getEvent(ev)) {
            g_hid.handleEvent(ev);
        }
    } else {
        // Timeout with no semaphore: defensively re-check the queue in case
        // an event was queued but the "give" was lost (e.g. ISR fired while
        // the semaphore was already full).
        KeyEvent ev;
        while (g_keypad.getEvent(ev)) {
            g_hid.handleEvent(ev);
        }
    }
}
