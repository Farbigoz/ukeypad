#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "HidKeycode.h"

class Keypad;  // forward declaration

// ---------------------------------------------------------------------------
//  Config — runtime bind configuration over the USB-CDC Serial port.
//
//  Mode selection is at boot: hold ANY switch while plugging in -> config
//  mode (Serial listens for commands, key scanning is OFF). Plug in normally
//  -> keyboard mode (scanning + HID, Serial inert). One firmware, two modes.
//
//  Bindings persist in NVS, so a saved config survives power cycles without
//  reflashing. Only the HID keycode per slot is configurable; physical GPIOs
//  are fixed by the hardware.
//
//  Text protocol (line-oriented, case-insensitive), one command per line:
//    bind <slot> <key>   e.g. `bind 0 Z`  `bind 4 F13`  `bind 2 5`
//    list                print current bindings
//    save                write current bindings to NVS
//    reset               restore compiled-in defaults (in RAM only; `save` to persist)
//    info                show firmware and hardware information
//    test [on|off]       monitor raw GPIO transitions
//    debounce [get|set N] read/set debounce samples
//    stats [clear]       show/clear scan and queue counters
//    help                show usage
//  <slot> is 0..5. <key> is a name from the HidKeycode set (A..Z, 0..9,
//  F1..F24, ENTER, SPACE, arrows, etc.).
// ---------------------------------------------------------------------------
class Config {
public:
    Config();

    // Load bindings from NVS (overriding Keypad's defaults). If configMode,
    // prepare the Serial command interface (banner printed once a terminal
    // is attached).
    void begin(Keypad& keypad, bool configMode);

    // Non-blocking: read available Serial bytes, accumulate a line, dispatch.
    // Only does work in config mode. Safe to call from the main loop.
    void poll();

    // Drain debounced Keypad events in config mode. Events never reach HID;
    // when test mode is enabled they are reported over CDC instead.
    void processKeyEvents();

    bool isConfigMode() const { return _configMode; }

private:
    void        printBanner();
    void        printBindings();
    void        printHelp();
    void        handleLine();
    bool        parseKey(const char* tok, HidKeycode& out) const;
    void        loadFromNvs();
    void        saveToNvs();

    Keypad*     _keypad;
    bool        _configMode;
    bool        _bannerShown;
    bool        _testMode;
    char        _line[48];
    uint8_t     _lineLen;
    uint32_t    _lastStatsMs;
};

#endif // CONFIG_H
