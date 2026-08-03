#ifndef UKEYPAD_FIRMWARE_VERSION_H
#define UKEYPAD_FIRMWARE_VERSION_H

#include <stdint.h>

// Firmware release version. Keep the string and numeric components aligned.
namespace FirmwareVersion {

constexpr uint8_t MAJOR = 0;
constexpr uint8_t MINOR = 2;
constexpr uint8_t PATCH = 0;
constexpr const char* STRING = "0.2.0";

// CDC command/response contract version.
constexpr uint8_t PROTOCOL = 1;

// Binary NVS binding-record format version.
constexpr uint8_t CONFIG_FORMAT = 1;

} // namespace FirmwareVersion

#endif // UKEYPAD_FIRMWARE_VERSION_H
