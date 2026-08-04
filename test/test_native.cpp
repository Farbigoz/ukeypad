#include <unity.h>
#include <cstdio>
#include <cstring>
#include "Arduino.h"
#include "HwApiMock.h"
#include "KeyNameTable.h"
#include "Crc16.h"
#include "ButtonDebounce.h"
#include "Button.h"
#include "ConfigStorage.h"
#include "DeviceProfile.h"
#include "FirmwareVersion.h"
#include "Keypad.h"
#include "TextWriter.h"

uint8_t g_mockPinState = HIGH;

namespace {
constexpr size_t RECORD_SIZE = DeviceProfile::INPUT_COUNT + 7;

struct Capture {
    char text[64];
    size_t length;
};

void append(Capture& capture, const char* text)
{
    while (*text && capture.length + 1 < sizeof(capture.text)) {
        capture.text[capture.length++] = *text++;
    }
    capture.text[capture.length] = '\0';
}

void printText(void* context, const char* text) { append(*static_cast<Capture*>(context), text); }
void printNumber(void* context, long long value, int base)
{
    char buffer[32];
    if (base == HEX) std::snprintf(buffer, sizeof(buffer), "%llX", value);
    else std::snprintf(buffer, sizeof(buffer), "%lld", value);
    append(*static_cast<Capture*>(context), buffer);
}
void printlnText(void* context, const char* text)
{
    Capture& capture = *static_cast<Capture*>(context);
    append(capture, text);
    append(capture, "\n");
}
void printlnNumber(void* context, long long value, int base)
{
    printNumber(context, value, base);
    append(*static_cast<Capture*>(context), "\n");
}

void makeRecord(uint8_t* record, uint8_t debounce = 7)
{
    std::memset(record, 0, RECORD_SIZE);
    record[0] = 0x50;
    record[1] = 0x4B;
    record[2] = FirmwareVersion::CONFIG_FORMAT;
    record[3] = DeviceProfile::INPUT_COUNT;
    for (uint8_t i = 0; i < DeviceProfile::INPUT_COUNT; ++i) {
        record[4 + i] = static_cast<uint8_t>(DeviceProfile::INPUTS[i].defaultBinding);
    }
    record[4 + DeviceProfile::INPUT_COUNT] = debounce;
    const uint16_t crc = crc16Ccitt(record, RECORD_SIZE - 2);
    record[RECORD_SIZE - 2] = static_cast<uint8_t>(crc & 0xFF);
    record[RECORD_SIZE - 1] = static_cast<uint8_t>(crc >> 8);
}

void setUpStorage()
{
    resetStorageMock();
    g_mockPinState = HIGH;
}
}

void test_key_names()
{
    HidKeycode code = HidKeycode::None;
    TEST_ASSERT_TRUE(keyNameLookup("z", code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::Z), static_cast<uint8_t>(code));
    TEST_ASSERT_TRUE(keyNameLookup("RETURN", code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::Enter), static_cast<uint8_t>(code));
    TEST_ASSERT_TRUE(keyNameLookup("f13", code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::F13), static_cast<uint8_t>(code));
    TEST_ASSERT_FALSE(keyNameLookup("not-a-key", code));
    TEST_ASSERT_NULL(keyNameFor(static_cast<HidKeycode>(0xFF)));
    TEST_ASSERT_EQUAL_STRING("A", keyNameFor(HidKeycode::A));
}

void test_crc_and_hid_validation()
{
    const uint8_t vector[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16Ccitt(vector, sizeof(vector)));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16Ccitt(nullptr, 0));
    TEST_ASSERT_TRUE(isValidHidCode(0x00));
    TEST_ASSERT_TRUE(isValidHidCode(0x04));
    TEST_ASSERT_TRUE(isValidHidCode(0xA4));
    TEST_ASSERT_TRUE(isValidHidCode(0xE7));
    TEST_ASSERT_FALSE(isValidHidCode(0x03));
    TEST_ASSERT_FALSE(isValidHidCode(0xA5));
    TEST_ASSERT_FALSE(isValidHidCode(0xE8));
}

void test_debounce_step()
{
    uint8_t integrator = 0;
    bool state = false;
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, debounceStep(true, integrator, 3, state));
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, debounceStep(true, integrator, 3, state));
    TEST_ASSERT_EQUAL_INT(ButtonEvent::Press, debounceStep(true, integrator, 3, state));
    TEST_ASSERT_TRUE(state);
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, debounceStep(false, integrator, 3, state));
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, debounceStep(false, integrator, 3, state));
    TEST_ASSERT_EQUAL_INT(ButtonEvent::Release, debounceStep(false, integrator, 3, state));
    TEST_ASSERT_FALSE(state);
}

