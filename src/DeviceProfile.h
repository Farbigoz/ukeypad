#ifndef UKEYPAD_DEVICE_PROFILE_H
#define UKEYPAD_DEVICE_PROFILE_H

#include <stdint.h>
#include "HidKeycode.h"

namespace DeviceProfile {

// ---------------------------------------------------------------------------
//  Device identity and physical input profile
// ---------------------------------------------------------------------------

constexpr const char* MODEL = "ukeypad-esp32-s3";
constexpr const char* USB_MODE = "cdc+hid";

enum class InputType : uint8_t {
    Digital,
};

struct InputSlot {
    uint8_t gpio;
    InputType type;
    HidKeycode defaultBinding;
};

// One row describes one real physical button. Slot index is scan/binding index.
// UKEYPAD_PROFILE_8 is a compile-time validation profile; its GPIO wiring is
// not validated for a physical board.
#if defined(UKEYPAD_PROFILE_8)
constexpr InputSlot INPUTS[] = {
    { 1, InputType::Digital, HidKeycode::Z },
    { 2, InputType::Digital, HidKeycode::X },
    { 3, InputType::Digital, HidKeycode::C },
    { 4, InputType::Digital, HidKeycode::V },
    { 5, InputType::Digital, HidKeycode::F13 },
    { 6, InputType::Digital, HidKeycode::F14 },
    { 7, InputType::Digital, HidKeycode::F15 },
    { 8, InputType::Digital, HidKeycode::F16 },
};
#else
constexpr InputSlot INPUTS[] = {
    { 1, InputType::Digital, HidKeycode::Z },
    { 2, InputType::Digital, HidKeycode::X },
    { 3, InputType::Digital, HidKeycode::C },
    { 4, InputType::Digital, HidKeycode::V },
    { 5, InputType::Digital, HidKeycode::S },
    { 6, InputType::Digital, HidKeycode::D },
};
#endif

constexpr uint8_t MAX_INPUT_COUNT = 32;

constexpr uint8_t INPUT_COUNT = sizeof(INPUTS) / sizeof(INPUTS[0]);

// ---------------------------------------------------------------------------
//  ESP32-S3 legacy timer profile
// ---------------------------------------------------------------------------

constexpr uint8_t TIMER_NUMBER = 0;
constexpr uint32_t APB_CLOCK_HZ = 80000000;
constexpr uint16_t TIMER_DIVIDER = 80;
constexpr uint32_t TIMER_TICK_HZ = APB_CLOCK_HZ / TIMER_DIVIDER;
constexpr uint32_t SCAN_FREQUENCY_HZ = 2000;
constexpr uint64_t SCAN_ALARM_TICKS = TIMER_TICK_HZ / SCAN_FREQUENCY_HZ;

// ---------------------------------------------------------------------------
//  Input behavior and outputs
// ---------------------------------------------------------------------------

constexpr uint8_t DEFAULT_DEBOUNCE_SAMPLES = 4;
constexpr uint8_t LED_COUNT = 0;
constexpr const char* LED_TYPE = "none";

constexpr bool CAP_BINDS = true;
constexpr bool CAP_TEST = true;
constexpr bool CAP_DEBOUNCE = true;
constexpr bool CAP_STATS = true;

// ---------------------------------------------------------------------------
//  Compile-time profile validation
// ---------------------------------------------------------------------------

constexpr bool isSafeGpio(uint8_t gpio)
{
    return gpio != 19 && gpio != 20 && !(gpio >= 26 && gpio <= 32);
}

static_assert(INPUT_COUNT > 0, "Device profile must define at least one input");
static_assert(INPUT_COUNT <= MAX_INPUT_COUNT,
              "Device profile exceeds the supported input count");
static_assert(TIMER_TICK_HZ % SCAN_FREQUENCY_HZ == 0,
              "Scan frequency must divide the timer tick frequency");

constexpr bool allGpiosSafe(uint8_t index = 0)
{
    return index >= INPUT_COUNT
        ? true
        : isSafeGpio(INPUTS[index].gpio) && allGpiosSafe(index + 1);
}

constexpr bool allGpiosUnique(uint8_t first = 0, uint8_t second = 1)
{
    return first >= INPUT_COUNT
        ? true
        : second >= INPUT_COUNT
            ? allGpiosUnique(first + 1, first + 2)
            : INPUTS[first].gpio != INPUTS[second].gpio &&
              allGpiosUnique(first, second + 1);
}

static_assert(SCAN_ALARM_TICKS > 0 && SCAN_ALARM_TICKS <= 65535,
              "Timer alarm ticks are outside the supported range");
static_assert(DEFAULT_DEBOUNCE_SAMPLES > 0,
              "Default debounce must be at least one sample");
static_assert(allGpiosSafe(), "Device profile uses a reserved GPIO");
static_assert(allGpiosUnique(), "Device profile contains duplicate GPIOs");

// The current boot-keyboard HID adapter supports six simultaneous non-modifier
// keys. This is a report limitation, not a limit on the profile input count.

} // namespace DeviceProfile

#endif // UKEYPAD_DEVICE_PROFILE_H
