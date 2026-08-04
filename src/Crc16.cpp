#include "Crc16.h"

namespace {
constexpr uint16_t INITIAL = 0xFFFF;
constexpr uint16_t POLYNOMIAL = 0x1021;
constexpr uint16_t TOP_BIT = 0x8000;
}

uint16_t crc16Ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = INITIAL;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & TOP_BIT)
                ? static_cast<uint16_t>((crc << 1) ^ POLYNOMIAL)
                : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

bool isValidHidCode(uint8_t raw)
{
    return raw == 0x00 ||
           (raw >= 0x04 && raw <= 0xA4) ||
           (raw >= 0xE0 && raw <= 0xE7);
}
