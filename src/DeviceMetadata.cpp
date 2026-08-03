#include "DeviceMetadata.h"
#include "KeyNameTable.h"
#include "DeviceProfile.h"
#include "FirmwareVersion.h"

const DeviceMetadata DEVICE_METADATA = {
    DeviceProfile::MODEL,
    FirmwareVersion::STRING,
    DeviceProfile::USB_MODE,
    "digital",
    FirmwareVersion::PROTOCOL,
    FirmwareVersion::CONFIG_FORMAT,
    DeviceProfile::SCAN_FREQUENCY_HZ,
    DeviceProfile::LED_COUNT,
    DeviceProfile::CAP_BINDS,
    DeviceProfile::CAP_TEST,
    DeviceProfile::CAP_DEBOUNCE,
    DeviceProfile::CAP_STATS,
};

static void printDeviceFields(Print& out)
{
    out.print("model="); out.print(DEVICE_METADATA.model);
    out.print(" firmware="); out.print(DEVICE_METADATA.firmware);
    out.print(" protocol="); out.print(DEVICE_METADATA.protocolVersion);
    out.print(" config_version="); out.print(DEVICE_METADATA.configVersion);
}

static void printInputTypes(Print& out)
{
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        if (i > 0) out.print(',');
        out.print(DEVICE_METADATA.inputType);
    }
}

void printInfo(Print& out)
{
    out.print("OK info ");
    printDeviceFields(out);
    out.print(" input_count="); out.print(Keypad::KEY_COUNT);
    out.print(" input_types="); printInputTypes(out);
    out.print(" led_count="); out.print(DEVICE_METADATA.ledCount);
    out.print(" capabilities=binds="); out.print(DEVICE_METADATA.supportsBinds ? "true" : "false");
    out.print(",test="); out.print(DEVICE_METADATA.supportsTest ? "true" : "false");
    out.print(",debounce="); out.print(DEVICE_METADATA.supportsDebounce ? "true" : "false");
    out.print(",stats="); out.print(DEVICE_METADATA.supportsStats ? "true" : "false");
    out.print(" scan_hz="); out.print(DEVICE_METADATA.scanHz);
    out.print(" usb="); out.println(DEVICE_METADATA.usb);
}

void printDeviceDescription(const Keypad& keypad, Print& out)
{
    out.println("OK device_begin");
    out.print("OK device "); printDeviceFields(out); out.println();
    out.print("OK inputs count="); out.print(Keypad::KEY_COUNT);
    out.print(" types="); printInputTypes(out); out.print(" pins=");
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        if (i > 0) out.print(',');
        out.print(keypad.getPin(i));
    }
    out.println();
    out.print("OK leds count="); out.println(DEVICE_METADATA.ledCount);
    out.print("OK capabilities binds="); out.print(DEVICE_METADATA.supportsBinds ? "true" : "false");
    out.print(" test="); out.print(DEVICE_METADATA.supportsTest ? "true" : "false");
    out.print(" debounce="); out.print(DEVICE_METADATA.supportsDebounce ? "true" : "false");
    out.print(" stats="); out.println(DEVICE_METADATA.supportsStats ? "true" : "false");
    out.println("OK device_end");
}

void printBindings(const Keypad& keypad, Print& out)
{
    out.println("Current bindings:");
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        const HidKeycode code = keypad.getBinding(i);
        const char* name = keyNameFor(code);
        out.print("  slot "); out.print(i); out.print(" -> ");
        if (name) out.print(name);
        else { out.print("0x"); out.print(static_cast<uint8_t>(code), HEX); }
        out.print("  (0x");
        if (static_cast<uint8_t>(code) < 0x10) out.print('0');
        out.print(static_cast<uint8_t>(code), HEX); out.println(")");
    }
    out.println("OK list");
}

void printHelp(Print& out)
{
    out.println("Commands:");
    out.println("  bind <slot> <key>   set slot (0..5) to a key");
    out.println("  list                show current bindings");
    out.println("  save                write bindings to flash (NVS)");
    out.println("  reset               restore defaults (RAM; 'save' to keep)");
    out.println("  info                firmware and hardware information");
    out.println("  get_device          complete machine-readable device description");
    out.println("  test [on|off]       monitor raw GPIO transitions");
    out.println("  debounce [get|set N] read/set debounce samples");
    out.println("  stats [clear]       show/clear scan and queue counters");
    out.println("  help                this message");
    out.println();
    out.println("Keys: A-Z  0-9  F1-F24  ENTER SPACE TAB ESC");
    out.println("  ARROWS (UP DOWN LEFT RIGHT)  MODIFIERS (CTRL SHIFT ALT GUI WIN)");
    out.println("  BACKSPACE INSERT DELETE HOME END PAGEUP PAGEDOWN");
    out.println("  CAPSLOCK PRINTSCREEN SCROLLLOCK MUTE VOLUP VOLDN");
    out.println();
    out.println("Examples:");
    out.println("  bind 0 Z       -> slot 0 sends Z");
    out.println("  bind 4 F13     -> slot 4 sends F13");
    out.println("  bind 2 5       -> slot 2 sends digit 5");
}
