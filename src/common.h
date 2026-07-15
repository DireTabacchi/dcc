#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define FNV_OFFSET_BASIS    0xcbf29ce484222325
#define FNV_PRIME           0x00000100000001B3

// Return the number of base-10 places this number contains.
int integer_len(int num);

size_t fnv1a_hash(char *data);

size_t fnv1a_hashn(char *data, size_t n);


#endif
