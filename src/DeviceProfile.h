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
constexpr InputSlot INPUTS[] = {
    { 4,  InputType::Digital, HidKeycode::Z   },
    { 5,  InputType::Digital, HidKeycode::X   },
    { 6,  InputType::Digital, HidKeycode::C   },
    { 7,  InputType::Digital, HidKeycode::V   },
    { 15, InputType::Digital, HidKeycode::F13 },
    { 16, InputType::Digital, HidKeycode::F14 },
};

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
static_assert(INPUT_COUNT <= 6, "Device profile exceeds the boot HID rollover");
static_assert(TIMER_TICK_HZ % SCAN_FREQUENCY_HZ == 0,
              "Scan frequency must divide the timer tick frequency");
static_assert(SCAN_ALARM_TICKS > 0 && SCAN_ALARM_TICKS <= 65535,
              "Timer alarm ticks are outside the supported range");
static_assert(DEFAULT_DEBOUNCE_SAMPLES > 0,
              "Default debounce must be at least one sample");

// The current profile contains six inputs. Keep these checks explicit because
// the installed toolchain uses the older C++ constexpr rules and cannot run
// loops in a constexpr function. A future profile with another count should
// extend this small validation block alongside its INPUTS table.
static_assert(isSafeGpio(INPUTS[0].gpio) &&
              isSafeGpio(INPUTS[1].gpio) &&
              isSafeGpio(INPUTS[2].gpio) &&
              isSafeGpio(INPUTS[3].gpio) &&
              isSafeGpio(INPUTS[4].gpio) &&
              isSafeGpio(INPUTS[5].gpio),
              "Device profile uses a reserved GPIO");
static_assert(INPUTS[0].gpio != INPUTS[1].gpio &&
              INPUTS[0].gpio != INPUTS[2].gpio &&
              INPUTS[0].gpio != INPUTS[3].gpio &&
              INPUTS[0].gpio != INPUTS[4].gpio &&
              INPUTS[0].gpio != INPUTS[5].gpio &&
              INPUTS[1].gpio != INPUTS[2].gpio &&
              INPUTS[1].gpio != INPUTS[3].gpio &&
              INPUTS[1].gpio != INPUTS[4].gpio &&
              INPUTS[1].gpio != INPUTS[5].gpio &&
              INPUTS[2].gpio != INPUTS[3].gpio &&
              INPUTS[2].gpio != INPUTS[4].gpio &&
              INPUTS[2].gpio != INPUTS[5].gpio &&
              INPUTS[3].gpio != INPUTS[4].gpio &&
              INPUTS[3].gpio != INPUTS[5].gpio &&
              INPUTS[4].gpio != INPUTS[5].gpio,
              "Device profile contains duplicate GPIOs");

} // namespace DeviceProfile

#endif // UKEYPAD_DEVICE_PROFILE_H
