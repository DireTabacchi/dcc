#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dd_string.h"
#include "interner.h"

void StrInterner_init(StrInterner *si) {
    si->cap = INTERNER_MIN_CAP;
    si->load = 0;
    si->strs = (InternedString *)calloc(si->cap, sizeof(InternedString));
}

void StrInterner_deinit(StrInterner *si) {
    if (si == NULL) return;
    if (si->strs == NULL) return;

    for (size_t si_idx = 0; si_idx < si->cap; si_idx++) {
        InternedString int_str = si->strs[si_idx];
        if (int_str.status == ISS_OCCUPIED) {
            String_free(int_str.str);
            free(int_str.str);
        }
    }
    free(si->strs);
}

static String *StrInterner_intern_raw(StrInterner *si, String str, size_t hash) {
    size_t idx = hash % si->cap;

    while (si->strs[idx].status == ISS_OCCUPIED) {
        if (si->strs[idx].hash == hash && String_equal(*si->strs[idx].str, str)) {
            return si->strs[idx].str;
        }
        idx = (idx + 1) % si->cap;
    }

    InternedString *slot = &si->strs[idx];
    slot->status = ISS_OCCUPIED;
    slot->hash = hash;
    slot->str = malloc(sizeof(String));
    *slot->str = String_init_length(str.len);
    memcpy(slot->str->cstr, str.cstr, str.len);
    si->load += 1;
    return slot->str;
}

static void StrInterner_resize(StrInterner *si) {
    size_t old_cap = si->cap;
    size_t new_cap = si->cap + (si->cap >> 1);

    si->cap = new_cap;
    InternedString *old_table = si->strs;
    si->strs = (InternedString *)calloc(new_cap, sizeof(InternedString));

    for (size_t old_idx = 0; old_idx < old_cap; old_idx++) {
        if (old_table[old_idx].status == ISS_OCCUPIED) {
            InternedString old = old_table[old_idx];

            size_t idx = old.hash % si->cap;
            while (si->strs[idx].status == ISS_OCCUPIED) {
                idx = (idx + 1) % si->cap;
            }

            si->strs[idx] = old;
        }
    }

    free(old_table);
}

const String *StrInterner_intern(StrInterner *si, String str) {
    if (si == NULL) return NULL;
    if (si->strs == NULL) return NULL;

    if ((float)(si->load + 1) / si->cap > INTERNER_LOAD_FACTOR) {
        StrInterner_resize(si);
    }

    size_t hash = fnv1a_hashn(str.cstr, str.len);
    return StrInterner_intern_raw(si, str, hash);
}
