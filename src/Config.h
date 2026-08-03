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

    void begin(Keypad& keypad, bool configMode);
    void poll();
    void processKeyEvent(const KeyEvent& event);

    bool isConfigMode() const { return _configMode; }

private:
    void handleLine();
    void loadFromNvs();
    bool saveToNvs();
    void warnNvsLoad(ConfigError reason) const;
    void printBanner() const;

    Keypad* _keypad;
    bool _configMode;
    bool _bannerShown;
    bool _testMode;
    char _line[48];
    uint8_t _lineLen;
    uint32_t _lastStatsMs;
};

#endif // CONFIG_H
