#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dd_string.h"
#include "comp_driver.h"
#include "sem_analysis.h"

// TODO: implement with interned strings

void SymTable_init(SymTable *table) {
    table->cap = TABLE_MIN_CAP;
    table->load = 0;
    table->syms = (SymEntry *)calloc(table->cap, sizeof(SymEntry));
}

void SymTable_deinit(SymTable *table) {
    for (size_t idx = 0; idx < table->cap; idx++) {
        if (table->syms[idx].status == STE_OCCUPIED) {
            //String_free(&table->syms[idx].key);
            //String_free(&table->syms[idx].value);
        }
    }
    free(table->syms);
}

static void SymTable_insert_raw(SymTable *table, String *key, String *value, size_t hash) {
    size_t idx = hash % table->cap;

    printf("[SymTable_insert] `%s` calculated index in hashtable is %ld\n", key->cstr, idx);

    while (table->syms[idx].status == STE_OCCUPIED) {
        if (table->syms[idx].hash == hash && strcmp(table->syms[idx].key->cstr, key->cstr) == 0) {
            table->syms[idx].value = value;
            return;
        }
        idx = (idx + 1) % table->cap;
    }

    printf("[SymTable_insert] `%s` hashed to 0x%016lX\n", key->cstr, hash);
    printf("[SymTable_insert] `%s` index in hashtable is %ld\n", key->cstr, idx);
    printf("[SymTable_insert] unique varname is `%s`\n", value->cstr);

    table->syms[idx].hash = hash;
    table->syms[idx].status = STE_OCCUPIED;
    table->syms[idx].key = key;
    table->syms[idx].value = value;
    table->load += 1;
}

void SymTable_insert(SymTable *table, String *key, String *value) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        printf("[SymTable_insert] resizing table\n");
        size_t old_cap = table->cap;
        size_t new_cap = old_cap + (old_cap >> 1);
        printf("[SymTable_insert] old_cap: %ld\n[SymTable_insert] new_cap: %ld\n", old_cap, new_cap);
        SymEntry *old_entries = table->syms;
        table->syms = (SymEntry *)calloc(new_cap, sizeof(SymEntry));
        table->cap = new_cap;
        for (size_t se_idx = 0; se_idx < old_cap; se_idx++) {
            if (old_entries[se_idx].status == STE_OCCUPIED) {
                SymTable_insert_raw(table,
                    old_entries[se_idx].key,
                    old_entries[se_idx].value,
                    old_entries[se_idx].hash);
            }
        }
        free(old_entries);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymTable_insert_raw(table, key, value, hash);
}

bool SymTable_contains(SymTable *table, char *key) {
    size_t hash = fnv1a_hash(key);
    size_t idx = hash % table->cap;
    size_t start_idx = idx;

    while (table->syms[idx].status == STE_OCCUPIED) {
        if (table->syms[idx].hash == hash && strcmp(table->syms[idx].key->cstr, key) == 0) {
            return true;
        }
        idx = (idx + 1) % table->cap;
        if (start_idx == idx) break;
    }

    return false;
}

String *SymTable_get(SymTable *table, char *key) {
    size_t hash = fnv1a_hash(key);
    size_t idx = hash % table->cap;
    size_t start_idx = idx;

    while (table->syms[idx].status == STE_OCCUPIED) {
        if (table->syms[idx].hash == hash && strcmp(table->syms[idx].key->cstr, key) == 0) {
            return table->syms[idx].value;
        }
        idx = (idx + 1) % table->cap;
        if (start_idx == idx) break;
    }

    return NULL;
}

void SymTable_print(SymTable *table) {
    if (table == NULL) {
        puts("table is NULL");
        return;
    }

    for (size_t idx = 0; idx < table->cap; idx++) {
        if (table->syms[idx].status == STE_OCCUPIED) {
            printf("[%03ld] -> (%s, %s), 0x%016lX\n", idx, table->syms[idx].key->cstr, table->syms[idx].value->cstr, table->syms[idx].hash);
        }
    }
}

static String *create_unique_varname(CompDriver *cd, String varname) {
    int digit_len = integer_len(cd->parser.var_count);

    String uvar_string = String_init_length(varname.len + digit_len);
    snprintf(uvar_string.cstr, uvar_string.len+1, "%s%ld", varname.cstr, cd->parser.var_count);
    cd->parser.var_count += 1;
    String *uvar_name = (String *)StrInterner_intern(&cd->str_table, uvar_string);
    String_free(&uvar_string);
    return uvar_name;
}

void sem_analyze(CompDriver *cd) {
    SymTable table = {0};
    SymTable_init(&table);
    for (size_t tok_idx = 0; tok_idx < cd->tokenizer.tokens.len; tok_idx++) {
        Token tok = cd->tokenizer.tokens.toks[tok_idx];
        if (tok.kind == TOKEN_IDENTIFIER) {
            if (!SymTable_contains(&table, tok.text->cstr)) {
                String *unique_varname = create_unique_varname(cd, *tok.text);
                SymTable_insert(&table, tok.text, unique_varname);
            }
        }
    }
    SymTable_print(&table);
    SymTable_deinit(&table);
}
