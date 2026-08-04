#include <unity.h>
#include "Arduino.h"
#include "KeyNameTable.h"
#include "Crc16.h"
#include "ButtonDebounce.h"
#include "Button.h"

uint8_t g_mockPinState = HIGH;

void test_key_names()
{
    HidKeycode code = HidKeycode::None;
    TEST_ASSERT_TRUE(keyNameLookup("z", code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::Z), static_cast<uint8_t>(code));
    TEST_ASSERT_TRUE(keyNameLookup("RETURN", code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HidKeycode::Enter), static_cast<uint8_t>(code));
    TEST_ASSERT_FALSE(keyNameLookup("not-a-key", code));
    TEST_ASSERT_EQUAL_STRING("A", keyNameFor(HidKeycode::A));
}

void test_crc_and_hid_validation()
{
    const uint8_t vector[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16Ccitt(vector, sizeof(vector)));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16Ccitt(nullptr, 0));
    TEST_ASSERT_TRUE(isValidHidCode(0x00));
    TEST_ASSERT_TRUE(isValidHidCode(0x04));
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

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_key_names);
    RUN_TEST(test_crc_and_hid_validation);
    RUN_TEST(test_debounce_step);
    RUN_TEST(test_button_with_mock_gpio);
    return UNITY_END();
}

void setup() {}
void loop() {}

// Keep Arduino-style entry points available for shared test helpers.
static void (*const ukeypad_keep_arduino_symbols)() = setup;

#ifdef _WIN32
int WinMain(void*, void*, char*, int) { return main(); }
#endif
