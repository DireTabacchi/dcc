#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dd_string.h"
#include "sym_table.h"

void SymTable_init(SymTable *table) {
    table->cap = TABLE_MIN_CAP;
    table->load = 0;
    table->syms = (SymEntry *)calloc(table->cap, sizeof(SymEntry));
}

void SymTable_deinit(SymTable *table) {
    for (size_t idx = 0; idx < table->cap; idx++) {
        if (table->syms[idx].status == STE_OCCUPIED) {
        }
    }
    free(table->syms);
}

static void SymTable_insert_raw(SymTable *table, SymEntry se, size_t hash) {
    size_t idx = hash % table->cap;

    while (table->syms[idx].status == STE_OCCUPIED) {
        SymEntry *table_ent = &table->syms[idx];
        if (table_ent->type == se.type) {
                if (table_ent->hash == hash &&
                    strcmp(table_ent->key->cstr, se.key->cstr) == 0)
                {
                    switch (table_ent->type) {
                    case SYMTYPE_MAPPING:
                        table_ent->as.mapping.name = se.as.mapping.name;
                        return;
                    case SYMTYPE_LABEL:
                        table_ent->as.lbl.status = se.as.lbl.status;
                        return;
                    }
                }
        }
        idx = (idx + 1) % table->cap;
    }

    table->syms[idx].hash = hash;
    table->syms[idx].status = STE_OCCUPIED;
    table->syms[idx].pos = se.pos;
    table->syms[idx].key = se.key;
    table->syms[idx].type = se.type;
    switch (se.type) {
    case SYMTYPE_MAPPING:
        table->syms[idx].as.mapping.name = se.as.mapping.name;
        break;
    case SYMTYPE_LABEL:
        table->syms[idx].as.lbl.status = se.as.lbl.status;
        break;
    }
    table->load += 1;
}

static void SymTable_resize(SymTable *table) {
        size_t old_cap = table->cap;
        size_t new_cap = old_cap + (old_cap >> 1);
        SymEntry *old_entries = table->syms;
        table->syms = (SymEntry *)calloc(new_cap, sizeof(SymEntry));
        table->cap = new_cap;
        for (size_t se_idx = 0; se_idx < old_cap; se_idx++) {
            if (old_entries[se_idx].status == STE_OCCUPIED) {
                SymTable_insert_raw(table,
                    old_entries[se_idx],
                    old_entries[se_idx].hash);
            }
        }
        free(old_entries);
}

void SymTable_insert_mapping(SymTable *table, Position pos, const String *key, const String *value) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(key->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_MAPPING, .key = key,
        .pos = pos, .as.mapping.name = value
    };
    SymTable_insert_raw(table, se, hash);
}

void SymTable_insert_label(SymTable *table, Position pos, const String *txt, LabelStatus status) {
    if ((float)(table->load + 1) / table->cap > TABLE_LOAD_FACTOR) {
        SymTable_resize(table);
    }

    size_t hash = fnv1a_hash(txt->cstr);
    SymEntry se = (SymEntry){
        .hash = hash, .status = STE_OCCUPIED, .type = SYMTYPE_LABEL, .key = txt,
        .pos = pos, .as.lbl.status = status
    };
    SymTable_insert_raw(table, se, hash);
}

bool SymTable_contains(SymTable *table, char *key, SymType type) {
    return SymTable_get(table, key, type) != NULL;
}

SymEntry *SymTable_get(SymTable *table, char *key, SymType type) {
    size_t hash = fnv1a_hash(key);
    size_t idx = hash % table->cap;
    size_t start_idx = idx;

    while (table->syms[idx].status == STE_OCCUPIED) {
        if (table->syms[idx].type == type &&
            table->syms[idx].hash == hash &&
            strcmp(table->syms[idx].key->cstr, key) == 0)
        {
            return &table->syms[idx];
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
            SymEntry se = table->syms[idx];
            switch (se.type) {
            case SYMTYPE_MAPPING:
                printf("[%03ld] -> (%s, %s), 0x%016lX\n", idx,
                    table->syms[idx].key->cstr,
                    table->syms[idx].as.mapping.name->cstr, table->syms[idx].hash);
                break;
            case SYMTYPE_LABEL:
                printf("[%03ld] -> (%s, %d), 0x%016lX\n", idx,
                    table->syms[idx].key->cstr,
                    table->syms[idx].as.lbl.status, table->syms[idx].hash);
                break;
            }
        }
    }
}

