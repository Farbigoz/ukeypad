#ifndef UKEYPAD_TEST_PREFERENCES_H
#define UKEYPAD_TEST_PREFERENCES_H
#include <cstddef>
class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    void end() {}
    size_t getBytes(const char*, void*, size_t) { return 0; }
    size_t putBytes(const char*, const void*, size_t length) { return length; }
};
#endif
