#include "Config.h"
#include "Keypad.h"
#include <Arduino.h>
#include <Preferences.h>

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

// ---------------------------------------------------------------------------
//  Config implementation
// ---------------------------------------------------------------------------

Config::Config()
    : _keypad(nullptr)
    , _configMode(false)
    , _bannerShown(false)
    , _testMode(false)
    , _lineLen(0)
    , _lastStatsMs(0)
{
}

void Config::begin(Keypad& keypad, bool configMode)
{
    _keypad    = &keypad;
    _configMode = configMode;

    // Always load saved bindings (so your config persists across reboots in
    // normal mode too). If NVS has nothing yet, Keypad's defaults stay.
    loadFromNvs();

    if (_configMode) {
        // CDC Serial: begin() is a no-op for CDC but harmless and conventional.
        Serial.begin(115200);
        // Banner is printed from poll() once a terminal is attached, because
        // the host may not have enumerated the CDC port yet at this instant.
    }
}

// --- NVS persistence (6 hidCode bytes under key "binds") -------------------

void Config::loadFromNvs()
{
    Preferences prefs;
    prefs.begin("osukp", true);   // read-only
    uint8_t buf[Keypad::KEY_COUNT];
    size_t n = prefs.getBytes("binds", buf, sizeof(buf));
    prefs.end();
    if (n == sizeof(buf)) {
        for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
            _keypad->setBinding(i, static_cast<HidKeycode>(buf[i]));
        }
    }
}

void Config::saveToNvs()
{
    uint8_t buf[Keypad::KEY_COUNT];
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        buf[i] = static_cast<uint8_t>(_keypad->getBinding(i));
    }
    Preferences prefs;
    prefs.begin("osukp", false);  // read-write
    prefs.putBytes("binds", buf, sizeof(buf));
    prefs.end();
}

// --- key-name parsing -------------------------------------------------------

bool Config::parseKey(const char* tok, HidKeycode& out) const
{
    for (uint8_t i = 0; i < KEY_NAME_COUNT; ++i) {
        if (nameEq(tok, KEY_NAMES[i].name)) {
            out = KEY_NAMES[i].code;
            return true;
        }
    }
    return false;
}

// --- serial command handling -------------------------------------------------

void Config::processKeyEvents()
{
    if (!_configMode || !_keypad) return;

    KeyEvent event;
    while (_keypad->getEvent(event)) {
        if (!_testMode) {
            // Config mode always consumes events, but never forwards them to
            // HidKeyboard. This keeps the timer/debounce path identical to
            // normal mode without generating host keypresses.
            continue;
        }

        const uint8_t slotCode = static_cast<uint8_t>(event.keyCode);
        Serial.print("OK test event=");
        Serial.print(event.type == KeyEventType::Press ? "PRESS" : "RELEASE");
        Serial.print(" key=0x");
        if (slotCode < 0x10) Serial.print('0');
        Serial.println(slotCode, HEX);
    }
}

void Config::poll()
{
    if (!_configMode) return;

    // Print the banner once a terminal is attached; re-show on reconnect.
    if (Serial) {
        if (!_bannerShown) {
            printBanner();
            _bannerShown = true;
        }
    } else {
        _bannerShown = false;
    }

    // Non-blocking line accumulation.
    while (Serial.available()) {
        const int c = Serial.read();
        if (c < 0) break;
        if (c == '\r') continue;            // ignore CR; lines end with LF
        if (c == '\n') {
            _line[_lineLen] = '\0';
            handleLine();
            _lineLen = 0;
            return;
        }
        if (_lineLen < sizeof(_line) - 1) {
            _line[_lineLen++] = static_cast<char>(c);
        }
        // else: silently drop overflow chars until newline
    }
}

// strtok-free tokenizer: split _line on spaces into up to 3 tokens.
static uint8_t splitTokens(char* line, char* tok[3])
{
    uint8_t n = 0;
    char* p = line;
    while (*p && n < 3) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (*p) { *p = '\0'; ++p; }
    }
    return n;
}

