#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#include <stddef.h>
#include "dd_string.h"

typedef struct {
    String *strs;
    size_t len;
    size_t cap;
} StringArray;

void StringArray_init(StringArray *sa);
void StringArray_deinit(StringArray *sa);
void StringArray_append(StringArray *sa, String str);

#endif
