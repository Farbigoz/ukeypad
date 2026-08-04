#ifndef UKEYPAD_TEST_HW_API_MOCK_H
#define UKEYPAD_TEST_HW_API_MOCK_H

#include <cstddef>
#include <cstdint>

constexpr size_t MOCK_STORAGE_CAPACITY = 64;

extern uint8_t g_storageReadData[MOCK_STORAGE_CAPACITY];
extern size_t g_storageReadLength;
extern bool g_storageReadOk;
extern uint8_t g_storageWriteData[MOCK_STORAGE_CAPACITY];
extern size_t g_storageWriteLength;
extern bool g_storageWriteCalled;

void resetStorageMock();

#endif
