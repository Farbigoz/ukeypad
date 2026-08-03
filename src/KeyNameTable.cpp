#include "KeyNameTable.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Key-name -> HidKeycode lookup (case-insensitive). Covers keys most likely
//  to be bound on a keypad; extend as needed. Names mirror HidKeycode members.
//  Digits are accepted as "0".."9" (mapping to Num0..Num9).
// ---------------------------------------------------------------------------
struct KeyName {
    const char* name;
    HidKeycode  code;
};

static const KeyName KEY_NAMES[] = {
    // letters
    {"A", HidKeycode::A}, {"B", HidKeycode::B}, {"C", HidKeycode::C},
    {"D", HidKeycode::D}, {"E", HidKeycode::E}, {"F", HidKeycode::F},
    {"G", HidKeycode::G}, {"H", HidKeycode::H}, {"I", HidKeycode::I},
    {"J", HidKeycode::J}, {"K", HidKeycode::K}, {"L", HidKeycode::L},
    {"M", HidKeycode::M}, {"N", HidKeycode::N}, {"O", HidKeycode::O},
    {"P", HidKeycode::P}, {"Q", HidKeycode::Q}, {"R", HidKeycode::R},
    {"S", HidKeycode::S}, {"T", HidKeycode::T}, {"U", HidKeycode::U},
    {"V", HidKeycode::V}, {"W", HidKeycode::W}, {"X", HidKeycode::X},
    {"Y", HidKeycode::Y}, {"Z", HidKeycode::Z},
    // digits (top row)
    {"0", HidKeycode::Num0}, {"1", HidKeycode::Num1}, {"2", HidKeycode::Num2},
    {"3", HidKeycode::Num3}, {"4", HidKeycode::Num4}, {"5", HidKeycode::Num5},
    {"6", HidKeycode::Num6}, {"7", HidKeycode::Num7}, {"8", HidKeycode::Num8},
    {"9", HidKeycode::Num9},
    // function row
    {"F1",  HidKeycode::F1},  {"F2",  HidKeycode::F2},  {"F3",  HidKeycode::F3},
    {"F4",  HidKeycode::F4},  {"F5",  HidKeycode::F5},  {"F6",  HidKeycode::F6},
    {"F7",  HidKeycode::F7},  {"F8",  HidKeycode::F8},  {"F9",  HidKeycode::F9},
    {"F10", HidKeycode::F10}, {"F11", HidKeycode::F11}, {"F12", HidKeycode::F12},
    {"F13", HidKeycode::F13}, {"F14", HidKeycode::F14}, {"F15", HidKeycode::F15},
    {"F16", HidKeycode::F16}, {"F17", HidKeycode::F17}, {"F18", HidKeycode::F18},
    {"F19", HidKeycode::F19}, {"F20", HidKeycode::F20}, {"F21", HidKeycode::F21},
    {"F22", HidKeycode::F22}, {"F23", HidKeycode::F23}, {"F24", HidKeycode::F24},
    // common named keys
    {"ENTER",      HidKeycode::Enter},     {"RETURN", HidKeycode::Enter},
    {"ESC",        HidKeycode::Escape},    {"ESCAPE", HidKeycode::Escape},
    {"TAB",        HidKeycode::Tab},       {"SPACE",  HidKeycode::Space},
    {"BACKSPACE",  HidKeycode::Backspace},
    {"INSERT",     HidKeycode::Insert},    {"DELETE",  HidKeycode::Delete},
    {"HOME",       HidKeycode::Home},      {"END",     HidKeycode::End},
    {"PAGEUP",     HidKeycode::PageUp},    {"PAGEDOWN", HidKeycode::PageDown},
    {"PGUP",       HidKeycode::PageUp},    {"PGDN",    HidKeycode::PageDown},
    {"UP",         HidKeycode::ArrowUp},   {"DOWN",    HidKeycode::ArrowDown},
    {"LEFT",       HidKeycode::ArrowLeft}, {"RIGHT",   HidKeycode::ArrowRight},
    {"CAPSLOCK",   HidKeycode::CapsLock},
    {"PRINTSCREEN",HidKeycode::PrintScreen},{"PRTSC",  HidKeycode::PrintScreen},
    {"SCROLLLOCK", HidKeycode::ScrollLock},
    {"MENU",       HidKeycode::Menu},      {"APP",     HidKeycode::Application},
    {"MUTE",       HidKeycode::Mute},
    {"VOLUP",      HidKeycode::VolumeUp},  {"VOLDN",   HidKeycode::VolumeDown},
    // modifiers
    {"CTRL",    HidKeycode::ControlLeft},  {"CONTROL", HidKeycode::ControlLeft},
    {"LCTRL",   HidKeycode::ControlLeft},  {"RCTRL",   HidKeycode::ControlRight},
    {"SHIFT",   HidKeycode::ShiftLeft},    {"LSHIFT",  HidKeycode::ShiftLeft},
    {"RSHIFT",  HidKeycode::ShiftRight},
    {"ALT",     HidKeycode::AltLeft},      {"LALT",    HidKeycode::AltLeft},
    {"RALT",    HidKeycode::AltRight},     {"ALTGR",   HidKeycode::AltRight},
    {"GUI",     HidKeycode::GuiLeft},      {"WIN",     HidKeycode::GuiLeft},
    {"LWIN",    HidKeycode::GuiLeft},      {"RWIN",    HidKeycode::GuiRight},
    {"CMD",     HidKeycode::GuiLeft},
};

static constexpr uint8_t KEY_NAME_COUNT =
    sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]);

// Case-insensitive ASCII compare; both sides uppercased internally.
static bool nameEq(const char* a, const char* b)
{
    while (*a && *b) {
        if (toupper(static_cast<unsigned char>(*a)) !=
            toupper(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return (*a == '\0' && *b == '\0');
}

// Reverse-lookup: HidKeycode -> human-readable name (first match).
static const char* keyName(HidKeycode code)
{
    const uint8_t raw = static_cast<uint8_t>(code);
    for (uint8_t i = 0; i < KEY_NAME_COUNT; ++i) {
        if (static_cast<uint8_t>(KEY_NAMES[i].code) == raw) {
            return KEY_NAMES[i].name;
        }
    }
    return nullptr;
}

bool keyNameLookup(const char* token, HidKeycode& out)
{
    for (uint8_t i = 0; i < KEY_NAME_COUNT; ++i) {
        if (nameEq(token, KEY_NAMES[i].name)) {
            out = KEY_NAMES[i].code;
            return true;
        }
    }
    return false;
}

const char* keyNameFor(HidKeycode code)
{
    return keyName(code);
}
