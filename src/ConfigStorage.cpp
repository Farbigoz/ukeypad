#include "ConfigStorage.h"
#include "Keypad.h"
#include "FirmwareVersion.h"
#include <Preferences.h>

namespace {

constexpr const char* NVS_NAMESPACE = "ukeypad";
constexpr const char* NVS_KEY = "bindings";
constexpr uint16_t CRC16_INITIAL = 0xFFFF;
constexpr uint16_t CRC16_POLYNOMIAL = 0x1021;
constexpr uint16_t CRC16_TOP_BIT = 0x8000;
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

uint16_t crc16(const uint8_t* data, size_t length)
{
    uint16_t crc = CRC16_INITIAL;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & CRC16_TOP_BIT)
                ? static_cast<uint16_t>((crc << 1) ^ CRC16_POLYNOMIAL)
                : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

bool isValidHidCode(uint8_t raw)
{
    return raw == HID_NONE ||
           (raw >= HID_FIRST_DEFINED && raw <= HID_LAST_DEFINED) ||
           (raw >= HID_FIRST_MODIFIER && raw <= HID_LAST_MODIFIER);
}

} // namespace

StorageResult loadBindings(Keypad& keypad)
{
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return StorageResult::OpenFailed;

    uint8_t record[NVS_RECORD_SIZE];
    const size_t bytesRead = prefs.getBytes(NVS_KEY, record, sizeof(record));
    prefs.end();
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
    if (crc16(record, crcOffset) != storedCrc) return StorageResult::BadCrc;

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
    const uint16_t calculatedCrc = crc16(record, crcOffset);
    record[crcOffset] = static_cast<uint8_t>(calculatedCrc & 0xFF);
    record[crcOffset + 1] = static_cast<uint8_t>(calculatedCrc >> 8);

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return StorageResult::OpenFailed;
    const size_t bytesWritten = prefs.putBytes(NVS_KEY, record, sizeof(record));
    prefs.end();
    return bytesWritten == sizeof(record) ? StorageResult::Loaded : StorageResult::WriteFailed;
}
