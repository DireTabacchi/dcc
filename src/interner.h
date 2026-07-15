#ifndef INTERNER_H
#define INTERNER_H

#include "dd_string.h"

#define INTERNER_LOAD_FACTOR 0.7
#define INTERNER_MIN_CAP 64
//#define INTERNER_MIN_CAP 16

typedef enum internedStringStatus_ {
    ISS_EMPTY,
    ISS_OCCUPIED
} IntStrStatus;

typedef struct internedString_ {
    size_t hash;
    IntStrStatus status;
    String *str;
} InternedString;

typedef struct stringInterner_ {
    InternedString *strs;
    size_t load;
    size_t cap;
} StrInterner;

void StrInterner_init(StrInterner *si);
void StrInterner_deinit(StrInterner *si);
const String *StrInterner_intern(StrInterner *si, String str);

#endif // INTERNER_H