void Config::handleLine()
{
    if (_lineLen == 0) return;

    char* tok[3] = {nullptr, nullptr, nullptr};
    const uint8_t n = splitTokens(_line, tok);
    if (n == 0) return;

    if (nameEq(tok[0], "info")) {
        Serial.println("OK info model=esp32-s3-keypad firmware=0.1.0 keys=6 scan_hz=2000 usb=cdc+hid");
    } else if (nameEq(tok[0], "test")) {
        if (n == 1 || nameEq(tok[1], "on")) {
            _testMode = true;
            Serial.println("OK test=on");
        } else if (nameEq(tok[1], "off")) {
            _testMode = false;
            Serial.println("OK test=off");
        } else {
            Serial.println("ERR code=INVALID_ARGUMENT usage=test [on|off]");
        }
    } else if (nameEq(tok[0], "debounce")) {
        if (n == 1 || nameEq(tok[1], "get")) {
            Serial.print("OK debounce_samples="); Serial.println(_keypad->debounce());
        } else if (n >= 3 && nameEq(tok[1], "set")) {
            char* endp = nullptr; long value = strtol(tok[2], &endp, 10);
            if (*endp != '\0' || value < 1 || value > 255) {
                Serial.println("ERR code=INVALID_VALUE debounce_range=1..255");
            } else {
                _keypad->setDebounce(static_cast<uint8_t>(value));
                Serial.print("OK debounce_samples="); Serial.println(value);
            }
        } else {
            Serial.println("ERR code=INVALID_ARGUMENT usage=debounce [get|set N]");
        }
    } else if (nameEq(tok[0], "stats")) {
        if (n >= 2 && nameEq(tok[1], "clear")) {
            _keypad->clearStats();
            Serial.println("OK stats=cleared");
        } else if (n == 1) {
            Serial.print("OK stats scan="); Serial.print(_keypad->scanCount());
            Serial.print(" events="); Serial.print(_keypad->eventCount());
            Serial.print(" overflow="); Serial.print(_keypad->overflowCount());
            Serial.print(" queue_max="); Serial.println(_keypad->maxQueueDepth());
        } else {
            Serial.println("ERR code=INVALID_ARGUMENT usage=stats [clear]");
        }
    } else if (nameEq(tok[0], "help") || nameEq(tok[0], "?")) {
        printHelp();
    } else if (nameEq(tok[0], "list")) {
        printBindings();
    } else if (nameEq(tok[0], "save")) {
        saveToNvs();
        Serial.println("OK saved");
    } else if (nameEq(tok[0], "reset")) {
        _keypad->loadDefaultBindings();
        Serial.println("OK defaults restored (RAM). Type 'save' to persist.");
    } else if (nameEq(tok[0], "bind")) {
        if (n < 3) {
            Serial.println("ERR code=INVALID_ARGUMENT usage=bind <slot> <key>");
            return;
        }
        // Parse slot (0..5) with full validation.
        char* endp = nullptr;
        long slot = strtol(tok[1], &endp, 10);
        if (*endp != '\0' || slot < 0 || slot >= Keypad::KEY_COUNT) {
            Serial.println("ERR code=INVALID_SLOT expected=0..5");
            return;
        }
        HidKeycode code;
        if (!parseKey(tok[2], code)) {
            Serial.print("ERR code=UNKNOWN_KEY key=");
            Serial.println(tok[2]);
            return;
        }
        _keypad->setBinding(static_cast<uint8_t>(slot), code);
        const char* nm = keyName(code);
        Serial.print("OK slot ");
        Serial.print(slot);
        Serial.print(" -> ");
        Serial.println(nm ? nm : tok[2]);
    } else {
        Serial.print("ERR code=UNKNOWN_COMMAND command=");
        Serial.println(tok[0]);
    }
}

// --- output helpers ----------------------------------------------------------

void Config::printBanner()
{
    Serial.println();
    Serial.println("=== USB HID keypad — CONFIG MODE ===");
    Serial.println("Hold any switch while plugging in to enter this mode.");
    Serial.println("Type 'help' for commands. Reboot (RESET) to play.");
    Serial.println();
    printBindings();
}

void Config::printBindings()
{
    Serial.println("Current bindings:");
    for (uint8_t i = 0; i < Keypad::KEY_COUNT; ++i) {
        const HidKeycode code = _keypad->getBinding(i);
        const char* nm = keyName(code);
        Serial.print("  slot ");
        Serial.print(i);
        Serial.print(" -> ");
        if (nm) {
            Serial.print(nm);
        } else {
            Serial.print("0x");
            Serial.print(static_cast<uint8_t>(code), HEX);
        }
        Serial.print("  (0x");
        if (static_cast<uint8_t>(code) < 0x10) Serial.print('0');
        Serial.print(static_cast<uint8_t>(code), HEX);
        Serial.println(")");
    }
}

void Config::printHelp()
{
    Serial.println("Commands:");
    Serial.println("  bind <slot> <key>   set slot (0..5) to a key");
    Serial.println("  list                show current bindings");
    Serial.println("  save                write bindings to flash (NVS)");
    Serial.println("  reset               restore defaults (RAM; 'save' to keep)");
    Serial.println("  info                firmware and hardware information");
    Serial.println("  test [on|off]       monitor raw GPIO transitions");
    Serial.println("  debounce [get|set N] read/set debounce samples");
    Serial.println("  stats [clear]       show/clear scan and queue counters");
    Serial.println("  help                this message");
    Serial.println();
    Serial.println("Keys: A-Z  0-9  F1-F24  ENTER SPACE TAB ESC");
    Serial.println("  ARROWS (UP DOWN LEFT RIGHT)  MODIFIERS (CTRL SHIFT ALT GUI WIN)");
    Serial.println("  BACKSPACE INSERT DELETE HOME END PAGEUP PAGEDOWN");
    Serial.println("  CAPSLOCK PRINTSCREEN SCROLLLOCK MUTE VOLUP VOLDN");
    Serial.println();
    Serial.println("Examples:");
    Serial.println("  bind 0 Z       -> slot 0 sends Z");
    Serial.println("  bind 4 F13     -> slot 4 sends F13");
    Serial.println("  bind 2 5       -> slot 2 sends digit 5");
}
