#include "../HwApi.h"
#include <Arduino.h>

namespace Hw {

namespace {
void cdcPrintText(void*, const char*) {}
void cdcPrintNumber(void*, long long, int) {}
void cdcPrintlnText(void*, const char*) {}
void cdcPrintlnNumber(void*, long long, int) {}
}

Cdc::Cdc()
    : TextWriter(nullptr, cdcPrintText, cdcPrintNumber,
                 cdcPrintlnText, cdcPrintlnNumber)
{
}

Cdc cdc;

void Cdc::begin(uint32_t) {}
bool Cdc::connected() const { return true; }
int Cdc::available() const { return 0; }
int Cdc::read() { return -1; }
void Cdc::flush() {}

void gpioBeginInputPullup(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }
bool gpioReadPressed(uint8_t pin) { return digitalRead(pin) == LOW; }
bool beginScanTimer(uint8_t, uint16_t, uint64_t, ScanCallback) { return true; }
bool waitForScanEvent(uint32_t) { return false; }
void signalScanEventFromIsr() {}
bool hidBegin() { return true; }
bool hidPress(uint8_t) { return true; }
void hidRelease(uint8_t) {}
void hidReleaseAll() {}
void hidOnDisconnect(HidDisconnectCallback) {}
bool storageRead(const char*, const char*, void*, size_t, size_t& actual) { actual = 0; return true; }
bool storageWrite(const char*, const char*, const void*, size_t) { return true; }
void enterBootloader() {}

} // namespace Hw
