#include "common.h"

int integer_len(int num) {
    if (num == 0) {
        return 1;
    }

    int digit_len = 0;
    while (num != 0) {
        num /= 10;
        digit_len += 1;
    }
    return digit_len;
}

size_t fnv1a_hash(char *cstr) {
    size_t hash = FNV_OFFSET_BASIS;
    while (*cstr != 0) {
        hash ^= *cstr;
        hash *= FNV_PRIME;
        cstr++;
    }
    return hash;
}

size_t fnv1a_hashn(char *cstr, size_t n) {
    size_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < n && *cstr != 0; i++) {
        hash ^= *cstr;
        hash *= FNV_PRIME;
        cstr++;
    }
    return hash;
}
