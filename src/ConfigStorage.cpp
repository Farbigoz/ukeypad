#include "ConfigStorage.h"
#include "Keypad.h"
#include "FirmwareVersion.h"
#include "hw/HwApi.h"
#include "Crc16.h"

namespace {

constexpr const char* NVS_NAMESPACE = "ukeypad";
constexpr const char* NVS_KEY = "bindings";
constexpr uint16_t NVS_MAGIC = 0x4B50;
constexpr uint8_t NVS_VERSION = FirmwareVersion::CONFIG_FORMAT;
constexpr uint8_t NVS_HEADER_SIZE = 4;
constexpr uint8_t NVS_DEBOUNCE_SIZE = 1;
constexpr uint8_t NVS_CRC_SIZE = 2;
constexpr uint8_t NVS_PAYLOAD_SIZE = Keypad::KEY_COUNT + NVS_DEBOUNCE_SIZE;
constexpr uint8_t NVS_RECORD_SIZE = NVS_HEADER_SIZE + NVS_PAYLOAD_SIZE + NVS_CRC_SIZE;

constexpr uint8_t HID_NONE = 0x00;
constexpr uint8_t HID_FIRST_DEFINED = 0x04;
constexpr uint8_t HID_LAST_DEFINED = 0xA4;
constexpr uint8_t HID_FIRST_MODIFIER = 0xE0;
constexpr uint8_t HID_LAST_MODIFIER = 0xE7;

} // namespace

StorageResult loadBindings(Keypad& keypad)
{
    uint8_t record[NVS_RECORD_SIZE];
    size_t bytesRead = 0;
    if (!Hw::storageRead(NVS_NAMESPACE, NVS_KEY, record, sizeof(record), bytesRead)) {
        return StorageResult::OpenFailed;
    }
    if (bytesRead != sizeof(record)) {
        return bytesRead == 0 ? StorageResult::Missing : StorageResult::SizeMismatch;
    }

    const uint16_t magic = static_cast<uint16_t>(record[0]) |
                           (static_cast<uint16_t>(record[1]) << 8);
    if (magic != NVS_MAGIC) return StorageResult::BadMagic;
    if (record[2] != NVS_VERSION) return StorageResult::BadVersion;
    if (record[3] != Keypad::KEY_COUNT) return StorageResult::BadLength;

    const uint8_t crcOffset = NVS_HEADER_SIZE + NVS_PAYLOAD_SIZE;
    const uint16_t storedCrc = static_cast<uint16_t>(record[crcOffset]) |
                               (static_cast<uint16_t>(record[crcOffset + 1]) << 8);
    if (crc16Ccitt(record, crcOffset) != storedCrc) return StorageResult::BadCrc;

    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        if (!isValidHidCode(record[NVS_HEADER_SIZE + i])) {
            return StorageResult::BadHidCode;
        }
    }

    const uint8_t debounce = record[NVS_HEADER_SIZE + Keypad::KEY_COUNT];
    if (debounce == 0) return StorageResult::BadLength;

    // Validate the complete record before changing any live settings.
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        keypad.setBinding(i, static_cast<HidKeycode>(record[NVS_HEADER_SIZE + i]));
    }
    keypad.setDebounce(debounce);
    return StorageResult::Loaded;
}

StorageResult saveBindings(const Keypad& keypad)
{
    uint8_t record[NVS_RECORD_SIZE];
    record[0] = static_cast<uint8_t>(NVS_MAGIC & 0xFF);
    record[1] = static_cast<uint8_t>(NVS_MAGIC >> 8);
    record[2] = NVS_VERSION;
    record[3] = Keypad::KEY_COUNT;

    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        record[NVS_HEADER_SIZE + i] = static_cast<uint8_t>(keypad.getBinding(i));
    }
    record[NVS_HEADER_SIZE + Keypad::KEY_COUNT] = keypad.debounce();

    const uint8_t crcOffset = NVS_HEADER_SIZE + NVS_PAYLOAD_SIZE;
    const uint16_t calculatedCrc = crc16Ccitt(record, crcOffset);
    record[crcOffset] = static_cast<uint8_t>(calculatedCrc & 0xFF);
    record[crcOffset + 1] = static_cast<uint8_t>(calculatedCrc >> 8);

    return Hw::storageWrite(NVS_NAMESPACE, NVS_KEY, record, sizeof(record))
        ? StorageResult::Loaded : StorageResult::WriteFailed;
}
