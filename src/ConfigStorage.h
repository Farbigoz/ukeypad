#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>

class Keypad;

// Result of a configuration load or save operation. Bindings and the shared
// debounce threshold are stored together; the CDC layer formats these results.
enum class StorageResult : uint8_t {
    Loaded,
    Missing,
    OpenFailed,
    SizeMismatch,
    BadMagic,
    BadVersion,
    BadLength,
    BadCrc,
    BadHidCode,
    WriteFailed,
};

// Load and validate the versioned binding/debounce record from NVS.
//
// On success, bindings and the shared debounce threshold are applied to
// `keypad`. The keypad is not modified when validation fails. Older records
// are intentionally rejected; they are not migrated.
StorageResult loadBindings(Keypad& keypad);

// Serialize the current keypad bindings and store them in NVS.
StorageResult saveBindings(const Keypad& keypad);

#endif // CONFIG_STORAGE_H
