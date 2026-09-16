#include <stdlib.h>
#include <string.h>

#include "dd_string.h"
#include "string_array.h"

void StringArray_init(StringArray *sa) {
    sa->cap = 2;
    sa->len = 0;
    sa->strs = (String *)calloc(sa->cap, sizeof(String));
}

void StringArray_deinit(StringArray *sa) {
    if (sa == NULL) return;
    if (sa->strs == NULL) return;

    for (size_t sa_idx = 0; sa_idx < sa->len; sa_idx++) {
        String_free(&sa->strs[sa_idx]);
    }

    free(sa->strs);
    sa->cap = 0;
    sa->len = 0;
}

void StringArray_append(StringArray *sa, String str) {
    if (sa == NULL) return;
    if (sa->strs == NULL) return;

    if (sa->len == sa->cap) {
        size_t old_cap = sa->cap;
        size_t new_cap = old_cap / 2 + old_cap;
        String *new_strs = calloc(new_cap, sizeof(String));
        memcpy(new_strs, sa->strs, old_cap*sizeof(String));
        sa->strs = new_strs;
        sa->cap = new_cap;
    }

    memcpy(&sa->strs[sa->len], &str, sizeof(String));
    sa->len += 1;
}
