#include "Config.h"
#include "Keypad.h"
#include <Arduino.h>
#include "DeviceMetadata.h"
#include "KeyNameTable.h"
#include "ConfigStorage.h"

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
    Serial.print("ERR code=");
    Serial.println(configErrorName(error));
}

static void printError(ConfigError error, const char* field, const char* value)
{
    Serial.print("ERR code=");
    Serial.print(configErrorName(error));
    Serial.print(' ');
    Serial.print(field);
    Serial.print('=');
    Serial.println(value);
}

static void printError(ConfigError error, const char* field, long value)
{
    Serial.print("ERR code=");
    Serial.print(configErrorName(error));
    Serial.print(' ');
    Serial.print(field);
    Serial.print('=');
    Serial.println(value);
}

static void printNvsWarning(ConfigError reason, bool enabled)
{
    if (enabled) {
        Serial.print("WARN code=NVS_LOAD_FAILED reason=");
        Serial.println(configErrorName(reason));
    }
}

void Config::warnNvsLoad(ConfigError reason) const
{
    printNvsWarning(reason, _configMode);
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

void Config::printBanner() const
{
    Serial.println();
    Serial.println("=== USB HID keypad — CONFIG MODE ===");
    Serial.println("Hold any switch while plugging in to enter this mode.");
    Serial.println("Type 'help' for commands. Reboot (RESET) to play.");
    Serial.println();
    printBindings(*_keypad, Serial);
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
        Serial.begin(CDC_BAUD_RATE);
        // Banner is printed from poll() once a terminal is attached, because
        // the host may not have enumerated the CDC port yet at this instant.
    }
}

// --- Versioned NVS persistence ----------------------------------------------

static ConfigError configErrorFor(StorageResult result) { switch(result) { case StorageResult::OpenFailed:return ConfigError::NvsOpenFailed; case StorageResult::Missing:return ConfigError::NvsMissing; case StorageResult::SizeMismatch:return ConfigError::NvsSizeMismatch; case StorageResult::BadMagic:return ConfigError::NvsBadMagic; case StorageResult::BadVersion:return ConfigError::NvsBadVersion; case StorageResult::BadLength:return ConfigError::NvsBadLength; case StorageResult::BadCrc:return ConfigError::NvsBadCrc; case StorageResult::BadHidCode:return ConfigError::NvsBadHidCode; case StorageResult::WriteFailed:return ConfigError::NvsWriteFailed; case StorageResult::Loaded: break; } return ConfigError::NvsOpenFailed; }

void Config::loadFromNvs() { const StorageResult result=loadBindings(*_keypad); if(result!=StorageResult::Loaded && result!=StorageResult::Missing) warnNvsLoad(configErrorFor(result)); }
bool Config::saveToNvs() { return saveBindings(*_keypad)==StorageResult::Loaded; }


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
        printInfo(Serial);
    } else if (nameEq(tok[0], "get_device")) {
        printDeviceDescription(*_keypad, Serial);
    } else if (nameEq(tok[0], "test")) {
        if (n == 1 || nameEq(tok[1], "on")) {
            _testMode = true;
            Serial.println("OK test=on");
        } else if (nameEq(tok[1], "off")) {
            _testMode = false;
            Serial.println("OK test=off");
        } else {
            printError(ConfigError::InvalidArgument, "usage", "test [on|off]");
        }
    } else if (nameEq(tok[0], "debounce")) {
        if (n == 1 || nameEq(tok[1], "get")) {
            Serial.print("OK debounce_samples="); Serial.println(_keypad->debounce());
        } else if (n >= 3 && nameEq(tok[1], "set")) {
            char* endp = nullptr; long value = strtol(tok[2], &endp, 10);
            if (*endp != '\0' || value < 1 || value > 255) {
                printError(ConfigError::InvalidValue, "debounce_range", "1..255");
            } else {
                _keypad->setDebounce(static_cast<uint8_t>(value));
                Serial.print("OK debounce_samples="); Serial.println(value);
            }
        } else {
            printError(ConfigError::InvalidArgument, "usage", "debounce [get|set N]");
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
            printError(ConfigError::InvalidArgument, "usage", "stats [clear]");
        }
    } else if (nameEq(tok[0], "help") || nameEq(tok[0], "?")) {
        printHelp(Serial);
    } else if (nameEq(tok[0], "list")) {
        printBindings(*_keypad, Serial);
    } else if (nameEq(tok[0], "save")) {
        if (saveToNvs()) {
            Serial.println("OK saved");
        } else {
            printError(ConfigError::NvsWriteFailed);
        }
    } else if (nameEq(tok[0], "reset")) {
        _keypad->loadDefaultBindings();
        Serial.println("OK reset defaults_restored=true persisted=false");
    } else if (nameEq(tok[0], "bind")) {
        if (n < 3) {
            printError(ConfigError::InvalidArgument, "usage", "bind <slot> <key>");
            return;
        }
        // Parse slot (0..5) with full validation.
        char* endp = nullptr;
        long slot = strtol(tok[1], &endp, 10);
        if (*endp != '\0' || slot < 0 || slot >= Keypad::KEY_COUNT) {
            printError(ConfigError::InvalidSlot, "expected", "0..5");
            return;
        }
        HidKeycode code;
        if (!keyNameLookup(tok[2], code)) {
            printError(ConfigError::UnknownKey, "key", tok[2]);
            return;
        }
        _keypad->setBinding(static_cast<uint8_t>(slot), code);
        const char* nm = keyNameFor(code);
        Serial.print("OK slot ");
        Serial.print(slot);
        Serial.print(" -> ");
        Serial.println(nm ? nm : tok[2]);
    } else {
        printError(ConfigError::UnknownCommand, "command", tok[0]);
    }
}
