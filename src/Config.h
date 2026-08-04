#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "HidKeycode.h"
#include "Keypad.h"

class Keypad;

enum class ConfigError : uint8_t {
    InvalidArgument,
    InvalidValue,
    InvalidSlot,
    UnknownKey,
    UnknownCommand,
    NvsWriteFailed,
    NvsOpenFailed,
    NvsMissing,
    NvsSizeMismatch,
    NvsBadMagic,
    NvsBadVersion,
    NvsBadLength,
    NvsBadCrc,
    NvsBadHidCode,
};

const char* configErrorName(ConfigError error);

class Config {
public:
    Config();

    void begin(Keypad& keypad);
    void poll();
    void processKeyEvent(const KeyEvent& event);

private:
    void handleLine();
    void loadFromNvs();
    bool saveToNvs();
    void warnNvsLoad(ConfigError reason) const;

    Keypad* _keypad;
    bool _testMode;
    char _line[48];
    uint8_t _lineLen;
};

#endif // CONFIG_H
