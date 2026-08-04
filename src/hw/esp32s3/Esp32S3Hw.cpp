#include "../HwApi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <freertos/semphr.h>
#include "esp32-hal-tinyusb.h"

namespace {

void cdcPrintText(void*, const char* value) { Serial.print(value); }
void cdcPrintNumber(void*, long long value, int base) { Serial.print(value, base); }
void cdcPrintlnText(void*, const char* value) { Serial.println(value); }
void cdcPrintlnNumber(void*, long long value, int base) { Serial.println(value, base); }

USBHIDKeyboard s_keyboard;
SemaphoreHandle_t s_scanSemaphore = nullptr;
hw_timer_t* s_scanTimer = nullptr;
Hw::HidDisconnectCallback s_disconnectCallback = nullptr;
Hw::ScanCallback s_scanCallback = nullptr;

void ARDUINO_ISR_ATTR scanTimerCallback()
{
    if (s_scanCallback != nullptr) s_scanCallback();
}

} // namespace

namespace Hw {

Cdc::Cdc()
    : TextWriter(nullptr, cdcPrintText, cdcPrintNumber,
                 cdcPrintlnText, cdcPrintlnNumber)
{
}

Cdc cdc;

void Cdc::begin(uint32_t baud) { Serial.begin(baud); }
bool Cdc::connected() const { return static_cast<bool>(Serial); }
int Cdc::available() const { return Serial.available(); }
int Cdc::read() { return Serial.read(); }
void Cdc::flush() { Serial.flush(); }

void gpioBeginInputPullup(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }
bool gpioReadPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

bool beginScanTimer(uint8_t timerNumber, uint16_t divider,
                    uint64_t alarmTicks, ScanCallback callback)
{
    s_scanCallback = callback;
    s_scanSemaphore = xSemaphoreCreateBinary();
    if (s_scanSemaphore == nullptr) return false;
    s_scanTimer = timerBegin(timerNumber, divider, true);
    if (s_scanTimer == nullptr) return false;
    timerAttachInterrupt(s_scanTimer, callback, true);
    timerAlarmWrite(s_scanTimer, alarmTicks, true);
    timerAlarmEnable(s_scanTimer);
    return true;
}

bool waitForScanEvent(uint32_t timeoutMs)
{
    if (s_scanSemaphore == nullptr) return false;
    return xSemaphoreTake(s_scanSemaphore, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void signalScanEventFromIsr()
{
    if (s_scanSemaphore == nullptr) return;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_scanSemaphore, &higherPriorityTaskWoken);
    if (higherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

bool hidBegin()
{
    s_keyboard.begin();
    USB.begin();
    return true;
}

bool hidPress(uint8_t usage) { return s_keyboard.pressRaw(usage) != 0; }
void hidRelease(uint8_t usage) { s_keyboard.releaseRaw(usage); }
void hidReleaseAll() { s_keyboard.releaseAll(); }

void hidOnDisconnect(HidDisconnectCallback callback)
{
    s_disconnectCallback = callback;
    USB.onEvent(ARDUINO_USB_STOPPED_EVENT,
                [](void*, esp_event_base_t, int32_t id, void*) {
                    if (id == ARDUINO_USB_STOPPED_EVENT &&
                        s_disconnectCallback != nullptr) {
                        s_disconnectCallback();
                    }
                });
}

bool storageRead(const char* nameSpace, const char* key, void* buffer,
                 size_t capacity, size_t& actual)
{
    Preferences prefs;
    if (!prefs.begin(nameSpace, true)) { actual = 0; return false; }
    actual = prefs.getBytes(key, buffer, capacity);
    prefs.end();
    return true;
}

bool storageWrite(const char* nameSpace, const char* key,
                  const void* buffer, size_t length)
{
    Preferences prefs;
    if (!prefs.begin(nameSpace, false)) return false;
    const size_t written = prefs.putBytes(key, buffer, length);
    prefs.end();
    return written == length;
}

void enterBootloader() { usb_persist_restart(RESTART_BOOTLOADER); }

} // namespace Hw