void test_button_with_mock_gpio()
{
    Button button;
    button.begin(1);
    button.setDebounce(2);
    g_mockPinState = LOW;
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, button.update());
    TEST_ASSERT_EQUAL_INT(ButtonEvent::Press, button.update());
    g_mockPinState = HIGH;
    TEST_ASSERT_EQUAL_INT(ButtonEvent::None, button.update());
    TEST_ASSERT_EQUAL_INT(ButtonEvent::Release, button.update());
}

void test_keypad_bindings_and_defaults()
{
    Keypad keypad;
    keypad.begin();
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceProfile::INPUTS[i].defaultBinding),
                                static_cast<uint8_t>(keypad.getBinding(i)));
        TEST_ASSERT_EQUAL_UINT8(DeviceProfile::INPUTS[i].gpio, keypad.getPin(i));
    }
    keypad.setBinding(0, HidKeycode::A);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::A), static_cast<uint8_t>(keypad.getBinding(0)));
    keypad.setBinding(Keypad::KEY_COUNT, HidKeycode::B);
    TEST_ASSERT_EQUAL_INT(HidKeycode::None, keypad.getBinding(Keypad::KEY_COUNT));
    TEST_ASSERT_EQUAL_UINT8(0, keypad.getPin(Keypad::KEY_COUNT));
    keypad.loadDefaultBindings();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::Z), static_cast<uint8_t>(keypad.getBinding(0)));
}

void test_keypad_scan_events_and_stats()
{
    Keypad keypad;
    keypad.begin();
    keypad.setDebounce(1);
    g_mockPinState = LOW;
    TEST_ASSERT_EQUAL_UINT8(Keypad::KEY_COUNT, keypad.scan());
    TEST_ASSERT_EQUAL_UINT32(1, keypad.scanCount());
    TEST_ASSERT_EQUAL_UINT32(Keypad::KEY_COUNT, keypad.eventCount());
    TEST_ASSERT_EQUAL_UINT8(Keypad::KEY_COUNT, keypad.maxQueueDepth());
    KeyEvent event;
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        TEST_ASSERT_TRUE(keypad.getEvent(event));
        TEST_ASSERT_EQUAL_INT(KeyEventType::Press, event.type);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceProfile::INPUTS[i].defaultBinding),
                                static_cast<uint8_t>(event.keyCode));
    }
    TEST_ASSERT_FALSE(keypad.getEvent(event));
    g_mockPinState = HIGH;
    TEST_ASSERT_EQUAL_UINT8(Keypad::KEY_COUNT, keypad.scan());
    TEST_ASSERT_EQUAL_UINT32(Keypad::KEY_COUNT * 2, keypad.eventCount());
    keypad.clearStats();
    TEST_ASSERT_EQUAL_UINT32(0, keypad.scanCount());
    TEST_ASSERT_EQUAL_UINT32(0, keypad.eventCount());
    TEST_ASSERT_EQUAL_UINT32(0, keypad.overflowCount());
    TEST_ASSERT_EQUAL_UINT8(0, keypad.maxQueueDepth());
}

void test_keypad_queue_overflow()
{
    Keypad keypad;
    keypad.begin();
    keypad.setDebounce(1);
    for (uint8_t cycle = 0; cycle < 4; ++cycle) {
        g_mockPinState = LOW;
        keypad.scan();
        g_mockPinState = HIGH;
        keypad.scan();
    }
    TEST_ASSERT_TRUE(keypad.overflowCount() > 0);
    TEST_ASSERT_TRUE(keypad.maxQueueDepth() <= 31);
}

void test_storage_load_valid_and_atomic_failure()
{
    Keypad keypad;
    keypad.begin();
    keypad.setBinding(0, HidKeycode::A);
    const HidKeycode before = keypad.getBinding(0);
    uint8_t record[RECORD_SIZE];
    makeRecord(record, 9);
    std::memcpy(g_storageReadData, record, RECORD_SIZE);
    g_storageReadLength = RECORD_SIZE;
    TEST_ASSERT_EQUAL_INT(StorageResult::Loaded, loadBindings(keypad));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceProfile::INPUTS[0].defaultBinding), static_cast<uint8_t>(keypad.getBinding(0)));
    TEST_ASSERT_EQUAL_UINT8(9, keypad.debounce());
    record[4] = 0xFF;
    const uint16_t badHidCrc = crc16Ccitt(record, RECORD_SIZE - 2);
    record[RECORD_SIZE - 2] = static_cast<uint8_t>(badHidCrc & 0xFF);
    record[RECORD_SIZE - 1] = static_cast<uint8_t>(badHidCrc >> 8);
    std::memcpy(g_storageReadData, record, RECORD_SIZE);
    TEST_ASSERT_EQUAL_INT(StorageResult::BadHidCode, loadBindings(keypad));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceProfile::INPUTS[0].defaultBinding), static_cast<uint8_t>(keypad.getBinding(0)));
    TEST_ASSERT_NOT_EQUAL(before, keypad.getBinding(0));
}

