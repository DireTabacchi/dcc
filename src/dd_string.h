#ifndef DD_STRING_H
#define DD_STRING_H

#include <stddef.h>
#include <sys/types.h>

typedef struct string_ {
    char *cstr;     // NULL-terminated string (C-string, a.k.a. cstr)
    size_t len;     // Length of cstr (minus NULL byte)
} String;

/* Create a String object from a NULL-terminated C-string. */
String String_init_cstr(const char *str);

// Create a String object with a known byte length `len`, minus the NULL byte.
// The returned String will have enough memory to hold `len` characters plus a NULL byte.
String String_init_length(size_t len);
void String_free(String *str);

static inline ssize_t dd_strlen(const char *str) {
    if (str == NULL) {
        return 0;
    }

    const char* begin = str;
    while (*str != 0) {
        str++;
    }

    return str - begin;
}

#endif // DD_STRING_H
