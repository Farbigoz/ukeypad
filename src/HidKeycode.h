#ifndef HIDKEYCODE_H
#define HIDKEYCODE_H

#include <stdint.h>

// ---------------------------------------------------------------------------
//  HidKeycode — named USB HID "Keyboard/Keypad" usage codes.
//
//  Why a dedicated enum instead of TinyUSB's HID_KEY_* macros?
//  TinyUSB defines HID_KEY_Z, HID_KEY_F13, ... in <class/hid/hid.h>, but
//  that header is part of the USB stack. Including it in Keypad would break
//  the layering rule "Keypad knows nothing about USB". This enum gives us
//  human-readable, type-safe key labels with zero USB dependency, so the
//  binding table reads as HidKeycode::Z / HidKeycode::F13 instead of magic
//  hex. The values are the raw usage IDs from the USB HID Usage Table
//  (page 0x07 "Keyboard/Keypad"), matching TinyUSB's HID_KEY_* exactly.
//
//  Names mirror the HID_KEY_<NAME> macros with the prefix stripped:
//    HID_KEY_ARROW_RIGHT -> ArrowRight, HID_KEY_BRACKET_LEFT -> BracketLeft,
//  etc. Add any missing key here when new binds/macros need it.
// ---------------------------------------------------------------------------

enum class HidKeycode : uint8_t {
    // --- Reserved / none ---
    None = 0x00,

    // --- Letters A-Z (0x04..0x1D) ---
    A = 0x04, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // --- Digits 1-0 (0x1E..0x27) ---
    Num1 = 0x1E, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,

    // --- Control / editing (0x28..0x38) ---
    Enter        = 0x28,
    Escape       = 0x29,
    Backspace    = 0x2A,
    Tab          = 0x2B,
    Space        = 0x2C,
    Minus        = 0x2D,
    Equal        = 0x2E,
    BracketLeft  = 0x2F,
    BracketRight = 0x30,
    Backslash    = 0x31,
    Europe1      = 0x32,   // non-US # and ~
    Semicolon    = 0x33,
    Apostrophe   = 0x34,
    Grave        = 0x35,   // ` and ~
    Comma        = 0x36,
    Period       = 0x37,
    Slash        = 0x38,

    // --- Locks / function row (0x39..0x45) ---
    CapsLock = 0x39,
    F1  = 0x3A, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // --- Navigation cluster (0x46..0x4E) ---
    PrintScreen = 0x46,
    ScrollLock  = 0x47,
    Pause       = 0x48,
    Insert      = 0x49,
    Home        = 0x4A,
    PageUp      = 0x4B,
    Delete      = 0x4C,
    End         = 0x4D,
    PageDown    = 0x4E,

    // --- Arrows (0x4F..0x52) ---
    ArrowRight = 0x4F,
    ArrowLeft  = 0x50,
    ArrowDown  = 0x51,
    ArrowUp    = 0x52,

    // --- Numpad (0x53..0x63) ---
    NumLock        = 0x53,
    KeypadDivide   = 0x54,
    KeypadMultiply = 0x55,
    KeypadSubtract = 0x56,
    KeypadAdd      = 0x57,
    KeypadEnter    = 0x58,
    Keypad1        = 0x59,
    Keypad2        = 0x5A,
    Keypad3        = 0x5B,
    Keypad4        = 0x5C,
    Keypad5        = 0x5D,
    Keypad6        = 0x5E,
    Keypad7        = 0x5F,
    Keypad8        = 0x60,
    Keypad9        = 0x61,
    Keypad0        = 0x62,
    KeypadDecimal  = 0x63,

    // --- Misc (0x64..0x67) ---
    Europe2     = 0x64,   // non-US \ and |
    Application = 0x65,   // Menu / Context
    Power       = 0x66,
    KeypadEqual = 0x67,

    // --- F13-F24 (0x68..0x73) ---
    F13 = 0x68, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,

    // --- Application / media (0x74..0x83) ---
    Execute          = 0x74,
    Help             = 0x75,
    Menu             = 0x76,
    Select           = 0x77,
    Stop             = 0x78,
    Again            = 0x79,
    Undo             = 0x7A,
    Cut              = 0x7B,
    Copy             = 0x7C,
    Paste            = 0x7D,
    Find             = 0x7E,
    Mute             = 0x7F,
    VolumeUp         = 0x80,
    VolumeDown       = 0x81,
    LockingCapsLock  = 0x82,
    LockingNumLock   = 0x83,
    LockingScrollLock= 0x84,

    // --- Keypad extras / intl (0x85..0x8F) ---
    KeypadComma       = 0x85,
    KeypadEqualSign   = 0x86,
    Kanji1            = 0x87,
    Kanji2            = 0x88,
    Kanji3            = 0x89,
    Kanji4            = 0x8A,
    Kanji5            = 0x8B,
    Kanji6            = 0x8C,
    Kanji7            = 0x8D,
    Kanji8            = 0x8E,
    Kanji9            = 0x8F,

    // --- Language / reserved (0x90..0x98) ---
    Lang1 = 0x90, Lang2, Lang3, Lang4, Lang5, Lang6, Lang7, Lang8, Lang9,

    // --- Misc controls (0x99..0xA4) ---
    AlternateErase  = 0x99,
    SysreqAttention = 0x9A,
    Cancel          = 0x9B,
    Clear           = 0x9C,
    Prior           = 0x9D,
    Return          = 0x9E,
    Separator       = 0x9F,
    Out             = 0xA0,
    Oper            = 0xA1,
    ClearAgain      = 0xA2,
    CrselProps      = 0xA3,
    Exsel           = 0xA4,

    // --- Modifiers (0xE0..0xE7) ---
    // Sent as modifier bits in a HID report, but the usage codes exist too;
    // pressRaw()/releaseRaw() handle 0xE0..0xE7 by setting modifier flags.
    ControlLeft  = 0xE0,
    ShiftLeft    = 0xE1,
    AltLeft      = 0xE2,
    GuiLeft      = 0xE3,   // Win/Cmd
    ControlRight = 0xE4,
    ShiftRight   = 0xE5,
    AltRight     = 0xE6,   // AltGr
    GuiRight     = 0xE7
};

#endif // HIDKEYCODE_H
