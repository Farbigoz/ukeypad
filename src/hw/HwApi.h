#ifndef UKEYPAD_HW_API_H
#define UKEYPAD_HW_API_H

#include <stddef.h>
#include <stdint.h>
#include "../TextWriter.h"

namespace Hw {

class Cdc : public TextWriter {
public:
    Cdc();
    void begin(uint32_t baud);
    bool connected() const;
    int available() const;
    int read();
    void flush();
};

extern Cdc cdc;

void gpioBeginInputPullup(uint8_t pin);
bool gpioReadPressed(uint8_t pin);

using ScanCallback = void (*)();
bool beginScanTimer(uint8_t timerNumber, uint16_t divider,
                   uint64_t alarmTicks, ScanCallback callback);
bool waitForScanEvent(uint32_t timeoutMs);
void signalScanEventFromIsr();

bool hidBegin();
bool hidPress(uint8_t usage);
void hidRelease(uint8_t usage);
void hidReleaseAll();
using HidDisconnectCallback = void (*)();
void hidOnDisconnect(HidDisconnectCallback callback);

bool storageRead(const char* nameSpace, const char* key, void* buffer,
                 size_t capacity, size_t& actual);
bool storageWrite(const char* nameSpace, const char* key,
                  const void* buffer, size_t length);

void enterBootloader();

} // namespace Hw

#endif
