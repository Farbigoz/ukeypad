#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>

class Keypad;

// Result of a binding configuration load or save operation. The storage layer
// returns typed results; the CDC protocol layer is responsible for formatting
// them as OK/ERR/WARN responses.
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

// Load and validate the versioned binding record from NVS.
//
// On success, bindings are applied to `keypad` and Loaded is returned. The
// keypad is not modified when any record validation check fails. The old raw
// six-byte format is intentionally rejected; it is not migrated.
StorageResult loadBindings(Keypad& keypad);

// Serialize the current keypad bindings and store them in NVS.
StorageResult saveBindings(const Keypad& keypad);

#endif // CONFIG_STORAGE_H
