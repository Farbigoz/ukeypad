#ifndef KEY_NAME_TABLE_H
#define KEY_NAME_TABLE_H

#include "HidKeycode.h"

bool keyNameLookup(const char* token, HidKeycode& out);
const char* keyNameFor(HidKeycode code);

#endif // KEY_NAME_TABLE_H
