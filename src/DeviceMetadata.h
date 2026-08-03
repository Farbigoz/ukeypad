#ifndef DEVICE_METADATA_H
#define DEVICE_METADATA_H

#include <stdint.h>
#include <Arduino.h>
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

void printInfo(Print& out);
void printDeviceDescription(const Keypad& keypad, Print& out);
void printBindings(const Keypad& keypad, Print& out);
void printHelp(Print& out);

#endif // DEVICE_METADATA_H
