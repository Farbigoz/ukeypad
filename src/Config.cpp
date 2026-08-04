#include "Config.h"
#include "Keypad.h"
#include "hw/HwApi.h"
#include <ctype.h>
#include <stdlib.h>
#include "DeviceMetadata.h"
#include "KeyNameTable.h"
#include "ConfigStorage.h"
#include "DeviceProfile.h"

namespace {
static bool nameEq(const char* a, const char* b)
{
    while (*a && *b) {
        if (toupper(static_cast<unsigned char>(*a)) !=
            toupper(static_cast<unsigned char>(*b))) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

constexpr uint32_t CDC_BAUD_RATE = 115200;

}


const char* configErrorName(ConfigError error)
{
    switch (error) {
        case ConfigError::InvalidArgument: return "INVALID_ARGUMENT";
        case ConfigError::InvalidValue: return "INVALID_VALUE";
        case ConfigError::InvalidSlot: return "INVALID_SLOT";
        case ConfigError::UnknownKey: return "UNKNOWN_KEY";
        case ConfigError::UnknownCommand: return "UNKNOWN_COMMAND";
        case ConfigError::NvsWriteFailed: return "NVS_WRITE_FAILED";
        case ConfigError::NvsOpenFailed: return "OPEN_FAILED";
        case ConfigError::NvsMissing: return "MISSING";
        case ConfigError::NvsSizeMismatch: return "SIZE_MISMATCH";
        case ConfigError::NvsBadMagic: return "BAD_MAGIC";
        case ConfigError::NvsBadVersion: return "BAD_VERSION";
        case ConfigError::NvsBadLength: return "BAD_LENGTH";
        case ConfigError::NvsBadCrc: return "BAD_CRC";
        case ConfigError::NvsBadHidCode: return "BAD_HIDCODE";
    }
    return "UNKNOWN";
}

static void printError(ConfigError error)
{
    Hw::cdc.print("ERR code=");
    Hw::cdc.println(configErrorName(error));
}

static void printError(ConfigError error, const char* field, const char* value)
{
    Hw::cdc.print("ERR code=");
    Hw::cdc.print(configErrorName(error));
    Hw::cdc.print(' ');
    Hw::cdc.print(field);
    Hw::cdc.print('=');
    Hw::cdc.println(value);
}

static void printError(ConfigError error, const char* field, long value)
{
    Hw::cdc.print("ERR code=");
    Hw::cdc.print(configErrorName(error));
    Hw::cdc.print(' ');
    Hw::cdc.print(field);
    Hw::cdc.print('=');
    Hw::cdc.println(value);
}

static void printNvsWarning(ConfigError reason)
{
    Hw::cdc.print("WARN code=NVS_LOAD_FAILED reason=");
    Hw::cdc.println(configErrorName(reason));
}

void Config::warnNvsLoad(ConfigError reason) const
{
    printNvsWarning(reason);
}

// ---------------------------------------------------------------------------
//  Config implementation
// ---------------------------------------------------------------------------

Config::Config()
    : _keypad(nullptr)
    , _testMode(false)
    , _lineLen(0)
{
}

void Config::begin(Keypad& keypad)
{
    _keypad = &keypad;
    // CDC is always available in the unified operating mode.
    Hw::cdc.begin(CDC_BAUD_RATE);
    loadFromNvs();
}

// --- Versioned NVS persistence ----------------------------------------------

static ConfigError configErrorFor(StorageResult result) { switch(result) { case StorageResult::OpenFailed:return ConfigError::NvsOpenFailed; case StorageResult::Missing:return ConfigError::NvsMissing; case StorageResult::SizeMismatch:return ConfigError::NvsSizeMismatch; case StorageResult::BadMagic:return ConfigError::NvsBadMagic; case StorageResult::BadVersion:return ConfigError::NvsBadVersion; case StorageResult::BadLength:return ConfigError::NvsBadLength; case StorageResult::BadCrc:return ConfigError::NvsBadCrc; case StorageResult::BadHidCode:return ConfigError::NvsBadHidCode; case StorageResult::WriteFailed:return ConfigError::NvsWriteFailed; case StorageResult::Loaded: break; } return ConfigError::NvsOpenFailed; }

void Config::loadFromNvs()
{
    const StorageResult result = loadBindings(*_keypad);
    if (result != StorageResult::Loaded && result != StorageResult::Missing) {
        warnNvsLoad(configErrorFor(result));
    }
}

bool Config::saveToNvs()
{
    return saveBindings(*_keypad) == StorageResult::Loaded;
}


void Config::processKeyEvent(const KeyEvent& event)
{
    if (!_testMode) return;

    const uint8_t keyCode = static_cast<uint8_t>(event.keyCode);
    Hw::cdc.print("OK test event=");
    Hw::cdc.print(event.type == KeyEventType::Press ? "PRESS" : "RELEASE");
    Hw::cdc.print(" key=0x");
    if (keyCode < 0x10) Hw::cdc.print('0');
    Hw::cdc.println(keyCode, HEX);
}

void Config::poll()
{
    // CDC commands and HID output are active together.
    // Non-blocking line accumulation.
    while (Hw::cdc.available()) {
        const int c = Hw::cdc.read();
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
        printInfo(Hw::cdc);
    } else if (nameEq(tok[0], "get_device")) {
        printDeviceDescription(*_keypad, Hw::cdc);
    } else if (nameEq(tok[0], "test")) {
        if (n == 1 || nameEq(tok[1], "on")) {
            _testMode = true;
            Hw::cdc.println("OK test=on");
        } else if (nameEq(tok[1], "off")) {
            _testMode = false;
            Hw::cdc.println("OK test=off");
        } else {
            printError(ConfigError::InvalidArgument, "usage", "test [on|off]");
        }
    } else if (nameEq(tok[0], "debounce")) {
        if (n == 1 || nameEq(tok[1], "get")) {
            Hw::cdc.print("OK debounce_samples="); Hw::cdc.println(_keypad->debounce());
        } else if (n >= 3 && nameEq(tok[1], "set")) {
            char* endp = nullptr; long value = strtol(tok[2], &endp, 10);
            if (*endp != '\0' || value < 1 || value > 255) {
                printError(ConfigError::InvalidValue, "debounce_range", "1..255");
            } else {
                _keypad->setDebounce(static_cast<uint8_t>(value));
                Hw::cdc.print("OK debounce_samples="); Hw::cdc.println(value);
            }
        } else {
            printError(ConfigError::InvalidArgument, "usage", "debounce [get|set N]");
        }
    } else if (nameEq(tok[0], "stats")) {
        if (n >= 2 && nameEq(tok[1], "clear")) {
            _keypad->clearStats();
            Hw::cdc.println("OK stats=cleared");
        } else if (n == 1) {
            Hw::cdc.print("OK stats scan="); Hw::cdc.print(_keypad->scanCount());
            Hw::cdc.print(" events="); Hw::cdc.print(_keypad->eventCount());
            Hw::cdc.print(" overflow="); Hw::cdc.print(_keypad->overflowCount());
            Hw::cdc.print(" queue_max="); Hw::cdc.println(_keypad->maxQueueDepth());
        } else {
            printError(ConfigError::InvalidArgument, "usage", "stats [clear]");
        }
    } else if (nameEq(tok[0], "help") || nameEq(tok[0], "?")) {
        printHelp(Hw::cdc);
    } else if (nameEq(tok[0], "list")) {
        printBindings(*_keypad, Hw::cdc);
    } else if (nameEq(tok[0], "boot") || nameEq(tok[0], "download")) {
        if (n != 1) {
            printError(ConfigError::InvalidArgument, "usage", "boot");
            return;
        }
        Hw::cdc.println("OK boot=download");
        Hw::cdc.flush();
        Hw::enterBootloader();
        return;
    } else if (nameEq(tok[0], "save")) {
        if (saveToNvs()) {
            Hw::cdc.println("OK saved");
        } else {
            printError(ConfigError::NvsWriteFailed);
        }
    } else if (nameEq(tok[0], "reset")) {
        _keypad->loadDefaultBindings();
        _keypad->setDebounce(DeviceProfile::DEFAULT_DEBOUNCE_SAMPLES);
        Hw::cdc.println("OK reset defaults_restored=true persisted=false");
    } else if (nameEq(tok[0], "bind")) {
        if (n < 3) {
            printError(ConfigError::InvalidArgument, "usage", "bind <slot> <key>");
            return;
        }
        // Parse slot (0..5) with full validation.
        char* endp = nullptr;
        long slot = strtol(tok[1], &endp, 10);
        if (*endp != '\0' || slot < 0 || slot >= Keypad::KEY_COUNT) {
            Hw::cdc.print("ERR code=");
            Hw::cdc.print(configErrorName(ConfigError::InvalidSlot));
            Hw::cdc.print(" expected=0..");
            Hw::cdc.println(Keypad::KEY_COUNT - 1);
            return;
        }
        HidKeycode code;
        if (!keyNameLookup(tok[2], code)) {
            printError(ConfigError::UnknownKey, "key", tok[2]);
            return;
        }
        _keypad->setBinding(static_cast<uint8_t>(slot), code);
        const char* nm = keyNameFor(code);
        Hw::cdc.print("OK slot ");
        Hw::cdc.print(slot);
        Hw::cdc.print(" -> ");
        Hw::cdc.println(nm ? nm : tok[2]);
    } else {
        printError(ConfigError::UnknownCommand, "command", tok[0]);
    }
}
