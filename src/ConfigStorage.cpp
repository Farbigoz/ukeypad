#include "ConfigStorage.h"
#include "Keypad.h"
#include "FirmwareVersion.h"
#include <Preferences.h>

namespace {

// NVS location. These names are part of the firmware's storage contract.
constexpr const char* NVS_NAMESPACE = "ukeypad";
constexpr const char* NVS_KEY = "bindings";

// CRC-16/CCITT-FALSE parameters.
constexpr uint16_t CRC16_INITIAL = 0xFFFF;
constexpr uint16_t CRC16_POLYNOMIAL = 0x1021;
constexpr uint16_t CRC16_TOP_BIT = 0x8000;

// Versioned binding-record header.
//
//   0..1  magic (little-endian "KP")
//   2     format version
//   3     payload length
//   4..9  HID code for each physical slot
//   10..11 CRC-16 over bytes 0..9
//
// Keep the record layout explicit and derived from KEY_COUNT. This makes a
// change to the physical device size visible at compile time.
constexpr uint16_t NVS_MAGIC = 0x4B50;
constexpr uint8_t NVS_VERSION = FirmwareVersion::CONFIG_FORMAT;
constexpr uint8_t NVS_MAGIC_OFFSET = 0;
constexpr uint8_t NVS_VERSION_OFFSET = 2;
constexpr uint8_t NVS_LENGTH_OFFSET = 3;
constexpr uint8_t NVS_HEADER_SIZE = 4;
constexpr uint8_t NVS_CRC_SIZE = 2;
constexpr uint8_t NVS_RECORD_SIZE =
    NVS_HEADER_SIZE + Keypad::KEY_COUNT + NVS_CRC_SIZE;

// Valid USB HID Keyboard/Keypad usage ranges represented by HidKeycode.
constexpr uint8_t HID_NONE = 0x00;
constexpr uint8_t HID_FIRST_DEFINED = 0x04;
constexpr uint8_t HID_LAST_DEFINED = 0xA4;
constexpr uint8_t HID_FIRST_MODIFIER = 0xE0;
constexpr uint8_t HID_LAST_MODIFIER = 0xE7;

// Calculate CRC-16/CCITT-FALSE without a lookup table. Records are tiny, so
// the table-free implementation keeps flash usage and dependencies minimal.
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
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return StorageResult::OpenFailed;
    }

    uint8_t record[NVS_RECORD_SIZE];
    const size_t bytesRead = prefs.getBytes(NVS_KEY, record, sizeof(record));
    prefs.end();

    if (bytesRead != sizeof(record)) {
        // A raw six-byte record from an earlier development build is not a
        // supported format and must not be partially interpreted as valid.
        return bytesRead == 0 ? StorageResult::Missing
                              : StorageResult::SizeMismatch;
    }

    const uint16_t magic = static_cast<uint16_t>(record[NVS_MAGIC_OFFSET]) |
                           (static_cast<uint16_t>(record[NVS_MAGIC_OFFSET + 1]) << 8);
    if (magic != NVS_MAGIC) {
        return StorageResult::BadMagic;
    }

    if (record[NVS_VERSION_OFFSET] != NVS_VERSION) {
        return StorageResult::BadVersion;
    }

    if (record[NVS_LENGTH_OFFSET] != Keypad::KEY_COUNT) {
        return StorageResult::BadLength;
    }

    const uint8_t crcOffset = NVS_HEADER_SIZE + Keypad::KEY_COUNT;
    const uint16_t storedCrc = static_cast<uint16_t>(record[crcOffset]) |
                               (static_cast<uint16_t>(record[crcOffset + 1]) << 8);
    const uint16_t calculatedCrc =
        crc16(record, NVS_HEADER_SIZE + Keypad::KEY_COUNT);
    if (calculatedCrc != storedCrc) {
        return StorageResult::BadCrc;
    }

    // Validate every slot before changing any live binding. A corrupt record
    // therefore falls back atomically to the defaults already held by Keypad.
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        if (!isValidHidCode(record[NVS_HEADER_SIZE + i])) {
            return StorageResult::BadHidCode;
        }
    }

    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        keypad.setBinding(i,
                          static_cast<HidKeycode>(record[NVS_HEADER_SIZE + i]));
    }

    return StorageResult::Loaded;
}

StorageResult saveBindings(const Keypad& keypad)
{
    uint8_t record[NVS_RECORD_SIZE];

    record[NVS_MAGIC_OFFSET] = static_cast<uint8_t>(NVS_MAGIC & 0xFF);
    record[NVS_MAGIC_OFFSET + 1] = static_cast<uint8_t>(NVS_MAGIC >> 8);
    record[NVS_VERSION_OFFSET] = NVS_VERSION;
    record[NVS_LENGTH_OFFSET] = Keypad::KEY_COUNT;

    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        record[NVS_HEADER_SIZE + i] =
            static_cast<uint8_t>(keypad.getBinding(i));
    }

    const uint16_t calculatedCrc =
        crc16(record, NVS_HEADER_SIZE + Keypad::KEY_COUNT);
    const uint8_t crcOffset = NVS_HEADER_SIZE + Keypad::KEY_COUNT;
    record[crcOffset] = static_cast<uint8_t>(calculatedCrc & 0xFF);
    record[crcOffset + 1] = static_cast<uint8_t>(calculatedCrc >> 8);

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return StorageResult::OpenFailed;
    }

    const size_t bytesWritten = prefs.putBytes(NVS_KEY, record, sizeof(record));
    prefs.end();

    return bytesWritten == sizeof(record) ? StorageResult::Loaded
                                           : StorageResult::WriteFailed;
}
