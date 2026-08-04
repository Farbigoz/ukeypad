#ifndef UKEYPAD_CRC16_H
#define UKEYPAD_CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16Ccitt(const uint8_t* data, size_t length);
bool isValidHidCode(uint8_t raw);

#endif // UKEYPAD_CRC16_H