void test_storage_error_paths()
{
    Keypad keypad;
    keypad.begin();
    uint8_t record[RECORD_SIZE];
    makeRecord(record);
    g_storageReadLength = 0;
    TEST_ASSERT_EQUAL_INT(StorageResult::Missing, loadBindings(keypad));
    g_storageReadOk = false;
    TEST_ASSERT_EQUAL_INT(StorageResult::OpenFailed, loadBindings(keypad));
    g_storageReadOk = true;
    g_storageReadLength = RECORD_SIZE - 1;
    TEST_ASSERT_EQUAL_INT(StorageResult::SizeMismatch, loadBindings(keypad));
    std::memcpy(g_storageReadData, record, RECORD_SIZE);
    g_storageReadLength = RECORD_SIZE;
    g_storageReadData[0] ^= 1;
    TEST_ASSERT_EQUAL_INT(StorageResult::BadMagic, loadBindings(keypad));
    makeRecord(record);
    record[2]++;
    std::memcpy(g_storageReadData, record, RECORD_SIZE);
    TEST_ASSERT_EQUAL_INT(StorageResult::BadVersion, loadBindings(keypad));
    makeRecord(record);
    record[RECORD_SIZE - 1]++;
    std::memcpy(g_storageReadData, record, RECORD_SIZE);
    TEST_ASSERT_EQUAL_INT(StorageResult::BadCrc, loadBindings(keypad));
}

void test_storage_save_format_and_round_trip()
{
    Keypad keypad;
    keypad.begin();
    keypad.setBinding(0, HidKeycode::A);
    keypad.setDebounce(11);
    TEST_ASSERT_EQUAL_INT(StorageResult::Loaded, saveBindings(keypad));
    TEST_ASSERT_TRUE(g_storageWriteCalled);
    TEST_ASSERT_EQUAL_UINT32(RECORD_SIZE, g_storageWriteLength);
    TEST_ASSERT_EQUAL_HEX8(0x50, g_storageWriteData[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4B, g_storageWriteData[1]);
    TEST_ASSERT_EQUAL_UINT8(DeviceProfile::INPUT_COUNT, g_storageWriteData[3]);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(HidKeycode::A), g_storageWriteData[4]);
    TEST_ASSERT_EQUAL_UINT8(11, g_storageWriteData[4 + DeviceProfile::INPUT_COUNT]);
    std::memcpy(g_storageReadData, g_storageWriteData, RECORD_SIZE);
    g_storageReadLength = RECORD_SIZE;
    Keypad loaded;
    loaded.begin();
    TEST_ASSERT_EQUAL_INT(StorageResult::Loaded, loadBindings(loaded));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::A), static_cast<uint8_t>(loaded.getBinding(0)));
    TEST_ASSERT_EQUAL_UINT8(11, loaded.debounce());
}

void test_textwriter_function_pointer_output()
{
    Capture capture = {{0}, 0};
    TextWriter writer(&capture, printText, printNumber, printlnText, printlnNumber);
    writer.print("value=");
    writer.print(255, HEX);
    writer.println(" ok");
    TEST_ASSERT_EQUAL_STRING("value=FF ok\n", capture.text);
}

void setUp()
{
    g_mockPinState = HIGH;
    resetStorageMock();
}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_key_names);
    RUN_TEST(test_crc_and_hid_validation);
    RUN_TEST(test_debounce_step);
    RUN_TEST(test_button_with_mock_gpio);
    RUN_TEST(test_keypad_bindings_and_defaults);
    RUN_TEST(test_keypad_scan_events_and_stats);
    RUN_TEST(test_keypad_queue_overflow);
    RUN_TEST(test_storage_load_valid_and_atomic_failure);
    RUN_TEST(test_storage_error_paths);
    RUN_TEST(test_storage_save_format_and_round_trip);
    RUN_TEST(test_textwriter_function_pointer_output);
    return UNITY_END();
}

void setup() {}
void loop() {}
static void (*const ukeypad_keep_arduino_symbols)() = setup;

#ifdef _WIN32
int WinMain(void*, void*, char*, int) { return main(); }
#endif
