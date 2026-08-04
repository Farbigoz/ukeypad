#ifndef DEVICE_METADATA_H
#define DEVICE_METADATA_H

#include <stdint.h>
#include "TextWriter.h"
#include "Keypad.h"

struct DeviceMetadata {
    const char* model;
    const char* firmware;
    const char* usb;
    const char* inputType;
    uint16_t protocolVersion;
    uint16_t configVersion;
    uint16_t scanHz;
    uint8_t ledCount;
    bool supportsBinds;
    bool supportsTest;
    bool supportsDebounce;
    bool supportsStats;
};

extern const DeviceMetadata DEVICE_METADATA;

void printInfo(TextWriter& out);
void printDeviceDescription(const Keypad& keypad, TextWriter& out);
void printBindings(const Keypad& keypad, TextWriter& out);
void printHelp(TextWriter& out);

#endif // DEVICE_METADATA_H
