#include "../HwApi.h"
#include <Arduino.h>
#include <cstring>
#include "../../../test/mocks/HwApiMock.h"

uint8_t g_storageReadData[MOCK_STORAGE_CAPACITY] = {};
size_t g_storageReadLength = 0;
bool g_storageReadOk = true;
uint8_t g_storageWriteData[MOCK_STORAGE_CAPACITY] = {};
size_t g_storageWriteLength = 0;
bool g_storageWriteCalled = false;

void resetStorageMock()
{
    std::memset(g_storageReadData, 0, sizeof(g_storageReadData));
    g_storageReadLength = 0;
    g_storageReadOk = true;
    std::memset(g_storageWriteData, 0, sizeof(g_storageWriteData));
    g_storageWriteLength = 0;
    g_storageWriteCalled = false;
}

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
bool storageRead(const char*, const char*, void* buffer, size_t capacity,
                 size_t& actual)
{
    if (!g_storageReadOk) {
        actual = 0;
        return false;
    }
    actual = g_storageReadLength;
    const size_t copied = actual < capacity ? actual : capacity;
    std::memcpy(buffer, g_storageReadData, copied);
    return true;
}

bool storageWrite(const char*, const char*, const void* buffer, size_t length)
{
    g_storageWriteCalled = true;
    g_storageWriteLength = length;
    if (length > MOCK_STORAGE_CAPACITY) return false;
    std::memcpy(g_storageWriteData, buffer, length);
    return true;
}
void enterBootloader() {}

} // namespace Hw
